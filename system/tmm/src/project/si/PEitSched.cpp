/*
 * PEitSched.cpp
 *
 *  Created on: 02/11/2015
 *      Author: Felippe Nagato
 */

#include "project/si/PEitSched.h"
#include <cstdio>

namespace br {
namespace pucrio {
namespace telemidia {
namespace tool {

PEitSched::PEitSched() {
	ProjectInfo();
	init();
}

PEitSched::PEitSched(int id) {
	ProjectInfo();
	init();
	this->id = id;
}

void PEitSched::init() {
	projectType = PT_EIT_SCHED8;
	highestSched = PT_EIT_SCHED8;
	layer = 0x01; //HIERARCHY_A
	countryCode.assign("BRA");
	utc = 0;
	auxEventList = NULL;
	currentMidnight = 0;
}

PEitSched::~PEitSched() {

}

void PEitSched::setAuxiliaryEventList(map<time_t, EventInfo*>* auxList) {
	auxEventList = auxList;
}

map<time_t, EventInfo*>* PEitSched::getAuxiliaryEventList() {
	return auxEventList;
}

void PEitSched::setHighestSched(unsigned char hs) {
	highestSched = hs;
}

unsigned char PEitSched::getHighestSched() {
	return highestSched;
}

void PEitSched::compareAndUpdateEit(unsigned short id, Eit *eit) {
	if (sectionsCrc.count(id)) {
		if (eit->getCrc32() != sectionsCrc[id]) {
			eit->setVersionNumber(sectionsVersion[id] + 1);
			sectionsVersion[id] = eit->getVersionNumber();
			eit->updateStream();
			sectionsCrc[id] = eit->getCrc32();
		}
	} else {
		sectionsCrc[id] = eit->getCrc32();
	}
}

void PEitSched::addSectionToList(vector<pair<char*,int>*>* list, Eit *eit) {
	char* tempStream;
	pair<char*,int>* myp = new pair<char*,int>;
	int length = eit->getStream(&tempStream);
	myp->first = new char[length];
	memcpy(myp->first, tempStream, length);
	myp->second = length;
	list->push_back(myp);
}

unsigned char PEitSched::eventCounter(time_t dateTime, unsigned short interval) {
	map<time_t, EventInfo*>::iterator it;
	unsigned char counter = 0;
	time_t toDateTime = dateTime + (interval * 3600);

	it = auxEventList->begin();
	while (it != auxEventList->end()) {
		if (it->second) {
			if ((it->second->startTime >= dateTime) &&
				(it->second->startTime < toDateTime)) {
				counter++;
			}
		}
		++it;
	}

	return counter;
}

int PEitSched::encode(int64_t stc, vector<pair<char*,int>*>* list) {
	map<time_t, EventInfo*>::iterator itEvent;
	map<unsigned short, unsigned char>::iterator itVersion;
	unsigned char sn, fsn, slsn, c, counter;
	unsigned short id, seg, defInitSeg = 0, defSeg = 0;
	time_t midnight, posMidnight;
	time_t dateTime;
	double elapsedTime;
	Eit* eit;

	try {
		itEvent = auxEventList->begin();
	} catch (...) {
		cout << "EIT Schedule error." << endl;
		return -1;
	}

	elapsedTime = Stc::stcToSecond(stc - stcBegin);
	dateTime = timeBegin + (elapsedTime + 0.5f);

	midnight = dateTime / 86400 * 86400;
	if (currentMidnight != midnight) {
		currentMidnight = midnight;
		itVersion = sectionsVersion.begin();
		while (itVersion != sectionsVersion.end()) {
			itVersion->second += 1;
			++itVersion;
		}
	}

	switch (projectType) {
	case PT_EIT_SCHED8:
		defSeg = 64;
		break;
	case PT_EIT_SCHED30:
		defInitSeg = 64;
		defSeg = 256;
		break;
	case PT_EIT_SCHED60:
		defInitSeg = 256;
		defSeg = 512;
		break;
	}

	switch (highestSched) {
	case PT_EIT_SCHED8:
		lastTableId = 0x51;
		break;
	case PT_EIT_SCHED30:
		lastTableId = 0x57;
		break;
	case PT_EIT_SCHED60:
		lastTableId = 0x5F;
		break;
	}

	midnight += 3*defInitSeg*3600;
	posMidnight = midnight;

	for (seg = defInitSeg; seg < defSeg; seg++) {//each segment has 3 hours period.
		posMidnight += 10800; //add 3 hours
		//at this point, midnight = X and posMidnight = X + 3 hours
		counter = eventCounter(midnight, 3);
		fsn = (seg * 8) % 256;
		slsn = counter / 10;
		if ((counter % 10 == 0) && (counter > 0)) slsn -= 1;
		slsn += fsn;
		tableId = 0x50+seg/32;
		if (counter) {
			c = 0;
			for (sn = fsn; sn <= slsn; sn++) {
				//let's insert up to 10 events per section...
				//first, we create a section.
				eit = new Eit();
				//Jury-rigging the UTC time.
				if ((countryCode == "BRA") || (countryCode == "ARG"))
					eit->setForceUtc(utc);
				id = ((tableId << 8) & 0xFF00) | sn;
				//if it's the first section of a segment, we start looking from the beginning.
				if (sn == fsn) itEvent = auxEventList->begin();
				while (itEvent != auxEventList->end()) {
					if ((itEvent->first < posMidnight) &&
						(itEvent->first >= midnight)) {
						//fill associated events until we get up to 10 units.
						itEvent->second->runningStatus = RS_NOT_RUNNING;
						eit->addEventInfo(itEvent->second);
						c++;
						if ((c == 10) || (counter == c)) {
							++itEvent;
							break;
						}
					}
					++itEvent;
				}
				eit->setTableId(tableId);
				eit->setSectionNumber(sn);
				eit->setLastSectionNumber(255);
				eit->setSegmentLastSectionNumber(slsn);
				eit->setLastTableId(lastTableId);
				eit->setVersionNumber(sectionsVersion[id]);
				eit->setPrivateIndicator(1);
				eit->setCurrentNextIndicator(1);
				eit->setTableIdExtension(tableIdExtension);
				eit->setTransportStreamId(transportStreamId);
				eit->setOriginalNetworkId(originalNetworkId);
				eit->setDestroyEvents(false);
				eit->updateStream();

				compareAndUpdateEit(id, eit);
				addSectionToList(list, eit);

				delete eit;
			}
		} else {
			//empty section
			eit = new Eit();
			id = ((tableId << 8) & 0xFF00) | fsn;
			eit->setTableId(tableId);
			eit->setSectionNumber(fsn);
			eit->setLastSectionNumber(255);
			eit->setSegmentLastSectionNumber(slsn);
			eit->setLastTableId(lastTableId);
			eit->setVersionNumber(sectionsVersion[id]);
			eit->setPrivateIndicator(1);
			eit->setCurrentNextIndicator(1);
			eit->setTableIdExtension(tableIdExtension);
			eit->setTransportStreamId(transportStreamId);
			eit->setOriginalNetworkId(originalNetworkId);
			eit->setDestroyEvents(false);
			eit->updateStream();

			compareAndUpdateEit(id, eit);
			addSectionToList(list, eit);

			delete eit;
		}
		midnight += 10800;
	}

	return 0;
}

}
}
}
}
