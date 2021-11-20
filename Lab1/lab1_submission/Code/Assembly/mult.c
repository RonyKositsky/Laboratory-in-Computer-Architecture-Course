/*
 * SP ASM: Simple Processor assembler
 *
 * usage: mult
 */
#include <stdio.h>
#include <stdlib.h>

#define ADD 0
#define SUB 1
#define LSF 2
#define RSF 3
#define AND 4
#define OR  5
#define XOR 6
#define LHI 7
#define LD 8
#define ST 9
#define JLT 16
#define JLE 17
#define JEQ 18
#define JNE 19
#define JIN 20
#define HLT 24

#define MEM_SIZE_BITS	(16)
#define MEM_SIZE	(1 << MEM_SIZE_BITS)
#define MEM_MASK	(MEM_SIZE - 1)
unsigned int mem[MEM_SIZE];

int pc = 0;

static void asm_cmd(int opcode, int dst, int src0, int src1, int immediate)
{
	int inst;

	inst = ((opcode & 0x1f) << 25) | ((dst & 7) << 22) | ((src0 & 7) << 19) | ((src1 & 7) << 16) | (immediate & 0xffff);
	mem[pc++] = inst;
}

static void assemble_program(char *program_name)
{
	FILE *fp;
	int addr, i, last_addr;

	for (addr = 0; addr < MEM_SIZE; addr++)
		mem[addr] = 0;

	pc = 0;

	/*
	 * Program starts here
	 */
    asm_cmd(LD, 2, 0, 1, 1000); // 0:   R2 = mem[1000] (Multipier)
    asm_cmd(LD, 3, 0, 1, 1001); // 1:   R3 = mem[1001] (Multiplicand)
    asm_cmd(ADD, 4, 0, 0, 0);   // 2:   R4 = 0
    asm_cmd(AND, 5, 2, 1, 1);   // 3:   R5 = R2 & 1
    asm_cmd(JEQ, 0, 5, 0, 6);   // 4:   if (R5 == 0) goto 6
    asm_cmd(ADD, 4, 4, 3, 0);   // 5:   R4 = R4 + R3
    asm_cmd(LSF, 3, 3, 1, 1);   // 6:   R3 = R3 << 1
    asm_cmd(RSF, 2, 2, 1, 1);   // 7:   R2 = R2 >> 1
    asm_cmd(JNE, 0, 2, 0, 3);   // 8:   if (R2 != 0) goto 3
    asm_cmd(ST, 0, 4, 1, 1002); // 9:   mem[1002] = R4
    asm_cmd(HLT, 0, 0, 0, 0);   // 10:  halt
    
	/* 
	 * Constants are planted into the memory somewhere after the program code:
	 */
	mem[1000] = 192;
    mem[1001] = -57;

	last_addr = 1002;

	fp = fopen(program_name, "w");
	if (fp == NULL) {
		printf("couldn't open file %s\n", program_name);
		exit(1);
	}
	addr = 0;
	while (addr < last_addr) {
		fprintf(fp, "%08x\n", mem[addr]);
		addr++;
	}
}


int main(int argc, char *argv[])
{
	
	if (argc != 2){
		printf("usage: asm program_name\n");
		return -1;
	}else{
		assemble_program(argv[1]);
		printf("SP assembler generated machine code and saved it as %s\n", argv[1]);
		return 0;
	}
	
}
