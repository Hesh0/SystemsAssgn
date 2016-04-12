CC=clang
CFLAGS= -std=c11 -Wall -g

clutter = SpreadSheet.txt SpreadSheet.o SpreadSheet.bin

SpreadSheet: SpreadSheet.o
	 clang -o ./bin/SpreadSheet SpreadSheet.o

SpreadSheet.o: SpreadSheet.c SpreadSheet.h
	clang -c SpreadSheet.c

clean:
	rm $(clutter)
