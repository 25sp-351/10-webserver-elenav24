all: webserver

OBJS = webserver.o
CC = clang
CFLAGS = -g -Wall
TEST_SCRIPT = tests.sh

webserver: webserver.o
	clang -o webserver $(CFLAGS) $(OBJS)
webserver.o: webserver.c
	clang -c $(CFLAGS) webserver.c
	clang -o webserver webserver.o $(CFLAGS)

clean:
	rm -f webserver $(OBJS)

test: webserver
	./$(TEST_SCRIPT)