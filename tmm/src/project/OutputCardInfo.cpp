/*
 * OutputCardInfo.cpp
 *
 *  Created on: 7 de set de 2017
 *      Author: Felippe Nagato
 */

#include "project/OutputCardInfo.h"

namespace br {
namespace pucrio {
namespace telemidia {
namespace tool {

OutputCardInfo::OutputCardInfo() : PhysicalInfo() {
	packetSize = 0;
	bitrate = 0;
	frequency = 0;
	txpower = 0;
}

OutputCardInfo::~OutputCardInfo() {

}

void OutputCardInfo::setPacketSize(unsigned char size) {
	packetSize = size;
}

unsigned char OutputCardInfo::getPacketSize() {
	return packetSize;
}

void OutputCardInfo::setBitrate(unsigned int rate) {
	bitrate = rate;
}

unsigned int OutputCardInfo::getBitrate() {
	return bitrate;
}

void OutputCardInfo::setFrequency(unsigned int freq) {
	frequency = freq;
}

unsigned int OutputCardInfo::getFrequency() {
	return frequency;
}

void OutputCardInfo::setTxPower(char power) {
	txpower = power;
}

char OutputCardInfo::getTxPower() {
	return txpower;
}


}
}
}
}


