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
 * Muxer.cpp
 *
 *  Created on: 05/02/2013
 *      Author: Felippe Nagato
 */

#include "tsparser/Muxer.h"

#include <bts_tx.h>

namespace br {
namespace pucrio {
namespace telemidia {
namespace tool {

Muxer::Muxer(unsigned char packetSize, unsigned short packetsInBuffer) {
	stc = SYSTEM_CLOCK_FREQUENCY * 10;
	stcZeroed = 0;
	this->packetSize = packetSize;
	stcOffset = 0;
	pFile = NULL;
	server = NULL;
	buffer = new Buffer;
	streamBufferSize = packetSize * packetsInBuffer;
	this->packetsInBuffer = packetsInBuffer;
	streamBuffer = new char[streamBufferSize];
	streamBufferLength = 0;
	tsBitrate = MUX_RATE;
	ofdmFrameSize = 4352; //for guard interval 1/16 and transmission mode 3.
	transmissionMode = 3;
	iip = NULL;
	pPipe = NULL;
	isPipe = false;
	destType = 0x01;
	externalApp = "";
	appParams = "";
	appPid = 0;
	forceWait = false;
	//packetCounter = 0;
	tspCounterLayerA = 0;
	tspCounterLayerB = 0;
	tspCounterLayerC = 0;
	pktNumSinceLastStepLayerA = 0;
	pktNumSinceLastStepLayerB = 0;
	pktNumSinceLastStepLayerC = 0;
	sm.showErrorMessages(false);
	sm.setBufferSize(1);
	sm.setSharedMemoryName("dektec_status");
	if (!sm.openSharedMemory()) {
		cout << "Device feedback is not available." << endl;
	}
	ofdmBufferReady = false;
	ofdmFrameSignal = 0;
	ofdmBuffer = NULL;
}

Muxer::~Muxer() {
	close();
	delete buffer;
	if (server) delete server;
	if (streamBuffer) delete streamBuffer;
	removeAllElementaryStreams();
}

unsigned int Muxer::gcd(unsigned int a, unsigned int b) {
	if (b == 0) return a; else return gcd(b, a % b);
}

unsigned short Muxer::calculateNumberOfTsps(TransmissionParameters* tp,
											unsigned char &mode) {
	if (tp) {
		if ((tp->modulationScheme == MCCI_MODULATION_SCHEME_DQPSK) ||
			(tp->modulationScheme == MCCI_MODULATION_SCHEME_QPSK)) {
			if (tp->codingRateOfInnerCode == MCCI_CONVOLUTIONAL_CODING_RATE_1_2) {
				if (mode == 1) return (12 * tp->numberOfSegments);
				if (mode == 2) return (24 * tp->numberOfSegments);
				if (mode == 3) return (48 * tp->numberOfSegments);
			}
			if (tp->codingRateOfInnerCode == MCCI_CONVOLUTIONAL_CODING_RATE_2_3) {
				if (mode == 1) return (16 * tp->numberOfSegments);
				if (mode == 2) return (32 * tp->numberOfSegments);
				if (mode == 3) return (64 * tp->numberOfSegments);
			}
			if (tp->codingRateOfInnerCode == MCCI_CONVOLUTIONAL_CODING_RATE_3_4) {
				if (mode == 1) return (18 * tp->numberOfSegments);
				if (mode == 2) return (36 * tp->numberOfSegments);
				if (mode == 3) return (72 * tp->numberOfSegments);
			}
			if (tp->codingRateOfInnerCode == MCCI_CONVOLUTIONAL_CODING_RATE_5_6) {
				if (mode == 1) return (20 * tp->numberOfSegments);
				if (mode == 2) return (40 * tp->numberOfSegments);
				if (mode == 3) return (80 * tp->numberOfSegments);
			}
			if (tp->codingRateOfInnerCode == MCCI_CONVOLUTIONAL_CODING_RATE_7_8) {
				if (mode == 1) return (21 * tp->numberOfSegments);
				if (mode == 2) return (42 * tp->numberOfSegments);
				if (mode == 3) return (84 * tp->numberOfSegments);
			}
		}
		if (tp->modulationScheme == MCCI_MODULATION_SCHEME_16QAM) {
			if (tp->codingRateOfInnerCode == MCCI_CONVOLUTIONAL_CODING_RATE_1_2) {
				if (mode == 1) return (24 * tp->numberOfSegments);
				if (mode == 2) return (48 * tp->numberOfSegments);
				if (mode == 3) return (96 * tp->numberOfSegments);
			}
			if (tp->codingRateOfInnerCode == MCCI_CONVOLUTIONAL_CODING_RATE_2_3) {
				if (mode == 1) return (32 * tp->numberOfSegments);
				if (mode == 2) return (64 * tp->numberOfSegments);
				if (mode == 3) return (128 * tp->numberOfSegments);
			}
			if (tp->codingRateOfInnerCode == MCCI_CONVOLUTIONAL_CODING_RATE_3_4) {
				if (mode == 1) return (36 * tp->numberOfSegments);
				if (mode == 2) return (72 * tp->numberOfSegments);
				if (mode == 3) return (144 * tp->numberOfSegments);
			}
			if (tp->codingRateOfInnerCode == MCCI_CONVOLUTIONAL_CODING_RATE_5_6) {
				if (mode == 1) return (40 * tp->numberOfSegments);
				if (mode == 2) return (80 * tp->numberOfSegments);
				if (mode == 3) return (160 * tp->numberOfSegments);
			}
			if (tp->codingRateOfInnerCode == MCCI_CONVOLUTIONAL_CODING_RATE_7_8) {
				if (mode == 1) return (42 * tp->numberOfSegments);
				if (mode == 2) return (84 * tp->numberOfSegments);
				if (mode == 3) return (168 * tp->numberOfSegments);
			}
		}
		if (tp->modulationScheme == MCCI_MODULATION_SCHEME_64QAM) {
			if (tp->codingRateOfInnerCode == MCCI_CONVOLUTIONAL_CODING_RATE_1_2) {
				if (mode == 1) return (36 * tp->numberOfSegments);
				if (mode == 2) return (72 * tp->numberOfSegments);
				if (mode == 3) return (144 * tp->numberOfSegments);
			}
			if (tp->codingRateOfInnerCode == MCCI_CONVOLUTIONAL_CODING_RATE_2_3) {
				if (mode == 1) return (48 * tp->numberOfSegments);
				if (mode == 2) return (96 * tp->numberOfSegments);
				if (mode == 3) return (192 * tp->numberOfSegments);
			}
			if (tp->codingRateOfInnerCode == MCCI_CONVOLUTIONAL_CODING_RATE_3_4) {
				if (mode == 1) return (54 * tp->numberOfSegments);
				if (mode == 2) return (108 * tp->numberOfSegments);
				if (mode == 3) return (216 * tp->numberOfSegments);
			}
			if (tp->codingRateOfInnerCode == MCCI_CONVOLUTIONAL_CODING_RATE_5_6) {
				if (mode == 1) return (60 * tp->numberOfSegments);
				if (mode == 2) return (120 * tp->numberOfSegments);
				if (mode == 3) return (240 * tp->numberOfSegments);
			}
			if (tp->codingRateOfInnerCode == MCCI_CONVOLUTIONAL_CODING_RATE_7_8) {
				if (mode == 1) return (63 * tp->numberOfSegments);
				if (mode == 2) return (126 * tp->numberOfSegments);
				if (mode == 3) return (252 * tp->numberOfSegments);
			}
		}
	}
	return 0;
}

unsigned short Muxer::calculateOfdmFrameSize(unsigned char &mode,
											unsigned char &guard) {
	if ((mode == 1) && (guard == MCCI_GUARD_INTERVAL_1_4)) return 1280;
	if ((mode == 1) && (guard == MCCI_GUARD_INTERVAL_1_8)) return 1152;
	if ((mode == 1) && (guard == MCCI_GUARD_INTERVAL_1_16)) return 1088;
	if ((mode == 1) && (guard == MCCI_GUARD_INTERVAL_1_32)) return 1056;
	if ((mode == 2) && (guard == MCCI_GUARD_INTERVAL_1_4)) return 2560;
	if ((mode == 2) && (guard == MCCI_GUARD_INTERVAL_1_8)) return 2304;
	if ((mode == 2) && (guard == MCCI_GUARD_INTERVAL_1_16)) return 2176;
	if ((mode == 2) && (guard == MCCI_GUARD_INTERVAL_1_32)) return 2112;
	if ((mode == 3) && (guard == MCCI_GUARD_INTERVAL_1_4)) return 5120;
	if ((mode == 3) && (guard == MCCI_GUARD_INTERVAL_1_8)) return 4608;
	if ((mode == 3) && (guard == MCCI_GUARD_INTERVAL_1_16)) return 4352;
	if ((mode == 3) && (guard == MCCI_GUARD_INTERVAL_1_32)) return 4224;
	return 0;
}

unsigned short Muxer::calculatePcrPacketPosition(unsigned char transmissionMode,
						unsigned short ofdmFrameSize) {
	switch (transmissionMode) {
	case 1:
		return (ofdmFrameSize / 1);
		break;
	case 2:
		return (ofdmFrameSize / 2);
		break;
	case 3:
		return (ofdmFrameSize / 4);
		break;
	}
	return 0;
}

int64_t Muxer::calculatePcrTime(int64_t &timeBegin, int64_t timeRemainder,
		unsigned int &pktPosition, unsigned int bitrate, int64_t *remainder) {

	*remainder = (((int64_t)8 * pktPosition * 188 * SYSTEM_CLOCK_FREQUENCY) +
					timeRemainder) % bitrate;
	return timeBegin + ((((int64_t)8 * pktPosition * 188 *
						SYSTEM_CLOCK_FREQUENCY) + timeRemainder) / bitrate);

}

void Muxer::setTsBitrate(unsigned int rate) {
	tsBitrate = rate;
}

unsigned int Muxer::getTsBitrate() {
	return tsBitrate;
}

void Muxer::setPacketSize(unsigned char size) {
	packetSize = size;
	if (streamBuffer) delete streamBuffer;
	streamBufferSize = packetSize * packetsInBuffer;
	streamBuffer = new char[streamBufferSize];
	streamBufferLength = 0;
}

unsigned char Muxer::getPacketSize() {
	return packetSize;
}

void Muxer::setDestination(const string& dest) {
	destination = dest;
	if (!dest.size()) return;
	isFileMode = true;

	int found = dest.find("://");
	if (found != -1) {
		isFileMode = false;
	}
}

string Muxer::getDestination() {
	return destination;
}

unsigned char Muxer::getDestType() {
	return destType;
}

void Muxer::setDestType(unsigned char type) {
	destType = type;
}

void Muxer::setIsPipe(bool isPipe) {
	this->isPipe = isPipe;
}

void Muxer::setExternalApp(const string& app) {
	externalApp = app;
}

void Muxer::setAppParams(const string& params) {
	appParams = params;
}

void Muxer::setStcReference(int64_t stcBegin) {
	if (stcBegin < SYSTEM_CLOCK_FREQUENCY * 10) stc = SYSTEM_CLOCK_FREQUENCY * 10;
		else stc = stcBegin;
	this->stcBegin = stc;
	stc_s.setReference(stc);
}

int64_t Muxer::getCurrentStc() {
	return stc + stcOffset;
}

int64_t Muxer::getStcBegin() {
	return stcBegin;
}

int64_t Muxer::getRelativeStc() {
	return getCurrentStc() - stcBegin;
}

void Muxer::setTTL(unsigned char t) {
	ttl = t;
}

void Muxer::setForceWait(bool force) {
	forceWait = force;
}

void Muxer::checkOutputStatus() {
	char* buf;
	if (sm.readSharedMemory(&buf) > 0) {
		ofdmFrameSignal = buf[0];
		sm.grantAccessToForeign();
	}
}

int Muxer::open() {
	if (isFileMode) {
		if (!isPipe) {
			pFile = fopen(destination.c_str(), "wb");
			if (pFile == NULL) {
				cout << "Muxer::open - Unable to open file: " <<
						destination << endl;
				return -1;
			}
		} else {
			pPipe = new Pipe(destination);
			if (externalApp.size()) {
				cout << "Starting external application...";
				if (LocalLibrary::executeApp(externalApp, appParams, &appPid)) {
					cout << " done." << endl;
				} else {
					cout << " fail." << endl;
				}
			}
			if (!pPipe->createPipe()) {
				cout << "Muxer::open - Unable to open pipe: " <<
						pPipe->getPipeName() << endl;
				return -2;
			}
		}
	} else {
		string ip, port;
		unsigned found = destination.find("://");
		ip.assign(destination, found+3, 25);
		found = ip.find(":");
		port.assign(ip, found+1, 5);
		ip.assign(ip, 0, found);
		int num = atol(port.c_str());
		server = new MulticastServer(ip.c_str(), num);
		if (!server->createSocket()) {
			cout << "Muxer::open - Unable to open socket: " <<
					destination << endl;
			delete server;
			server = NULL;
			return -3;
		}
		server->setTTL(ttl);
	}
	return 0;
}

int Muxer::close() {
	if (isFileMode) {
		if (!isPipe) {
			if (pFile) fclose(pFile);
			pFile = NULL;
		} else {
			if (pPipe) {
				if (!LocalLibrary::killApp(appPid)) {
					cout << "Muxer::close - External application could ";
					cout << "not be closed!" << endl;
				}
				pPipe->closePipe();
				delete pPipe;
			}
			pPipe = NULL;
		}
	}
	return 0;
}

int Muxer::sendStreamBuffer() {
	if (streamBufferLength == streamBufferSize) {
		streamBufferLength = 0;
		if (destType == 0x01) {
			//virtual output
			if ((!isFileMode) || forceWait) Stc::waitFor(stc + stcOffset - stc_s.getStc());
			if (isFileMode) {
				int rw = -1;
				if (!isPipe) {
					rw = fwrite(streamBuffer, 1, streamBufferSize, pFile);
				} else {
					rw = pPipe->writePipe(streamBuffer, streamBufferSize);
				}
				if (rw != streamBufferSize) {
					return -1;
				}
			} else {
				return server->send(streamBuffer, streamBufferSize);
			}
		} else if (destType == 0x00) {

                    for(int id = 0; id < nr_outputs; id++)
                        write_buffer(&mod_pars[id].buffer_out, (uint8_t *) streamBuffer, streamBufferSize);
		}
	}
	return 0;
}

bool Muxer::calculateStcStep() {
	if (packetSize == 204) {
		pcrPktPosition = calculatePcrPacketPosition(transmissionMode, ofdmFrameSize);
		pktPerStepInterval = pcrPktPosition;
		stcStep = (int64_t)pcrPktPosition * 8 * 188 * SYSTEM_CLOCK_FREQUENCY / MUX_RATE;
		pktPerStepIntervalLayerA = (tspsLayerA * pktPerStepInterval) / ofdmFrameSize;
		pktPerStepIntervalLayerB = (tspsLayerB * pktPerStepInterval) / ofdmFrameSize;
		pktPerStepIntervalLayerC = (tspsLayerC * pktPerStepInterval) / ofdmFrameSize;
	} else {
		if (tsBitrate == MUX_RATE) {
			pcrPktPosition = calculatePcrPacketPosition(transmissionMode, ofdmFrameSize);
		} else {
			pcrPktPosition = ((tsBitrate / 8) * 0.06) / 188;
			tsBitrate = (((tsBitrate / 8) / pcrPktPosition) * pcrPktPosition) * 8;
		}
		pktPerStepInterval = pcrPktPosition;
		stcStep = (int64_t)pcrPktPosition * 8 * 188 * SYSTEM_CLOCK_FREQUENCY / tsBitrate;
	}

	return true;
}

//this function returns the PCR pid in order to insert it immediately in TS stream.
int Muxer::countPktPcrs(bool count) {
	map<unsigned short, unsigned int>::iterator it;
	int pid = -1;
	it = pktCounterPcrList.begin();
	while (it != pktCounterPcrList.end()) {
		if (count) it->second++;
		if (it->second == pktPcrList[it->first]) {
			pid = it->first;
		}
		++it;
	}
	return pid;
}

void Muxer::fillAllBufferes() {
	map<unsigned short, vector<Stream*>*>::iterator it;
	vector<Stream*>::iterator itStr;

	for (it = streamList.begin(); it != streamList.end(); ++it) {
		for (itStr = it->second->begin(); itStr != it->second->end(); ++itStr) {
			if (*itStr) {
				(*itStr)->fillBuffer();
			} else {
				cout << "Muxer::fillAllBufferes - No stream available for pid = " <<
						it->first << endl;
			}
		}
	}
}

int64_t Muxer::getNextStcStreamToMultiplex(unsigned short *pid,
										   int *index,
										   bool *streamScheduled) {
	map<unsigned short, vector<Stream*>*>::iterator it;
	vector<Stream*>::iterator itStr;
	int64_t nextSend = 0x7FFFFFFFFFFFFFFF;
	unsigned int idx;

	for (it = streamList.begin(); it != streamList.end(); ++it) {
		idx = -1;
		for (itStr = it->second->begin(); itStr != it->second->end(); ++itStr) {
			idx++;
			if ((*itStr)->getNextSend() < nextSend) {
				nextSend = (*itStr)->getNextSend();
				*pid = it->first;
				*index = idx;
				*streamScheduled = true;
			}
		}
	}

	return nextSend;
}

//Notice: Never use the same stream for different PIDs.
bool Muxer::addElementaryStream(unsigned short pid, Stream* stream) {
	vector<Stream*>* strList;
	vector<Stream*>::iterator itStr;
	map<unsigned short, vector<Stream*>*>::iterator it = streamList.begin();

	if (!streamList.count(pid)) {
		strList = new vector<Stream*>();
		streamList.insert(
				it, std::pair<unsigned short, vector<Stream*>*>(pid, strList));
	} else {
		strList = streamList[pid];
	}
	for (itStr = strList->begin(); itStr != strList->end(); ++itStr) {
		if (*itStr == stream) return false;
	}
	strList->push_back(stream);

	return true;
}

bool Muxer::removeElementaryStream(unsigned short pid) {
	map<unsigned short, vector<Stream*>*>::iterator it;

	it = streamList.find(pid);
	if (it != streamList.end()){
		delete it->second;
		streamList.erase(it);
		return true;
	}

	return false;
}

bool Muxer::removeAllElementaryStreams() {
	map<unsigned short, vector<Stream*>*>::iterator it;

	for (it = streamList.begin(); it != streamList.end(); ++it) {
		delete (it->second);
	}
	streamList.clear();

	return true;
}

unsigned char Muxer::findFreePcrPosition() {
	unsigned char pos = 1;
	map<unsigned char, unsigned short>::iterator it;

	it = positionPcrList.find(pos);
	while (it != positionPcrList.end()) {
		pos++;
		it = positionPcrList.find(pos);
	}

	return pos;
}

bool Muxer::addPcrPid(unsigned short pid,unsigned char factor) {
	map<unsigned short, unsigned int>::iterator itPcr;

	if (factor == 0) return false;
	if (stcPcrList.count(pid)) return false;

	stcPcrList[pid] = stc;
	stcRemainderPcrList[pid] = 0;
	factorPcrList[pid] = factor;
	pktPcrList[pid] = pcrPktPosition * factorPcrList[pid];
	positionPcrList[pid] = findFreePcrPosition();
	pktCounterPcrList[pid] = pktPcrList[pid] - (positionPcrList[pid] - 1);

	return true;
}

bool Muxer::removePcrPid(unsigned short pid) {
	if (stcPcrList.count(pid)) {
		stcPcrList.erase(pid);
		stcRemainderPcrList.erase(pid);
		pktCounterPcrList.erase(pid);
		pktPcrList.erase(pid);
		factorPcrList.erase(pid);
		positionPcrList.erase(pid);
		return true;
	}
	return false;
}

unsigned char Muxer::getPcrFactor(unsigned short pid) {
	if (factorPcrList.count(pid)) {
		return factorPcrList[pid];
	}
	return 0;
}

map<unsigned short, int64_t>* Muxer::getStcPcrList() {
	return &stcPcrList;
}

map<unsigned short, vector<Stream*>*>* Muxer::getStreamList() {
	return &streamList;
}

void Muxer::setTspsLayerA(unsigned short tsps) {
	tspsLayerA = tsps;
}

void Muxer::setTspsLayerB(unsigned short tsps) {
	tspsLayerB = tsps;
}

void Muxer::setTspsLayerC(unsigned short tsps) {
	tspsLayerC = tsps;
}

void Muxer::setOfdmFrameSize(unsigned short size) {
	ofdmFrameSize = size;
}

unsigned short Muxer::getOfdmFrameSize() {
	return ofdmFrameSize;
}

void Muxer::setTransmissionMode(unsigned char mode) {
	transmissionMode = mode;
}

unsigned char Muxer::getTransmissionMode() {
	return transmissionMode;
}

bool Muxer::addPidToLayer(unsigned short pid, unsigned char layer) {
	if (!pidToLayerList.count(pid)) {
		pidToLayerList[pid] = layer;
		return true;
	}
	return false;
}

map<unsigned short, unsigned char>* Muxer::getPidToLayerList() {
	return &pidToLayerList;
}

void Muxer::clearPidToLayerList() {
	pidToLayerList.clear();
}

bool Muxer::prepareMultiplexer(int64_t stcBegin) {
	remainderStcStep = 0;

	if (!calculateStcStep()) return false;

	odfmFrameEven = false;
	ofdmFrameCounter = 0;
	pktNumSinceLastStep = 0;

	if (ofdmBuffer) delete ofdmBuffer;
	ofdmBuffer = new char[ofdmFrameSize * packetSize];

	stcOffset = 0;
	stcOffsetRemainder = 0;

	if (stcBegin < SYSTEM_CLOCK_FREQUENCY * 10) stc = SYSTEM_CLOCK_FREQUENCY * 10;
		else stc = stcBegin;
	this->stcBegin = stc;

	for (int i = 0; i < 8192; i++) pktControl[i] = 0;

	if (open() < 0) {
		cout << "Muxer::prepareMultiplexer - Destination not set." << endl;
		return false;
	}

	if (isPipe) {
		cout << "Destination: \\\\.\\pipe\\tmm." << destination << endl;
	} else {
		cout << "Destination: " << destination << endl;
	}
	cout << "STC begins at " << this->stcBegin << " (" <<
			Stc::stcToSecond(this->stcBegin) << "s)" << endl;

	stc_s.setReference(stc);

	return true;
}

void Muxer::resetPacketCounters() {
	pktNumSinceLastStep = 0;

	if (packetSize == 204) {
		pktNumSinceLastStepLayerA = 0;
		pktNumSinceLastStepLayerB = 0;
		pktNumSinceLastStepLayerC = 0;
	}
}

void Muxer::newStcStep() {
	stc = calculatePcrTime(stc, remainderStcStep, pcrPktPosition, tsBitrate,
							&remainderStcStep);
	stcOffset = 0;
	stcOffsetRemainder = 0;
}

int Muxer::mainLoop() {
	int64_t nextSend;
	unsigned short pid = 0xFFFF;
	int idx;
	bool streamScheduled = false;
	Stream* stream;

	checkOutputStatus();
	fillAllBufferes();

	idx = -1;
	//get next stream to multiplex
	nextSend = getNextStcStreamToMultiplex(&pid, &idx, &streamScheduled);
	if (idx < 0) return -1;
	stream = streamList[pid]->at(idx);

	//write any PCR because this could be the beginning of a step.
	writePcrs();

	//wait for it
	while (stc < nextSend) {
		if (stc + stcStep * LOOK_AHEAD > nextSend) {
			//before a newStcStep, there are some packets to send.
			break;
		} else {
			processNullPackets();
			newStcStep();
		}
		if ((!isFileMode) || forceWait) Stc::waitFor(stc + stcOffset - stc_s.getStc());
	};

	while (streamScheduled) {
		vector<Buffer*>::iterator it;
		streamScheduled = false;

		stream->setCurrStc(stc + stcOffset); //or just stc alone?
		if (stream->getBuffer(&buffer)) {
			//write it as a TS
			writeBufferInTS(pid, stream->getType());
		} else {
			//cout << "Muxer::mainLoop - Buffer slot underflow? PID = " <<
			///		pid << endl;
		}
		stream->disposeBuffer();
		stream->updateNextSend(stc + stcOffset);//parameter not actually used.
		idx = -1;
		nextSend = getNextStcStreamToMultiplex(&pid, &idx, &streamScheduled);
		if (idx < 0) return -1;
		stream = streamList[pid]->at(idx);
		stream->fillBuffer();

		if (stc + stcStep * LOOK_AHEAD < nextSend) break;
	}

	return 0;
}

int Muxer::writePcrs() {
	TSPacket* packet;
	TSAdaptationField* tsaf;
	char* pktBuffer;
	int rw = 0;
	map<unsigned short, unsigned int>::iterator it;

	it = pktCounterPcrList.begin();
	while (it != pktCounterPcrList.end()) {
		if (it->second == pktPcrList[it->first]) {
			packet = new TSPacket();
			tsaf = new TSAdaptationField(stcPcrList[it->first]);
			packet->setAdaptationField(tsaf);
			packet->setAdaptationFieldControl(TSPacket::NO_PAYLOAD);
			packet->setPid(it->first);
			packet->setSectionType(false);
			packet->setStartIndicator(0);
			if (packetSize == 188) packet->setContinuityCounter(pktControl[it->first]);
			packet->getStream(&pktBuffer);
			if (packetSize == 204) {
				if (fillPacket204(pktBuffer, it->first, NULL) > 0) {
					setPacketContinuityCounterDirectly(pktBuffer, pktControl[it->first]);
					rw = writeStream(pktBuffer);
				}
			} else {
				rw = writeStream(pktBuffer);
			}

			delete packet;

			it->second = 0;
			stcPcrList[it->first] = calculatePcrTime(stcPcrList[it->first],
					stcRemainderPcrList[it->first], pktPcrList[it->first],
					tsBitrate, &(stcRemainderPcrList[it->first]));

			if (countPktPcrs() < 0) break; //because writeStream() has been called.
		}
		++it;
	}

	return rw;
}

int Muxer::writeBufferInTS(unsigned short pid, unsigned char type) {
	unsigned int pos = 0;
	int rw = 0;
	int pcrPid = -1;
	unsigned char payloadLength;
	TSPacket* packet;
	TSAdaptationField* tsaf;
	char* pktBuffer;
	unsigned short npid;

	if (type > 1) {
		cout << "Muxer::writeTsStream - Type not implemented." << endl;
		return -1;
	}

	while (pos < buffer->size) {
		packet = NULL;
		tsaf = NULL;
		rw = -1;

		//this code should be deleted...
		/*pcrPid = countPktPcrs(false);
		if (pcrPid > 0) {
			cout << "Muxer::writeBufferInTS - PCR written (while begin)" << endl;
			writePcrs();
			newStcStep();
			continue;
		}*/ // ...until here.

		if (buffer->size - pos >= 184) {
			payloadLength = 184;
		} else {
			//this case is used to fill the packet with stuff bytes.
			payloadLength = buffer->size - pos;
			if ((type == 0) && (tsaf == NULL)) {
				tsaf = new TSAdaptationField();
				tsaf->setAdaptationFieldLength(183 - payloadLength);
			}
		}

		packet = new TSPacket(type, buffer->buffer + pos, payloadLength, tsaf);
		if (pos == 0 && buffer->startIndicator) {
			packet->setStartIndicator(1);
		} else {
			packet->setStartIndicator(0);
		}
		packet->setPid(pid);
		if (packetSize == 188) packet->setContinuityCounter(++pktControl[pid]);
		//the payloadLength actually used calling packet->getStream()
		payloadLength = packet->getStream(&pktBuffer);
		pos += payloadLength;
		if (packetSize == 204) {
			if (fillPacket204(pktBuffer, pid, &npid) > 0) {
				setPacketContinuityCounterDirectly(pktBuffer, ++pktControl[npid]);
				rw = writeStream(pktBuffer);
			}
		} else {
			rw = writeStream(pktBuffer);
		}

		delete packet;

		if (rw >= 0) {
			pcrPid = countPktPcrs();
			if (pcrPid > 0) { //because writeStream() has been called.
				rw = writePcrs();
				newStcStep();
			}
		}
	}

	return rw;
}

void Muxer::setIip(IIP* iip) {
	this->iip = iip;
}

int Muxer::writeIIPPacket(char* stream, bool isEvenFrame) {
	TSPacket* packet;
	char* payload;
	unsigned char len;

	if (!iip) return -1;

	if (iip->getMcci()) {
		if (isEvenFrame) iip->getMcci()->setTMCCSynchronizationWord(0); else
			iip->getMcci()->setTMCCSynchronizationWord(1);
	}

	iip->updateStream();
	len = iip->getStream(&payload);

	packet = new TSPacket(0, payload, len, NULL);
	packet->setAdaptationFieldControl(TSPacket::PAYLOAD_ONLY);
	packet->setPid(0x1FF0);
	packet->setStartIndicator(1);
	packet->setContinuityCounter(pktControl[0x1FF0]++);
	packet->getStream(&payload);
	memcpy(stream, payload, 188);

	delete packet;

	return 0;
}

int Muxer::fillPacket204(char* stream, unsigned short pid, unsigned short* npid) {
	char* isdbtInfoStream;
	char* awaiting;
	map<unsigned short, unsigned char>::iterator itLayer;

	itLayer = pidToLayerList.find(pid);
	if (itLayer != pidToLayerList.end()) {
		switch (itLayer->second) {
		case HIERARCHY_A:
			if (pktNumSinceLastStepLayerA < pktPerStepIntervalLayerA) {
				isdbtInfo.setLayerIndicator(itLayer->second);
				pktNumSinceLastStepLayerA++;
				if (!stcPcrList.count(pid) && awaintingPktA.size()) {
					awaiting = new char[204];
					memcpy(awaiting, stream, 204);
					awaintingPktA.push_back(awaiting);
					memcpy(stream, awaintingPktA[0], 204);
					delete awaintingPktA[0];
					awaintingPktA.erase(awaintingPktA.begin());
				}
			} else if (pktPerStepIntervalLayerA) {
				awaiting = new char[204];
				memcpy(awaiting, stream, 204);
				awaintingPktA.push_back(awaiting);
				return 0;
			}
			break;
		case HIERARCHY_B:
			if (pktNumSinceLastStepLayerB < pktPerStepIntervalLayerB) {
				isdbtInfo.setLayerIndicator(itLayer->second);
				pktNumSinceLastStepLayerB++;
				if (!stcPcrList.count(pid) && awaintingPktB.size()) {
					awaiting = new char[204];
					memcpy(awaiting, stream, 204);
					awaintingPktB.push_back(awaiting);
					memcpy(stream, awaintingPktB[0], 204);
					delete awaintingPktB[0];
					awaintingPktB.erase(awaintingPktB.begin());
				}
			} else if (pktPerStepIntervalLayerB) {
				awaiting = new char[204];
				memcpy(awaiting, stream, 204);
				awaintingPktB.push_back(awaiting);
				return 0;
			}
			break;
		case HIERARCHY_C:
			if (pktNumSinceLastStepLayerC < pktPerStepIntervalLayerC) {
				isdbtInfo.setLayerIndicator(itLayer->second);
				pktNumSinceLastStepLayerC++;
				if (!stcPcrList.count(pid) && awaintingPktC.size()) {
					awaiting = new char[204];
					memcpy(awaiting, stream, 204);
					awaintingPktC.push_back(awaiting);
					memcpy(stream, awaintingPktC[0], 204);
					delete awaintingPktC[0];
					awaintingPktC.erase(awaintingPktC.begin());
				}
			} else if (pktPerStepIntervalLayerC) {
				awaiting = new char[204];
				memcpy(awaiting, stream, 204);
				awaintingPktC.push_back(awaiting);
				return 0;
			}
			break;
		}
	} else {
		if (pid == 0x1FFF) {
			//Instead of inserting a NULL packet, check if there is an awaiting
			//packet to replace by it.
			if (pktNumSinceLastStepLayerA < pktPerStepIntervalLayerA) {
				if (awaintingPktA.size()) {
					memcpy(stream, awaintingPktA[0], 204);
					delete awaintingPktA[0];
					awaintingPktA.erase(awaintingPktA.begin());
				}
				isdbtInfo.setLayerIndicator(HIERARCHY_A);
				pktNumSinceLastStepLayerA++;
			} else if (pktNumSinceLastStepLayerB < pktPerStepIntervalLayerB) {
				if (awaintingPktB.size()) {
					memcpy(stream, awaintingPktB[0], 204);
					delete awaintingPktB[0];
					awaintingPktB.erase(awaintingPktB.begin());
				}
				isdbtInfo.setLayerIndicator(HIERARCHY_B);
				pktNumSinceLastStepLayerB++;
			} else if (pktNumSinceLastStepLayerC < pktPerStepIntervalLayerC) {
				if (awaintingPktC.size()) {
					memcpy(stream, awaintingPktC[0], 204);
					delete awaintingPktC[0];
					awaintingPktC.erase(awaintingPktC.begin());
				}
				isdbtInfo.setLayerIndicator(HIERARCHY_C);
				pktNumSinceLastStepLayerC++;
			} else {
				if (ofdmFrameCounter == (ofdmFrameSize - 2)) {
					writeIIPPacket(stream, odfmFrameEven);
					isdbtInfo.setLayerIndicator(IIP_NO_HIERARCHY);
				} else {
					isdbtInfo.setLayerIndicator(NULL_TSP);
				}
			}
		} else {
			isdbtInfo.setLayerIndicator(NULL_TSP);
			cout << "Muxer::fillPacket204 - Warning: Packet PID = " <<
					pid << " hasn't been mapped to any layer." << endl;
		}
	}
	isdbtInfo.setFrameHeadPacketFlag(ofdmFrameCounter == 0);
	isdbtInfo.setFrameIndicator(odfmFrameEven);
	isdbtInfo.setTSPCounter(ofdmFrameCounter);
	ofdmFrameCounter++;
	if (ofdmFrameCounter == ofdmFrameSize) {
		odfmFrameEven = !odfmFrameEven;
		ofdmFrameCounter = 0;
		if (ofdmFrameSignal) {
			ofdmFrameSignal = false;
			prepareDummyOfdmFrame();
		}
	}
	isdbtInfo.updateStream();
	isdbtInfo.getStream(&isdbtInfoStream);
	memcpy(stream + 188, isdbtInfoStream, 8);

	//addRS();

	memset(stream + 196, 0xFF, 8);

	if (npid) {
		*npid = (((stream[1] & 0x1F) << 8) | (stream[2] & 0xFF));
		if (*npid == 0x1FF0) *npid = 0x1FFF;
	}

	return 204;
}

int Muxer::processNullPackets() {
	map<unsigned short, int64_t>::iterator it;
	TSPacket* packet;
	char* pktBuffer;
	int rw = 0;
	unsigned short pid;

	while (pktNumSinceLastStep < pktPerStepInterval) {
		rw = -1;

		packet = new TSPacket(0, NULL, 0, NULL);
		packet->setAdaptationFieldControl(1);
		packet->setStartIndicator(0);
		packet->setPid(8191);
		packet->setContinuityCounter(0);
		packet->getStream(&pktBuffer);

		if (packetSize == 204) {
			if (fillPacket204(pktBuffer, 8191, &pid) > 0) {
				if (pid != 0x1FFF) {
					//An awaiting packet has been used instead of a NULL packet.
					setPacketContinuityCounterDirectly(pktBuffer, ++pktControl[pid]);
				}
				rw = writeStream(pktBuffer);
			}
		} else {
			rw = writeStream(pktBuffer);
		}

		delete packet;

		if (rw >= 0) {
			if (countPktPcrs() > 0) { //because writeStream() has been called.
				rw = writePcrs();
				break;
			}
		}
	}
	return rw;
}

int Muxer::writeStream(char* pktBuffer) {
	int rw;

	memcpy(streamBuffer + streamBufferLength, pktBuffer, packetSize);
	streamBufferLength += packetSize;
	rw = sendStreamBuffer();
	if ((rw != 0) && (rw != streamBufferSize)) {
		cout << "Muxer::writeStream - Packet not sent." << endl;
		return -2;
	}

	if (ofdmBufferReady && (ofdmFrameCounter == 0)) { //by signal only
		int pos = 0, rem, ofs;
        ofs = ofdmFrameSize * packetSize;
		ofdmBufferReady = false;
		while (pos < ofs) {
			rem = streamBufferSize - streamBufferLength;
			if (rem > ofs - pos) rem = ofs - pos;
			memcpy(streamBuffer + streamBufferLength, ofdmBuffer + pos, rem);
			streamBufferLength += rem;
			pos += rem;
			rw = sendStreamBuffer();
		}
		cout << "OFDM frame inserted." << endl;
	}

	//packetCounter++;
	pktNumSinceLastStep++;
	if (pktNumSinceLastStep == pktPerStepInterval) {
		resetPacketCounters();
	} else {
		stcOffset = calculatePcrTime(stcZeroed, 0, pktNumSinceLastStep, tsBitrate, &stcOffsetRemainder);
	}
	return rw;
}

void Muxer::setPacketContinuityCounterDirectly(char* stream, unsigned char cc) {
	stream[3] = stream[3] & 0xF0;
	stream[3] = stream[3] | (cc & 0x0F);
}

void Muxer::prepareDummyOfdmFrame() {
	TSPacket* packet;
	char* pktBuffer;
	char* isdbtBuffer;
	ISDBTInformation *isdbt;
	int i;

	isdbt = new ISDBTInformation();
	isdbt->setLayerIndicator(NULL_TSP);

	packet = new TSPacket(0, NULL, 0, NULL);
	packet->setAdaptationFieldControl(1);
	packet->setStartIndicator(0);
	packet->setPid(8191);
	packet->setContinuityCounter(0);
	packet->getStream(&pktBuffer);

	for (i = 0; i < ofdmFrameSize; i++) {
		isdbt->setFrameHeadPacketFlag(i == 0);
		isdbt->setFrameIndicator(odfmFrameEven);
		isdbt->setTSPCounter(i);
		isdbt->updateStream();
		isdbt->getStream(&isdbtBuffer);
		memcpy(pktBuffer + 188, isdbtBuffer, 8);
		memcpy(ofdmBuffer + (i * 204), pktBuffer, 204);
	}
	odfmFrameEven = !odfmFrameEven;
	ofdmBufferReady = true;

	delete packet;
	delete isdbt;
}

}
}
}
}


