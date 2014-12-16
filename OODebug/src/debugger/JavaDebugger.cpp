/***********************************************************************************************************************
**
** Copyright (c) 2011, 2014 ETH Zurich
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
** following conditions are met:
**
**    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following
**      disclaimer.
**    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
**      following disclaimer in the documentation and/or other materials provided with the distribution.
**    * Neither the name of the ETH Zurich nor the names of its contributors may be used to endorse or promote products
**      derived from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
** INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
** WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
***********************************************************************************************************************/

#include "JavaDebugger.h"

#include "ReferenceFinder.h"
#include "../run_support/JavaRunner.h"
#include "jdwp/messages/AllMessages.h"
#include "jdwp/Location.h"

#include "JavaExport/src/exporter/JavaExporter.h"

#include "ModelBase/src/nodes/Node.h"

#include "OOModel/src/declarations/Method.h"
#include "OOModel/src/declarations/Class.h"
#include "OOModel/src/declarations/Module.h"
#include "OOModel/src/declarations/Project.h"
#include "OOModel/src/expressions/Expression.h"
#include "OOModel/src/expressions/types/TypeExpression.h"
#include "OOModel/src/expressions/types/PrimitiveTypeExpression.h"
#include "OOModel/src/expressions/types/ArrayTypeExpression.h"
#include "OOModel/src/expressions/types/ClassTypeExpression.h"
#include "OOModel/src/expressions/VariableDeclarationExpression.h"
#include "OOModel/src/types/PrimitiveType.h"
#include "OOModel/src/statements/ExpressionStatement.h"
#include "OOModel/src/statements/DeclarationStatement.h"

#include "VisualizationBase/src/items/Item.h"
#include "VisualizationBase/src/overlays/OverlayAccessor.h"
#include "VisualizationBase/src/overlays/MessageOverlay.h"

