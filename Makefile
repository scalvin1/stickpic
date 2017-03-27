all: stickpic

stickpic: stickpic.c minilzo.c
	gcc -c minilzo.c -O3
	gcc -o stickpic stickpic.c minilzo.o -O3 -lgd -lm

clean:
	rm -f minilzo.o stickpic
