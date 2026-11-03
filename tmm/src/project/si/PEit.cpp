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
 * PEit.cpp
 *
 *  Created on: 02/08/2013
 *      Author: Felippe Nagato
 */

#include "project/si/PEit.h"
#include <cstdio>

namespace br {
namespace pucrio {
namespace telemidia {
namespace tool {

PEit::PEit() {
	ProjectInfo();
	init();
}

PEit::PEit(int id) {
	ProjectInfo();
	init();
	this->id = id;
}

void PEit::init() {
	projectType = PT_EIT_PF;
	lastEventId = -1;
	offsetAdjusted = false;
	layer = 0x01; //HIERARCHY_A
	countryCode.assign("BRA");
	utc = 0;
	releaseOldEvents = false;
	autoEventId = 1;
}

PEit::~PEit() {

}

bool PEit::getReleaseOldEvents() {
	return releaseOldEvents;
}

void PEit::setReleaseOldEvents(bool release) {
	releaseOldEvents = release;
}

unsigned short PEit::getAutoEventId() {
	return autoEventId;
}

void PEit::setAutoEventId(unsigned short id) {
	autoEventId = id;
}

int PEit::createNewEventId() {
	unsigned short start = autoEventId;
	while (hasEvent(autoEventId)) {
		autoEventId++;
		if (autoEventId == start) return -1;
	}
	return autoEventId;
}

bool PEit::deleteEvent(unsigned short id) {
	map<time_t, EventInfo*>::iterator itEvent;

	itEvent = eventList.begin();
	while (itEvent != eventList.end()) {
		if (itEvent->second->eventId == id) {
			vector<MpegDescriptor*>::iterator itDesc;
			EventInfo* ei = itEvent->second;
			itDesc = ei->descriptorsList.begin();
			while (itDesc != ei->descriptorsList.end()) {
				delete (*itDesc);
				itDesc++;
			}
			delete ei;
			eventList.erase(itEvent);
			return true;
		}
		++itEvent;
	}

	return false;
}

void PEit::setStcBegin(int64_t stc) {
	stcBegin = stc;
}

void PEit::setTimeBegin(time_t dateTime) {
	timeBegin = dateTime;
}

time_t PEit::getTimeBegin() {
	return timeBegin;
}

void PEit::setCountryCode(const string& country) {
	countryCode.assign(country);
}

short PEit::getUtc() {
	return utc;
}

void PEit::setUtc(short utc) {
	this->utc = utc;
}

EventInfo* PEit::findRunningEvent(time_t dateTime) {
	map<time_t, EventInfo*>::iterator it;
	it = eventList.begin();
	while (it != eventList.end()) {
		if (it->second) {
			if ((dateTime >= it->second->startTime) &&
				(dateTime < (it->second->startTime + it->second->duration))) {
				//event occurring
				return it->second;
			}
		}
		++it;
	}
	return NULL;
}

EventInfo* PEit::findNextEvent(time_t dateTime) {
	time_t minor = 0xFFFFFFFF;
	EventInfo* ei = NULL;
	map<time_t, EventInfo*>::iterator it;
	it = eventList.begin();
	while (it != eventList.end()) {
		if (it->second) {
			if (dateTime >= it->second->startTime) {
				if (minor > it->second->startTime) {
					minor = it->second->startTime;
					ei = it->second;
				}
			}
		}
		++it;
	}
	return ei;
}

bool PEit::releaseAllOldEvents(time_t dateTime) {
	EventInfo* ei = NULL;
	map<time_t, EventInfo*>::iterator itEvent;
	vector<MpegDescriptor*>::iterator itDesc;
	itEvent = eventList.begin();
	while (itEvent != eventList.end()) {
		ei = itEvent->second;
		if (ei) {
			if (dateTime > ei->startTime + ei->duration) {
				itDesc = ei->descriptorsList.begin();
				while (itDesc != ei->descriptorsList.end()) {
					delete (*itDesc);
					++itDesc;
				}
				delete ei;
				eventList.erase(itEvent);
				itEvent = eventList.begin(); //start again
				continue;
			}
		}
		++itEvent;
	}
	return true;
}

int PEit::encode(int64_t stc, vector<pair<char*,int>*>* list) {
	map<time_t, EventInfo*>::iterator itEvent;
	unsigned char sn;
	time_t dateTime, dur = -1;
	double elapsedTime;
	int eventId = -1;
	EventInfo* ei;
	Eit* eit;

	elapsedTime = Stc::stcToSecond(stc - stcBegin);
	dateTime = timeBegin + (elapsedTime + 0.5f);

	if (releaseOldEvents) releaseAllOldEvents(dateTime);

	//EIT Present and Following
	//=========================

	//Present event   - Located in section 0
	//Following event - Located in section 1

	for (sn = 0; sn < 2; sn++) {
		eit = new Eit();
		if ((countryCode == "BRA") || (countryCode == "ARG")) //Jury-rigging the UTC time.
			eit->setForceUtc(utc);
		if (sn == 0) {
			ei = findRunningEvent(dateTime);
			if (ei != NULL) {
				//event occurring
				ei->runningStatus = RS_RUNNING;
				dur = ei->duration;
				eit->addEventInfo(ei);
				eventId = ei->eventId;
				if ((eventId != lastEventId) || ei->updated) {
					versionNumber++;
					lastEventId = eventId;
					ei->updated = false;
				}
			}
			if ((ei == NULL) && (lastEventId != -1)) {
				//outside EIT timeline
				itEvent = eventList.begin();
				while (itEvent != eventList.end()) {
					if (itEvent->second->eventId == lastEventId) {
						//using last event.
						ei = itEvent->second;
						ei->runningStatus = RS_RUNNING;
						eit->addEventInfo(ei);
						itEvent = eventList.begin();
						break;
					}
					++itEvent;
				}
				if (itEvent == eventList.end()) {
					//table with nothing inside (EIT timeline unavailable).
					lastEventId = -1;
					versionNumber++;
				}
			}
		} else {
			if (dur > 0) {
				ei = findRunningEvent(dateTime + dur);
				if (ei != NULL) {
					//event not running yet
					ei->runningStatus = RS_NOT_RUNNING;
					eit->addEventInfo(ei);
				} else {
					//no events
					ei = findNextEvent(dateTime + dur);
					if (ei != NULL) {
						// this is the nearest one in the future.
						ei->runningStatus = RS_NOT_RUNNING;
						eit->addEventInfo(ei);
					} else {
						//ok, there is nothing available in the future.
					}
				}
			} else {

			}
		}
		eit->setSectionNumber(sn);
		eit->setLastSectionNumber(1);
		eit->setPrivateIndicator(1);
		eit->setTableIdExtension(tableIdExtension);
		eit->setCurrentNextIndicator(1);
		eit->setVersionNumber(versionNumber);
		eit->setSegmentLastSectionNumber(1);
		eit->setLastTableId(0x4E);
		eit->setTransportStreamId(transportStreamId);
		eit->setOriginalNetworkId(originalNetworkId);
		eit->setDestroyEvents(false);
		eit->updateStream();
		char* tempStream;
		pair<char*,int>* myp = new pair<char*,int>;
		int length = eit->getStream(&tempStream);
		myp->first = new char[length];
		memcpy(myp->first, tempStream, length);
		myp->second = length;
		list->push_back(myp);

		delete eit;
	}

	return 0;
}

}
}
}
}