namespace OODebug {

const QString JavaDebugger::BREAKPOINT_OVERLAY_GROUP{"Breakpoint overlay"};

JavaDebugger& JavaDebugger::instance()
{
	static JavaDebugger instance;
	return instance;
}

void JavaDebugger::debugTree(Model::TreeManager* manager, const QString& pathToProjectContainerDirectory)
{
	auto project = DCast<OOModel::Project>(manager->root());
	Q_ASSERT(project);

	JavaRunner::runTree(manager, pathToProjectContainerDirectory, true);

	// All previously set breakpoints have to be unset again.
	unsetBreakpoints_ << setBreakpoints_.values();
	setBreakpoints_.clear();

	exportMap_ = JavaExport::JavaExporter::exportMaps().map(project);
	debugConnector_.connect();
}

bool JavaDebugger::toggleBreakpoint(Visualization::Item* target, QKeyEvent* event)
{
	if (event->modifiers() == Qt::NoModifier && (event->key() == Qt::Key_F8))
	{
		if (auto overlay = breakpointOverlayOf(target))
		{
			if (currentBreakpointItem_ == target) currentBreakpointItem_ = nullptr;
			target->scene()->removeOverlay(overlay);
			if (debugConnector_.vmAlive())
			{
				int index = unsetBreakpoints_.indexOf(target);
				if (index > 0)
				{
					unsetBreakpoints_.removeAt(index);
				}
				else
				{
					qint32 breakpointId = setBreakpoints_.key(target);
					debugConnector_.clearBreakpoint(breakpointId);
					setBreakpoints_.remove(breakpointId);
				}
			}
		}
		else
		{
			addBreakpointOverlay(target);
			if (debugConnector_.vmAlive())
			{
				auto node = target->node();
				if (isParentClassLoaded(node))
				{
					qint32 requestId = debugConnector_.setBreakpoint(nodeToLocation(node));
					setBreakpoints_[requestId] = target;
				}
				else
				{
					unsetBreakpoints_ << target;
					breaktAtParentClassLoad(node);
				}
			}
			else
			{
				unsetBreakpoints_ << target;
			}
		}
		return true;
	}
	return false;
}

bool JavaDebugger::resume(Visualization::Item*, QKeyEvent* event)
{
	if (event->modifiers() == Qt::NoModifier && (event->key() == Qt::Key_F6))
	{
		debugConnector_.resume();
		if (currentBreakpointItem_)
		{
			if (auto overlay = breakpointOverlayOf(currentBreakpointItem_))
			{
				// unset the breakpoint
				overlay->setStyle(Visualization::MessageOverlay::itemStyles().get("default"));
			}
			else
			{
				// there is no current breakpoint no more
				currentBreakpointItem_ = nullptr;
			}
		}
		return true;
	}
	return false;
}

bool JavaDebugger::trackVariable(Visualization::Item* target, QKeyEvent* event)
{
	if (event->modifiers() == Qt::NoModifier && (event->key() == Qt::Key_F12))
	{
		auto node = target->node();
		Q_ASSERT(node);
		OOModel::VariableDeclaration* variableDeclaration = nullptr;
		if (auto exprStmt = DCast<OOModel::ExpressionStatement>(node))
		{
			if (auto varDeclarationExpr = DCast<OOModel::VariableDeclarationExpression>(exprStmt->expression()))
				variableDeclaration = varDeclarationExpr->decl();
		}
		else if (auto declStmt = DCast<OOModel::DeclarationStatement>(node))
		{
			variableDeclaration = DCast<OOModel::VariableDeclaration>(declStmt->declaration());
		}
		if (!variableDeclaration) return false;

		ReferenceFinder refFinder;
		refFinder.setSearchNode(node);
		auto containingMethod = node->firstAncestorOfType<OOModel::Method>();
		refFinder.visit(containingMethod);
		// TODO: use the references to track values!
		qDebug() << refFinder.references().length();
		return true;
	}
	return false;
}

JavaDebugger::JavaDebugger()
{
	debugConnector_.addEventListener(Protocol::EventKind::CLASS_PREPARE, [this] (Event e) { handleClassPrepare(e);});
	debugConnector_.addEventListener(Protocol::EventKind::BREAKPOINT,
												[this] (Event e) { handleBreakpoint(e.breakpoint()); });
	debugConnector_.addEventListener(Protocol::EventKind::VM_START, [this] (Event e) { handleVMStart(e); });
}

void JavaDebugger::addBreakpointOverlay(Visualization::Item* target)
{
	// TODO: Use a custom overlay for breakpoints.
	auto scene = target->scene();
	auto overlayGroup = scene->overlayGroup(BREAKPOINT_OVERLAY_GROUP);

	if (!overlayGroup) overlayGroup = scene->addOverlayGroup(BREAKPOINT_OVERLAY_GROUP);
	auto overlay = new Visualization::MessageOverlay(target,
																	 [](Visualization::MessageOverlay *){
			return QString("BP");
});
	overlayGroup->addOverlay(makeOverlay(overlay));
}

QString JavaDebugger::jvmSignatureFor(OOModel::Class* theClass)
{
	// from JNI spec fully qualified class: http://docs.oracle.com/javase/1.5.0/docs/guide/jni/spec/types.html#wp16432
	QString signature = fullNameFor(theClass, '/');
	signature.prepend("L").append(";");
	return signature;
}

QString JavaDebugger::fullNameFor(OOModel::Class* theClass, QChar delimiter)
{
	QString fullName = theClass->name();
	auto module = theClass->firstAncestorOfType<OOModel::Module>();
	while (module)
	{
		fullName.prepend(module->name() + delimiter);
		module = module->firstAncestorOfType<OOModel::Module>();
	}
	return fullName;
}

bool JavaDebugger::isParentClassLoaded(Model::Node* node)
{
	auto containerClass = node->firstAncestorOfType<OOModel::Class>();
	qint64 id = debugConnector_.getClassId(jvmSignatureFor(containerClass));
	return id != DebugConnector::NO_RESULT;
}

void JavaDebugger::breaktAtParentClassLoad(Model::Node* node)
{
	auto containerClass = node->firstAncestorOfType<OOModel::Class>();
	debugConnector_.breakAtClassLoad(fullNameFor(containerClass, '.'));
}

Location JavaDebugger::nodeToLocation(Model::Node* node)
{
	auto method = node->firstAncestorOfType<OOModel::Method>();
	auto containerClass = method->firstAncestorOfType<OOModel::Class>();
	qint64 classId =  debugConnector_.getClassId(jvmSignatureFor(containerClass));
	Q_ASSERT(classId != debugConnector_.NO_RESULT);
	QString methodName = method->name();
	if (method->methodKind() == OOModel::Method::MethodKind::Constructor)
		methodName = "<init>";
	// TODO: function to get signature of a method: for Java classes we would need the full java library.
	// Once fixed also fix the implementation of getMethodId().
	qint64 methodId = debugConnector_.getMethodId(classId, methodName);
	Q_ASSERT(methodId != debugConnector_.NO_RESULT);

	auto tagKind = Protocol::TypeTagKind::CLASS;
	if (containerClass->constructKind() == OOModel::Class::ConstructKind::Interface)
		tagKind = Protocol::TypeTagKind::INTERFACE;
	else if (containerClass->constructKind() != OOModel::Class::ConstructKind::Class)
		Q_ASSERT(false); // This should not happen for a Java project!

	auto locations = exportMap_->locations(node);
	int line = locations.at(0).span_.startLine_ + 1;

	// get line info for this method.
	static constexpr qint64 NO_INDEX = -2;
	qint64 methodIndex = NO_INDEX;
	auto lineTable = debugConnector_.getLineTable(classId, methodId);
	for (auto val : lineTable.mappings())
		if (line == val.lineNumber()) methodIndex = val.lineCodeIndex();

	Q_ASSERT(methodIndex != NO_INDEX);
	return Location(tagKind, classId, methodId, methodIndex);
}

void JavaDebugger::trySetBreakpoints()
{
	auto it = unsetBreakpoints_.begin();
	while (it != unsetBreakpoints_.end())
	{
		auto target = *it;
		auto targetNode = target->node();
		if (isParentClassLoaded(targetNode))
		{
			qint32 requestId = debugConnector_.setBreakpoint(nodeToLocation(targetNode));
			setBreakpoints_[requestId] = *it;
			it = unsetBreakpoints_.erase(it);
		}
		else
		{
			breaktAtParentClassLoad(targetNode);
			++it;
		}
	}
}

void JavaDebugger::handleVMStart(Event)
{
	trySetBreakpoints();
	debugConnector_.resume();
}

void JavaDebugger::handleClassPrepare(Event)
{
	trySetBreakpoints();
	debugConnector_.resume();
}

void JavaDebugger::handleBreakpoint(BreakpointEvent breakpointEvent)
{
	if (auto breakpointItem = setBreakpoints_[breakpointEvent.requestID()])
	{
		currentBreakpointItem_ = breakpointItem;
		if (auto overlay = breakpointOverlayOf(breakpointItem))
			overlay->setStyle(Visualization::MessageOverlay::itemStyles().get("error"));
		auto containingMethod = breakpointItem->node()->firstAncestorOfType<OOModel::Method>();
		// Get frames
		auto frames = debugConnector_.getFrames(breakpointEvent.thread(), 1);
		auto location = breakpointEvent.location();
		auto variableTable = debugConnector_.getVariableTable(location.classId(), location.methodId());
		auto currentFrame = frames.frames()[0];
		int currentIndex = currentFrame.location().methodIndex();

		QList<StackVariable> varsToGet;
		for (auto variableDetails : variableTable.variables())
		{
			if (variableDetails.name().contains("this")) continue;
			if (variableDetails.codeIndex() <= currentIndex &&
				 currentIndex < variableDetails.codeIndex() + variableDetails.length())
			{
				varsToGet << StackVariable(variableDetails.slot(), typeOfVariable(containingMethod, variableDetails));
			}
		}

		auto values = debugConnector_.getValues(breakpointEvent.thread(), currentFrame.frameID(), varsToGet);
		Q_ASSERT(values.values().length() == varsToGet.length());
		for (auto val : values.values())
		{
			// TODO: for now this is just to demonstrate that we can fetch values, we should probably do something
			// more useful here.
			if (val.kind() == MessagePart::cast(Protocol::Tag::INT))
				qDebug() << "#INT\t#" << val.intValue();
			else if (val.kind() == MessagePart::cast(Protocol::Tag::BOOLEAN))
				qDebug() << "#BOOL\t#" << val.boolean();
			else if (val.kind() == MessagePart::cast(Protocol::Tag::STRING))
				qDebug() << "#STRING\t#" << debugConnector_.getString(val.stringId());
		}
	}
}

Protocol::Tag JavaDebugger::typeOfVariable(OOModel::Method* containingMethod, VariableDetails variable)
{
	int numArgs = containingMethod->arguments()->size();
	// Member functions have the this pointer as first argument
	int varSlot = containingMethod->isStatic() ? variable.slot() : variable.slot() - 1;
	if (varSlot < numArgs)
	{
		auto typeExpression = containingMethod->arguments()->at(varSlot)->typeExpression();
		return typeExpressionToTag(typeExpression);
	}
	else
	{
		int neededIndex = varSlot - numArgs;
		int currentIndex = 0;
		for (auto item : *containingMethod->items())
		{
			if (item->symbolType() == Model::Node::VARIABLE)
			{
				if (currentIndex == neededIndex)
				{
					OOModel::VariableDeclaration* variableDeclaration = nullptr;
					if (auto exprStmt = DCast<OOModel::ExpressionStatement>(item))
					{
						if (auto varDeclarationExpr = DCast<OOModel::VariableDeclarationExpression>(exprStmt->expression()))
							variableDeclaration = varDeclarationExpr->decl();
					}
					else if (auto declStmt = DCast<OOModel::DeclarationStatement>(item))
					{
						variableDeclaration = DCast<OOModel::VariableDeclaration>(declStmt->declaration());
					}
					Q_ASSERT(variableDeclaration);
					if (variableDeclaration->name() != variable.name())
						qDebug() << "Name differs, Decl:" << variableDeclaration->name() << "var" << variable.name();
					return typeExpressionToTag(variableDeclaration->typeExpression());
				}
				++currentIndex;
			}
		}
	}
	// If we don't find the declration something is wrong
	Q_ASSERT(false);
}

Protocol::Tag JavaDebugger::typeExpressionToTag(OOModel::Expression* e)
{
	if (auto typeExpression = DCast<OOModel::TypeExpression>(e))
	{
		if (auto primitiveType = DCast<OOModel::PrimitiveTypeExpression>(typeExpression))
		{
			switch (primitiveType->typeValue())
			{
				case OOModel::PrimitiveType::INT:
					return Protocol::Tag::INT;
				case OOModel::PrimitiveType::LONG:
					return Protocol::Tag::LONG;
				case OOModel::PrimitiveType::UNSIGNED_INT:
					return Protocol::Tag::INT;
				case OOModel::PrimitiveType::UNSIGNED_LONG:
					return Protocol::Tag::LONG;
				case OOModel::PrimitiveType::FLOAT:
					return Protocol::Tag::FLOAT;
				case OOModel::PrimitiveType::DOUBLE:
					return Protocol::Tag::DOUBLE;
				case OOModel::PrimitiveType::BOOLEAN:
					return Protocol::Tag::BOOLEAN;
				case OOModel::PrimitiveType::CHAR:
					return Protocol::Tag::CHAR;
				case OOModel::PrimitiveType::VOID:
					return Protocol::Tag::VOID;
			}
		}
		if (DCast<OOModel::ArrayTypeExpression>(typeExpression)) return Protocol::Tag::ARRAY;
		if (auto classTypeExpression = DCast<OOModel::ClassTypeExpression>(typeExpression))
		{
			auto referenceExpression = classTypeExpression->typeExpression();
			if (referenceExpression->name() == "String") return Protocol::Tag::STRING;
			return Protocol::Tag::CLASS_OBJECT;
		}
	}
	else if (auto referenceExpression = DCast<OOModel::ReferenceExpression>(e))
	{
		if (referenceExpression->name() == "String") return Protocol::Tag::STRING;
		if (DCast<OOModel::Class>(referenceExpression->target())) return Protocol::Tag::CLASS_OBJECT;
		// TODO: Handle this properly
		// Here we hit if we don't know what the target points to, thus we don't know if we deal with a class object.
		// This will probably catch us if we debug in a method with variables of Type of any "in-built" Java class
		// (except String, which is handled above)
		// Q_ASSERT(false);

		// The assertion is disabled for testing purposes.
		qDebug() << "JAVADEBUGGER: WARNING: Reference expression where target type is uknown.";
		return Protocol::Tag::CLASS_OBJECT;
	}
	// No other types possible, or we have to implement it!
	Q_ASSERT(false);
}

Visualization::MessageOverlay* JavaDebugger::breakpointOverlayOf(Visualization::Item* item)
{
	// TODO: for this we could have a more general mechanism in Item
	auto overlayGroup = item->scene()->overlayGroup(BREAKPOINT_OVERLAY_GROUP);
	if (!overlayGroup) return nullptr;

	for (auto overlayAccessor : overlayGroup->overlays())
	{
		auto overlayItem = overlayAccessor->overlayItem();
		if (auto breakpointOverlay = DCast<Visualization::MessageOverlay>(overlayItem))
			if (breakpointOverlay->associatedItem() == item) return breakpointOverlay;
	}
	return nullptr;
}

} /* namespace OODebug */
