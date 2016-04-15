#include "SpreadsheetServer.h"

static bool recoveredSpreadSheet = false;

const unsigned char MULTIPLEX = 1;
const unsigned char ITERATIVE = 2;

unsigned char mode;

int main(int argc, char* argv[])
{
   int socketfd;
   socklen_t cli_addr_len;
   struct sockaddr_in cli_addr;

   if (argc < 2)
      die("Usage: SpreadsheetServer mode(mult or iter)");
   if (strcmp(argv[1], "mult") != 0 && strcmp(argv[1], "iter") != 0)
      die("Usage: SpreadsheetServer mode(mult or iter)");
   if (argc > 2)
      die("Too many args\nUsage: SpreadsheetServer mode(mult or iter)");
   if (strcmp(argv[1], "mult") == 0)
      mode = MULTIPLEX;
   else
      mode = ITERATIVE;


   socketfd = initServer(&cli_addr, &cli_addr_len);
   acceptConnections(socketfd, &cli_addr, &cli_addr_len);

   close(socketfd);
}

// Checks for the binary file of the last saved spreadsheet to determine
// whether to prompt for a reload.
bool isFirstRun()
{
    struct stat st;
    int result = stat(BIN_FILE, &st);
    return result != 0;
}

int initServer(struct sockaddr_in* cli_addr, socklen_t* cli_addr_len)
{
   int socketfd;
   struct sockaddr_in serv_addr;

   socketfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
   if (socketfd < 0)
      die("Error creating socket");

   memset(&serv_addr, 0, sizeof serv_addr);
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   serv_addr.sin_port = htons(PORT_NO);

   if (bind(socketfd, (struct sockaddr*) &serv_addr, sizeof serv_addr) < 0)
      die("Error on binding server");

   printf("Server with address %s listening on port: %d\n",LOCAL_HOST, PORT_NO);
   if (listen(socketfd, BACKLOG) < 0)
      die("Error on listen");
   *cli_addr_len = sizeof cli_addr;

   return socketfd;
}

void acceptConnections(int socketfd, struct sockaddr_in* cli_addr, socklen_t* cli_addr_len)
{
   int newsockfd;

   // Multiplex declarations
   int ret, nfds, fd, j;
   fd_set masterfds, readfds, writefds;


   FD_ZERO(&masterfds);
   FD_ZERO(&readfds);
   FD_ZERO(&writefds);
   FD_SET(socketfd, &masterfds);
   nfds = socketfd;

   if (mode == MULTIPLEX) // Fucked currently.
   {
      while (true)
      {
         readfds = masterfds;
         // adds a file descriptor to the set.
         if ((ret = select(nfds + 1, &readfds, &writefds, 0, NULL)) < 0)
            die("Error on select");
         printf("Ret was %d\n", ret);
         printf("IS_SET? %d\n",FD_ISSET(socketfd, &readfds) );
         for (fd = socketfd; fd <= nfds; fd++)
         {
            if (FD_ISSET(fd, &readfds)) // got one
            {
               if (fd == socketfd)
               {
                  newsockfd = accept(socketfd, (struct sockaddr*) &(*cli_addr), &(*cli_addr_len));
                  if (newsockfd < 0)
                     die("Error on accept");
                  FD_SET(newsockfd, &masterfds); // add to master set
                  if (newsockfd > nfds)   // keep track of the max
                     nfds = newsockfd;
                  printf("Connection accepted\n");
                  while(doSpreadsheetStuff(newsockfd))
                     ;
                  close(fd);
               }
            }
            else // handle the input from client.
            {
               for(j = 0; j <= nfds; j++)
               {
                  // send to everyone!
                  if (FD_ISSET(j, &masterfds))
                  {
                  // except the listener and ourselves
                     if (j != socketfd && j != fd)
                     {
                        while (doSpreadsheetStuff(j))
                           ;
                        close(j);
                     }
                  }
               }
            }
         }
      }
   }
   // Iterative Server.
   else
   {
      while (true)
      {
         newsockfd = accept(socketfd, (struct sockaddr*) &(*cli_addr), &(*cli_addr_len));
         if (newsockfd < 0)
            die("Error on accept");
         printf("Connection accepted\n");

         while(doSpreadsheetStuff(newsockfd))
            ;
         printf("Flowed off 1 client on to the next\n");
         printf("newsockfd closed\n");
         close(newsockfd);
      }
   }
}

