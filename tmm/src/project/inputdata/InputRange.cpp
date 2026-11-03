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
 * InputRange.cpp
 *
 *  Created on: 27/02/2013
 *      Author: Felippe Nagato
 */

#include "project/inputdata/InputRange.h"

namespace br {
namespace pucrio {
namespace telemidia {
namespace tool {

InputRange::InputRange() {

}

InputRange::InputRange(int id) {
	this->id = id;
}

InputRange::~InputRange() {

}

void InputRange::setId(int id) {
	this->id = id;
}

void InputRange::setBegin(int begin) {
	this->begin = begin;
}

void InputRange::setEnd(int end) {
	this->end = end;
}

int InputRange::getId() {
	return id;
}

int InputRange::getBegin() {
	return begin;
}

int InputRange::getEnd()  {
	return end;
}

}
}
}
}


