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
 * TSReader.cpp
 *
 *  Created on: 23/08/2015
 *      Author: Felippe Nagato
 */

#include "tsparser/TSReader.h"

namespace br {
namespace pucrio {
namespace telemidia {
namespace tool {

TSReader::TSReader() {
	init();
}

TSReader::TSReader(const string& filename) {
	init();
	this->filename = filename;
}

TSReader::~TSReader() {
	close();
	if (buffer) delete buffer;
}

void TSReader::init() {
	pFile = NULL;
	sm = NULL;
	pipe = NULL;
	buffer = NULL;
	packetSize = 188;
	loopCount = 0;
	length = -1;
	bufferPos = 0;
	bufferLength = 0;
	bufferSize = BUFFER_SIZE;
	srcType = 0; //file
}

void TSReader::setFilename(const string& filename) {
	this->filename = filename;
	close();
}

unsigned char TSReader::getSrcType() {
	return srcType;
}

void TSReader::setSrcType(unsigned char type) {
	srcType = type;
}

int TSReader::open() {
	int len;

	if (buffer) delete buffer;
	switch (srcType) {
	case 0: //read from file
		pFile = fopen64(filename.c_str(), "rb");
		if (pFile == NULL ) {
			cout << "TSReader::open - Unable to open file: " << filename << endl;
			return -2;
		}
		fseeko64(pFile, 0, SEEK_END);
		length = ftello64(pFile);
		fseeko64(pFile, 0, SEEK_SET);
		buffer = new char[bufferSize];
		break;
	case 1: //read from pipe
		if (pipe) delete pipe;
		pipe = new Pipe(filename);
		if (!pipe->openPipe()) {
			return -2;
		}
		length = 0;
		bufferSize = 512000;
		buffer = new char[bufferSize];
		break;
	case 2: //read from shared memory
		if (sm) delete sm;
		sm = new SharedMemory(filename);
		sm->setBufferSize(512000);
		if (!sm->openSharedMemory()) {
			return -2;
		}
		length = 0;
		bufferSize = 512000 + 5;
		buffer = new char[bufferSize];
		break;
	}

	len = read();
	packetSize = checkPacketSize(buffer);
	if (packetSize == 0) packetSize = 188;
	return len;
}

int TSReader::close() {
	switch (srcType) {
	case 0: //read from file
		if (pFile) fclose(pFile);
		pFile = NULL;
		break;
	case 1: //read from pipe
		if (pipe) pipe->closePipe();
		pipe = NULL;
		break;
	case 2: //read from shared memory
		if (sm) sm->closeSharedMemory();
		sm = NULL;
		break;
	}
	loopCount = 0;
	return 0;
}

int TSReader::getLoopCount() {
	return loopCount;
}

int64_t TSReader::getLength() {
	return length;
}

void TSReader::resetLoopCount() {
	loopCount = 0;
}

int TSReader::goTo(int64_t position) {
	switch (srcType) {
	case 0: //read from file
		fseeko64(pFile, position, SEEK_SET);
		bufferPos = 0;
		bufferLength = 0;
		return read();
	case 1: //read from pipe
	case 2: //read from shared memory
		cout << "TSReader::goTo - Command not allowed." << endl;
		return -1;
		break;
	}

	return -1;
}

unsigned char TSReader::getPacketSize() {
	return packetSize;
}

int TSReader::searchSyncByte() {
	int counter = 0;
	while (1) {
		if (buffer[bufferPos] == 0x47) {
			return counter;
		}
		bufferPos++;
		counter++;
		if (bufferPos == bufferLength) {
			if (read() < 0) return -1;
		}
	}
	return -2;
}

int TSReader::readPacketSize(const string& filename) {
	//weak method
	FILE* fd;
	char buf[2041];
	int rb;

	fd = fopen(filename.c_str(), "rb");
	if (fd == NULL) return -1;

	rb = fread(buf, 1, 2041, fd);
	fclose(fd);
	if (rb == 0) return -2;

	return checkPacketSize(buf);
}

unsigned char TSReader::checkPacketSize(char* buf) {
	//weak method
	int i = 0;
	while (i < 1837) {
		if (buf[i] == 0x47) {
			if (buf[i+188] == 0x47) {
				return 188;
			} else if (buf[i+204] == 0x47) {
				return 204;
			}
		}
		++i;
	}
	return 0;
}

int TSReader::getBuffer(char** buffer) {
	switch (srcType) {
	case 0: //read from file
		if (!pFile) if (open() < 0) return -1;
		break;
	case 1: //read from pipe
		if (!pipe) if (open() < 0) return -1;
		break;
	case 2: //read from shared memory
		if (!sm) if (open() < 0) return -1;
		break;
	}

	while (bufferPos + packetSize > bufferLength) {
		if (read() < packetSize) {
			*buffer = NULL;
			return 0;
		}
		if (this->buffer[bufferPos] != 0x47) {
			if (searchSyncByte() < 0) return -2;
		}
	}

	*buffer = this->buffer + bufferPos;
	bufferPos += packetSize;
	return packetSize;
}

int TSReader::read() {
	int pos = 0, len;
	char* buf;

	if (bufferPos < bufferLength) {
		pos = bufferLength - bufferPos;
		memcpy(buffer, buffer + bufferPos, pos);
	}

	switch (srcType) {
	case 0: //read from file
		if (!pFile) {
			cout << "TSReader::read - File descriptor is NULL." << endl;
			return -1;
		}
		if (feof(pFile)) {
			fseeko64(pFile, 0, SEEK_SET);
			loopCount++;
		}
		bufferLength = fread(buffer + pos, 1, bufferSize - pos, pFile) + pos;
		break;
	case 1: //read from pipe
		if (!pipe) {
			cout << "TSReader::read - Pipe descriptor is NULL." << endl;
			return -1;
		}
		len = pipe->readPipe(buffer + pos, bufferSize - pos);
		if (len < 0) {
			cout << "TSReader::read - Error reading pipe." << endl;
			return -2;
		}
		bufferLength = len + pos;
		break;
	case 2: //read from shared memory
		if (!sm) {
			cout << "TSReader::read - pipe descriptor is NULL." << endl;
			return -1;
		}
		len = sm->readSharedMemory(&buf);
		if (len < 0) {
			cout << "TSReader::read - Error reading shared memory." << endl;
			return -2;
		}
		bufferLength = len + pos;
		break;
	}

	bufferPos = 0;
	return bufferLength;
}

bool TSReader::rewind() {
	switch (srcType) {
	case 0: //read from file
		if (!pFile) if (open() < 0) return false;
		fseeko64(pFile, 0, SEEK_SET);
		bufferPos = 0;
		bufferLength = 0;
		read();
		break;
	case 1: //read from pipe
	case 2: //read from shared memory
		cout << "TSReader::rewind - Command not allowed." << endl;
		return false;
		break;
	}

	return true;
}

}
}
}
}