bool doSpreadsheetStuff(int socket)
{
   // int numBytes;
   char buffer[BUFFER_SIZE];
   char formula[IN_BUF_LIMIT] = "";
   FormulaType formulaType = SUM;
   FormulaType *formulaTypePtr = &formulaType;
   size_t addr = 0;
   size_t *addressPtr = &addr;
   char response[IN_BUF_LIMIT] = "";
   char value[IN_BUF_LIMIT] = "";
   Cell spreadsheet[GRID];

   memset(buffer, 0, BUFFER_SIZE);
   if (!isFirstRun())
   {
      SOCKET_WRITE(socket, "Do you want to load last spreadsheet? y/n: ", 0);
      read(socket, response, 3);
      printf("%s\n", response);
      if (toupper(response[0]) == 'Y')
      {
         if (loadLastSpreadSheet(spreadsheet, socket) != true)
            SOCKET_WRITE(socket, "Could not load last SpreadSheet\n", MSG_DONTWAIT);
         else
         {
            SOCKET_WRITE(socket, "Last spreadSheet loaded\n", MSG_DONTWAIT);
            recoveredSpreadSheet = true;
         }
      }
   }
   run(response, spreadsheet, value, addressPtr, formula, formulaTypePtr, socket); //runs program until user quits
   saveLastSpreadSheet(spreadsheet);
   SOCKET_WRITE(socket, "Session saved\n", MSG_DONTWAIT);
   return false;
}

bool loadLastSpreadSheet(Cell spreadsheet[], int socket)
{
  int valsRead;
  int i;
  char formatStr[80];

  FILE* file = fopen(BIN_FILE, "rb+");
  if (file == NULL)
  {
     SOCKET_WRITE(socket, "File was null\n", MSG_DONTWAIT);
     return false;
  }

  for (i = 0, valsRead = 0; i < GRID; i++)
     valsRead += fread(&spreadsheet[i], sizeof(Cell), 1, file);
  if (valsRead != GRID)
  {
     sprintf(formatStr, "Vals read = %d\n", valsRead);
     SOCKET_WRITE(socket, formatStr, MSG_DONTWAIT);
     SOCKET_WRITE(socket, "Well sheet not all vals read\n", MSG_DONTWAIT);
     fclose(file);
     return false;
  }
  fclose(file);

  return true;
}

/**
* Initiates loop to run spreadsheet program until user quits
* @param response    Pointer to store the value entered by user at command prompt
* @param spreadsheet The spreadsheet of cells
* @param value       Pointer that stores the value to be entered into a cell within the spreadsheet
* @param address     Pointer that stores the address of cell to store value
* @param formula     Pointer that stores formula
* @param fType       Pointer that stores the FormulaType to perform calculations
*/
void run(char *response, Cell spreadsheet[], char *value, size_t *address, char *formula, FormulaType *fType, int socket) {
   size_t addr;
   char val[IN_BUF_LIMIT];
   char cacheAddr[CELL_ADDRESS];
   char formatStr[80];
   if (!recoveredSpreadSheet)
     initSpreadSheet(spreadsheet);
   displaySpreadSheetToClient(spreadsheet, socket);

   while (true) {
      if (!prompt(response, value, socket))
         break;
      // cell guaranteed to be valid, no need for if statement.
      findCell(spreadsheet, response, address);
      addr = *address;
      printf("Address is: %lu\n", addr);
      printf("Value has %lu chars and is %s\n", strlen(value), value);
      sprintf(formatStr, "Old value: %s. New value: %s.\n", spreadsheet[addr].value, value);
      // fflush(st)
      printf("%s\n", value);
      // SOCKET_WRITE(socket, formatStr, MSG_DONTWAIT);
      SOCKET_WRITE(socket, "Return to reload spreadsheet", 0);


      if (isFormula(value, formula, fType)) {
         strcpy(cacheAddr, response);
         spreadsheet[addr].type = FORMULA;
         puts("FORMULA");
         sprintf(val, "%f", calculateFormula(value, fType, spreadsheet, address));
         printf("Val is %s\n", val);
         strcpy(spreadsheet[addr].value, val);

         spreadsheet[addr].address[0] = cacheAddr[0];
         spreadsheet[addr].address[1] = cacheAddr[1];
         spreadsheet[addr].address[2] = '\0';
     } else if (cellAlNum(value) == true) {
         spreadsheet[addr].type = ALNUM;
         puts("ALNUM");
         strcpy(spreadsheet[addr].value, value);
     } else {
         spreadsheet[addr].type = NUM;
         puts("NUM");
         strcpy(spreadsheet[addr].value, value); // SEG-FAULTS HERE!
     }
     displaySpreadSheetToClient(spreadsheet, socket);
     saveWorkSheet(spreadsheet, socket);
   }
   SOCKET_WRITE(socket, "U got served bitch!", MSG_DONTWAIT);
}

