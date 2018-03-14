# You can compile with either gcc or g++
CC = g++
CFLAGS = -g -o
LINKED = -lPocoNet -lPocoUtil -lPocoFoundation -lPocoCrypto -lPocoNetSSL -lssl -lcrypto
OBJ = ProxyServerConnection.o ProxyServerCache.o ProxyRequestHandler.o ProxyRequestHandlerFactory.o proxy_server.o

all:
	$(CC) -c proxy_server.cpp -std=c++11
	$(CC) -c ProxyRequestHandler.cpp -std=c++11
	$(CC) -c ProxyRequestHandlerFactory.cpp -std=c++11
	$(CC) -c ProxyServerConnection.cpp -std=c++11
	$(CC) -c ProxyServerCache.cpp -std=c++11
	$(CC) $(CFLAGS) proxy_server $(OBJ) $(LINKED) -std=c++11

clean:
	rm proxy_server
