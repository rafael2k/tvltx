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
 * PTot.cpp
 *
 *  Created on: 03/05/2013
 *      Author: Felippe Nagato
 */

#include "project/si/PTot.h"

namespace br {
namespace pucrio {
namespace telemidia {
namespace tool {

PTot::PTot() {
	ProjectInfo();
	init();
}

PTot::PTot(int id) {
	ProjectInfo();
	this->id = id;
	init();
}

PTot::~PTot() {

}

void PTot::init() {
 	offset = 0;
 	useCurrentTime = true;
 	timeBegin = 0;
 	utc = Tot::localTimezone();
 	normalizedUtc = utc;
	daylightSavingTime = true;
	dstEligible = true;
	dstValue = 0;
 	projectType = PT_TOT;
 	layer = 0x01; //HIERARCHY_A
 	countryCode.assign("BRA");
 	countryRegionId = 3;//Brazil: RJ - ABNT NBR 15608-3:2008 page 39
 	dstBeginDate = 0;
	dstEndDate = 0;
}

void PTot::printDateTime(time_t now, string format) {
	struct tm tstruct;
	char buf[80];

	if (!format.length()) format.assign("%Y-%m-%d %X");

	tstruct = *localtime(&now);
	strftime(buf, sizeof(buf), format.c_str(), &tstruct);
	cout << buf;
}

void PTot::printDateTime(string format) {
	struct tm tstruct;
	char buf[80];
	short localUtc;
	time_t dt = dateTime + (normalizedUtc * 60);

	if (!format.length()) format.assign("%Y-%m-%d %X");

	tstruct = *gmtime(&dt);
	strftime(buf, sizeof(buf), format.c_str(), &tstruct);
	cout << buf;

	localUtc = normalizedUtc;
	cout << " UTC";
	if (localUtc >= 0) cout << "+"; else {
		cout << "-";
		localUtc *= -1;
	}
	cout << (short)(localUtc/60) << ":";
	printf("%02d", localUtc%60);
}

char PTot::dayOfWeek(time_t date) {
	return gmtime(&date)->tm_wday;
}

//TODO: Improve this code, please.
time_t PTot::makeUtcDate(const string& dateTime, bool utc) {
	int dd, mo, yy, hh, mm, ss, r;
	struct tm when = {0};

	r = sscanf(dateTime.c_str(), "%d-%d-%dT%d:%d:%d",
			&yy, &mo, &dd, &hh, &mm, &ss);
	if (r != 6) return false;

	when.tm_mday = dd;
	when.tm_mon = mo - 1;
	when.tm_year = yy - 1900;
	when.tm_hour = hh;
	when.tm_min = mm;
	when.tm_sec = ss;
	when.tm_isdst = -1;

	if (utc) return mktime(&when) + (this->utc * 60);
		else return mktime(&when) + ((this->utc - normalizedUtc) * 60);
}

time_t PTot::makeUtcDate(unsigned short yy, unsigned short mo,
						 unsigned short dd, bool utc) {
	struct tm when = {0};
	when.tm_mday = dd;
	when.tm_mon = mo - 1;
	when.tm_year = yy - 1900;
	when.tm_hour = 0;
	when.tm_min = 0;
	when.tm_sec = 0;

	if (utc) return mktime(&when) + (localTimezone() * 60);
		else return mktime(&when) + ((this->utc - normalizedUtc) * 60);
}

time_t PTot::easterSunday(unsigned short wYear) {
	unsigned short wMonth;
	unsigned short wDay;
    unsigned short wCorrection = 0;

    if( wYear < 1700 )      wCorrection = 4;
    else if( wYear < 1800 ) wCorrection = 5;
    else if( wYear < 1900 ) wCorrection = 6;
    else if( wYear < 2100 ) wCorrection = 0;
    else if( wYear < 2200 ) wCorrection = 1;
    else if( wYear < 2300 ) wCorrection = 2;
    else if( wYear < 2500 ) wCorrection = 3;

    wDay = (19 * (wYear % 19) + 24) % 30;
    wDay = 22 + wDay + ((2 * (wYear % 4) + 4 * (wYear % 7) + 6 * wDay + 5 + wCorrection) % 7);

    if( wDay > 31 ) {
        wMonth = 4;
        wDay -= 31;
    } else {
        wMonth = 3;
    }

    return makeUtcDate(wYear, wMonth, wDay, false);
}

time_t PTot::carnivalSunday(unsigned short year) {
	return easterSunday(year) + (-49*86400);
}

time_t PTot::daylightStart(unsigned short year) {
	time_t out1st = makeUtcDate(year, 10, 1, false);
	time_t outFirstSunday = out1st + (((7 - dayOfWeek(out1st)) % 7) * 86400);
	return outFirstSunday + (14*86400);
}

time_t PTot::daylightEnd(unsigned short year) {
	time_t feb1st = makeUtcDate(year + 1, 2, 1, false);
	time_t febFirstSunday = feb1st + (((7 - dayOfWeek(feb1st)) % 7) * 86400);
	return febFirstSunday + (14 * 86400);
}

time_t PTot::nextTimeChange() {
	if (afterDaylightInterval()) calculateDaylightInterval();

	if ((dstBeginDate - dateTime) > 0) {
		return dstBeginDate;
	} else {
		return dstEndDate;
	}
}

bool PTot::afterDaylightInterval() {
	return ((dateTime >= dstEndDate));
}

bool PTot::inDaylightInterval() {
	return ((dateTime >= dstBeginDate) && (dateTime < dstEndDate));
}

void PTot::calculateDaylightInterval() {
	struct tm tstruct;
	unsigned short year;
	time_t dt = dateTime + (normalizedUtc * 60);
	tstruct = *gmtime(&dt);
	year = tstruct.tm_year + 1900;
	dstEndDate = daylightEnd(year - 1) - 3600;
	if (dstEndDate - dateTime > 0) {
		year -= 1;
	}
	dstBeginDate = daylightStart(year);
	dstEndDate = daylightEnd(year);
	if (countryCode == "BRA") { //DECRETO Nº 6.558, DE 8 DE SETEMBRO DE 2008.
		//RS, SC, PR, SP, RJ, ES, MG, GO, MT, MS, TO, DF.
		if (dstEndDate == carnivalSunday(year+1)) {
			dstEndDate += (7 * 86400);
		}
	}
}

short PTot::getNormalizedUtc() {
	return normalizedUtc;
}

void PTot::setStcBegin(int64_t stc) {
	stcBegin = stc;
}

void PTot::setUseCurrentTime(bool ct) {
	useCurrentTime = ct;
}

bool PTot::getUseCurrentTime() {
	return useCurrentTime;
}

bool PTot::setTimeBegin(const string& dateTime) {
	timeBegin = makeUtcDate(dateTime, false);
	this->dateTime = timeBegin;
	defineDstEligible();
	calculateDaylightInterval();
	defineLocalTimeOffset();
	if (daylightSavingTime && dstEligible) { //executed just once
		if (inDaylightInterval()) {
			dstValue = -1;
			normalizedUtc += 60;
			timeBegin -= 3600;
		}
	}
	return true;
}

/* the beginning time (utc)
 * In order to set a new time in the future, don't forget to set the new
 * stcBegin associated with that time.
 */
void PTot::setTimeBegin(time_t *dateTime) {
	timeBegin = *dateTime;
	this->dateTime = timeBegin;
	defineDstEligible();
	calculateDaylightInterval();
	defineLocalTimeOffset();
	if (daylightSavingTime && dstEligible) { //executed just once
		if (inDaylightInterval()) {
			dstValue = -1;
			normalizedUtc += 60;
		}
	}
}

time_t PTot::getTimeBegin() {
	return timeBegin;
}

double PTot::updateDateTime(int64_t stc) {
	double elapsedTime = 0.0;

	elapsedTime = Stc::stcToSecond(stc - stcBegin);
	dateTime = timeBegin + (elapsedTime + 0.5f);

	if (daylightSavingTime && dstEligible && afterDaylightInterval()) {
		calculateDaylightInterval();
		defineLocalTimeOffset();

		//experimental
		if (!inDaylightInterval()) {
			if (dstValue == -1) {
				dstValue = 0;
				normalizedUtc -= 60;
			}
		} else {
			if (dstValue == 0) {
				dstValue = -1;
				normalizedUtc += 60;
			}
		}
	}

	return elapsedTime;
}

void PTot::setUtc(short utc) {
	this->utc = utc;
	normalizedUtc = utc;
}

short PTot::getUtc() {
	return utc;
}

void PTot::setDaylightSavingTime(bool dst) {
	daylightSavingTime = dst;
}

bool PTot::getDaylightSavingTime() {
	return daylightSavingTime;
}

int PTot::getOffset() {
	return offset;
}

void PTot::setOffset(int os) {
	offset = os;
}

int PTot::getUtcOffset() {
	return utcOffset;
}

//this method causes the UTC time has an offset.
void PTot::setUtcOffset(int uos) {
	utcOffset = uos;
}

void PTot::setCountryCode(const string& country) {
	countryCode.assign(country);
}

string PTot::getCountryCode() {
	return countryCode;
}

void PTot::setCountryRegionId(unsigned char id) {
	countryRegionId = id;
}

void PTot::defineDstEligible() {
	if (countryCode == "BRA") {
		if (((countryRegionId > 0) && (countryRegionId < 3)) ||
			(countryRegionId == 4) || (countryRegionId == 6)) {
			dstEligible = false;
		} else {
			dstEligible = true;
		}
	} else if (countryCode == "ARG") {
		dstEligible = false;
	}
}

void PTot::defineLocalTimeOffset() {
	bool dst = inDaylightInterval();

	if (countryCode == "BRA") {
		//The weird semantic definition of fields in Brazilian Standard.
		if (countryRegionId > 3) {
			localTimeOffsetPolarity = 1;
		} else {
			localTimeOffsetPolarity = 0;
		}
		if ((countryRegionId == 1) || (countryRegionId == 4) ||
			(countryRegionId == 5)) {
			if (daylightSavingTime && dstEligible && dst) localTimeOffset = 0;
				else localTimeOffset = 60;
		} else if ((countryRegionId == 6) || (countryRegionId == 7)) {
			if (daylightSavingTime && dstEligible && dst) localTimeOffset = 60;
				else localTimeOffset = 120;
		} else {
			if (daylightSavingTime && dstEligible && dst) localTimeOffset = 60;
				else localTimeOffset = 0;
		}
	} else if (countryCode == "ARG") {
		localTimeOffsetPolarity = 1;
		localTimeOffset = 0;
	} else {
		//The normal way.
		if (normalizedUtc < 0) {
			localTimeOffsetPolarity = 1;
			localTimeOffset = normalizedUtc * -1; // localTimeOffset is a positive value
		} else {
			localTimeOffsetPolarity = 0;
			localTimeOffset = normalizedUtc;
		}
	}
}

int PTot::encode(int64_t stc, vector<pair<char*,int>*>* list) {
	time_t dt;
	double elapsedTime = 0.0;

	elapsedTime = updateDateTime(stc);

	releaseAllDescriptors();
	currentNextIndicator = 1;
	lastSectionNumber = 0;
	sectionNumber = 0;
	privateIndicator = 1;
	timeOffset = normalizedUtc;

	LocalTimeData* ltd = new LocalTimeData();
	ltd->timeOfChange = 0;
	ltd->countryCode = countryCode;
	ltd->countryRegionId = countryRegionId;
	ltd->localTimeOffsetPolarity = localTimeOffsetPolarity;

	cout << "tot = ";
	printDateTime("");
	cout << " ~ elapsed time = " << (int64_t) elapsedTime << endl;

	dt = Tot::makeUtcTime(localTimeOffset/60, localTimeOffset%60, 0);
	ltd->localTimeOffset = dt;

	if (daylightSavingTime && dstEligible)
		ltd->timeOfChange = nextTimeChange();

	if (countryCode == "BRA") {
		ltd->nextTimeOffset = dt;
		if (daylightSavingTime && dstEligible) ltd->timeOfChange -= (normalizedUtc * 60);
	} else if (countryCode == "ARG") {
		ltd->nextTimeOffset = dt;
	} else {
		dt = 0;
		if (daylightSavingTime && dstEligible)
			if (!inDaylightInterval()) dt = Tot::makeUtcTime(1, 0, 0);
		ltd->nextTimeOffset = dt;
	}

	LocalTimeOffset* lto = new LocalTimeOffset();
	lto->addLocalTimeData(ltd);
	addDescriptor(lto);

	int length = updateStream();
	pair<char*,int>* streamData = new pair<char*,int>;
	streamData->first = stream;
	streamData->second = length;

	list->push_back(streamData);

	return 0;
}

}
}
}
}


