CPP = g++
CC = gcc

CPPFLAGS = -Wno-write-strings -Wno-multichar -fno-rtti -fexceptions -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -O2 -g -fcommon
CFLAGS = -std=gnu99 -Wall -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -O2 -g -fcommon
LDFLAGS = -ldl -lm -lrt -lpthread 


ts_muxer: ts_muxer.o DekTec/DtPlay.o DekTec/DtOpt.o ring_buffer.o DekTec/DTAPI64.o 
	$(CPP) $(LDFLAGS) ts_muxer.o DekTec/DtPlay.o DekTec/DtOpt.o ring_buffer.o DekTec/DTAPI64.o -o ts_muxer

ts_muxer.o: ts_muxer.c
	$(CC) $(CFLAGS) -c ts_muxer.c -o ts_muxer.o

DekTec/DtPlay.o: DekTec/DtPlay.cpp output.h
	$(CPP) $(CPPFLAGS) $(INCLUDE_FLAGS) DekTec/DtPlay.cpp -c -o DekTec/DtPlay.o

DekTec/DtOpt.o: DekTec/DtOpt.cpp
	$(CPP) $(CPPFLAGS) $(INCLUDE_FLAGS) DekTec/DtOpt.cpp -c -o DekTec/DtOpt.o

ring_buffer.o: ring_buffer.c
	$(CC) $(CFLAGS) ring_buffer.c -c -o ring_buffer.o

clean:
	rm -f ts_muxer ts_muxer.o DekTec/DtPlay.o DekTec/DtOpt.o ring_buffer.o

install: ts_muxer
	install ts_muxer /usr/bin
