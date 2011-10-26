/***********************************************************************************************************************
**
** Copyright (c) 2011, ETH Zurich
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

/***********************************************************************************************************************
 * Attribute.h
 *
 *  Created on: Nov 26, 2010
 *      Author: Dimitar Asenov
 **********************************************************************************************************************/

#ifndef ATTRIBUTE_H_
#define ATTRIBUTE_H_

#include "../../modelbase_api.h"
#include <QtCore/QString>

namespace Model {

class MODELBASE_API Attribute
{
	private:
		QString name_;
		QString type_;
		bool optional_;
		bool partialHint_;
		bool persistent_;

	public:
		Attribute();

		Attribute(QString name, QString type, bool optional, bool partialHint, bool persistent);

		/**
		 * Used for comparisons only.
		 */
		Attribute(QString name);

		const QString& name() const;
		const QString& type() const;
		bool optional() const;
		bool partialHint() const;
		bool persistent() const;

		bool operator==(const Attribute &other) const;
};

}

#endif /* ATTRIBUTE_H_ */
