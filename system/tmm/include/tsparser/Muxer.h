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
 * Muxer.h
 *
 *  Created on: 05/02/2013
 *      Author: Felippe Nagato
 */

#ifndef MUXER_H_
#define MUXER_H_

#include "Stc.h"
#include "TSPacket.h"
#include "ISDBTInformation.h"
#include "IIP.h"
#include "Stream.h"
#include "socket/MulticastServer.h"
#include "Pipe.h"
#include "LocalLibrary.h"

#include <iostream>
#include <map>
#include <cstdlib>
#include <ctime>
#include <math.h>

using namespace std;
using namespace br::pucrio::telemidia::mpeg2;
using namespace br::pucrio::telemidia::tool::isdbt;

namespace br {
namespace pucrio {
namespace telemidia {
namespace tool {

#define LOOK_AHEAD 4	// it must be greater or equal to 1

class Muxer {
	private:

	protected:
		//PCR
		map<unsigned short, int64_t> stcPcrList; //pid, current stc
		map<unsigned short, int64_t> stcRemainderPcrList; //pid, current stc remainder
		map<unsigned short, unsigned int> pktCounterPcrList; //pid, packets counter
		map<unsigned short, unsigned int> pktPcrList; //pid, packets
		map<unsigned short, unsigned char> factorPcrList; //pid, factor
		map<unsigned char, unsigned short> positionPcrList; //position, pid
		unsigned int pcrPktPosition;

		//STC
		int64_t stcZeroed;
		int64_t stcOffset, stcOffsetRemainder;
		int64_t stcStep;
		int64_t remainderStcStep;
		Stc stc_s; //for streaming output
		int64_t stc; //for file output
		int64_t stcBegin;
		unsigned int pktPerStepInterval;
		unsigned int pktNumSinceLastStep;
		bool forceWait;

		//TS control
		char pktControl[8192];
		unsigned int tsBitrate;
		unsigned char packetSize;

		//I/O interfaces
		SocketServer* server;
		Pipe* pPipe;
		FILE * pFile;
		bool isFileMode;
		bool isPipe;
		unsigned char destType;
		string destination;
		string externalApp;
		string appParams;
		unsigned int appPid;
		SharedMemory sm;

		char* streamBuffer;
		int streamBufferSize;
		int streamBufferLength;
		unsigned short packetsInBuffer;
		unsigned char ttl;

		//layers
		int pktNumSinceLastStepLayerA;
		int pktNumSinceLastStepLayerB;
		int pktNumSinceLastStepLayerC;
		int pktPerStepIntervalLayerA;
		int pktPerStepIntervalLayerB;
		int pktPerStepIntervalLayerC;
		vector<char*> awaintingPktA;
		vector<char*> awaintingPktB;
		vector<char*> awaintingPktC;
		unsigned short tspsLayerA, tspsLayerB, tspsLayerC;
		unsigned short tspCounterLayerA, tspCounterLayerB, tspCounterLayerC;
		map<unsigned short, unsigned char> pidToLayerList;

		//TMCC
		IIP* iip;
		bool odfmFrameEven;
		unsigned short ofdmFrameSize, ofdmFrameCounter;
		ISDBTInformation isdbtInfo;
		unsigned char transmissionMode;

		//others
		map<unsigned short, vector<Stream*>*> streamList;
		Buffer* buffer;
		char* ofdmBuffer;
		bool ofdmBufferReady;
		char ofdmFrameSignal;

		//I/O interfaces
		int open();
		int close();
		int sendStreamBuffer();
		void checkOutputStatus();

		//controls
		int countPktPcrs(bool count = true);
		void resetPacketCounters();
		void newStcStep();
		void prepareDummyOfdmFrame();

		//calculators
		unsigned char findFreePcrPosition();
		bool calculateStcStep();

		//buffers writers
		void fillAllBufferes();
		int processNullPackets();
		int writeBufferInTS(unsigned short pid, unsigned char type);
		int writePcrs();
		int writeStream(char* pktBuffer);
		int writeIIPPacket(char* stream, bool isEvenFrame);
		int fillPacket204(char* stream, unsigned short pid, unsigned short* npid);
		void setPacketContinuityCounterDirectly(char* stream, unsigned char cc);

		//buffers access
		int64_t getNextStcStreamToMultiplex(unsigned short *pid,
											int *index,
											bool *streamScheduled);

	public:
		Muxer(unsigned char packetSize, unsigned short packetsInBuffer);
		virtual ~Muxer();

		//static functions

		//Greatest Common Divisor
		static unsigned int gcd(unsigned int a, unsigned int b);
		static unsigned short calculateNumberOfTsps(TransmissionParameters* tp,
													unsigned char &mode);
		static unsigned short calculateOfdmFrameSize(unsigned char &mode,
								unsigned char &guard);
		static double calculatePcrPeriod(unsigned char transmissionMode,
				unsigned short ofdmFrameSize);
		static unsigned short calculatePcrPacketPosition(unsigned char transmissionMode,
						unsigned short ofdmFrameSize);
		static int64_t calculatePcrTime(int64_t &timeBegin, int64_t timeRemainder,
				unsigned int &pktPosition, unsigned int bitrate, int64_t *remainder);

		//class access
		void setTsBitrate(unsigned int rate);
		unsigned int getTsBitrate();
		void setPacketSize(unsigned char size);
		unsigned char getPacketSize();
		void setDestination(const string& dest);
		string getDestination();
		unsigned char getDestType();
		void setDestType(unsigned char type);
		void setIsPipe(bool isPipe);
		void setExternalApp(const string& app);
		void setAppParams(const string& params);
		void setStcReference(int64_t stcBegin);
		int64_t getCurrentStc();
		int64_t getStcBegin();
		int64_t getRelativeStc();
		void setTTL(unsigned char t);
		void setForceWait(bool force);

		void setTspsLayerA(unsigned short tsps);
		void setTspsLayerB(unsigned short tsps);
		void setTspsLayerC(unsigned short tsps);
		void setOfdmFrameSize(unsigned short size);
		unsigned short getOfdmFrameSize();
		void setTransmissionMode(unsigned char mode);
		unsigned char getTransmissionMode();
		void setIip(IIP* iip);

		//insert and remove operations
		map<unsigned short, vector<Stream*>*>* getStreamList();
		bool addElementaryStream(unsigned short pid, Stream* stream);
		bool removeElementaryStream(unsigned short pid);
		bool removeAllElementaryStreams();
		bool addPcrPid(unsigned short pid, unsigned char factor); //pid,us
		bool removePcrPid(unsigned short pid);
		map<unsigned short, int64_t>* getStcPcrList();
		unsigned char getPcrFactor(unsigned short pid);

		bool addPidToLayer(unsigned short pid, unsigned char layer);
		map<unsigned short, unsigned char>* getPidToLayerList();
		void clearPidToLayerList();

		//main operations
		bool prepareMultiplexer(int64_t stcBegin);
		int mainLoop();
};

}
}
}
}


#endif /* MUXER_H_ */
