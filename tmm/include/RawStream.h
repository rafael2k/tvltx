/******************************************************************************
    Copyright (c) 1989-2015 PUC-Rio/Laboratorio TeleMidia

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*******************************************************************************/

/*
 * RawStream.h
 *
 *  Created on: 26/09/2013
 *      Author: Felippe Nagato
 */

#ifndef RAWSTREAM_H_
#define RAWSTREAM_H_

#include "Stream.h"
#include "Dependents.h"
#include "PrivateSection.h"
#include <vector>
#include <iostream>
#include <cstring>

using namespace std;
using namespace br::pucrio::telemidia::mpeg2;

namespace br {
namespace pucrio {
namespace telemidia {
namespace tool {

class RawStream : public Stream, public Publisher {

	private:

	protected:
		vector<pair<char*,int>*> blockList;
		unsigned int currPos;
		bool destroyBlocks;

	public:
		RawStream();
		virtual ~RawStream();

		bool getBuffer(Buffer** buffer);
		void fillBuffer(bool fillAll = false);

		void releaseBlockList();
		void setDestroyBlocks(bool destroy);

		bool addBlock(char* stream, int length);
		void addSection(PrivateSection* sec);
		bool addSection(const string& filename);

};

}
}
}
}


#endif /* ANYSTREAM_H_ */
