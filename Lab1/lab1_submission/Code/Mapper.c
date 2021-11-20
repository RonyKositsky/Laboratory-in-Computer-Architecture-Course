/*!
******************************************************************************
\file Mapper.c
\date 17 October 2021
\author Rony Kositsky & Ofir Guthman
\brief 

\details

\par Copyright
(c) Copyright 2021 Ofir & Rony
\par
ALL RIGHTS RESERVED
*****************************************************************************/

/************************************
*      include                      *
************************************/
#include "Mapper.h"
//
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/************************************
*      definitions                 *
************************************/
#define _CRT_SECURE_NO_WARNINGS
#define BRANCH_REGISTER_STORE_VALUE 7
#define IMMEDIATE_REGISTER          1
//
#define HALT_INSTRUCTION_COMMAND    (0x30000000)

/************************************
*       types                       *
************************************/

/************************************
*      variables                    *
************************************/
static uint32_t gRegisterArray[NUMBER_OF_REGISTERS] = { 0 };
static uint32_t gMemory[MAX_MEMORY_SIZE] = { 0 };
static uint16_t gProgramCounter = 0;
static bool gProgramIsRunning = true;
//
static bool gInvalidOperation = false;

/************************************
*      static functions             *
************************************/
/*0*/   static void Add(uint16_t dst, uint16_t src0, uint16_t src1);
/*1*/   static void Sub(uint16_t dst, uint16_t src0, uint16_t src1);
/*2*/   static void Lsf(uint16_t dst, uint16_t src0, uint16_t src1);
/*3*/   static void Rsf(uint16_t dst, uint16_t src0, uint16_t src1);
/*4*/   static void And(uint16_t dst, uint16_t src0, uint16_t src1);
/*5*/   static void Or(uint16_t dst, uint16_t src0, uint16_t src1);
/*6*/   static void Xor(uint16_t dst, uint16_t src0, uint16_t src1);
/*7*/   static void Lhi(uint16_t dst, uint16_t src0, uint16_t src1);
/*8*/   static void Ld(uint16_t dst, uint16_t src0, uint16_t src1);
/*9*/   static void St(uint16_t dst, uint16_t src0, uint16_t src1);
/*16*/  static void Jlt(uint16_t dst, uint16_t src0, uint16_t src1);
/*17*/  static void Jle(uint16_t dst, uint16_t src0, uint16_t src1);
/*18*/  static void Jeq(uint16_t dst, uint16_t src0, uint16_t src1);
/*19*/  static void Jne(uint16_t dst, uint16_t src0, uint16_t src1);
/*20*/  static void Jin(uint16_t dst, uint16_t src0, uint16_t src1);
/*24*/  static void Hlt(uint16_t dst, uint16_t src0, uint16_t src1);
//
//
static bool check_for_memory_error(uint32_t address);
static bool register_violation(uint16_t dst, uint16_t src0, uint16_t src1);

static void Jump(uint16_t pc_location);
//
//
//
static opcode_s OpcodeMapping[NUMBER_OF_OPCODES] =
{
	{ADD,	"ADD", 	Add},
	{SUB,	"SUB", 	Sub},
	{LSF,	"LSF", 	Lsf},
	{RSF,	"RSF", 	Rsf},
	{AND,	"AND",	And},
	{OR,	"OR", 	Or},
	{XOR,	"XOR", 	Xor},
	{LHI,	"LHI", 	Lhi},
	{LD,	"LD", 	Ld},
	{ST,	"ST", 	St},
	{JLT,	"JLT", 	Jlt},
	{JLE,	"JLE", 	Jle},
	{JEQ, 	"JEQ", 	Jeq},
	{JNE, 	"JNE", 	Jne},
	{JIN, 	"JIN", 	Jin},
	{HLT, 	"HLT", 	Hlt},
};

/************************************
*       API implementation          *
************************************/
uint16_t Mapper_InitMemory(FILE *memoryFile)
{
    uint16_t lineInProgram = 0;
    while (lineInProgram < MAX_MEMORY_SIZE && fscanf(memoryFile, "%08x", (unsigned int*)&(gMemory[lineInProgram])) != EOF)
		lineInProgram++;

    return lineInProgram;
}

opcode_s Mapper_GetOpcode(uint16_t opcode)
{
    for(int i = 0; i < NUMBER_OF_OPCODES; i++)
	{
		if (OpcodeMapping[i].code == opcode)
			return OpcodeMapping[i];
	}
    //
    // We get an invalid opcode, so we will exit
    printf("Wrong opcode was passed (%u), exit the program", opcode);
    return OpcodeMapping[NUMBER_OF_OPCODES - 1];    // return halt command
}

bool Mapper_IsProgramRunning(void)
{
    return gProgramIsRunning;
}

uint32_t Mapper_GetFromMemory(uint16_t location)
{
    return gMemory[location];
}

uint32_t Mapper_GetNextInstruction(uint16_t *pc)
{
    if (gInvalidOperation == true)
        return HALT_INSTRUCTION_COMMAND;

    *pc = gProgramCounter;
    //
    uint32_t mem = gMemory[gProgramCounter];
    gProgramCounter++;
    return mem;
}

uint16_t Mapper_GetProgramCounter(void)
{
    return gProgramCounter;
}

void Mapper_SetImmediateRegister(uint16_t imm_value)
{
    gRegisterArray[IMMEDIATE_REGISTER] = imm_value;
}

void Mapper_GetRegistersSnapshot(uint32_t regs[NUMBER_OF_REGISTERS])
{
    memcpy((uint8_t *)regs, (uint8_t *)gRegisterArray, sizeof(gRegisterArray));
}

