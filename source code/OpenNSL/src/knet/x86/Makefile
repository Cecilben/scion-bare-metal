
knet: ../knet.cpp common.o
	g++ -std=c++14 -I ../../../include -I ../../common ../knet.cpp common.o -L ../../../bin/as5812 -l:libopennsl.so.1 -o knet

common.o: ../../common/common.h ../../common/common.cpp
	g++ -std=c++14 -I ../../../include -c ../../common/common.cpp -o common.o

clean:
	rm -f *.o
	rm -f knet
