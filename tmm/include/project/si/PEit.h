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
 * PEit.h
 *
 *  Created on: 01/08/2013
 *      Author: Felippe Nagato
 */

#ifndef PEIT_H_
#define PEIT_H_

#include "project/ProjectInfo.h"
#include "si/Eit.h"
#include "Dependents.h"
#include "Stc.h"
#include <iostream>

using namespace std;
using namespace br::pucrio::telemidia::mpeg2;

namespace br {
namespace pucrio {
namespace telemidia {
namespace tool {

class PEit : public ProjectInfo, public Eit, public Subscriber {

	private:
		void init();

	protected:
		int64_t stcBegin; // for reference purpose
		time_t timeBegin; // date and time when the muxer starts (from project)
		int lastEventId;
		bool offsetAdjusted;
		bool releaseOldEvents;
		short utc;
		unsigned short autoEventId;
		string countryCode;

		//TODO: Allow events working with the system time.

	public:
		PEit();
		PEit(int id);
		virtual ~PEit();

		bool getReleaseOldEvents();
		void setReleaseOldEvents(bool release);
		unsigned short getAutoEventId();
		void setAutoEventId(unsigned short id);
		int createNewEventId();
		bool deleteEvent(unsigned short id);
		void setStcBegin(int64_t stc);
		void setTimeBegin(time_t dateTime);
		time_t getTimeBegin();
		void setCountryCode(const string& country);
		short getUtc();
		void setUtc(short utc);
		EventInfo* findRunningEvent(time_t dateTime);
		EventInfo* findNextEvent(time_t dateTime);
		bool releaseAllOldEvents(time_t dateTime);

		virtual int encode(int64_t stc, vector<pair<char*,int>*>* list);

};

}
}
}
}


#endif /* PEIT_H_ */