bool validCell(char* cellLabel)
{
	int row;
	unsigned char col;
   col = cellLabel[0];

	if ((row = atoi(&cellLabel[1])) == 0)
		return false;

	// Check if Col is between A-I
	else if (!(col >= 65 && col <= 73))
		return false;

	else if (!(row >= 1 && row <= 9))
		return false;

	return true;
}

/**
* Prompts user for input
* @param response Pointer to store address user inputted
* @param value    Pointer to store value user inputted
*/
bool prompt(char *response, char *value, int socket) {
   // printf("In %s\n",__func__);
   SOCKET_WRITE(socket, "Enter row and column you would like to manipulate or enter 'exit' to exit program: ", 0);
   read(socket, response, IN_BUF_LIMIT);
   printf("%s\n", response);
   if (strcmp(response, "exit") == 0)
      return false;

   response[0] = toupper(response[0]);

   while(!validCell(response) || !response)
   {
      SOCKET_WRITE(socket, "Invalid cell. Enter cell to edit ('exit' to quit): ", 0);
      read(socket, response, IN_BUF_LIMIT);
      if (strcmp(response, "exit") == 0)
         return false;
      response[0] = toupper(response[0]);
   }
   SOCKET_WRITE(socket, "Enter value: ", 0);
   read(socket, value, IN_BUF_LIMIT);

   return true;
}

/**
* Calculates the result of entered formula
* @param  value   The formula
* @param  ftype   Type of formula (SUM, RANGE, AVERAGE)
* @param  sheet   The spreadsheet
* @param  address Address of the cell to store result
* @return         Result of calculation
*/
double calculateFormula(char *value, FormulaType *ftype, Cell sheet[], size_t *address) {
   char addr1[CELL_ADDRESS]; //stores the extracted value of the first address in formula
   char addr2[CELL_ADDRESS]; //stores the extracted value of the first address in formula
   double answer = 0.0;
   size_t i, start, end, temp, count;
   switch (*ftype) {
      case AVERAGE:
           addr1[0] = value[8];
           addr1[1] = value[9];
           addr1[2] = '\0';
           addr2[0] = value[11];
           addr2[1] = value[12];
           addr2[2] = '\0';
           findCell(sheet, addr1, address);
           start = *address;
           findCell(sheet, addr2, address);
           end = *address;

           // Swaps start and end if start address is greater than end address
           if (start > end) {
               temp = start;
               start = end;
               end = temp;
           }

           // If addresses are in the same column then do calculations with values vertically
           if (addr1[0] == addr2[0]) {
               count = 0;
               for (i = start; i <= end;) {
                   if (sheet[i].type == NUM) {
                       answer += atof(sheet[i].value);
                       ++count;
                   }
                   i += 9;
               }
               answer = answer / count;
           // If addresses are in the same row then do calculations with values horizontally
           } else if (toupper(addr1[1]) == toupper(addr2[1])) {
               count = 0;
               for (i = start; i < end; i++) {
                   if (sheet[i].type == NUM) {
                       answer += atof(sheet[i].value);
                       ++count;
                   }
               }
               if (answer > 0)
                   answer = answer / count;
               else
                   answer = 0;
           }
           break;
      case SUM:
           addr1[0] = value[4];
           addr1[1] = value[5];
           addr1[2] = '\0';
           addr2[0] = value[7];
           addr2[1] = value[8];
           addr2[2] = '\0';
           findCell(sheet, addr1, address);
           start = *address;
           findCell(sheet, addr2, address);
           end = *address;

           // Swaps start and end if start address is greater than end address
           if (start > end) {
               temp = start;
               start = end;
               end = temp;
           }

           // If addresses are in the same column then do calculations with values vertically
           if (addr1[0] == addr2[0]) {
               for (i = start; i <= end;) {
                   if (sheet[i].type == NUM)
                       answer += atof(sheet[i].value);
                   i += 9;
               }

           // If addresses are in the same row then do calculations with values horizontally
           } else if (toupper(addr1[1]) == toupper(addr2[1])) {
               for (i = start; i < end; i++) {
                   if (sheet[i].type == NUM)
                       answer += atof(sheet[i].value);
               }
           }
           break;
      case RANGE:
           addr1[0] = value[6];
           addr1[1] = value[7];
           addr1[2] = '\0';
           addr2[0] = value[9];
           addr2[1] = value[10];
           addr2[2] = '\0';
           findCell(sheet, addr1, address);
           start = *address;
           findCell(sheet, addr2, address);
           end = *address;

           // Swaps start and end if start address is greater than end address
           if (start > end) {
               temp = start;
               start = end;
               end = temp;
           }

           // If addresses are in the same column then do calculations with values vertically
           if (addr1[0] == addr2[0]) {
               answer = 0;
               for (i = start; i <= end;) {
                   if (sheet[i].type == NUM) {
                       if (atof(sheet[i].value) > answer)
                           answer = atof(sheet[i].value);
                   }
                   i += 9;
               }

           // If addresses are in the same row then do calculations with values horizontally
           } else if (toupper(addr1[1]) == toupper(addr2[1])) {
               for (i = start; i < end; i++) {
                   if (sheet[i].type == NUM) {
                       if (atof(sheet[i].value) > answer)
                           answer = atof(sheet[i].value);
                   }
               }
           }
           break;
   }
   return answer;
}

