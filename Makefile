# Stupid makefile for building the proxy.
CFLAGS = -c -g
FLAGS = -g

PROXY = test_proxy.o message_source_gny.o message_source_11.o message_sink_11.o message_sink_gny.o http_message.o common.o http.o server.o client.o http_proxy.o multiconn.o requester.o identity.o
SERVER = test_http_server.o http_message.o http.o message_source_gny.o message_source_11.o message_sink_11.o message_sink_gny.o common.o http_server.o server.o multiconn.o identity.o
REQUESTER = http.o http_message.o message_source_gny.o message_sink_gny.o message_source_11.o message_sink_11.o requester.o client.o multiconn.o common.o request_gen.o identity.o

default: PROXY HTTP_SERVER REQUESTER

http_message.o: http_message.cpp http_message.h
	g++ $(CFLAGS) http_message.cpp

common.o: common.cpp common.h
	g++ $(CFLAGS) common.cpp

identity.o: identity.cpp identity.h
	g++ $(CFLAGS) identity.cpp

multiconn.o: multiconn.cpp multiconn.h
	g++ $(CFLAGS) multiconn.cpp

http.o: http.cpp http.h
	g++ $(CFLAGS) http.cpp

message_sink_gny.o: message_sink_gny.cpp message_sink_gny.h
	g++ $(CFLAGS) message_sink_gny.cpp

message_source_gny.o: message_source_gny.cpp message_source_gny.h
	g++ $(CFLAGS) message_source_gny.cpp 

message_source_11.o: message_source_11.cpp message_source_11.h
	g++ $(CFLAGS) message_source_11.cpp 

message_sink_11.o: message_sink_11.cpp message_sink_11.h
	g++ $(CFLAGS) message_sink_11.cpp

client.o: client.cpp client.h
	g++ $(CFLAGS) client.cpp

server.o: server.cpp server.h
	g++ $(CFLAGS) server.cpp

http_server.o: http_server.cpp http_server.h
	g++ $(CFLAGS) http_server.cpp

http_proxy.o: http_proxy.cpp http_proxy.h
	g++ $(CFLAGS) http_proxy.cpp

requester.o: requester.cpp requester.h
	g++ $(CFLAGS) requester.cpp

# Test files.
test_http_server.o: test_http_server.cpp 
	g++ $(CFLAGS) test_http_server.cpp

test_proxy.o: test_proxy.cpp
	g++ $(CFLAGS) test_proxy.cpp

request_gen.o: request_gen.cpp
	g++ $(CFLAGS) request_gen.cpp

# Binaries
PROXY: $(PROXY)
	g++ $(PROXY) $(FLAGS) -o proxy

HTTP_SERVER: $(SERVER)
	g++ $(SERVER) $(FLAGS) -o http_server

REQUESTER: $(REQUESTER)
	g++ $(REQUESTER) $(FLAGS) -o request_gen

clean:
	rm -f *.o proxy http_server request_gen
