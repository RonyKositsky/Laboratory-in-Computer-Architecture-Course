#include "Helpers.h"
#define BRANCH_REGISTER_STORE_VALUE 7

void Jump(long int imm)
{
	RegisterArray[BRANCH_REGISTER_STORE_VALUE] = ProgramCounter;
	ProgramCounter = imm && MEMORY_SIZE - 1;
	/*pc=imm[15:0]-1 becuase we later increment it so we subtract 1
	to jump to the right value of pc at the end of the execution of the comand*/
}

void Add(int dst, int src0, int src1)
{
	RegisterArray[dst] = RegisterArray[src0] + RegisterArray[src1];
}

void Sub(int dst, int src0, int src1, long int imm)
{
	RegisterArray[dst] = RegisterArray[src0] - RegisterArray[src1];
}

void And(int dst, int src0, int src1, long int imm)
{
	RegisterArray[dst] = RegisterArray[src0] & RegisterArray[src1];
}
void Or(int dst, int src0, int src1, long int imm)
{
	RegisterArray[dst] = RegisterArray[src0] | RegisterArray[src1];
}
void Xor(int dst, int src0, int src1, long int imm)
{
	RegisterArray[dst] = RegisterArray[src0] ^ RegisterArray[src1];

}
void Ld(int dst, int src0, int src1, long int imm)
{
	RegisterArray[dst] = Meomry[src0];
}
void St(int dst, int src0, int src1, long int imm)
{
	Meomry[src1] = RegisterArray[src0];
}
void Jeq(int dst, int src0, int src1, long int imm)
{
	if (RegisterArray[src0] == RegisterArray[src1])
		Jump(imm);
}
void Jin(int dst, int src0, int src1, long int imm)
{
	Jump(imm);
}
void Jlt(int dst, int src0, int src1, long int imm)
{
	if (RegisterArray[src0] < RegisterArray[src1])
		Jump(imm);
}
void Jle(int dst, int src0, int src1, long int imm)
{
	if (RegisterArray[src0] <= RegisterArray[src1])
		Jump(imm);
}
void Jne(int dst, int src0, int src1, long int imm)
{
	if (RegisterArray[src0] != RegisterArray[src1])
		Jump(imm);
}
void Lhi(int dst, int src0, int src1, long int imm)
{

}

void Lsf(int dst, int src0, int src1, long int imm)
{
	RegisterArray[dst] = RegisterArray[src0] << RegisterArray[src1];
}

void Rsf(int dst, int src0, int src1, long int imm)
{
	RegisterArray[dst] = RegisterArray[src0] >> RegisterArray[src1];
}

