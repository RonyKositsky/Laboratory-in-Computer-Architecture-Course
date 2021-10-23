/*
 * SP ASM: Simple Processor assembler
 *
 * usage: fibo
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
    asm_cmd(ADD, 2, 0, 1, 1);       // 0:   R2 = 1
    asm_cmd(ADD, 3, 0, 1, 1);       // 1:   R3 = 1
    asm_cmd(ST, 0, 2, 1, 1000);     // 2:   mem[1000] = R2
    asm_cmd(ST, 0, 3, 1, 1001);     // 3:   mem[1001] = R3
    asm_cmd(ADD, 4, 0, 1, 1002);    // 4:   R4 = 1002
    asm_cmd(ADD, 5, 0, 1, 1040);    // 5:   R5 = 1040 (last address)
    asm_cmd(ADD, 3, 3, 2, 0);       // 6:   R3 = R3 + R2
    asm_cmd(SUB, 2, 3, 2, 0);       // 7:   R2 = R3 - R2
    asm_cmd(ST, 0, 3, 4, 0);        // 8:   mem[R4] = R3
    asm_cmd(ADD, 4, 4, 1, 1);       // 9:   R4++
    asm_cmd(JLT, 0, 4, 5, 6);       // 10:  if (R4 < R5) goto 6
    asm_cmd(HLT, 0, 0, 0, 0);       // 11:  halt
    
	/* 
	 * Constants are planted into the memory somewhere after the program code:
	 */

	last_addr = 1040;

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
