/*
 * PEitSched.cpp
 *
 *  Created on: 02/11/2015
 *      Author: Felippe Nagato
 */

#ifndef INCLUDE_PROJECT_SI_PEITSCHED_H_
#define INCLUDE_PROJECT_SI_PEITSCHED_H_

#include "project/si/PEit.h"

using namespace std;
using namespace br::pucrio::telemidia::mpeg2;

namespace br {
namespace pucrio {
namespace telemidia {
namespace tool {

class PEitSched : public PEit {

	private:
		void init();
		map<time_t, EventInfo*>* auxEventList;
		unsigned char eventCounter(time_t dateTime, unsigned short interval);

	protected:
		time_t currentMidnight;
		unsigned char highestSched;
		map<unsigned short, unsigned int> sectionsCrc;
		map<unsigned short, unsigned char> sectionsVersion;

		void compareAndUpdateEit(unsigned short id, Eit *eit);
		void addSectionToList(vector<pair<char*,int>*>* list, Eit *eit);

	public:
		PEitSched();
		PEitSched(int id);
		virtual ~PEitSched();

		void setAuxiliaryEventList(map<time_t, EventInfo*>* auxList);
		map<time_t, EventInfo*>* getAuxiliaryEventList();
		void setHighestSched(unsigned char hs);
		unsigned char getHighestSched();

		virtual int encode(int64_t stc, vector<pair<char*,int>*>* list);

};

}
}
}
}



#endif /* INCLUDE_PROJECT_SI_PEITSCHED_H_ */
