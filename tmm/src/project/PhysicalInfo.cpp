/*
 * PhysicalInfo.cpp
 *
 *  Created on: 01/05/2017
 *      Author: Felippe Nagato
 */

#include "project/PhysicalInfo.h"

namespace br {
namespace pucrio {
namespace telemidia {
namespace tool {

PhysicalInfo::PhysicalInfo() {
	id = 0;
	cardNumber = 0;
	brand = 0;
	type = 0;
}

PhysicalInfo::~PhysicalInfo() {

}

void PhysicalInfo::setId(int id) {
	this->id = id;
}

int PhysicalInfo::getId() {
	return id;
}

void PhysicalInfo::setCardNumber(unsigned char num) {
	cardNumber = num;
}

unsigned char PhysicalInfo::getCardNumber() {
	return cardNumber;
}

void PhysicalInfo::setBrand(unsigned char brand) {
	this->brand = brand;
}

unsigned char PhysicalInfo::getBrand() {
	return brand;
}

void PhysicalInfo::setType(unsigned char type) {
	this->type = type;
}

unsigned char PhysicalInfo::getType() {
	return type;
}

}
}
}
}