/**
* Determines if value entered is a formula
* @param  cellValue Value entered to check is it's a formula or NOT
* @param  formula   Pointer to store formula
* @param  ftype     Pointer to store the type of the formula
* @return           true if value is a formula, false otherwise
*/
bool isFormula(char *cellValue, char *formula, FormulaType *ftype) {
 // Determines if value is a AVERAGE calculation
   if (toupper(cellValue[0]) == 'A' && toupper(cellValue[1]) == 'V' && toupper(cellValue[2]) == 'E'
      && toupper(cellValue[3]) == 'R' && toupper(cellValue[4]) == 'A' && toupper(cellValue[5]) == 'G'
      && toupper(cellValue[6]) == 'E' && cellValue[7] == '(' && toupper(cellValue[8]) >= 'A' &&
      toupper(cellValue[8]) <= 'I'
      && isdigit(cellValue[9]) != 0 && cellValue[10] == ',' && toupper(cellValue[11]) >= 'A' &&
      toupper(cellValue[11]) <= 'I' &&
      isdigit(cellValue[12]) != 0 && cellValue[13] == ')' && (cellValue[9] == cellValue[12] ||
                                                               cellValue[8] == cellValue[11])) {
      strcpy(formula, cellValue);
      *ftype = AVERAGE;
      return true;

   // Determines if value is a SUM calculation
   } else if (toupper(cellValue[0]) == 'S' && toupper(cellValue[1]) == 'U' && toupper(cellValue[2]) == 'M'

              && cellValue[3] == '(' && toupper(cellValue[4]) >= 'A' && toupper(cellValue[4]) <= 'I' && isdigit(cellValue[5]) != 0 &&
              cellValue[6] == ','
              && toupper(cellValue[7]) >= 'A' && toupper(cellValue[7]) <= 'I' && isdigit(cellValue[8]) != 0 &&
              cellValue[9] == ')'
              && (cellValue[5] == cellValue[8] ||
                  cellValue[4] == cellValue[7])) {
      strcpy(formula, cellValue);
      *ftype = SUM;
      return true;

   // Determines if value is a RANGE calculation
   } else if (toupper(cellValue[0]) == 'R' && toupper(cellValue[1]) == 'A' && toupper(cellValue[2]) == 'N'
              && toupper(cellValue[3]) == 'G' && toupper(cellValue[4]) == 'E' && cellValue[5]
                 == '(' && toupper(cellValue[6]) >= 'A' && toupper(cellValue[6]) <= 'I' && isdigit(cellValue[7]) != 0
              && cellValue[8] == ',' && toupper(cellValue[9]) >= 'A' && toupper(cellValue[9]) <= 'I' &&
              isdigit(cellValue[10]) != 0
              && cellValue[11] == ')' && (cellValue[7] == cellValue[10] ||
                                          cellValue[6] == cellValue[9])) {
      strcpy(formula, cellValue);
      *ftype = RANGE;
      return true;
   }
   return false;
}

/**
* Determines if cell is alphanumeric
* @param  cellValue Value contained within cellAlNum
* @return           true if cell is alphanumeric, false otherwise
*/
bool cellAlNum(char *cellValue) {
   size_t i;
   for (i = 0; i < strlen(cellValue); i++) {
      if (isalpha(cellValue[i]) == 1)
           return true;
   }
   return false;
}

