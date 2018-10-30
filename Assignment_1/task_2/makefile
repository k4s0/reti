CC = gcc
ARGS = -Wall -Wno-pointer-sign -Wno-implicit-function-declaration

all:	udpserver udpclient

udpserver:	udpserver.c
	$(CC) $(ARGS) -o udpserver udpserver.c

udpclient:	udpclient.c
	$(CC) $(ARGS) -o udpclient udpclient.c
clean:
	rm -f *.o udpserver udpclient *~
