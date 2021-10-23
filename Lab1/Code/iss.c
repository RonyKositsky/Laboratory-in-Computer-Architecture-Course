/*!
******************************************************************************
\file iss.c
\date 17 October 2021
\author Rony Kositsky & Ofir Guthman
\brief 
	Instruction set simulator (iss)
\details
	simulating a program till HALT is encountered
	generating an output of memory image and trace

\par Copyright
(c) Copyright 2021 Ofir & Rony
\par
ALL RIGHTS RESERVED
*****************************************************************************/

/************************************
*      include                      *
************************************/
#include "Mapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/************************************
*      definitions                 *
************************************/
#define _CRT_SECURE_NO_WARNINGS


/************************************
*       types                       *
************************************/
typedef union
{
	struct
	{
		uint16_t immediate		: 16;	// [0:15]  Immediate value
		uint16_t source_1		: 3;	// [16:18] src1 value
		uint16_t source_0		: 3;	// [19:21] src0 value
		uint16_t destination	: 3;	// [22:24] src0 value
		uint16_t opcode			: 5;	// [25:29] opcode value
		uint16_t not_in_use		: 2;	// not relevant
	} bits;
	
	uint32_t command;
} instruction_format_s;

/************************************
*      variables                    *
************************************/
// files
static FILE* gMemoryInFile = NULL;
static FILE* gMemoryOutFile = NULL;
static FILE* gTraceFile = NULL;
//
//
static uint16_t gLinesInProgram = 0;
//
//

//
//
static struct
{
	instruction_format_s instruction_code;
	uint16_t instruction_counter;
	uint16_t program_counter;
	opcode_s opcode;
} gInstructionData;


/************************************
*      static functions             *
************************************/
static void initialize(void);
//
static void OpenFiles(char *inputFileName);
static void CloseFiles(void);
//
static void PrintRawData(uint32_t regs[NUMBER_OF_REGISTERS]);
static void PrintExecLine(uint32_t regs[NUMBER_OF_REGISTERS]);
static void MemoryDump(void);

/************************************
*       API implementation          *
************************************/
int main(int argc, char* argv[])
{
	// check if there is exact args.
	assert(argc == 2);

	// init all variables.
	initialize();

	char* inputFileName = argv[1];
	OpenFiles(inputFileName);

	// init memory
	uint16_t linesInProgram = Mapper_InitMemory(gMemoryInFile);
	fprintf(gTraceFile, "program %s loaded, %d lines\n\n", inputFileName, linesInProgram);

	while (Mapper_IsProgramRunning())
	{
		// get command from memory & parse the command
		gInstructionData.instruction_code.command = Mapper_GetNextInstruction(&gInstructionData.program_counter);
        // set immediate register
        Mapper_SetImmediateRegister(gInstructionData.instruction_code.bits.immediate);

		// Get opcode function
		gInstructionData.opcode = Mapper_GetOpcode(gInstructionData.instruction_code.bits.opcode);

		// Print trace
        uint32_t regs[NUMBER_OF_REGISTERS];
        Mapper_GetRegistersSnapshot(regs);
        PrintRawData(regs);

		// execute operation
		gInstructionData.opcode.OperationFunction(gInstructionData.instruction_code.bits.destination,
													gInstructionData.instruction_code.bits.source_0,
													gInstructionData.instruction_code.bits.source_1);
        
        PrintExecLine(regs);

		// Increase counters
		gInstructionData.instruction_counter++;
	}

    fprintf(gTraceFile, "sim finished at pc %u, %u instructions", gInstructionData.program_counter, gInstructionData.instruction_counter);
    MemoryDump();
	CloseFiles();

    return 0;
}

/************************************
* static implementation             *
************************************/
static void initialize(void)
{
	memset((uint8_t *)&gInstructionData, 0, sizeof(gInstructionData));
}

static void OpenFiles(char *inputFileName)
{
	if ((gMemoryInFile = fopen(inputFileName, "r")) == NULL || (gMemoryOutFile = fopen("sram_out.txt", "w")) == NULL
		|| (gTraceFile = fopen("trace.txt", "w")) == NULL) 
	{
		printf("Error: failed opening file. \n");
		exit(1);
	}
}

static void CloseFiles(void)
{
	fclose(gMemoryInFile);
	fclose(gMemoryOutFile);
	fclose(gTraceFile);
}

static void PrintRawData(uint32_t regs[NUMBER_OF_REGISTERS])
{
	instruction_format_s *ins_code = &gInstructionData.instruction_code;

	fprintf(gTraceFile, "--- instruction %i (%04x) @ PC %ld (%04lx) -----------------------------------------------------------\n",
		gInstructionData.instruction_counter, gInstructionData.instruction_counter, gInstructionData.program_counter, gInstructionData.program_counter);
	fprintf(gTraceFile, "pc = %04ld, inst = %08lx, opcode = %ld (%s), dst = %ld, src0 = %ld, src1 = %ld, immediate = %08lx\n", 
		gInstructionData.program_counter, ins_code->command, ins_code->bits.opcode, gInstructionData.opcode.operationString,
		ins_code->bits.destination, ins_code->bits.source_0, ins_code->bits.source_1, ins_code->bits.immediate);

	fprintf(gTraceFile, "r[0] = %08lx r[1] = %08lx r[2] = %08lx r[3] = %08lx \nr[4] = %08lx r[5] = %08lx r[6] = %08lx r[7] = %08lx \n\n",
		regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7]);
}

static void PrintExecLine(uint32_t regs[NUMBER_OF_REGISTERS])
{
	instruction_format_s *ins_code = &gInstructionData.instruction_code;
	
	switch (gInstructionData.opcode.code)
	{
		case ADD:
		case SUB: 
		case LSF: 
		case RSF: 
		case AND: 
		case OR: 
		case XOR: 
		case LHI:
			fprintf(gTraceFile, ">>>> EXEC: R[%d] = %ld %s %ld <<<<\n\n", 
					ins_code->bits.destination, regs[ins_code->bits.source_0], gInstructionData.opcode.operationString, regs[ins_code->bits.source_1]);
			break;
		case LD:
			fprintf(gTraceFile, ">>>> EXEC: R[%d] = MEM[%ld] = %08lx <<<<\n\n",
					ins_code->bits.destination, regs[ins_code->bits.source_1], Mapper_GetFromMemory(regs[ins_code->bits.source_1]));
			break;
		case ST:
			fprintf(gTraceFile, ">>>> EXEC: MEM[%ld] = R[%d] = %08lx <<<<\n\n", 
					regs[ins_code->bits.source_1], ins_code->bits.source_0, regs[ins_code->bits.source_0]);
			break;
		case HLT:
			fprintf(gTraceFile, ">>>> EXEC: HALT at PC %04lx<<<<\n", gInstructionData.program_counter);
			break;
		case JLE: 
		case JEQ: 
		case JNE: 
		case JLT: 
		case JIN:
			fprintf(gTraceFile, ">>>> EXEC: %s %ld, %ld, %ld <<<<\n\n", 
					gInstructionData.opcode.operationString, regs[ins_code->bits.source_0], regs[ins_code->bits.source_1], Mapper_GetProgramCounter());
			break;
	}		
}

static void MemoryDump(void)
{
    for (int i = 0; i < MAX_MEMORY_SIZE; i++)
        fprintf(gMemoryOutFile, "%08lx\n", Mapper_GetFromMemory(i));
}

