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
 * InputData.h
 *
 *  Created on: 27/02/2013
 *      Author: Felippe Nagato
 */

#ifndef INPUTDATA_H_
#define INPUTDATA_H_

#include "project/ProjectInfo.h"
#include "InputRange.h"

#include <inttypes.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>

using namespace std;

namespace br {
namespace pucrio {
namespace telemidia {
namespace tool {

class InputData : public ProjectInfo {

	#define VIDEO_OUT_MODE_1080_P_60	0x01
	#define VIDEO_OUT_MODE_1080_P_5994	0x02
	#define VIDEO_OUT_MODE_1080_P_50	0x03
	#define VIDEO_OUT_MODE_1080_I_5994	0x04
	#define VIDEO_OUT_MODE_1080_I_60	0x05
	#define VIDEO_OUT_MODE_1080_I_50	0x06
	#define VIDEO_OUT_MODE_1080_P_30	0x07
	#define VIDEO_OUT_MODE_1080_P_2997	0x08
	#define VIDEO_OUT_MODE_1080_P_25	0x09
	#define VIDEO_OUT_MODE_720_P_60		0x0A
	#define VIDEO_OUT_MODE_720_P_5994	0x0B
	#define VIDEO_OUT_MODE_720_P_50		0x0C
	#define VIDEO_OUT_MODE_720_P_30		0x0D
	#define VIDEO_OUT_MODE_720_P_2997	0x0E
	#define VIDEO_OUT_MODE_720_P_25		0x0F
	#define VIDEO_OUT_MODE_480_I_5994	0x10
	#define VIDEO_OUT_MODE_480_P_2997	0x11
	#define VIDEO_OUT_MODE_576_I_50		0x12
	#define VIDEO_OUT_MODE_576_P_25		0x13
	#define VIDEO_OUT_MODE_288_P_30	0x14
	#define VIDEO_OUT_MODE_288_P_2997	0x15
	#define VIDEO_OUT_MODE_288_P_25		0x16
	#define VIDEO_OUT_MODE_240_P_30	0x17
	#define VIDEO_OUT_MODE_240_P_2997	0x18
	#define VIDEO_OUT_MODE_240_P_25		0x19
	#define VIDEO_OUT_MODE_180_P_30	0x1A
	#define VIDEO_OUT_MODE_180_P_2997	0x1B
	#define VIDEO_OUT_MODE_180_P_25		0x1C

	private:
		string filename;
		unsigned short pid;
		unsigned char streamType;
		bool hasDts;
		unsigned char srcType;
		unsigned char readSrcAs;
		int offset;
		int64_t firstPts;
		double duration;
		unsigned int memorySize;
		int refInputCardId;
		unsigned char mode;
		unsigned char gopSize;
		unsigned int bitrate;
		unsigned int audioMapping;
		unsigned char audioLayout;
		vector<InputRange*>* inputRangeList;

	protected:
		void init();

	public:
		InputData();
		InputData(int id);
		virtual ~InputData();

		void setFilename(const string& filename);
		string getFilename();
		void setPid(unsigned short pid);
		unsigned short getPid();
		void setStreamType(unsigned char st);
		unsigned char getStreamType();
		void setHasDts(bool has);
		bool getHasDts();
		unsigned char getSrcType();
		void setSrcType(unsigned char type);
		unsigned char getReadSrcAs();
		void setReadSrcAs(unsigned char type);
		void setOffset(int os);
		int getOffset();
		void setFirstPts(int64_t pts);
		int64_t getFirstPts();
		void setDuration(double dur);
		double getDuration();
		void setMemorySize(unsigned int size);
		unsigned int getMemorySize();
		void setRefInputCardId(int refid);
		int getRefInputCardId();
		void setMode(unsigned char mode);
		unsigned char getMode();
		void setGopSize(unsigned short gopSize);
		unsigned short getGopSize();
		void setBitrate(unsigned int bitrate);
		unsigned int getBitrate();
		void setAudioMapping(unsigned int map);
		unsigned int getAudioMapping();
		void setAudioLayout(unsigned char layout);
		unsigned char getAudioLayout();
		bool addRange(InputRange* ir);
		bool addRange(int id, int64_t begin, int64_t end);
		bool removeRange(int id);
		bool getRange(int id, int64_t* begin, int64_t *end);
		vector<InputRange*>* getInputRangeList();

};

}
}
}
}


#endif /* INPUTDATA_H_ */
