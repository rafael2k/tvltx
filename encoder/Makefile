#
#    TVLTx - TV Lovers Encoder
#    Copyright (C) 2012-2017 SDR Telecom
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU Affero General Public License as
#    published by the Free Software Foundation, either version 3 of the
#    License, or (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU Affero General Public License for more details.
#
#    You should have received a copy of the GNU Affero General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#  Instructions:
# In order to build with BlackMagic input cards support:
# $ make
#
# In order to build without BackMagic input cards support:
# $ make encoder_no_bm
#

CC=gcc
CPP=g++
CPPFLAGS=-Wno-multichar -I./bmd -fno-rtti
CFLAGS=-Wall -std=gnu99 -I/usr/include/fdk-aac/
LDFLAGS=-lm -ldl -lrt -lpthread -lfdk-aac
BM_LDFLAGS=-lDeckLinkAPI
BM_CFLAGS=-DHAS_BLACKMAGIC__

all: encoder

encoder_no_bm: main.o hexdump.o buffer.o pes.o input_no_bm.o encoder.o output.o ring_buffer.o
	$(CC) $(LDFLAGS) main.o hexdump.o buffer.o pes.o input_no_bm.o encoder.o output.o ring_buffer.o -o encoder_no_bm

encoder: main.o hexdump.o buffer.o pes.o input.o encoder.o output.o ring_buffer.o bmd/Capture.o
	$(CPP) $(LDFLAGS) $(BM_LDFLAGS) main.o hexdump.o buffer.o pes.o input.o encoder.o output.o ring_buffer.o bmd/Capture.o -o encoder

main.o: main.c encoder_opts.h macros.h
	$(CC) $(CFLAGS) main.c -c -o main.o

hexdump.o: hexdump.c hexdump.h macros.h
	$(CC) $(CFLAGS) hexdump.c -c -o hexdump.o

buffer.o: buffer.c ring_buffer.h
	$(CC) $(CFLAGS) buffer.c -c -o buffer.o

pes.o: pes.c pes.h
	$(CC) $(CFLAGS) pes.c -c -o pes.o

input.o: input.c input.h encoder_opts.h macros.h
	$(CC) $(CFLAGS) $(BM_CFLAGS) input.c -c -o input.o

input_no_bm.o: input.c input.h encoder_opts.h macros.h
	$(CC) $(CFLAGS) input.c -c -o input_no_bm.o

encoder.o: encoder.c encoder.h encoder_opts.h macros.h
	$(CC) $(CFLAGS) encoder.c -c -o encoder.o

output.o: output.c output.h encoder_opts.h macros.h
	$(CC) $(CFLAGS) output.c -c -o output.o

ring_buffer.o: ring_buffer.c ring_buffer.h encoder_opts.h macros.h
	$(CC) $(CFLAGS) ring_buffer.c -c -o ring_buffer.o

bmd/Capture.o: bmd/Capture.cpp bmd/Capture.h
	$(CPP) $(CPPFLAGS) bmd/Capture.cpp -c -o bmd/Capture.o

clean:
	rm -f encoder main.o buffer.o input.o input_no_bm.o encoder.o output.o ring_buffer.o bmd/Capture.o *~

install:
	install encoder /usr/bin
