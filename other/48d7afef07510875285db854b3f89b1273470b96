all:
	gcc -g -c -fpic mem.c -Wall 
	gcc -g -shared -o libmem.so mem.o
	gcc -g -lmem -L. -lm -o test test.c -Wall 
clean:
	rm -f libmem.so *.o

export LD_LIBRARY_PATH=$(pwd)