/**
* Renders the spreadsheet to stdout
* @param spreadsheet Represents the spreadsheet
*/
void renderSpreadSheet(Cell spreadsheet[], FILE* file, int socket) {
   size_t i;
   char cell1[DISPLAY], cell2[DISPLAY], cell3[DISPLAY], cell4[DISPLAY], cell5[DISPLAY], cell6[DISPLAY], cell7[DISPLAY], cell8[DISPLAY], cell9[DISPLAY];
   char trun1[TRUNCATE], trun2[TRUNCATE], trun3[TRUNCATE], trun4[TRUNCATE], trun5[TRUNCATE], trun6[TRUNCATE], trun7[TRUNCATE], trun8[TRUNCATE], trun9[TRUNCATE];
   char buffer[BUFFER_SIZE];
   bool writeToSocket;
   FILE* fd = NULL;
   FILE* out = NULL;

   writeToSocket = socket != -1? true : false;
   if (writeToSocket)
   {
      fd = fdopen(socket, "w");
      // printf("fd\n");

      // change to full buffering instead of line buffering
      // to hold entire spreadsheet.
      if (setvbuf(fd, buffer, _IOFBF, BUFFER_SIZE) != 0)
         die("Couldn't create buffer for spreadsheet");
   }
   printf("Created buffer\n");

   for (i = 0; i < GRID;) {
      strcpy(cell1, prepareForDisplay(spreadsheet[i].value));
      strcpy(cell2, prepareForDisplay(spreadsheet[i + 1].value));
      strcpy(cell3, prepareForDisplay(spreadsheet[i + 2].value));
      strcpy(cell4, prepareForDisplay(spreadsheet[i + 3].value));
      strcpy(cell5, prepareForDisplay(spreadsheet[i + 4].value));
      strcpy(cell6, prepareForDisplay(spreadsheet[i + 5].value));
      strcpy(cell7, prepareForDisplay(spreadsheet[i + 6].value));
      strcpy(cell8, prepareForDisplay(spreadsheet[i + 7].value));
      strcpy(cell9, prepareForDisplay(spreadsheet[i + 8].value));
      strcpy(trun1, truncateOrNah(spreadsheet[i].value));
      strcpy(trun2, truncateOrNah(spreadsheet[i + 1].value));
      strcpy(trun3, truncateOrNah(spreadsheet[i + 2].value));
      strcpy(trun4, truncateOrNah(spreadsheet[i + 3].value));
      strcpy(trun5, truncateOrNah(spreadsheet[i + 4].value));
      strcpy(trun6, truncateOrNah(spreadsheet[i + 5].value));
      strcpy(trun7, truncateOrNah(spreadsheet[i + 6].value));
      strcpy(trun8, truncateOrNah(spreadsheet[i + 7].value));
      strcpy(trun9, truncateOrNah(spreadsheet[i + 8].value));
      // Renders the spreadsheet row by row
      out = writeToSocket? fd : file;
      fprintf(out, "+--------------+ +--------------+ +--------------+ +--------------+ +--------------+ +--------------+ +--------------+ +--------------+ +--------------+"
                      "\n|              | |              | |              | |              | |              | |              | |              | |              | |              |"
                      "\n|              | |              | |              | |              | |              | |              | |              | |              | |              |"
                      "\n|      %.*s    | |      %s    | |      %s    | |      %s    | |      %s    | |      %s    | |      %s    | |      %s    | |      %s    |"
                      "\n|  %s | |  %s | |  %s | |  %s | |  %s | |  %s | |  %s | |  %s | |  %s |"
                      "\n|              | |              | |              | |              | |              | |              | |              | |              | |              |"
                      "\n+------%s------+ +------%s------+ +------%s------+ +------%s------+ +------%s------+ +------%s------+ +------%s------+ +------%s------+ +------%s------+",
              4, cell1, cell2, cell3, cell4, cell5, cell6, cell7, cell8, cell9, trun1, trun2, trun3,
              trun4, trun5, trun6, trun7, trun8, trun9, spreadsheet[i].address, spreadsheet[i + 1].address,
              spreadsheet[i + 2].address, spreadsheet[i + 3].address, spreadsheet[i + 4].address,
              spreadsheet[i + 5].address, spreadsheet[i + 6].address,
              spreadsheet[i + 7].address, spreadsheet[i + 8].address);
      fprintf(out, "\n");
      i += 9;
   }
   // Send buffer contents(spreadsheet) to client.
   if (fd) {
      fflush(fd);
      if (setvbuf(fd, buffer, _IOLBF, IN_BUF_LIMIT) != 0)
         die("Couldn't change buffer to line buffering");
   }
}

