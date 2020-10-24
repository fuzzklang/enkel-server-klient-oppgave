CC = gcc
DEBUG = -Werror -Wfatal-errors -Wextra -Wpedantic -pedantic-errors
#DEBUG =
CFLAGS = -std=gnu99 -g -Wall $(DEBUG)
BIN = client server
OPTS =

all: $(BIN) makefile

client: client.o debug_print.o network.o files.o pgmread.o send_packet.o
	$(CC) $(CFLAGS) $^ -o $@

server: server.o debug_print.o network.o files.o pgmread.o send_packet.o
	$(CC) $(CFLAGS) $^ -o $@

client.o: client.c my_constants.h network.h
	$(CC) $(CFLAGS) -c $<

server.o: server.c my_constants.h network.h
	$(CC) $(CFLAGS) -c $<

network.o: network.c network.h debug_print.o my_constants.h
	$(CC) $(CFLAGS) -c $<

files.o: files.c files.h debug_print.o pgmread.o my_constants.h
	$(CC) $(CFLAGS) -c $<

debug_print.o: debug_print.c my_constants.h
	$(CC) $(CFLAGS) -c $<

pgmread.o: pgmread.c
	$(CC) $(CFLAGS) -c $<

send_packet.o: send_packet.c my_constants.h
	$(CC) $(CFLAGS) -c $<

test_client: client
	./client 127.0.0.1 2020 list_of_filenames.txt 10 $(OPTS)

test_server: server
	./server 2020 reduced_set compare_output.txt $(OPTS)

test_client_valgrind: client
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./client 127.0.0.1 2020 list_of_filenames.txt 10 $(OPTS)

test_server_valgrind: server
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./server 2020 reduced_set compare_output.txt $(OPTS)

clean:
	rm -f $(BIN) *.o
