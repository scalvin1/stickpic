all: stickpic stickpic-static

minilzo.o: minilzo.c minilzo.h
	gcc -c minilzo.c -O3

stickpic: stickpic.c minilzo.o
	gcc -o stickpic stickpic.c minilzo.o -g3 -lgd -lm 

stickpic-static: stickpic.c minilzo.o
	gcc -o stickpic-static stickpic.c minilzo.o -O3 -lgd -lpng -lm -lz -static -s

clean:
	rm -f minilzo.o stickpic stickpic-static
