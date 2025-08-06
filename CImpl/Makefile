
all: server client

server: server.o tcp_utils.o
	cc -o server server.o tcp_utils.o

client: client.o tcp_utils.o
	cc -o client client.o tcp_utils.o

server.o: server.c tcp_utils.h
	cc -c server.c

client.o: client.c tcp_utils.h
	cc -c client.c

tcp_utils.o: tcp_utils.c tcp_utils.h
	cc -c tcp_utils.c

clean:
	rm -f *.o

distclean: clean
	rm -f server client

