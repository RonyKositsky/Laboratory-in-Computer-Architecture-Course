#ifndef HELPERS_H_
#define HELPERS_H_

#define NUMBER_OF_OPCODES 25
#define MAX_LINE 10
#define MEMORY_SIZE 65536
#define NUMBER_OF_REGISTERS 8

static int RegisterArray[NUMBER_OF_REGISTERS] = { 0 };
static long int Meomry[MEMORY_SIZE] = { 0 };
static int ProgramCounter = 0;

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
}CodeNames;

typedef struct
{
	int code;
	char* operation;
	char* printFuntion;
	void (*OperationFunction)(int dst, int src0, int src1, long int imm);
}XOpcode;

XOpcode OpcodeMapping[NUMBER_OF_OPCODES];

#endif