/************************************
* static implementation             *
************************************/
static void Add(uint16_t dst, uint16_t src0, uint16_t src1)
{
    if (dst <= IMMEDIATE_REGISTER || register_violation(dst, src0, src1) == true)
        return;

	gRegisterArray[dst] = gRegisterArray[src0] + gRegisterArray[src1];
}

static void Sub(uint16_t dst, uint16_t src0, uint16_t src1)
{
    if (dst <= IMMEDIATE_REGISTER || register_violation(dst, src0, src1) == true)
        return;

	gRegisterArray[dst] = gRegisterArray[src0] - gRegisterArray[src1];
}

static void Lsf(uint16_t dst, uint16_t src0, uint16_t src1)
{
    if (dst <= IMMEDIATE_REGISTER || register_violation(dst, src0, src1) == true)
        return;

	gRegisterArray[dst] = gRegisterArray[src0] << gRegisterArray[src1];
}

static void Rsf(uint16_t dst, uint16_t src0, uint16_t src1)
{
    if (dst <= IMMEDIATE_REGISTER || register_violation(dst, src0, src1) == true)
        return;

	gRegisterArray[dst] = gRegisterArray[src0] >> gRegisterArray[src1];
}

static void And(uint16_t dst, uint16_t src0, uint16_t src1)
{
    if (dst <= IMMEDIATE_REGISTER || register_violation(dst, src0, src1) == true)
        return;

	gRegisterArray[dst] = gRegisterArray[src0] & gRegisterArray[src1];
}

static void Or(uint16_t dst, uint16_t src0, uint16_t src1)
{
    if (dst <= IMMEDIATE_REGISTER || register_violation(dst, src0, src1) == true)
        return;

    gRegisterArray[dst] = gRegisterArray[src0] | gRegisterArray[src1];
}

static void Xor(uint16_t dst, uint16_t src0, uint16_t src1)
{
    if (dst <= IMMEDIATE_REGISTER || register_violation(dst, src0, src1) == true)
        return;
    
   gRegisterArray[dst] = gRegisterArray[src0] ^ gRegisterArray[src1];
}

static void Lhi(uint16_t dst, uint16_t src0, uint16_t src1)
{
    if (dst <= IMMEDIATE_REGISTER || register_violation(dst, src0, src1) == true)
        return;
    
    gRegisterArray[dst] &= 0x0000FFFF;                                    // clean 16bit MSB
    gRegisterArray[dst] |= (gRegisterArray[IMMEDIATE_REGISTER] << 16);    // store immediate value at the 16bit MSB
}

static void Ld(uint16_t dst, uint16_t src0, uint16_t src1)
{
    if (dst <= IMMEDIATE_REGISTER || check_for_memory_error(gRegisterArray[src1]) == true || register_violation(dst, src0, src1) == true)
        return;

    gRegisterArray[dst] = gMemory[gRegisterArray[src1]];
}

static void St(uint16_t dst, uint16_t src0, uint16_t src1)
{
    if (check_for_memory_error(gRegisterArray[src1]) == true || register_violation(dst, src0, src1) == true)
        return;

	gMemory[gRegisterArray[src1]] = gRegisterArray[src0];
}

static void Jlt(uint16_t dst, uint16_t src0, uint16_t src1)
{
    if (register_violation(dst, src0, src1) == true)
        return;

    if (gRegisterArray[src0] < gRegisterArray[src1])
		Jump(gRegisterArray[IMMEDIATE_REGISTER]);
}

static void Jle(uint16_t dst, uint16_t src0, uint16_t src1)
{
    if (register_violation(dst, src0, src1) == true)
        return;
        
    if (gRegisterArray[src0] <= gRegisterArray[src1])
		Jump(gRegisterArray[IMMEDIATE_REGISTER]);
}

static void Jeq(uint16_t dst, uint16_t src0, uint16_t src1)
{
    if (register_violation(dst, src0, src1) == true)
        return;
        
	if (gRegisterArray[src0] == gRegisterArray[src1])
		Jump(gRegisterArray[IMMEDIATE_REGISTER]);
}

static void Jne(uint16_t dst, uint16_t src0, uint16_t src1)
{
    if (register_violation(dst, src0, src1) == true)
        return;
        
	if (gRegisterArray[src0] != gRegisterArray[src1])
		Jump(gRegisterArray[IMMEDIATE_REGISTER]);    
}

static void Jin(uint16_t dst, uint16_t src0, uint16_t src1)
{
    if (register_violation(dst, src0, src1) == true)
        return;
        
    Jump(gRegisterArray[src0]);    
}

static void Hlt(uint16_t dst, uint16_t src0, uint16_t src1)
{
	gProgramIsRunning = 0;
}

static void Jump(uint16_t pc_location)
{
    gRegisterArray[BRANCH_REGISTER_STORE_VALUE] = gProgramCounter - 1; // taking -1 because we increment the counter before executing the function
	gProgramCounter = pc_location;
}

static bool check_for_memory_error(uint32_t address)
{
    if (address >= MAX_MEMORY_SIZE)
    {
        printf("the memory address (%u) is not valid", address);
        gInvalidOperation = true;
        return true;
    }

    return false;
}

static bool register_violation(uint16_t dst, uint16_t src0, uint16_t src1)
{
    if (dst < 0 || dst > NUMBER_OF_REGISTERS ||
        src0 < 0 || src0 > NUMBER_OF_REGISTERS ||
        src1 < 0 || src1 > NUMBER_OF_REGISTERS)
    {
        printf("one of the registers (dst-%u, src0-%u, src1-%u) is not valid, valid range is [0, %d]", dst, src0, src1, NUMBER_OF_REGISTERS);
        gInvalidOperation = true;
        return true;
    }

    return false;
}

