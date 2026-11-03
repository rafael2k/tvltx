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
 * LiveServer.cpp
 *
 *  Created on: 05/09/2015
 *      Author: Felippe Nagato
 */

#include "project/LiveServer.h"
#include <cstring>
#include <cerrno>

using namespace std;

namespace br {
namespace pucrio {
namespace telemidia {
namespace tool {

LiveServer::LiveServer() {
	mode = 1; //Shared memory
	mOpened = 0;
	smBuffer = NULL;
	smBufferLength = UDP_BUFFER_LEN;
	udpBuffer = NULL;
	udpBufferLength = UDP_BUFFER_LEN;
}

LiveServer::~LiveServer() {
	if (smBuffer) delete smBuffer;
	if (udpBuffer) delete udpBuffer;
}

int LiveServer::openSocketUdp() {
#ifdef WIN32
	WSADATA wsaData;
	int L_Err = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (L_Err != 0) {
		if (showErrorMessages) cout << "Cannot find winsock." << endl;
		return -1;
	}
#endif
	sockUdp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	// Non-blocking before bind(): the muxer polls this socket on every cycle,
	// so it must never block, even when bind() fails (e.g. UDP_PORT < 1024
	// without root), in which case live editing is simply unavailable.
#ifdef WIN32
	u_long iMode = 1; //non blocking
	ioctlsocket(sockUdp, FIONBIO, &iMode);
#else
	int flags = fcntl(sockUdp, F_GETFL);
	flags |= O_NONBLOCK; //non blocking
	fcntl(sockUdp, F_SETFL, flags);
#endif
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(UDP_PORT);
	if (bind(sockUdp, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
#ifdef WIN32
		cout << "LiveServer: cannot bind UDP port " << UDP_PORT
			 << ", live editing disabled." << endl;
#else
		cout << "LiveServer: cannot bind UDP port " << UDP_PORT << " ("
			 << strerror(errno) << "), live editing disabled." << endl;
#endif
		return -2;
	}
	return 0;
}

int LiveServer::openServer() {
	unsigned char m, r = 0;
	int i;
	bool sOpen = false;

	cout << "External communication status: ";

	for (i = 0; i < 8; i++) {
		m = (mode >> i) & 0x01;
		if (m == 0) continue;
		switch (i) {
		case 0: //Shared memory
			sm.setSharedMemoryName("tmm.live.memory");
			sm.setBufferSize(smBufferLength);
			sm.showErrorMessages(showErrorMessages);
			if (sm.createSharedMemory()) {
				sm.grantAccessToForeign();
				r |= LS_SHARED_MEMORY;
				if (smBuffer) delete smBuffer;
				smBuffer = new char[smBufferLength];
				sOpen = true;
			}
			break;
		case 1: //Pipe

			break;
		case 2: //UDP
			if (openSocketUdp() >= 0) {
				r |= LS_UDP;
				if (udpBuffer) delete udpBuffer;
				udpBuffer = new char[udpBufferLength];
				sOpen = true;
			}
			break;
		case 3: //TCP

			break;
		}
	}

	if (sOpen) {
		cout << "Available" << endl;
	} else {
		cout << "Unavailable" << endl;
	}

	return r;
}

void LiveServer::closeServer(unsigned char server) {
	unsigned char m;
	int i;

	for (i = 0; i < 8; i++) {
		m = (server >> i) & 0x01;
		if (m == 0) continue;
		switch (i) {
		case 0: //Shared memory
			sm.closeSharedMemory();
			break;
		case 1: //Pipe

			break;
		case 2: //UDP
#ifdef WIN32
			closesocket(sockUdp);
			WSACleanup();
#else
			close(sockUdp);
#endif
			break;
		case 3: //TCP

			break;
		}
	}
}

void LiveServer::setServerMode(unsigned char mode) {
	this->mode = mode;
}

int LiveServer::readSmBuffer(char** buffer) {
	char* buf;
	int bufferSize;

	bufferSize = sm.readSharedMemory(&buf);
	if (bufferSize > 0) {
		memcpy(smBuffer, buf, bufferSize);
		*buffer = smBuffer;
	} else *buffer = NULL;

	return bufferSize;
}

int LiveServer::readUdpBuffer(char** buffer) {
	int bread, len;
	
	len = sizeof(clientAddr);	
	bread = recvfrom(sockUdp, udpBuffer, UDP_BUFFER_LEN, 0, (struct sockaddr*)&clientAddr, (socklen_t *) &len);
	if (bread > 0) {
		*buffer = udpBuffer;
		return bread;
	} else {
		*buffer = NULL;
#ifdef WIN32
		bread = WSAGetLastError();
		return (bread * -1);
#else
		return bread;
#endif
	}
}

int LiveServer::writeBuffer(char* buffer, unsigned int length, unsigned char server) {
	unsigned char m, r = 0;
	int i, bwrite;

	for (i = 0; i < 8; i++) {
		m = (mode >> i) & 0x01;
		if (m == 0) continue;
		switch (i) {
		case 0: //Shared memory
			bwrite = sm.writeSharedMemory(buffer, length);
			sm.grantAccessToForeign();
			if (bwrite > 0) r |= LS_SHARED_MEMORY;
			break;
		case 1: //Pipe

			break;
		case 2: //UDP
			bwrite = sendto(sockUdp, buffer, length, 0,
					(struct sockaddr*)&clientAddr, sizeof(clientAddr));
			if (bwrite > 0) r |= LS_UDP;
			break;
		case 3: //TCP

			break;
		}
	}

	return -1;
}

void LiveServer::setShowErrorMessages(bool show) {
	showErrorMessages = show;
}

void LiveServer::setSmBufferLength(unsigned char len) {
	smBufferLength = len;
}

void LiveServer::setUdpBufferLength(unsigned char len) {
	udpBufferLength = len;
}

}
}
}
}
