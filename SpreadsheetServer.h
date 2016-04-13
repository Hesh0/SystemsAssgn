#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>

#define BUFFER_SIZE 4096
#define IN_BUF_LIMIT 80 // Limit for stdin, etc.
#define GRID 81 // Size of spreadsheet.
#define CELL_ADDRESS 3
#define DISPLAY 5 // Size of value to be displayed in each cell.
#define TRUNCATE 12 // Size of value to be displayed in each cell if truncated.
#define LOCAL_HOST "127.0.0.1"
#define PORT_NO 60000
#define FILE_NAME "../SpreadSheet.txt"
#define BIN_FILE "../SpreadSheet.bin"

#define SOCKET_WRITE(socketfd, msg) write(socketfd, msg, strlen(msg))

/**
 * CellType enum to determine if value in cell is numeric, alphanumeric
 * or is a formula.
 */
typedef enum {
    ALNUM,
    FORMULA,
    NUM
} CellType;

/**
 * FormulaType enum to determine the type of formula entered (SUM, RANGE, AVERAGE).
 */
typedef enum {
    SUM,
    RANGE,
    AVERAGE
} FormulaType;

/**
 * struct to Represent a Cell within spreadsheet.
 * value: Value contained within cell.
 * address: address of cell.
 * type: type of value contained within cell (numeric, alphanumeric, formula).
 */
typedef struct {
    char value[IN_BUF_LIMIT];
    char address[CELL_ADDRESS];
    CellType type;
} Cell;

// void die(const char* errMsg);
bool isFirstRun();
// init
int initServer(struct sockaddr_in* cli_addr, socklen_t* cli_addr_len);
void acceptConnections(int socketfd, struct sockaddr_in* cli_addr, socklen_t* cli_addr_len);
bool doSpreadsheetStuff(int socket);
bool loadLastSpreadSheet(Cell spreadSheet[], int socket);
double calculateFormula(char *, FormulaType *, Cell [], size_t *);
bool isFormula(char *, char *, FormulaType *);
char *prepareForDisplay(char *);
char *truncateOrNah(char *);
char *trimEnd(char *);
bool validCell(char* cellLabel);
bool findCell(Cell[], char *, size_t *);
bool cellAlNum(char *cellValue);
void run(char *, Cell[], char *, size_t *, char *, FormulaType *, int socket);
void initSpreadSheet(Cell[]);
bool validCell(char* cellLabel);
bool prompt(char *, char *, int socket);
void renderSpreadSheet(Cell spreadsheet[], FILE* file, int socket);
void displaySpreadSheetToClient(Cell spreadsheet[], int socket);
void saveWorkSheet(Cell spreadsheet[], int socket);
void saveLastSpreadSheet(Cell spreadsheet[]);
bool loadLastSpreadSheet(Cell spreadsheet[], int socket);

void die(const char* errMsg)
{
   perror(errMsg);
   exit(EXIT_FAILURE);
}

char* readInput(char* str, int count, FILE* stream)
{
	char* find;
	char ch;

	fgets(str, count, stream);
	if (!str)
	{
		fprintf(stderr, "Error reading input into string\n");
		return NULL;
	}
	find = strchr(str, '\n');
	if (find)
		*find = '\0';
	else
		while (getchar() != '\n')
			continue;

	return str;
}
