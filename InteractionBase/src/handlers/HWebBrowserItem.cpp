/***********************************************************************************************************************
 **
 ** Copyright (c) 2011, 2014 ETH Zurich
 ** All rights reserved.
 **
 ** Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
 ** following conditions are met:
 **
 **    * Redistributions of source code must retain the above copyright notice, this list of conditions and the
 **      following disclaimer.
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
 **********************************************************************************************************************/
#include "HWebBrowserItem.h"
#include "VisualizationBase/src/views/MainView.h"
#include "VisualizationBase/src/items/ViewItem.h"
#include "VisualizationBase/src/CustomSceneEvent.h"
#include "VisualizationBase/src/VisualizationManager.h"

namespace Interaction {

HWebBrowserItem::HWebBrowserItem()
{}

HWebBrowserItem* HWebBrowserItem::instance()
{
	static HWebBrowserItem h;
	return &h;
}

void HWebBrowserItem::keyPressEvent(Visualization::Item* target, QKeyEvent *event)
{
	if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_F11)
	{
		if (auto scene = target->scene())
			for (auto view : scene->views())
				if (dynamic_cast<Visualization::MainView*>(view))
					view->centerOn(target);
	}
	else if (event->modifiers() == Qt::NoModifier && event->key() == Qt::Key_Escape
				&& (!target->parent() || target->parent()->typeId() == Visualization::ViewItem::typeIdStatic()))
	{
		auto mainScene = Visualization::VisualizationManager::instance().mainScene();
		QApplication::postEvent(mainScene, new Visualization::CustomSceneEvent{[=](){SAFE_DELETE_ITEM(target);}});
	}
	else GenericHandler::keyPressEvent(target, event);
	// TODO: Is it OK to propagate events to parents or should we just accept all events?
	//Propagating at least some events is necessary for updating an InfoNode
}

void HWebBrowserItem::mousePressEvent(Visualization::Item* target, QGraphicsSceneMouseEvent* event)
{
	if (target->parent()) GenericHandler::mousePressEvent(target, event);
	else HMovableItem::mousePressEvent(target, event);
}

void HWebBrowserItem::mouseMoveEvent(Visualization::Item* target, QGraphicsSceneMouseEvent* event)
{
	if (target->parent()) GenericHandler::mouseMoveEvent(target, event);
	else HMovableItem::mouseMoveEvent(target, event);
}

}