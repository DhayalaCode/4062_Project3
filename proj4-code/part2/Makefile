CFLAGS = -Wall -Werror -g
CC = gcc $(CFLAGS)
port = 8000

.PHONY: all test test-setup clean clean-tests zip

all: http_server concurrent_open.so

http_server: http_server.o http.o connection_queue.o
	$(CC) -pthread -o $@ $^

http_server.o: http_server.c
	$(CC) -pthread -c $<

http.o: http.c http.h
	$(CC) -c $<

connection_queue.o: connection_queue.c connection_queue.h
	$(CC) -pthread -c $<

concurrent_open.so: concurrent_open.c
	$(CC) $(CFLAGS) -shared -fpic -o $@ $^ -ldl

test-setup:
	@chmod u+x testius
	@rm -rf downloaded_files

test: test-setup http_server clean-tests concurrent_open.so
	PORT=$(port) ./testius test_cases/tests.json -v

clean:
	rm -rf *.o concurrent_open.so http_server

clean-tests:
	rm -rf test_results
	rm -rf downloaded_files

zip:
	@echo "ERROR: You cannot run 'make zip' from the part2 subdirectory. Change to the main proj4-code directory and run 'make zip' there."
