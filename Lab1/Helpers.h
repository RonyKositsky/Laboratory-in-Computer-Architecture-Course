#ifndef HELPERS_H_
#define HELPERS_H_

#define NUMBER_OF_OPCODES 16
#define MAX_LINE 10
#define MAX_MEMORY_SIZE 65536
#define NUMBER_OF_REGISTERS 8

static int RegisterArray[NUMBER_OF_REGISTERS] = { 0 };
static long int Meomry[MAX_MEMORY_SIZE] = { 0 };
static int ProgramCounter = 0;
static int InstructionCounter = 0;
static int ProgramIsRunning = 1;

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
}Codes;

typedef struct
{
	int code;
	char* operationString;
	void (*OperationFunction)(int dst, int src0, int src1, long int imm);
}XOpcode;

XOpcode OpcodeMapping[NUMBER_OF_OPCODES];
XOpcode GetOpcode(int opcode);

#endif