#define _CRT_SECURE_NO_WARNINGS
#include "iss.h"
#include "Helpers.h"
#include <cstdio>

FILE* memoryIn = NULL, * memoryOut = NULL, * trace = NULL;
int LinesInTheProgram = 0;
char* InputFileName = NULL;

void OpenFiles()
{
	if ((memoryIn = fopen(InputFileName, "r")) == NULL || (memoryOut = fopen("sram_out.txt", "w")) == NULL
		|| (trace = fopen("trace.txt", "w")) == NULL) {
		printf("Error: failed opening file. \n");
		exit(1);
	}
}

void InitMemory()
{
	while (LinesInTheProgram < MAX_MEMORY_SIZE && fscanf(memoryIn, "%08x", (unsigned int*)&(Meomry[LinesInTheProgram])) != EOF)
		LinesInTheProgram++;
	fprintf(trace, "program %s loaded, %d lines\n\n", InputFileName, LinesInTheProgram);
}

void GetCommand()
{

}

int main(int argc, char* argv[])
{
	assert(argc == 2);
	InputFileName = argv[1];
	OpenFiles();
	InitMemory();
	while (1)
	{
		GetCommand();
	}

}
