#define _CRT_SECURE_NO_WARNINGS
#include "iss.h"
#include "Helpers.h"
#include <cstdio>

FILE* memoryIn = NULL, * memoryOut = NULL, * trace = NULL;
int LinesInTheProgram = 0;
char* InputFileName = NULL;
int dst = 0, src0 = 0, src1 = 0, opcodeNumber = 0;
long int immediate = 0, instruction = 0;
XOpcode OpcodeCommand = NULL;

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
	instruction = memoryOut[ProgramCounter];
	opcodeNumber = (instruction & 0x3E000000) >> 0x19;
	dst = (instruction & 0x01c00000) >> 0x16;
	src0 = (instruction & 0x00380000) >> 0x13;
	src1 =  (instruction & 0x00070000) >> 0x10;
	immediate =  ((instruction & 0x0000ffff) << 16) >> 16;
}

void PrintRawData()
{
	fprintf(trace, "--- instruction %i (%04x) @ PC %ld (%04lx) -----------------------------------------------------------\n",
		InstructionCounter, InstructionCounter, ProgramCounter, ProgramCounter);
	fprintf(trace, "pc = %04ld, inst = %08lx, opcode = %ld (%s), ", ProgramCounter, instruction, opcodeNumber, OpcodeCommand.operationString);


	fprintf(trace, "dst = %ld, src0 = %ld, src1 = %ld, immediate = %08lx\n", dst, src0, src1, immediate);
	fprintf(trace, "r[0] = %08lx r[1] = %08lx r[2] = %08lx r[3] = %08lx \nr[4] = %08lx r[5] = %08lx r[6] = %08lx r[7] = %08lx \n\n",
		RegisterArray[0], RegisterArray[1], RegisterArray[2], RegisterArray[3], RegisterArray[4], RegisterArray[5],
		 RegisterArray[6], RegisterArray[7]);
}

void PrintExecLine()
{
	switch (opcodeNumber)
	{
		case ADD:
		case SUB: 
		case LSF: 
		case RSF: 
		case AND: 
		case OR: 
		case XOR: 
		case LHI:
			fprintf(trace, ">>>> EXEC: R[%d] = %ld %s %ld <<<<\n\n", dst, RegisterArray[src0], OpcodeCommand.operationString, RegisterArray[src1]);
			break;
		case LD:
			fprintf(trace, ">>>> EXEC: R[%d] = MEM[%ld] = %08lx <<<<\n\n", dst, RegisterArray[src1], memoryOut[RegisterArray[src1]]);
			break;
		case ST:
			fprintf(trace, ">>>> EXEC: MEM[%ld] = R[%d] = %08lx <<<<\n\n", memoryOut[RegisterArray[src1]], src0, RegisterArray[src0]);
			break;
		case HLT:
			fprintf(trace, ">>>> EXEC: HALT at PC %04lx<<<<\n", ProgramCounter);
			break;
		case JLE: 
		case JEQ: 
		case JNE: 
		case JLT: 
		case JIN:
			fprintf(trace, ">>>> EXEC: %s %ld, %ld, %ld <<<<\n\n", OpcodeCommand.operationString, RegisterArray[src0], RegisterArray[src1], ProgramCounter + 1);
			break;
	}		
}

void PrintTrace()
{
	PrintRawData();
	PrintExecLine();
}


void CloseFiles()
{
	fclose(memoryIn);
	fclose(memoryOut);
	fclose(trace);
}

int main(int argc, char* argv[])
{
	assert(argc == 2);
	InputFileName = argv[1];
	OpenFiles();
	InitMemory();
	while (ProgramIsRunning)
	{
		GetCommand();
		OpcodeCommand = GetOpcode(opcodeNumber);
		PrintTrace();
		OpcodeCommand.OperationFunction(dst, src0, src1, immediate);
		ProgramCounter++;
		InstructionCounter++;
	}

	CloseFiles();
}
