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
 * SharedMemory.cpp
 *
 *  Created on: 30/06/2014
 *      Author: Felippe Nagato
 */

#include "SharedMemory.h"

namespace br {
namespace pucrio {
namespace telemidia {
namespace tool {

SharedMemory::SharedMemory() {
	init();
	setSharedMemoryName("tmm.unnamedmemory.ts");
}

SharedMemory::SharedMemory(const string& sharedMemoryName) {
	init();
	setSharedMemoryName(sharedMemoryName);
}

SharedMemory::~SharedMemory() {
	closeSharedMemory();
}

void SharedMemory::init() {
	md = NULL;
	buffer = NULL;
	control = -1;
	foreignControl = -1;
	showErrorMsg = true;
	bufferSize = MEMORY_BUFFER_SIZE;
}

unsigned int SharedMemory::getBufferSize() {
	return bufferSize - 5;
}

void SharedMemory::setBufferSize(unsigned int size) {
	bufferSize = size + 5;
}

void SharedMemory::showErrorMessages(bool enabled) {
	showErrorMsg = enabled;
}

bool SharedMemory::checkMemoryDescriptor() {
#if defined(_WIN32)
		if (md > 0) return true; else return false;
#else
		if (md >= 0) return true; else return false;
#endif
}

void SharedMemory::setSharedMemoryName(const string& name) {
	sharedMemoryName = name;
#if defined(_WIN32)
		if (sharedMemoryName.find("Local\\") == std::string::npos) {
			sharedMemoryName = "Local\\" + name;
		}
#endif
}

string SharedMemory::getSharedMemoryName() {
	return sharedMemoryName;
}

bool SharedMemory::createSharedMemory() {
	unsigned int dataSize = 0;
	char* size = (char*)&dataSize;

	control = 1;
	foreignControl = 0;

#if defined(_WIN32)
	unsigned short bufLenHi, bufLenLo;
	bufLenHi = (bufferSize >> 16) & 0xFFFF;
	bufLenLo = bufferSize & 0xFFFF;

	md = CreateFileMapping(
				 INVALID_HANDLE_VALUE,    // use paging file
				 NULL,                    // default security
				 PAGE_READWRITE,          // read/write access
				 bufLenHi,                // maximum object size (high-order DWORD)
				 bufLenLo,      // maximum object size (low-order DWORD)
				 sharedMemoryName.c_str());// name of mapping object

	if (md == NULL) {
		if (showErrorMsg) {
			clog << "SharedMemory::createSharedMemory - Failed to create '";
			clog << sharedMemoryName << "' shared memory instance.";
			clog << endl;
		}
	    return false;
	}

	buffer = (const char*) MapViewOfFile(md,   // handle to map object
								FILE_MAP_ALL_ACCESS, // read/write permission
								0, 0, bufferSize);

	if (buffer == NULL) {
		if (showErrorMsg) {
			clog << "SharedMemory::createSharedMemory - Failed to get ";
			clog << " the pointer to '" << sharedMemoryName << "'.";
			clog << endl;
		}
		closeSharedMemory();
		return false;
	}

	CopyMemory((PVOID)buffer, &control, 1);
	CopyMemory((PVOID)(buffer + 1), size, 4);

#else
	/* Create a new memory object */
	md = shm_open(sharedMemoryName.c_str(), O_RDWR | O_CREAT, 0666 );
	if( md == -1 ) {
		if (showErrorMsg) {
			fprintf( stderr, "Open failed:%s\n",
				strerror( errno ) );
		}
		return false;
	}

	/* Set the memory object's size */
	if( ftruncate( md, bufferSize ) == -1 ) {
		if (showErrorMsg) {
			fprintf( stderr, "ftruncate: %s\n",
				strerror( errno ) );
		}
		return false;
	}

	/* Map the memory object */
	buffer = (uint8_t *) mmap( 0, bufferSize,
			PROT_READ | PROT_WRITE,
			MAP_SHARED, md, 0 );
	if( buffer == MAP_FAILED ) {
		if (showErrorMsg) {
			fprintf( stderr, "mmap failed: %s\n",
				strerror( errno ) );
		}
		return false;
	}

	memcpy(buffer, &control, 1);
	memcpy(buffer + 1, size, 4);
#endif

	return true;
}

bool SharedMemory::openSharedMemory() {
	control = 0;
	foreignControl = 1;

#if defined(_WIN32)
	md = OpenFileMapping(
			FILE_MAP_ALL_ACCESS,   // read/write access
			FALSE,                 // do not inherit the name
			sharedMemoryName.c_str());// name of mapping object

	if (md == NULL) {
		if (showErrorMsg) {
			clog << "SharedMemory::openSharedMemory - Failed to open '";
			clog << sharedMemoryName << "'";
			clog << endl;
		}
		return false;
	}

	buffer = (const char*) MapViewOfFile(md,// handle to map object
			   FILE_MAP_ALL_ACCESS,  		// read/write permission
			   0, 0, bufferSize);

	if (buffer == NULL) {
		if (showErrorMsg) {
			clog << "SharedMemory::openSharedMemory - Failed to get ";
			clog << " the pointer to '" << sharedMemoryName << "'.";
			clog << endl;
		}
		closeSharedMemory();
		return false;
	}

#else
	/* Create a new memory object */
	md = shm_open(sharedMemoryName.c_str(), O_RDWR, 0666 );
	if( md == -1 ) {
		if (showErrorMsg) {
			fprintf( stderr, "Open failed:%s\n",
				strerror( errno ) );
		}
		return false;
	}

	/* Map the memory object */
	buffer = (uint8_t *) mmap( 0, bufferSize,
			PROT_READ | PROT_WRITE,
			MAP_SHARED, md, 0 );
	if( buffer == MAP_FAILED ) {
		if (showErrorMsg) {
			fprintf( stderr, "mmap failed: %s\n",
				strerror( errno ) );
		}
		return false;
	}
#endif
	return true;
}

void SharedMemory::closeSharedMemory() {
	if (!checkMemoryDescriptor()) return;
#if defined(_WIN32)
	if (buffer) UnmapViewOfFile(buffer);
	CloseHandle(md);
#else
	close(md);
	shm_unlink( sharedMemoryName.c_str() );
#endif
	md = NULL;
	buffer = NULL;
	control = -1;
	foreignControl = -1;
}

int SharedMemory::readSharedMemory(char** data) {
	unsigned int *size;
	if (!checkMemoryDescriptor()) return -1;

	if (buffer[0] == control) {
		size = (unsigned int*)(buffer+1);
		if (size) {
			*data = (char *) (buffer + 5);
		}
	} else return -3;

	return *size;
}

int SharedMemory::writeSharedMemory(char* data, unsigned int dataSize) {
	char* size = (char*)&dataSize;

	if (!checkMemoryDescriptor()) return -1;

	if (dataSize > bufferSize - 5) {
		cout << "SharedMemory::writeSharedMemory - Message too long." << endl;
		return -2;
	}

#if defined(_WIN32)
	if (buffer[0] == control) {
		CopyMemory((PVOID)(buffer + 1), size, 4);
		CopyMemory((PVOID)(buffer + 5), data, dataSize);
	} else return -3;
#else
	if (buffer[0] == control) {
		memcpy(buffer + 1, size, 4);
		memcpy(buffer + 5, data, dataSize);
	} else return -3;
#endif

	return dataSize;
}

int SharedMemory::grantAccessToForeign() {
	if (!checkMemoryDescriptor()) return -1;

#if defined(_WIN32)
	CopyMemory((PVOID)buffer, &foreignControl, 1);
#else
	memcpy(buffer, &foreignControl, 1);
#endif

	return 0;
}

}
}
}
}


