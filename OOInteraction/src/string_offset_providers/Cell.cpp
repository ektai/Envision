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

#include "Cell.h"

namespace OOInteraction {

Cell::Cell(int x, Visualization::Item* item, int stringComponentsStart, int stringComponentsEnd)
	: Cell{x, 0, 1, 1, item, stringComponentsStart, stringComponentsEnd}
{}

Cell::Cell(int x, int y, Visualization::Item* item, int stringComponentsStart, int stringComponentsEnd)
	: Cell{x, y, 1, 1, item, stringComponentsStart, stringComponentsEnd}
{}

Cell::Cell(int x, int y, int width, int height, Visualization::Item* item, int stringComponentsStart,
				int stringComponentsEnd)
: region_{x, y, width, height}, item_{item}, stringComponentsStart_{stringComponentsStart},
  stringComponentsEnd_{stringComponentsEnd < 0 ? stringComponentsStart : stringComponentsEnd}
{
}

Cell::~Cell()
{
}

int Cell::offset(const QStringList& allComponents, Qt::Key key, int* length)
{
	int len = 0;
	for (int i = stringComponentsStart(); i <= stringComponentsEnd(); ++i)
		len += allComponents[i].length();

	if (length) *length = len;
	return StringOffsetProvider::itemOffset(item(), len, key);
}

void Cell::setOffset(int newOffset)
{
	StringOffsetProvider::setOffsetInItem(newOffset, item());
}

}
