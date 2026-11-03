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
 * PESStream.h
 *
 *  Created on: 25/03/2013
 *      Author: Felippe Nagato
 */

#ifndef PESSTREAM_H_
#define PESSTREAM_H_

#include "Stream.h"
#include "TSPacket.h"
#include "Pipe.h"
#include "SharedMemory.h"
#include "tsparser/Demuxer.h"
#include "PESPacket.h"
#include "project/inputdata/InputRange.h"
#include <iostream>

using namespace std;

namespace br {
namespace pucrio {
namespace telemidia {
namespace tool {

class PESStream : public Stream {

	private:

	protected:

		PESPacket* pesPacket;
		int64_t lastDtsAvailable;
		int64_t lastNextSendWithDts;
		SharedMemory *sm;
		Pipe *pipe;
		FILE * pFile;
		string filename;
		char* buffer;

		bool isVideoStream;
		bool hasDts;
		unsigned char readSrcAs;
		unsigned char srcType;
		unsigned int memorySize;
		vector<int64_t> ptsDtsDiff;
		vector<int64_t> periodList;
		vector<unsigned char> rangeChangeList;
		unsigned char rangeChange, lastRangeChange;
		bool isInRangeNow;
		Demuxer* demuxer;
		TSReader* tsReader;
		int64_t firstPts;
		int64_t timestamp1, timestamp2;
		int frameInterval;
		unsigned char streamId;

		vector<InputRange*>* inputRangeList;

		void updateSource();
		void addBuffer();
		bool timestampControl(PESPacket* packet);
		void resetControls();
		bool isInRange(int64_t pts);
		void processPesPacket();

	public:
		PESStream();
		virtual ~PESStream();

		void fillBuffer(bool fillAll = false);
		bool disposeBuffer();
		bool getBuffer(Buffer** buffer);

		void updateNextSend(int64_t stc);
		void setMaxBitrate(unsigned int rate){};//cannot be set
		void setIsVideoStream(bool v);
		void setHasDts(bool has);
		void addPidFilter(unsigned short pid);
		void setFilename(const string& filename);
		void setStreamId(unsigned char id);
		unsigned char getSrcType();
		void setSrcType(unsigned char type);
		unsigned char getReadSrcAs();
		void setReadSrcAs(unsigned char type);
		void setMemorySize(unsigned int size);
		unsigned int getMemorySize();

		void setInputRangeList(vector<InputRange*>* irl);

};

}
}
}
}


#endif /* PESSTREAM_H_ */
