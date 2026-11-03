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
 * TSReader.h
 *
 *  Created on: 23/08/2015
 *      Author: Felippe Nagato
 */

#ifndef TSREADER_H_
#define TSREADER_H_

#include "Pipe.h"
#include "SharedMemory.h"
#include <inttypes.h>
#include <iostream>
#include <cstdio>
#include <cstring>

using namespace std;

namespace br {
namespace pucrio {
namespace telemidia {
namespace tool {

class TSReader {
	#define BUFFER_SIZE 204 * 5000

	private:
		FILE * pFile;
		SharedMemory *sm;
		Pipe *pipe;
		unsigned char srcType;
		char* buffer;
		int bufferPos;
		int bufferLength;
		unsigned int bufferSize;
		unsigned char packetSize;
		int loopCount;
		string filename;
		int64_t length;

		int read();
		int open();
		int close();
		void init();

	protected:
		int searchSyncByte();
		static unsigned char checkPacketSize(char* buf);

	public:
		TSReader();
		TSReader(const string& filename);
		virtual ~TSReader();

		void setFilename(const string& filename);
		unsigned char getSrcType();
		void setSrcType(unsigned char type);

		int getBuffer(char** buffer);
		int getLoopCount();
		bool rewind();
		int64_t getLength();
		void resetLoopCount();
		int goTo(int64_t position);
		unsigned char getPacketSize();

		static int readPacketSize(const string& filename);

};

}
}
}
}


#endif /* TSREADER_H_ */
