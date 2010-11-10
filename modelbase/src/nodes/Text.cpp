/***********************************************************************************************************************
 * Text.cpp
 *
 *  Created on: Nov 9, 2010
 *      Author: Dimitar Asenov
 **********************************************************************************************************************/

#include "nodes/Text.h"
#include "commands/TextSet.h"

namespace Model {

NODE_DEFINE_TYPE_REGISTRATION_METHODS(Text)

Text::Text(Node *parent, Model* model) :
	Node(parent, model)
{
}

Text::Text(Node *parent, NodeIdType id, PersistentStore &store, bool) :
	Node(parent, id)
{
	text = store.loadStringValue();
}

const QString& Text::get() const
{
	return text;
}

Text::operator const QString&() const
{
	return text;
}

void Text::set(const QString &newText)
{
	execute(new TextSet(this, &text, newText));
}

void Text::save(PersistentStore &store)
{
	store.saveStringValue(text);
}

}
