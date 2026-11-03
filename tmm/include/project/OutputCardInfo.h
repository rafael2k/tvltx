/*
 * OutputCardInfo.h
 *
 *  Created on: 7 de set de 2017
 *      Author: Felippe Nagato
 */

#ifndef INCLUDE_PROJECT_OUTPUTCARDINFO_H_
#define INCLUDE_PROJECT_OUTPUTCARDINFO_H_

#include "PhysicalInfo.h"

using namespace std;

namespace br {
namespace pucrio {
namespace telemidia {
namespace tool {

class OutputCardInfo : public PhysicalInfo {

	#define OUTPUT_TYPE_NONE			0x00
	#define OUTPUT_TYPE_ASI				0x01
	#define OUTPUT_TYPE_MODULATION		0x02
	#define OUTPUT_TYPE_IP				0x03

	private:

	protected:
		unsigned char packetSize;
		unsigned int bitrate; //bps
		unsigned int frequency; //Hz
		char txpower; //dBm

	public:
		OutputCardInfo();
		virtual ~OutputCardInfo();

		void setPacketSize(unsigned char size);
		unsigned char getPacketSize();
		void setBitrate(unsigned int rate);
		unsigned int getBitrate();
		void setFrequency(unsigned int freq);
		unsigned int getFrequency();
		void setTxPower(char power);
		char getTxPower();

};

}
}
}
}


#endif /* INCLUDE_PROJECT_OUTPUTCARDINFO_H_ */
