/*
    TVLTx - TV Lovers Encoder
    Copyright (C) 2016-2017 SDR Telecom

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

#ifndef HAVE_INPUT_H__
#define HAVE_INPUT_H__

#ifdef __cplusplus
extern "C" {
#endif

int input_start(input_config *input);
void *input_bmd_thread(void *input_v);
void *input_test_thread_v(void *input_v);
void *input_test_thread_a(void *input_a);

#ifdef __cplusplus
};
#endif

#endif /* HAVE_INPUT__ */
