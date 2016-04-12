#pragma once

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define BUFFER_LIMIT 4096 // Limit for stdin, etc.
#define GRID 81 // Size of spreadsheet.
#define CELL_ADDRESS 3 // Size of cell address.
#define DISPLAY 5 // Size of value to be displayed in each cell.
#define TRUNCATE 12 // Size of value to be displayed in each cell if truncated.
#define FILE_NAME "../SpreadSheet.txt"
#define BIN_FILE "../SpreadSheet.bin"


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
    char value[BUFFER_LIMIT];
    char address[CELL_ADDRESS];
    CellType type;
} Cell;

bool isFirstRun();
double calculateFormula(char *, FormulaType *, Cell [], size_t *);
bool isFormula(char *, char *, FormulaType *);
char *prepareForDisplay(char *);
char *truncateOrNah(char *);
char *trimEnd(char *);
bool validCell(char* cellLabel);
bool findCell(Cell[], char *, size_t *);
bool cellAlNum(char *cellValue);
void run(char *, Cell[], char *, size_t *, char *, FormulaType *);
void initSpreadSheet(Cell[]);
bool validCell(char* cellLabel);
bool prompt(char *, char *);
void renderSpreadSheet(Cell spreadsheet[], FILE* out);
void displaySpreadSheet(Cell spreadsheet[]);
void saveWorkSheet(Cell spreadsheet[]);
void saveLastSpreadSheet(Cell spreadsheet[]);
bool loadLastSpreadSheet(Cell spreadsheet[]);

void kill(const char* message)
{
   perror(message);
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
