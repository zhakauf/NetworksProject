default: client server

client.o: client.c client.h trs.h
	gcc -c client.c -o client.o

client: client.o
	gcc client.o -o client

server.o: server.c server.h trs.h
	gcc -c server.c -o server.o

server: server.o
	gcc server.o -o server

clean:
	-rm -f client.o server.o
	-rm -f client server

