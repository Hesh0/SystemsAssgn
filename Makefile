CC=clang
CFLAGS= -std=c11 -Wall -g

clutter = SpreadSheet.txt SpreadSheet.bin

SpreadsheetServer: SpreadsheetServer.o
	 clang -o ./bin/SpreadsheetServer SpreadsheetServer.o

SpreadsheetServer.o: SpreadsheetServer.c SpreadsheetServer.h
	clang -c SpreadsheetServer.c

Client: Client.o
	clang -o ./bin/Client Client.o 


Client.o: Client.c SpreadsheetServer.h
	clang -c Client.c

clean:
	rm $(clutter) *.o
