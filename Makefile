CFLAGS+=$(shell pkg-config --cflags libuv)
LIBS+=$(shell pkg-config --libs libuv)
LIBS+=-lncurses

all: ppclient pingpong

ppclient: client.o
	$(CC) -o $@ $< $(LIBS)

pingpong: server.o
	$(CC) -o $@ $< $(LIBS)


clean:
	rm -f *.o
	rm -f ppclient
	rm -f pingpong

%.o:%.c
	$(CC) -c $(CFLAGS) -o $@ $^ 

.PHONY: clean
