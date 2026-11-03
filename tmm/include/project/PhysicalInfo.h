/*
 * PhysicalInfo.h
 *
 *  Created on: 01/05/2017
 *      Author: Felippe Nagato
 */

#ifndef PHYSICALINFO_H_
#define PHYSICALINFO_H_

using namespace std;

namespace br {
namespace pucrio {
namespace telemidia {
namespace tool {

class PhysicalInfo {

	#define CARD_TYPE_TEST				0x00
	#define CARD_TYPE_DEKTEC			0x01
	#define CARD_TYPE_BLACKMAGIC		0x02
	#define CARD_TYPE_DVEO				0x03
	#define CARD_TYPE_CHINA1			0x80
	#define CARD_TYPE_CHINA2			0x81
	#define CARD_TYPE_CHINA3			0x82

	private:

	protected:
		int id;
		unsigned char cardNumber;
		unsigned char brand;
		unsigned char type;

	public:
		PhysicalInfo();
		virtual ~PhysicalInfo();

		void setId(int id);
		int getId();
		void setCardNumber(unsigned char num);
		unsigned char getCardNumber();
		void setBrand(unsigned char brand);
		unsigned char getBrand();
		void setType(unsigned char type);
		unsigned char getType();

};

}
}
}
}


#endif /* PHYSICALINFO_H_ */
