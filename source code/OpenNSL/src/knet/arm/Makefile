
knet: ../knet.cpp util.o common.o
	arm-linux-gnueabi-g++ -std=c++14 -I ../../../include -I ../../common ../knet.cpp util.o common.o -L ../../../bin/as4610 -Wl,-Bdynamic -l:libopennsl.so.1 -static-libstdc++ -o knet

util.o: ../../common/util.c
	arm-linux-gnueabi-gcc -c ../../common/util.c -o util.o

common.o: ../../common/common.h ../../common/common.cpp
	arm-linux-gnueabi-g++ -std=c++14 -I ../../../include -c ../../common/common.cpp -o common.o

clean:
	rm -f *.o
	rm -f knet
