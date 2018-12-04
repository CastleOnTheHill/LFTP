CC = g++
# g++ server.cpp mytcp.cpp -o server -g
# g++ client.cpp mytcp.cpp -o client -g
compile : server.o client.o mytcp.o mutiClientServer.o
	$(CC) -g server.o mytcp.o -o server -lpthread
	$(CC) -g client.o mytcp.o -o client -lpthread
	$(CC) -g mutiClientServer.o -o mutiClientServer -lpthread


runc : client
	./client get 120.78.92.75 file.pdf
#	./client get 192.168.137.47 file.pdf
#	./client get 127.0.0.1 file.pdf
runs : server
	./mutiClientServer

mutiClientServer.o : mutiClientServer.cpp mytcp.h
	$(CC) -c -g mutiClientServer.cpp -o mutiClientServer.o -lpthread
server.o : server.cpp mytcp.h
	$(CC) -c -g server.cpp -o server.o -lpthread
client.o : client.cpp mytcp.h
	$(CC) -c -g client.cpp -o client.o -lpthread
mytcp.o : mytcp.cpp mytcp.h
	$(CC) -c -g mytcp.cpp -o mytcp.o -lpthread

clean : 
	rm server client mutiClientServer
	rm *.o