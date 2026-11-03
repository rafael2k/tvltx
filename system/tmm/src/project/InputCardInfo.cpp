/*
 * InputCardInfo.cpp
 *
 *  Created on: 01/05/2017
 *      Author: Felippe Nagato
 */

#include "project/InputCardInfo.h"

namespace br {
namespace pucrio {
namespace telemidia {
namespace tool {

InputCardInfo::InputCardInfo() : PhysicalInfo() {
	mode = 0;
	numberOfChannels = 0;
}

InputCardInfo::~InputCardInfo() {

}

void InputCardInfo::setMode(unsigned char mode) {
	this->mode = mode;
}

unsigned char InputCardInfo::getMode() {
	return mode;
}

void InputCardInfo::setNumberOfChannels(unsigned char num) {
	numberOfChannels = num;
}

unsigned char InputCardInfo::getNumberOfChannels() {
	return numberOfChannels;
}

void InputCardInfo::setVideoDevice(string dev) {
	videoDevice = dev;
}

string InputCardInfo::getVideoDevice() {
	return videoDevice;
}

void InputCardInfo::setAudioDevice(string dev) {
	audioDevice = dev;
}

string InputCardInfo::getAudioDevice() {
	return audioDevice;
}

void InputCardInfo::setVideoStandard(string std) {
	videoStandard = std;
}

string InputCardInfo::getVideoStandard() {
	return videoStandard;
}

}
}
}
}