/**
* Initializes all the cells in the spreadsheet.
* @param sheet Represents the spreadsheet
*/
void initSpreadSheet(Cell sheet[]) {
   size_t i;
   char num = '1', chr = 'B';

   // Initialize the values of cell one.
   strcpy(sheet[0].address, "A1");
   strcpy(sheet[0].value, "    ");
   sheet[0].type = ALNUM;

   // Initialize the values of the cells within spreadsheet
   for (i = 1; i < GRID; i++) {
      // revert num to 1 and increment chr to next letter for each column in spreadsheet
      if (i % 9 == 0) {
           ++num;
           chr = 'A';
      }
      sheet[i].address[0] = chr;
      sheet[i].address[1] = num;
      sheet[i].address[2] = '\0';

      strcpy(sheet[i].value, "    ");
      sheet[i].type = ALNUM; // Default type
      ++chr;
   }
}

void displaySpreadSheetToClient(Cell spreadsheet[], int socket)
{
  renderSpreadSheet(spreadsheet, NULL, socket);
}

void saveWorkSheet(Cell spreadsheet[], int socket)
{
  FILE* file = fopen(FILE_NAME, "w+");
  if (file == NULL )
  {
     fprintf(stderr, "Could not open %s for writing\n", FILE_NAME);
     SOCKET_WRITE(socket, "Sorry could not save spreadsheet\n", 0);
     return;
  }
  renderSpreadSheet(spreadsheet, file, -1);
  fclose(file);
}

void saveLastSpreadSheet(Cell spreadsheet[])
{
  int valsWritten;

  FILE* file = fopen(BIN_FILE, "wb+");
  if (file == NULL) {
     printf("Could not open file for writing\n");
     // SOCKET_WRITE(socket, "Sorry could not save spreadsheet\n");
     return;
  }
  valsWritten = fwrite(spreadsheet, sizeof(Cell), GRID, file);
  if (valsWritten != GRID)
  {
     printf("Well sheet not all vals written\n");
     fclose(file);
     file = fopen(BIN_FILE, "wb+");
  }
  fclose(file);
}

/**
* Determines if cell exists in spreadsheet and stores the address in a pointer.
* @param  sheet   Represents the spreadsheet
* @param  cell    Represents a cell within the spreadsheet
* @param  address Pointer to store the address of the located cell if it is found
* @return         true if cell exists in the spreadsheet, false otherwise
*/
bool findCell(Cell sheet[], char *cell, size_t *address) {
   size_t i;
   for (i = 0; i < GRID; i++) {
      // char *tempSheet = sheet[i].address;
      // tempSheet[0] = (char) toupper(tempSheet[0]);
      // cell[0] = (char) toupper(cell[0]);
      // cell = trimEnd(cell);
      if (strcmp(sheet[i].address, cell) == 0) {
           *address = i;
           return true;
      }
   }
   return false;
}

/**
* Prepares value of display to stdout. Value needs to be 4 characters to display properly.
* @param  str String to prepare for display
* @return     returns string ready for being displayed
*/
char *prepareForDisplay(char *str) {
   char *temp = malloc(DISPLAY);
   size_t i, len = strlen(str);
   // return if cell has default value of whitespace
   if (isspace(str[0]))
      return str;

   // return first 4 characters if string length greater than 4
   if (len > 4) {
      temp[0] = str[0];
      temp[1] = str[1];
      temp[2] = str[2];
      temp[3] = str[3];
      temp[4] = '\0';

   // if string length less 4 characters, take all characters and pad with whitespace to bring length to 4
   } else {
      for (i = 0; i < 4; i++) {
         if (str[i] == '\0') {
            while (strlen(temp) < 4) {
                temp[i] = ' ';
                i++;
            }
        } else {
            temp[i] = str[i];
        }
      }
   }
   return temp;
}

/**
* Determines if cell value is truncated or not.
* @param  str String to determine if truncated
* @return     String to show if value has been truncated or not
*/
char *truncateOrNah(char *str) {
   // If length of string greater than 4 then string has been truncated.
   if (strlen(str) > 4) {
      return "(truncated)";
   }
   return "           ";
}
