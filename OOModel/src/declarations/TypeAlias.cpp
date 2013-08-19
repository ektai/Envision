/***********************************************************************************************************************
**
** Copyright (c) 2011, 2013 ETH Zurich
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

#include "TypeAlias.h"
#include "../types/SymbolProviderType.h"

#include "ModelBase/src/nodes/TypedListDefinition.h"
DEFINE_TYPED_LIST(OOModel::TypeAlias)

namespace OOModel {

COMPOSITENODE_DEFINE_EMPTY_CONSTRUCTORS(TypeAlias)
COMPOSITENODE_DEFINE_TYPE_REGISTRATION_METHODS(TypeAlias)

REGISTER_ATTRIBUTE(TypeAlias, typeExpression, Expression, false, false, true)
REGISTER_ATTRIBUTE(TypeAlias, typeArguments, TypedListOfFormalTypeArgument, false, false, true)

TypeAlias::TypeAlias(const QString &name, Expression *typeExpression)
: Super(nullptr, TypeAlias::getMetaData())
{
	setName(name);
	if(typeExpression) setTypeExpression(typeExpression);
}

TypeAlias::SymbolTypes TypeAlias::symbolType() const
{
	SymbolTypes ret = UNSPECIFIED;
	auto type = const_cast<TypeAlias*>(this)->typeExpression()->type();
	if (auto sp = dynamic_cast<SymbolProviderType*>(type))
	{
		ret = sp->symbolProvider()->symbolType();
	}

	SAFE_DELETE(type);
	return ret;
}

QList<Model::Node*> TypeAlias::findSymbols(const Model::SymbolMatcher& matcher, Model::Node* source,
		FindSymbolDirection direction, SymbolTypes symbolTypes, bool exhaustAllScopes)
{
	QList<Model::Node*> symbols;

	// TODO: Search type arguments

	auto type = typeExpression()->type();
	auto symbolProviderType = dynamic_cast<SymbolProviderType*>(type);
	if (symbolProviderType)
		symbols = symbolProviderType->symbolProvider()
			->findSymbols(matcher, symbolProviderType->symbolProvider(), SEARCH_DOWN, symbolTypes, exhaustAllScopes);

	SAFE_DELETE(type);

	if (exhaustAllScopes || symbols.isEmpty())
		symbols << Super::findSymbols(matcher, source, direction, symbolTypes, exhaustAllScopes);
	return symbols;
}

}