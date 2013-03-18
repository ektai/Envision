/***********************************************************************************************************************
 **
 ** Copyright (c) 2011, 2013 ETH Zurich
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

#include "AnchorLayoutElement.h"

namespace Visualization {

/*
 * AnchorLayoutElement
 */

AnchorLayoutElement::AnchorLayoutElement()
: elementList_{QList<Element*>()}, horizontalConstraints_{QList<AnchorLayoutConstraint*>()},
  verticalConstraints_{QList<AnchorLayoutConstraint*>()}
{}

AnchorLayoutElement::~AnchorLayoutElement()
{
	// TODO: delete all contained elements
}

AnchorLayoutElement* AnchorLayoutElement::put(PlaceEdge placeEdge, Element* placeElement, AtEdge atEdge,
		Element* fixedElement)
{
	Edge edgeToBePlaced = static_cast<Edge>(placeEdge);
	Edge fixedEdge = static_cast<Edge>(atEdge);

	AnchorLayoutConstraint::Orientation orientation = inferOrientation(edgeToBePlaced, fixedEdge);

	return put(orientation, relativePosition(edgeToBePlaced), placeElement, 0, relativePosition(fixedEdge),
					fixedElement);
}

AnchorLayoutElement* AnchorLayoutElement::put(PlaceEdge placeEdge, Element* placeElement, int offset, FromEdge fromEdge,
		Element* fixedElement)
{
	Edge edgeToBePlaced = static_cast<Edge>(placeEdge);
	Edge fixedEdge = static_cast<Edge>(fromEdge);

	AnchorLayoutConstraint::Orientation orientation = inferOrientation(edgeToBePlaced, fixedEdge);

	// compute correct offset
	if (fixedEdge == Edge::Left || fixedEdge == Edge::Top)
		offset = -offset;

	return put(orientation, relativePosition(edgeToBePlaced), placeElement, offset, relativePosition(fixedEdge),
					fixedElement);
}

AnchorLayoutElement* AnchorLayoutElement::put(PlaceEdge placeEdge, Element* placeElement, float relativeEdgePosition,
		Element* fixedElement)
{
	Edge edgeToBePlaced = static_cast<Edge>(placeEdge);

	AnchorLayoutConstraint::Orientation orientation = this->orientation(edgeToBePlaced);
	Q_ASSERT(orientation != AnchorLayoutConstraint::Orientation::Auto);

	return put(orientation, relativePosition(edgeToBePlaced), placeElement, 0, relativeEdgePosition, fixedElement);
}

void AnchorLayoutElement::computeSize(Item* item, int /*availableWidth*/, int /*availableHeight*/)
{
	// TODO: what to do with the additional size?
	// compute size of each sub-element and set their position to (0, 0)
	for (int i=0; i<elementList_.length(); i++)
	{
		elementList_.at(i)->computeSize(item, 0, 0);
		elementList_.at(i)->setPos(QPoint(0, 0));
	}

	// place elements horizontally
	int minX = placeElements(horizontalConstraints_, AnchorLayoutConstraint::Orientation::Horizontal, item);

	// place elements vertically
	int minY = placeElements(verticalConstraints_, AnchorLayoutConstraint::Orientation::Vertical, item);

	// adjust positions, such that the minimum on each axis is at left/right margin, and compute overall element width
	// and height
	int adjustmentX = minX * -1 + leftMargin();
	int adjustmentY = minY * -1 + topMargin();
	int maxX = 0;
	int maxY = 0;
	for (int i=0; i<elementList_.length(); i++)
	{
		Element* element = elementList_.at(i);
		element->setPos(QPoint(element->pos().x() + adjustmentX, element->pos().y() + adjustmentY));
		int rightEdge = element->pos().x() + element->size().width();
		int bottomEdge = element->pos().y() + element->size().height();
		if (rightEdge > maxX)
			maxX = rightEdge;
		if (bottomEdge > maxY)
			maxY = bottomEdge;
	}
	setSize(QSize(maxX + rightMargin(), maxY + bottomMargin()));
}

void AnchorLayoutElement::setItemPositions(Item* item, int parentX, int parentY)
{
	for(int i=0; i<elementList_.length(); i++)
		elementList_.at(i)->setItemPositions(item, parentX + pos().x(), parentY + pos().y());
}

void AnchorLayoutElement::synchronizeWithItem(Item* item)
{
	for(int i=0; i<elementList_.length(); i++)
		if (elementList_.at(i) != nullptr)
			elementList_.at(i)->synchronizeWithItem(item);
}

bool AnchorLayoutElement::sizeDependsOnParent(const Item* /*item*/) const
{
	// TODO: implement sizeDependsOnParent
	return false;
}

