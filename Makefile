all: mush2
mush2: mush2.o mush2.h
	gcc -g -Wall -L ~pn-cs357/Given/Mush/lib64 -o mush2 mush2.o -lmush
mush2.o: mush2.c
	gcc -c -g -Wall -I ~pn-cs357/Given/Mush/include mush2.c
clean: 
	rm -f mush2.o


