
ofdpa_test_client: ../ofdpa_client.cpp common.o
	arm-linux-gnueabi-g++ -std=c++14 -I ../../include -I ../../common ../ofdpa_client.cpp common.o -L ../../../bin/as4610 -Wl,-Bdynamic -l:librpc_client.so.1 -static-libstdc++ -o ofdpa_test_client

common.o: ../../common/common.h ../../common/common.cpp
	arm-linux-gnueabi-g++ -std=c++14 -I ../../include -c ../../common/common.cpp -o common.o

clean:
	rm -f *.o
	rm -f ofdpa_test_client
