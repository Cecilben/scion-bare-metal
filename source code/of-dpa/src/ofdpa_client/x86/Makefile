
ofdpa_test_client: ../ofdpa_client.cpp common.o
	g++ -std=c++14 -I ../../include -I ../../common ../ofdpa_client.cpp common.o -L ../../../bin/as5812 -l:librpc_client.so.1 -o ofdpa_test_client

common.o: ../../common/common.h ../../common/common.cpp
	g++ -std=c++14 -I ../../include -c ../../common/common.cpp -o common.o

clean:
	rm -f *.o
	rm -f ofdpa_test_client
