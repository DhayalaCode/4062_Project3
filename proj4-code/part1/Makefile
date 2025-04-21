CFLAGS = -Wall -Werror -g
CC = gcc $(CFLAGS)

http_server: http_server.o http.o
	$(CC) -o $@ $^

http_server.o: http_server.c
	$(CC) -c $<

http.o: http.c http.h
	$(CC) -c $<

clean:
	rm -rf *.o http_server

zip:
	@echo "ERROR: You cannot run 'make zip' from the part1 subdirectory. Change to the main proj4-code directory and run 'make zip' there."