AnchorLayoutElement* AnchorLayoutElement::put(AnchorLayoutConstraint::Orientation orientation,
		float relativePlaceEdgePosition, Element* placeElement, int offset, float relativeFixedEdgePosition,
		Element* fixedElement)
{
	Q_ASSERT(orientation != AnchorLayoutConstraint::Orientation::Auto);

	if (!elementList_.contains(placeElement))
		elementList_.append(placeElement);
	if (!elementList_.contains(fixedElement))
		elementList_.append(fixedElement);

	if (orientation == AnchorLayoutConstraint::Orientation::Horizontal)
		addConstraint(horizontalConstraints_, relativePlaceEdgePosition, placeElement, offset,
															relativeFixedEdgePosition, fixedElement);
	else // orientation == AnchorLayoutConstraint::Orientation::Vertical
		addConstraint(verticalConstraints_, relativePlaceEdgePosition, placeElement, offset,
															relativeFixedEdgePosition, fixedElement);
	return this;
}

void AnchorLayoutElement::destroyChildItems(Item* item)
{
	for (int i = 0; i < elementList_.length(); ++i)
		elementList_.at(i)->destroyChildItems(item);
}

AnchorLayoutConstraint::Orientation AnchorLayoutElement::orientation(Edge edge)
{
	switch (edge) {
		case Edge::Left:
		case Edge::Right:
		case Edge::HCenter:
			return AnchorLayoutConstraint::Orientation::Horizontal;
		case Edge::Top:
		case Edge::Bottom:
		case Edge::VCenter:
			return AnchorLayoutConstraint::Orientation::Vertical;
		default:
			return AnchorLayoutConstraint::Orientation::Auto;
	}
}

/**
 * Computes orientation of the two edges, fails if orientation cannot be inferred (both are Center) of if the edges
 * have conflicting orientations (e.g. Orientation::Left and Orientation::Top).
 *
 * @return Either Orientation::Horizontal or Orientation::Vertical
 */
AnchorLayoutConstraint::Orientation AnchorLayoutElement::inferOrientation(Edge firstEdge, Edge secondEdge)
{
	AnchorLayoutConstraint::Orientation firstOrientation = orientation(firstEdge);
	AnchorLayoutConstraint::Orientation secondOrientation = orientation(secondEdge);

	Q_ASSERT(firstOrientation != AnchorLayoutConstraint::Orientation::Auto
			||secondOrientation != AnchorLayoutConstraint::Orientation::Auto);

	if (firstOrientation != AnchorLayoutConstraint::Orientation::Auto) {
		Q_ASSERT(firstOrientation == secondOrientation
				|| secondOrientation == AnchorLayoutConstraint::Orientation::Auto);
		return firstOrientation;
	}
	else  // secondOrientation != AnchorLayoutConstraint::Orientation::Auto
			// && firstOrientation == AnchorLayoutConstraint::Orientation::Auto
	{
		return secondOrientation;
	}
}

float AnchorLayoutElement::relativePosition(Edge edge)
{
	switch (edge) {
		case Edge::Left:
		case Edge::Top:
			return 0.0;
		case Edge::Right:
		case Edge::Bottom:
			return 1.0;
		default:
			return 0.5;
	}
}

void AnchorLayoutElement::addConstraint(QList<AnchorLayoutConstraint*>& constraints, float relativePlaceEdgePosition,
		Element* placeElement, int offset, float relativeFixedEdgePosition, Element* fixedElement)
{
	constraints.append(new AnchorLayoutConstraint(relativePlaceEdgePosition, placeElement, offset,
																	relativeFixedEdgePosition, fixedElement));
	sortConstraints(constraints);
}

int AnchorLayoutElement::placeElements(QList<AnchorLayoutConstraint*>& constraints,
		AnchorLayoutConstraint::Orientation orientation, Item* /*item*/)
{
	// place elements on axis
	int minPos = 0;
	for (int i=0; i<constraints.length(); i++)
	{
		int pos = constraints.at(i)->execute(orientation);
		if (pos < minPos)
			minPos = pos;
	}

	return minPos;
}

void AnchorLayoutElement::sortConstraints(QList<AnchorLayoutConstraint*>& constraints)
{
	// find all constraints which have a fixed node that depends on nothing
	QList<AnchorLayoutConstraint*> sortedConstraints;
	QList<Element*> elementQueue;
	for (auto c1 : constraints)
	{
		bool dependsOnSomething = false;
		for (auto c2 : constraints)
			if (c1->dependsOn(c2, constraints))
			{
				dependsOnSomething = true;
				break;
			}
		if (!dependsOnSomething)
		{
			if (!elementQueue.contains(c1->fixedElement()))
				elementQueue.append(c1->fixedElement());
		}
	}

	// TODO: if visitedElements is empty, but constraints_ isn't, then there is a circular dependency

	for (int elementIndex=0; elementIndex<elementQueue.length(); ++elementIndex)
	{
		for (auto c:constraints)
		{
			if (c->fixedElement() == elementQueue.at(elementIndex))
			{
				sortedConstraints.append(c);
				if (elementQueue.contains(c->placeElement()))
				{
					if (elementQueue.indexOf(c->placeElement()) <= elementIndex)
					{
						// TODO: circular dependency!!!
					}
				}
				else elementQueue.append(c->placeElement());
			}
		}
	}

	constraints = sortedConstraints;
}

}
