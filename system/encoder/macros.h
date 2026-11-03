/*
    TVLTx - TV Lovers Encoder
    Copyright (C) 2012-2017 SDR Telecom

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
*/

#ifndef HAVE_MACROS_H__
#define HAVE_MACROS_H__

#include <stdio.h>

// verbose level
#define VERBOSE__ 1

#define tvltx_info(...) fprintf(stderr, __VA_ARGS__)

#if (VERBOSE__ == 1)
#define tvltx_debug(...) fprintf(stderr, __VA_ARGS__)
#endif

#if (VERBOSE__ == 0)
#define tvltx_debug(...)
#endif

#endif /* HAVE_MACROS_H__ */
