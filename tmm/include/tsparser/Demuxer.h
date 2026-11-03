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
 * Demuxer.h
 *
 *  Created on: 05/02/2013
 *      Author: Felippe Nagato
 */

#ifndef DEMUXER_H_
#define DEMUXER_H_

#include "TSPacket.h"
#include "tsparser/TSReader.h"
#include <iostream>
#include <set>
#include <map>

using namespace std;
using namespace br::pucrio::telemidia::mpeg2;

namespace br {
namespace pucrio {
namespace telemidia {
namespace tool {

class Demuxer {

	private:
		TSReader* tsReader;
		set<unsigned short> *pidFilterList;
		map<unsigned short, char> continuityCounterList;

	protected:

	public:
		Demuxer();
		Demuxer(TSReader* tsReader);
		virtual ~Demuxer();

		void setTsReader(TSReader* tsReader);
		void addPidFilter(unsigned short pid);
		void removePidFilter(unsigned short pid);
		int getNextPacketbyFilter(TSPacket **packet);
		void setContinuityCounter(unsigned short pid, char cc);
		char getContinuityCounter(unsigned short pid);


};

}
}
}
}

#endif /* DEMUXER_H_ */
