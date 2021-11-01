/*
 * SP ASM: Simple Processor assembler
 *
 * usage: asm
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
#define CPY 25
#define ASK 26


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
	// insert operands values 
	asm_cmd(ADD, 3, 1, 0, 50); 		// 0: R3 = 50 (src)
	asm_cmd(ADD, 4, 1, 0, 60); 		// 1: R4 = 60 (dst)
	asm_cmd(ADD, 5, 1, 0, 50); 		// 2: R5 = 50 (copy length)

	// execute dma copy command 
	asm_cmd(CPY, 4, 3, 5, 0);		// 3: CPY length of R[5] values from R[3] (src) to R[4] (dst)

	// run memory operation while copy
	asm_cmd(ADD, 2, 1, 0, 50); 		// 4: R2 = 50
	asm_cmd(ADD, 5, 1, 0, 50); 		// 5: R5 = 50 (length)
	asm_cmd(ADD, 3, 0, 0, 0); 		// 6: R3 = 0
	asm_cmd(LD,  4, 0, 2, 0); 		// 7: R4 = MEM[R2]
	asm_cmd(ADD, 3, 3, 4, 0); 		// 8: R3+= R4
	asm_cmd(ADD, 2, 2, 1, 1); 		// 9: R2++
	asm_cmd(JLT, 0, 2, 5, 7); 		// 10: if R2 < R5 jump to line 7

	// check if DMA done copy, if not wait
	asm_cmd(ASK, 2, 0, 0, 0); 		// 11: R[2] = #copy_transactions_remaining
	asm_cmd(JNE, 2, 0, 0, 11); 		// 12: if (R[2] != 0) jump to 11

	// check if the copy succeed
	asm_cmd(ADD, 2, 1, 0, 1); 		// 13: R2 = 1 (copy passed)
	asm_cmd(ADD, 3, 1, 0, 50); 		// 14: R3 = 50
	asm_cmd(ADD, 4, 1, 0, 60); 		// 15: R4 = 60
	asm_cmd(ADD, 5, 1, 0, 100); 	// 16: R5 = 100 (length + R3)
	asm_cmd(LD, 6, 0, 3, 0); 		// 17: R6 = MEM[R3] 
	asm_cmd(LD, 7, 0, 4, 0); 		// 18: R7 = MEM[R4]
	asm_cmd(JEQ, 0, 6, 7, 22); 		// 19: if (R[6] == R[7]) goto 22 
	asm_cmd(ADD, 2, 0, 0, 0); 		// 20: R2 = 0 (copy failed)
	asm_cmd(JEQ, 0, 0, 0, 25); 		// 21: goto 25 (halt)
	asm_cmd(ADD, 3, 3, 1, 1); 		// 22: R3++
	asm_cmd(ADD, 4, 4, 1, 1); 		// 23: R4++
	asm_cmd(JLT, 0, 3, 5, 16); 		// 24: if R3 < R5 jump to line 16
	asm_cmd(HLT, 0, 0, 0, 0); 		// 25: HALT
	
	/* 
	 * Constants are planted into the memory somewhere after the program code:
	 */
	for (i = 0; i < 50; i++)
		mem[50+i] = i;

	last_addr = 100;

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
