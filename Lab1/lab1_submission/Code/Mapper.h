/*!
******************************************************************************
\file Mapper.h
\date 17 October 2021
\author Rony Kositsky & Ofir Guthman
\brief 
    this module is parsing opcodes to an executable functions

\details

\par Copyright
(c) Copyright 2021 Ofir & Rony
\par
ALL RIGHTS RESERVED
*****************************************************************************/

#ifndef __MAPPER_H_
#define __MAPPER_H_

/************************************
*      include                      *
************************************/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/************************************
*      definitions                 *
************************************/
#define NUMBER_OF_OPCODES 	16
#define MAX_LINE			10
#define MAX_MEMORY_SIZE 	(1 << 16)
#define NUMBER_OF_REGISTERS 8

/************************************
*       types                       *
************************************/
typedef enum
{
	ADD = 0,
	SUB,
	LSF,
	RSF,
	AND,
	OR,
	XOR,
	LHI,
	LD,
	ST,
	JLT = 16,
	JLE,
	JEQ,
	JNE,
	JIN,
	HLT = 24
}codes_e;

typedef struct
{
	codes_e code;                                             				 // opcode number
	char* operationString;                                   				 // opcode name
	void (*OperationFunction)(uint16_t dst, uint16_t src0, uint16_t src1);   // opcode operation function
}opcode_s;

/************************************
*       API                         *
************************************/
/*!
******************************************************************************
\brief
 Init memory array.

\details
 init memory by the input file

\param
 [in] memoryFile - file contianing the memory data.

\return none
*****************************************************************************/
uint16_t Mapper_InitMemory(FILE *memoryFile);

/*!
******************************************************************************
\brief
Get the opcode operations struct

\details

\param
 [in] opcode - opcode number

\return opcode_s struct 
        (see opcode_s)
*****************************************************************************/
opcode_s Mapper_GetOpcode(uint16_t opcode);

/*!
******************************************************************************
\brief
Get the run status of the program

\details

\return true if the program still running. otherwise, false
*****************************************************************************/
bool Mapper_IsProgramRunning(void);

/*!
******************************************************************************
\brief
Get value from memory

\param
 [in] location - memory location

\return memory value at location
*****************************************************************************/
uint32_t Mapper_GetFromMemory(uint16_t location);

/*!
******************************************************************************
\brief
 Get the next instruction

\details
 get memory at PC location.
 increase the PC by one.

\param
 [out] pc - program counter value

\return instruction command
*****************************************************************************/
uint32_t Mapper_GetNextInstruction(uint16_t *pc);

/*!
******************************************************************************
\brief
 Get the current position of Program Counter

\details
 get PC location.

\return instruction command
*****************************************************************************/
uint16_t Mapper_GetProgramCounter(void);

/*!
******************************************************************************
\brief
Set immediate values register.

\param
 [in] imm_value - immediate value

\return none
*****************************************************************************/
void Mapper_SetImmediateRegister(uint16_t imm_value);

/*!
******************************************************************************
\brief
 Get registers snapshot

\details
 get all registers value at this moment.

\param
 [out] regs - registers array

\return none
*****************************************************************************/
void Mapper_GetRegistersSnapshot(uint32_t regs[NUMBER_OF_REGISTERS]);

#endif // __MAPPER_H_