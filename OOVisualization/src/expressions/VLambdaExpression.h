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

#pragma once

#include "../oovisualization_api.h"
#include "VLambdaExpressionStyle.h"
#include "VExpression.h"

#include "OOModel/src/expressions/LambdaExpression.h"
#include "VisualizationBase/src/items/LayoutProvider.h"

namespace Visualization {
	class VList;
	class Static;
}

namespace OOVisualization {

class VStatementItemList;

class OOVISUALIZATION_API VLambdaExpression : public Super<VExpression<VLambdaExpression,
	Visualization::LayoutProvider<>, OOModel::LambdaExpression>>
{
	ITEM_COMMON(VLambdaExpression)

	public:
		VLambdaExpression(Item* parent, NodeType* node, const StyleType* style = itemStyles().get());
		virtual ~VLambdaExpression();

		Visualization::VList* arguments() const;
		Visualization::Static* icon() const;
		VStatementItemList* body() const;

	protected:
		void determineChildren() override;

	private:
		Visualization::VList* arguments_{};
		Visualization::Static* icon_{};
		VStatementItemList* body_{};
};

inline Visualization::VList* VLambdaExpression::arguments() const { return arguments_; }
inline Visualization::Static* VLambdaExpression::icon() const { return icon_; }
inline VStatementItemList* VLambdaExpression::body() const { return body_; }

}
