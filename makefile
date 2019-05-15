CFLAGS = -Wall -g -pthread
CC     = gcc $(CFLAGS)

blather : bl_server bl_client

bl_server : bl_server.o util.o server_funcs.o
	$(CC) -o bl_server bl_server.o util.o server_funcs.o

bl_client : bl_client.o util.o simpio.o
	$(CC) -o bl_client bl_client.o util.o simpio.o

bl_server.o : bl_server.c blather.h
	$(CC) -c $<

server_funcs.o : server_funcs.c blather.h
	$(CC) -c $<

bl_client.o : bl_client.c blather.h
	$(CC) -c $<

util.o : util.c blather.h
	$(CC) -c $<

simpio.o : simpio.c blather.h
	$(CC) -c $<

clean :
	rm -f bl_server bl_client *.o *.fifo

TEST_PROGRAMS = test_blather.sh test_blather_data.sh normalize.awk cat_sig.sh filter-semopen-bug.awk

test : test-blather

make test-blather : bl_client bl_server $(TEST_PROGRAMS)
	chmod u+rx $(TEST_PROGRAMS)
	./test_blather.sh

clean-tests :
	cd test-data && \
	rm -f *.*
