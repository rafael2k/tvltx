/*
 * InputCardInfo.h
 *
 *  Created on: 01/05/2017
 *      Author: Felippe Nagato
 */

#ifndef INPUTCARDINFO_H_
#define INPUTCARDINFO_H_

#include <string>
#include "PhysicalInfo.h"

using namespace std;

namespace br {
namespace pucrio {
namespace telemidia {
namespace tool {

class InputCardInfo : public PhysicalInfo {

	#define INPUT_TYPE_NONE				0x00
	#define INPUT_TYPE_SDI				0x01
	#define INPUT_TYPE_HDMI				0x02
	#define INPUT_TYPE_COMPOSITE		0x03
	#define INPUT_TYPE_COMPONENT		0x04
	#define INPUT_TYPE_OPTICALSDI		0x05
	#define INPUT_TYPE_SVIDEO			0x06

	#define VIDEO_IN_MODE_1080_P_60		0x01
	#define VIDEO_IN_MODE_1080_P_5994	0x02
	#define VIDEO_IN_MODE_1080_P_50		0x03
	#define VIDEO_IN_MODE_1080_I_5994	0x04
	#define VIDEO_IN_MODE_1080_I_60		0x05
	#define VIDEO_IN_MODE_1080_I_50		0x06
	#define VIDEO_IN_MODE_1080_P_30		0x07
	#define VIDEO_IN_MODE_1080_P_2997	0x08
	#define VIDEO_IN_MODE_1080_P_25		0x09
	#define VIDEO_IN_MODE_720_P_60		0x0A
	#define VIDEO_IN_MODE_720_P_5994	0x0B
	#define VIDEO_IN_MODE_720_P_50		0x0C
	#define VIDEO_IN_MODE_720_P_30		0x0D
	#define VIDEO_IN_MODE_720_P_2997	0x0E
	#define VIDEO_IN_MODE_720_P_25		0x0F
	#define VIDEO_IN_MODE_480_I_5994	0x10
	#define VIDEO_IN_MODE_480_P_2997	0x11
	#define VIDEO_IN_MODE_576_I_50		0x12
	#define VIDEO_IN_MODE_576_P_25		0x13

	private:

	protected:
		unsigned char mode;
		unsigned char numberOfChannels;
		string videoDevice;
		string audioDevice;
		string videoStandard;

	public:
		InputCardInfo();
		virtual ~InputCardInfo();

		void setMode(unsigned char mode);
		unsigned char getMode();
		void setNumberOfChannels(unsigned char num);
		unsigned char getNumberOfChannels();
		void setVideoDevice(string dev);
		string getVideoDevice();
		void setAudioDevice(string dev);
		string getAudioDevice();
		void setVideoStandard(string std);
		string getVideoStandard();

};

}
}
}
}


#endif /* INPUTCARDINFO_H_ */
