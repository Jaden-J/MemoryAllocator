test_mem:
	gcc -g -Wall -o $@ memalloc.c pintos/list.c pintos/test_mem.c -lpthread

clean:
	/bin/rm -rf test_mem *.o *.dSYM
