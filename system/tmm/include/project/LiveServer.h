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
 * LiveServer.h
 *
 *  Created on: 05/09/2015
 *      Author: Felippe Nagato
 */

#ifndef INCLUDE_PROJECT_LIVESERVER_H_
#define INCLUDE_PROJECT_LIVESERVER_H_

#include "../SharedMemory.h"
#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#endif

using namespace std;

namespace br {
namespace pucrio {
namespace telemidia {
namespace tool {

#define UDP_PORT 334
#define UDP_BUFFER_LEN 65000

class LiveServer {

	#define LS_SHARED_MEMORY 1
	#define LS_PIPE 2;
	#define LS_UDP 4
	#define LS_TCP 8
	#define LS_ALL 255

	private:

	protected:
		SharedMemory sm;
#ifdef WIN32
		SOCKET sockUdp;
#else
		int sockUdp;
#endif
		struct sockaddr_in serverAddr, clientAddr;
		unsigned char mode, mOpened;
		bool showErrorMessages;
		char *smBuffer;
		unsigned int smBufferLength;
		char *udpBuffer;
		unsigned int udpBufferLength;

		int openSocketUdp();

	public:
		LiveServer();
		virtual ~LiveServer();

		int openServer();
		void closeServer(unsigned char server);
		void setServerMode(unsigned char mode);
		int readSmBuffer(char** buffer);
		int readUdpBuffer(char** buffer);
		int writeBuffer(char* buffer, unsigned int length, unsigned char server);
		void setShowErrorMessages(bool show);
		void setSmBufferLength(unsigned char len);
		void setUdpBufferLength(unsigned char len);

};

}
}
}
}

#endif /* INCLUDE_PROJECT_LIVESERVER_H_ */
