CPP = g++
CC = gcc

CPPFLAGS = -Wno-write-strings -Wno-multichar -fno-rtti -fexceptions -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -O2 -g
CFLAGS = -std=gnu99 -Wall -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -O2 -g
LDFLAGS = -ldl -lm -lrt -lpthread 


bts_tx: bts_tx.o DekTec/DtPlay.o DekTec/DtOpt.o ring_buffer.o DekTec/DTAPI64.o 
	$(CPP) $(LDFLAGS) bts_tx.o DekTec/DtPlay.o DekTec/DtOpt.o ring_buffer.o DekTec/DTAPI64.o -o bts_tx

bts_tx.o: bts_tx.c bts_tx.h
	$(CC) $(CFLAGS) -c bts_tx.c -o bts_tx.o

DekTec/DtPlay.o: DekTec/DtPlay.cpp bts_tx.h
	$(CPP) $(CPPFLAGS) $(INCLUDE_FLAGS) DekTec/DtPlay.cpp -c -o DekTec/DtPlay.o

DekTec/DtOpt.o: DekTec/DtOpt.cpp bts_tx.h
	$(CPP) $(CPPFLAGS) $(INCLUDE_FLAGS) DekTec/DtOpt.cpp -c -o DekTec/DtOpt.o

ring_buffer.o: ring_buffer.c
	$(CC) $(CFLAGS) ring_buffer.c -c -o ring_buffer.o

clean:
	rm -f *~ bts_tx bts_tx.o DekTec/DtPlay.o DekTec/DtOpt.o ring_buffer.o

install: bts_tx
	install bts_tx /usr/bin
