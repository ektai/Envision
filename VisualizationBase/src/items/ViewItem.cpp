/***********************************************************************************************************************
 **
 ** Copyright (c) 2015 ETH Zurich
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
#include "ViewItem.h"

#include "../src/declarative/DeclarativeItemDef.h"
#include "declarative/DynamicGridFormElement.h"
#include "nodes/ViewItemNode.h"
#include "VViewItemNode.h"
#include "overlays/ArrowOverlay.h"
#include "nodes/InfoNode.h"

namespace Visualization {

ITEM_COMMON_DEFINITIONS(ViewItem, "item")

ViewItem::ViewItem(Item* parent, QString name, StyleType* style) :
		Super(parent, style), name_{name}
{
}

void ViewItem::initializeForms()
{
	addForm((new DynamicGridFormElement())
			->setSpacing(10, 10)->setMargins(10)
			->setMajorAxis(GridLayouter::ColumnMajor)
			->setNodesGetter([](Item* v)
				{ auto self = static_cast<I*>(v);
				  return self->nodesGetter(); }));
}

void ViewItem::insertColumn(int column)
{
	//Make sure we actually have enough columns
	ensureColumnExists(column);
	//Only insert the column, if the current one at index is not empty
	auto isEmpty = true;
	for (auto item : nodes_.at(column))
		isEmpty = isEmpty && item == nullptr;
	//If we have elements, insert a new column (else we can use the empty column)
	if (!isEmpty)
		nodes_.insert(column, {});
}

Model::Node* ViewItem::insertNode(Model::Node* node, int column, int row)
{
	auto ref = ViewItemNode::withReference(node);
	insertViewItemNode(ref, column, row);
	return ref;
}

void ViewItem::removeNode(Model::Node* node)
{
	auto point = positionOfNode(node);
	if (point.x() != -1)
	{
		//If somebody's spacing depends on the node, change it
		for (auto node : allNodes())
		{
			auto viewNode = DCast<ViewItemNode>(node);
			if (viewNode->spacingTarget() == nodes_[point.x()][point.y()])
				viewNode->setSpacingTarget(nullptr);
		}
		nodes_[point.x()].remove(point.y());
		//Need to remove any arrows depending on it
		removeArrowsForNode(node);
		setUpdateNeeded(StandardUpdate);
	}
}

QList<Model::Node*> ViewItem::allNodes() const
{
	QList<Model::Node*> result;
	for (auto column : nodes_)
		for (auto item : column)
			if (item)
				result.append(item);
	return result;
}

QPoint ViewItem::positionOfNode(Model::Node *node) const
{
	for (int i = 0; i < nodes_.size(); i++)
	{
		auto index = nodes_.at(i).indexOf(node);
		if (index != -1)
			return QPoint(i, index);
	}
	return QPoint(-1, -1);
}

QPoint ViewItem::positionOfItem(Item *item) const
{
	auto vref = DCast<VViewItemNode>(item);
	if (vref) return positionOfNode(vref->node());
	else return QPoint(-1, -1);
}

Model::Node* ViewItem::nodeAt(int column, int row)
{
	if (column < 0 || column >= nodes_.size())
		return nullptr;
	if (row < 0 || row >= nodes_[column].size())
		return nullptr;
	return nodes_[column][row];
}

void ViewItem::addSpacing(int column, int row, Model::Node* spacingTarget)
{
	insertViewItemNode(ViewItemNode::withSpacingTarget(spacingTarget), column, row);
}

void ViewItem::addArrow(Model::Node *from, Model::Node *to, QString layer)
{
	if (!arrows_.contains(layer))
		addArrowLayer(layer);
	arrowsToAdd_.append(ArrowToAdd(from, to, layer));
}

QList<QPair<Item*, Item*>> ViewItem::arrowsForLayer(QString layer)
{
	//First, adds all the pending arrows if any exist
	if (arrowsToAdd_.size() > 0)
	{
		auto copy = arrowsToAdd_;
		arrowsToAdd_.clear();
		for (auto line : copy)
		{
			auto item1 = findVisualizationOf(line.from_);
			auto item2 = findVisualizationOf(line.to_);
			if (item1 && item2)
				arrows_[line.layer_].append(QPair<Item*, Item*>(item1, item2));
			else arrowsToAdd_.append(line);
		}
	}
	//And then return the arrows in the layer
	return arrows_[layer];
}

void ViewItem::determineChildren()
{
	Super::determineChildren();
	//Update all the info nodes in the view
	for (auto node : allNodes())
	{
		auto asViewItemNode = DCast<ViewItemNode>(node);
		Q_ASSERT(asViewItemNode);
		if (auto info = DCast<InfoNode>(asViewItemNode->reference()))
		{
			//Update only if we don't already do so by default
			if (auto vis = findVisualizationOf(info))
				//determineChildren is protected in Item, but not in DeclarativeItemBase
				if (vis->needsUpdate() == Item::NoUpdate)
					DCast<DeclarativeItemBase>(vis)->determineChildren();
		}
	}
}

void ViewItem::updateGeometry(int availableWidth, int availableHeight)
{
	Super::updateGeometry(availableWidth, availableHeight);
	bool anyChanges = false;
	for (auto node : allNodes())
	{
		auto asViewItemNode = DCast<ViewItemNode>(node);
		Q_ASSERT(asViewItemNode);
		auto item = DCast<VViewItemNode>(findVisualizationOf(asViewItemNode));
		Q_ASSERT(item);
		if (item->currentFormIndex() == 1)
			anyChanges = item->determineSpacing() || anyChanges;
	}
	if (anyChanges)
		setUpdateNeeded(RepeatUpdate);
}

void ViewItem::addArrowLayer(QString layer)
{
	auto layerName = fullLayerName(layer);
	auto arrowLayer = scene()->overlayGroup(layerName);
	if (!arrowLayer) arrowLayer = scene()->addOverlayGroup(layerName);
	arrowLayer->setOverlayConstructor2Args([](Item* from, Item* to){return makeOverlay(new ArrowOverlay(from, to));});
	arrowLayer->setDynamic2Items([this, layer](){return arrowsForLayer(layer);});
}

void ViewItem::removeArrowsForNode(Model::Node *node)
{
	if (auto vis = findVisualizationOf(node))
	{
		for (auto key : arrows_.keys())
		{
			auto copy = arrows_[key];
			for (auto pair : copy)
				if (pair.first == vis || pair.second == vis)
					arrows_[key].removeAll(pair);
		}\
	}
}

void ViewItem::insertViewItemNode(ViewItemNode *node, int column, int row)
{
	//First, make sure the current grid is big enough to fit the node
	ensurePositionExists(column, row);
	//We can either put the node at a free space if exists, or insert otherwise
	if (nodes_[column][row] == nullptr)
		nodes_[column][row] = node;
	else
		nodes_[column].insert(row, node);
	setUpdateNeeded(StandardUpdate);
}

void ViewItem::ensurePositionExists(int column, int row)
{
	ensureColumnExists(column);
	if (nodes_[column].size() <= row)
		nodes_[column].resize(row + 1);
}

void ViewItem::ensureColumnExists(int column)
{
	if (nodes_.size() <= column)
		nodes_.resize(column + 1);
}

}
