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

#ifndef CRC32_H_
#define CRC32_H_

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

namespace br {
namespace pucrio {
namespace telemidia {
namespace mpeg2 {

class Crc32 {
	private:

	protected:
		unsigned int dsmcc_crc_tab[0x100];

	public:
		Crc32();
		~Crc32();

		unsigned int crc (char *data, unsigned int len);

};

}
}
}
}

#endif /* CRC32_H_ */
