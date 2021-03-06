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

#include "List.h"

#include "../commands/ListInsert.h"
#include "../commands/ListPut.h"
#include "../commands/ListRemove.h"
#include "../persistence/ClipboardStore.h"
#include "../util/ResolutionRequest.h"

namespace Model {

DEFINE_NODE_TYPE_REGISTRATION_METHODS(List)

List::List(Node *parent) : Super{parent}
{
}

List::List(Node *parent, PersistentStore &store, bool loadPartially) : Super{parent}
{
	if ( loadPartially )
		setPartiallyLoaded();
	else
	{
		QList<LoadedNode> children{store.loadAllSubNodes(this, {})};
		loadSubNodes(children);
	}

}

List::List(const List& other) : Super{other}, nodes_{other.nodes_.size(), nullptr}
{
	for (int i = 0; i<other.nodes_.size(); ++i)
	{
		nodes_[i] = other.nodes_[i]->clone();
		nodes_[i]->setParent(this);
	}
}

List::~List()
{
	for (int i = 0; i < nodes_.size(); ++i)
		SAFE_DELETE( nodes_[i] );
}

List* List::clone() const { return new List{*this}; }

void List::loadSubNodes(QList<LoadedNode>& nodeList)
{
	for (QList<LoadedNode>::iterator ln = nodeList.begin(); ln != nodeList.end(); ln++)
	{
		Q_ASSERT(ln->node->hierarchyTypeIds().contains(lowerTypeBoundForElements()));

		bool ok = true;
		int index = ln->name.toInt(&ok);

		if ( !ok ) throw ModelException{"Could not read the index of a list item. Index value is: " + ln->name};

		if ( index >= nodes_.size() ) nodes_.resize(index + 1);
		nodes_[index] = ln->node;
		ln->node->setParent(this);
	}
}

void List::save(PersistentStore &store) const
{
	Q_ASSERT(!isPartiallyLoaded());

	for (int i = 0; i < nodes_.size(); ++i)
		store.saveNode(nodes_[i], QString::number(i));
}

void List::load(PersistentStore &store)
{
	if (store.currentNodeType() != typeName())
		throw ModelException{"Trying to load a List node from an incompatible node type " + store.currentNodeType()};

	clear();

	QList<LoadedNode> children{store.loadAllSubNodes(this, {})};
	for (QList<LoadedNode>::iterator ln = children.begin(); ln != children.end(); ln++)
	{
		Q_ASSERT(ln->node->hierarchyTypeIds().contains(lowerTypeBoundForElements()));
		bool ok = true;
		int index = ln->name.toInt(&ok);
		if ( !ok ) throw ModelException{"Could not read the index of a list item. Index value is: " + ln->name};

		execute(new ListPut{this, nodes_, ln->node, index});
	}
}

QList<Node*> List::children() const
{
	return nodes_.toList();
}

int List::indexOf(const Node* item) const
{
	// TODO: is this a QT bug, this is fishy

	// Workaround, since the call after this line can't be made with item.
	Node *i = const_cast<Node *> (item);
	return nodes_.indexOf(i);
}

int List::indexToSubnode(const Node* node) const
{
	auto directChild = childToSubnode(node);
	if (directChild) return nodes_.indexOf(directChild);
	return -1;
}

void List::insert(int position, Node* node)
{
	Q_ASSERT(node);
	Q_ASSERT(node->hierarchyTypeIds().contains(lowerTypeBoundForElements()));

	execute(new ListInsert{this, nodes_, node, position});
}

int List::lowerTypeBoundForElements() const
{
	return Node::typeIdStatic();
}

void List::remove(int index)
{
	execute(new ListRemove{this, nodes_, index});
}

void List::remove(Node* instance )
{
	int index = nodes_.indexOf(instance);
	if ( index >= 0 ) remove(index);
}

void List::clear()
{
	while (!nodes_.isEmpty()) remove(0);
}

bool List::replaceChild(Node* child, Node* replacement)
{
	if (!child || !replacement) return false;

	int index = indexOf(child);
	if (index < 0) return false;

	remove(index);
	insert(index, replacement);
	return true;
}

void List::paste(ClipboardStore& clipboard, int position)
{
	for (int i = 0; i<clipboard.numNodes(); ++i)
	{
		// We provide a null parent as this will be set in the instruction below.
		Node* newNode = clipboard.create(manager(), nullptr);

		if (newNode->hierarchyTypeIds().contains(lowerTypeBoundForElements()))
			execute(new ListInsert{this, nodes_, newNode, position+i});
		else
			SAFE_DELETE(newNode);

		if (clipboard.hasNext() ) clipboard.next();
	}
}

bool List::isTransparentForNameResolution() const
{
	return true;
}

bool List::findSymbols(std::unique_ptr<ResolutionRequest> request) const
{
	Q_ASSERT(request->direction() != SEARCH_DOWN);

	bool found{};

	if (request->direction() == SEARCH_HERE)
		for (auto c : nodes_)
			found = c->findSymbols(request->clone(SEARCH_HERE, false)) || found;
	else if (request->direction() == SEARCH_UP)
	{
		auto ignore = childToSubnode(request->source());
		for (auto c : nodes_)
		{
			// Optimize the search by skipping this scope, since we've already searched there
			if (c != ignore)
				found = c->findSymbols(request->clone(SEARCH_HERE, false)) || found;
		}

		if ((request->exhaustAllScopes() || !found) && parent())
			found = parent()->findSymbols(request->clone(SEARCH_UP)) || found;
	}
	else if (request->direction() == SEARCH_UP_ORDERED)
	{
		bool found{};

		// Only search in items above the current one
		auto sourceIndex = indexToSubnode(request->source());
		if (sourceIndex < 0 || sourceIndex > size()) sourceIndex = size();

		auto ignore = childToSubnode(request->source());
		for (int i = 0; i<sourceIndex; ++i)
			if (at(i) != ignore)
			{
				// Optimize the search by skipping the scope of the source, since we've already searched there
				found = at(i)->findSymbols(request->clone(SEARCH_HERE, false)) || found;
			}

		if ((request->exhaustAllScopes() || !found) && parent())
			found = parent()->findSymbols(request->clone(SEARCH_UP)) || found;

		return found;
	}

	return found;
}

Node* List::createDefaultElement()
{
	return nullptr;
}

}
