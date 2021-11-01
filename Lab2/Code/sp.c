#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#include "llsim.h"

#define sp_printf(a...)						\
	do {							\
		llsim_printf("sp: clock %d: ", llsim->clock);	\
		llsim_printf(a);				\
	} while (0)

int nr_simulated_instructions = 0;
FILE *inst_trace_fp = NULL, *cycle_trace_fp = NULL;

typedef struct sp_registers_s {
	// 6 32 bit registers (r[0], r[1] don't exist)
	int r[8];

	// 16 bit program counter
	int pc;

	// 32 bit instruction
	int inst;

	// 5 bit opcode
	int opcode;

	// 3 bit destination register index
	int dst;

	// 3 bit source #0 register index
	int src0;

	// 3 bit source #1 register index
	int src1;

	// 32 bit alu #0 operand
	int alu0;

	// 32 bit alu #1 operand
	int alu1;

	// 32 bit alu output
	int aluout;

	// 32 bit immediate field (original 16 bit sign extended)
	int immediate;

	// 32 bit cycle counter
	int cycle_counter;

	// 3 bit control state machine state register
	int ctl_state;

	// control states
	#define CTL_STATE_IDLE		0
	#define CTL_STATE_FETCH0	1
	#define CTL_STATE_FETCH1	2
	#define CTL_STATE_DEC0		3
	#define CTL_STATE_DEC1		4
	#define CTL_STATE_EXEC0		5
	#define CTL_STATE_EXEC1		6

	//DMA
	#define DMA_IDLE			0
	#define DMA_WAIT			1
	#define DMA_COPY			2
} sp_registers_t;

typedef struct
{
	int source;
	int dest;
	int length;
	int state;
	int remaining_memory;
}DMA_register_t;

/*
 * Master structure
 */
typedef struct sp_s {
	// local sram
#define SP_SRAM_HEIGHT	64 * 1024
	llsim_memory_t *sram;

	unsigned int memory_image[SP_SRAM_HEIGHT];
	int memory_image_size;

	sp_registers_t *spro, *sprn;
	DMA_register_t* dma;
	int start;
	pthread_t dma_thread;
} sp_t;

static void sp_reset(sp_t *sp)
{
	sp_registers_t *sprn = sp->sprn;

	memset(sprn, 0, sizeof(*sprn));
}

//function declerations:
void add(sp_t* sp);
void sub(sp_t* sp);
void lsf(sp_t* sp);
void rsf(sp_t* sp);
void and (sp_t* sp);
void or (sp_t * sp);
void xor (sp_t* sp);
void lhi(sp_t* sp);
void ld(sp_t* sp);
void jlt(sp_t* sp);
void jle(sp_t* sp);
void jeq(sp_t* sp);
void jne(sp_t* sp);
void none(sp_t* sp);
void* copy_dma(void* args);

#define NUMBER_OF_FUNCTIONS_OPERATIONS 20

static void (*operations[NUMBER_OF_FUNCTIONS_OPERATIONS])(sp_t* sp) =
{
	add, sub, lsf, rsf,and, or ,xor, lhi, ld, none,
	none, none,none,none,none,none,jlt, jle, jeq, jne
};


/*
 * opcodes
 */
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

static char opcode_name[32][4] = {"ADD", "SUB", "LSF", "RSF", "AND", "OR", "XOR", "LHI",
				 "LD", "ST", "U", "U", "U", "U", "U", "U",
				 "JLT", "JLE", "JEQ", "JNE", "JIN", "U", "U", "U",
				 "HLT", "CPY", "ASK", "U", "U", "U", "U", "U"};

static void dump_sram(sp_t *sp)
{
	FILE *fp;
	int i;

	fp = fopen("sram_out.txt", "w");
	if (fp == NULL) {
                printf("couldn't open file sram_out.txt\n");
                exit(1);
	}
	for (i = 0; i < SP_SRAM_HEIGHT; i++)
		fprintf(fp, "%08x\n", llsim_mem_extract(sp->sram, i, 31, 0));
	fclose(fp);
}

void first_exec_state(int opcode, sp_t* sp)
{
	operations[opcode](sp);
}

void second_exec_init_print(sp_registers_t* spro)
{
	fprintf(inst_trace_fp, "--- instruction %i (%04x) @ PC %i (%04x)"
		" -----------------------------------------------------------\n",
		(spro->cycle_counter) / 6 - 1, (spro->cycle_counter) / 6 - 1, spro->pc, spro->pc);

	fprintf(inst_trace_fp, "pc = %04d, inst = %08x, opcode = %i (%s), dst = %i,"
		" src0 = %i, src1 = %i, immediate = %08x\n", spro->pc, spro->inst,
		spro->opcode, opcode_name[spro->opcode], spro->dst, spro->src0,
		spro->src1, spro->immediate);

	fprintf(inst_trace_fp, "r[0] = 00000000 r[1] = %08x r[2] = %08x r[3] = %08x \n",
		(spro->immediate != 0) ? spro->immediate : 0, spro->r[2], spro->r[3]);

	fprintf(inst_trace_fp, "r[4] = %08x r[5] = %08x r[6] = %08x r[7] = %08x \n\n",
		spro->r[4], spro->r[5], spro->r[6], spro->r[7]);
}

void basic_print(sp_registers_t* spro, sp_registers_t* sprn, sp_t *sp)
{
	if (spro->opcode == LD)
	{
		int loaded_mem = llsim_mem_extract_dataout(sp->sram, 31, 0);
		fprintf(inst_trace_fp, ">>>> EXEC: R[%i] = MEM[%i] = %08x <<<<\n\n",
			spro->dst, spro->alu1, loaded_mem);
		sprn->r[spro->dst] = loaded_mem;
	}
	else
	{
		fprintf(inst_trace_fp, ">>>> EXEC: R[%i] = %i %s %i <<<<\n\n", spro->dst,
			spro->alu0, opcode_name[spro->opcode], spro->alu1);
		sprn->r[spro->dst] = spro->aluout;
	}
	sprn->pc = spro->pc + 1;
}

void branch_print(sp_registers_t* spro, sp_registers_t* sprn, sp_t* sp)
{
	if(spro->opcode == JIN)
	{

		fprintf(inst_trace_fp, ">>>> EXEC: %s %i, %i, %i <<<<\n\n", opcode_name[spro->opcode],
			spro->r[spro->src0], spro->r[spro->src1], spro->immediate);

		sprn->r[7] = spro->pc;
		sprn->pc = spro->immediate;
	}
	else if (spro->opcode == ST)
	{
		fprintf(inst_trace_fp, ">>>> EXEC: MEM[%i] = R[%i] = %08x <<<<\n\n",
			(spro->src1 == 1) ? spro->immediate : spro->r[spro->src1], spro->src0, spro->r[spro->src0]);

		llsim_mem_write(sp->sram, spro->alu1);
		llsim_mem_set_datain(sp->sram, spro->alu0, 31, 0);
		sprn->pc = spro->pc + 1;
	}
	else
	{
		if (spro->aluout == 1)
		{
			fprintf(inst_trace_fp, ">>>> EXEC: %s %i, %i, %i <<<<\n\n", opcode_name[spro->opcode],
				spro->r[spro->src0], spro->r[spro->src1], spro->immediate);

			sprn->r[7] = spro->pc;
			sprn->pc = spro->immediate;
		}
		else
		{
			fprintf(inst_trace_fp, ">>>> EXEC: %s %i, %i, %i <<<<\n\n",
				opcode_name[spro->opcode], spro->r[spro->src0], spro->r[spro->src1], spro->pc + 1);
			sprn->pc = spro->pc + 1;
		}
	}
}

static void sp_ctl(sp_t *sp)
{
	sp_registers_t *spro = sp->spro;
	sp_registers_t *sprn = sp->sprn;
	int i;
	static int dma_flag = 0;

	// sp_ctl

	fprintf(cycle_trace_fp, "cycle %d\n", spro->cycle_counter);
	for (i = 2; i <= 7; i++)
		fprintf(cycle_trace_fp, "r%d %08x\n", i, spro->r[i]);
	fprintf(cycle_trace_fp, "pc %08x\n", spro->pc);
	fprintf(cycle_trace_fp, "inst %08x\n", spro->inst);
	fprintf(cycle_trace_fp, "opcode %08x\n", spro->opcode);
	fprintf(cycle_trace_fp, "dst %08x\n", spro->dst);
	fprintf(cycle_trace_fp, "src0 %08x\n", spro->src0);
	fprintf(cycle_trace_fp, "src1 %08x\n", spro->src1);
	fprintf(cycle_trace_fp, "immediate %08x\n", spro->immediate);
	fprintf(cycle_trace_fp, "alu0 %08x\n", spro->alu0);
	fprintf(cycle_trace_fp, "alu1 %08x\n", spro->alu1);
	fprintf(cycle_trace_fp, "aluout %08x\n", spro->aluout);
	fprintf(cycle_trace_fp, "cycle_counter %08x\n", spro->cycle_counter);
	fprintf(cycle_trace_fp, "ctl_state %08x\n\n", spro->ctl_state);

	sprn->cycle_counter = spro->cycle_counter + 1;

	switch (spro->ctl_state) 
	{
		case CTL_STATE_IDLE:
			sprn->pc = 0;
			if (sp->start)
				sprn->ctl_state = CTL_STATE_FETCH0;
			break;

		case CTL_STATE_FETCH0:
			llsim_mem_read(sp->sram, spro->pc);
			sprn->ctl_state = CTL_STATE_FETCH1;
			break;

		case CTL_STATE_FETCH1:
			sprn->inst = llsim_mem_extract_dataout(sp->sram, 31, 0);
			sprn->ctl_state = CTL_STATE_DEC0;
			break;

		case CTL_STATE_DEC0:
			sprn->opcode = (spro->inst >> 25) & 31;
			sprn->dst = (spro->inst >> 22) & 7;
			sprn->src0 = (spro->inst >> 19) & 7;
			sprn->src1 = (spro->inst >> 16) & 7;
			sprn->immediate = spro->inst & 65535;
			//sign extention
			if ((spro->inst & 32768) != 0)
				sprn->immediate = sprn->immediate + (65535 << 16);
			sprn->ctl_state = CTL_STATE_DEC1;
			break;

		case CTL_STATE_DEC1:
			if (spro->opcode == LHI) 
			{
				sprn->alu0 = (spro->r[spro->dst]) && 65535;
				sprn->alu1 = spro->immediate;
			}
			else 
			{
				sprn->alu0 = spro->r[spro->src0];
				sprn->alu1 = spro->r[spro->src1];
				if (spro->src0 == 1) 
				{
					sprn->alu0 = spro->immediate;
				}
				if (spro->src1 == 1) 
				{
					sprn->alu1 = spro->immediate;
				}
				if (spro->src0 == 0) 
				{
					sprn->alu0 = 0;
				}
				if (spro->src1 == 0) 
				{
					sprn->alu1 = 0;
				}
			}

			sprn->ctl_state = CTL_STATE_EXEC0;
			break;

		case CTL_STATE_EXEC0:
			if (spro->opcode == ADD || spro->opcode == SUB || spro->opcode == LSF || spro->opcode == RSF ||
				spro->opcode == AND || spro->opcode == OR || spro->opcode == XOR || spro->opcode == LHI ||
				spro->opcode == LD || spro->opcode == JLT || spro->opcode == JLE || spro->opcode == JEQ ||
				spro->opcode == JNE)
			{
				first_exec_state(spro->opcode, sp);
			}
			else if (spro->opcode == CPY) 
			{
				if (sp->dma->state == DMA_IDLE)
				{
					dma_flag = 1;
					sp->dma->state = DMA_WAIT;
				}
			}
			sprn->ctl_state = CTL_STATE_EXEC1;
			break;

		case CTL_STATE_EXEC1:
			second_exec_init_print(spro);

			switch (spro->opcode) 
			{
				case ADD: 
				case SUB: 
				case LSF: 
				case RSF: 
				case AND: 
				case OR: 
				case XOR: 
				case LHI: 
				case LD:
				{
					basic_print(spro, sprn, sp);
					break;
				}
				case JLT:
				case JLE:
				case JEQ:
				case JNE:
				case JIN:
				case ST:
				{
					branch_print(spro, sprn, sp);
					break;
				}
				case CPY:
				{
					fprintf(inst_trace_fp, ">>>> EXEC: CPY - Source: %i, Destination: %i, Length: %i <<<<\n\n",
						spro->r[spro->src0], spro->r[spro->dst], spro->r[spro->src1]);
					while (!(sp->dma->state == DMA_WAIT)) {}
					sp->dma->source = spro->r[spro->src0];
					sp->dma->dest = spro->r[spro->dst];
					sp->dma->length = spro->r[spro->src1];
					sp->dma->state = DMA_COPY;
					if (pthread_create(&sp->dma_thread, NULL, copy_dma, (void*)sp)) {
						fprintf(stderr, "error launching DMA thread\n");
					}
					sprn->pc = spro->pc + 1;
					break;
				}
				case ASK:
				{
					fprintf(inst_trace_fp, ">>>> EXEC: ASK: Remaining to copy: %i <<<<\n\n",
						sp->dma->remaining_memory);
					sprn->r[spro->dst] = sp->dma->remaining_memory;
					sprn->pc = spro->pc + 1;
					break;
				}
			}

			if (spro->opcode == HLT)
			{
				fprintf(inst_trace_fp, ">>>> EXEC: HALT at PC %04x<<<<\n", spro->pc);
				fprintf(inst_trace_fp, "sim finished at pc %i, %i instructions", spro->pc,
					(spro->cycle_counter) / 6);

				sprn->ctl_state = CTL_STATE_IDLE;
				sp->start = 0;
				if (dma_flag == 1) 
				{
					pthread_join(sp->dma_thread, NULL);
					sp->dma->state = DMA_IDLE;
				}
				dump_sram(sp);
				llsim_stop();
				break;
			}
			else 
			{
				sprn->ctl_state = CTL_STATE_FETCH0;
			}
			break;
	}
}

static void sp_run(llsim_unit_t *unit)
{
	sp_t *sp = (sp_t *) unit->private;

	if (llsim->reset) {
		sp_reset(sp);
		return;
	}

	sp->sram->read = 0;
	sp->sram->write = 0;

	sp_ctl(sp);
}

static void sp_generate_sram_memory_image(sp_t *sp, char *program_name)
{
        FILE *fp;
        int addr, i;

        fp = fopen(program_name, "r");
        if (fp == NULL) {
                printf("couldn't open file %s\n", program_name);
                exit(1);
        }
        addr = 0;
        while (addr < SP_SRAM_HEIGHT) {
                fscanf(fp, "%08x\n", &sp->memory_image[addr]);
                addr++;
                if (feof(fp))
                        break;
        }
	sp->memory_image_size = addr;

        fprintf(inst_trace_fp, "program %s loaded, %d lines\n\n", program_name, addr);

	for (i = 0; i < sp->memory_image_size; i++)
		llsim_mem_inject(sp->sram, i, sp->memory_image[i], 31, 0);
}

static void sp_register_all_registers(sp_t *sp)
{
	sp_registers_t *spro = sp->spro, *sprn = sp->sprn;

	// registers
	llsim_register_register("sp", "r_0", 32, 0, &spro->r[0], &sprn->r[0]);
	llsim_register_register("sp", "r_1", 32, 0, &spro->r[1], &sprn->r[1]);
	llsim_register_register("sp", "r_2", 32, 0, &spro->r[2], &sprn->r[2]);
	llsim_register_register("sp", "r_3", 32, 0, &spro->r[3], &sprn->r[3]);
	llsim_register_register("sp", "r_4", 32, 0, &spro->r[4], &sprn->r[4]);
	llsim_register_register("sp", "r_5", 32, 0, &spro->r[5], &sprn->r[5]);
	llsim_register_register("sp", "r_6", 32, 0, &spro->r[6], &sprn->r[6]);
	llsim_register_register("sp", "r_7", 32, 0, &spro->r[7], &sprn->r[7]);

	llsim_register_register("sp", "pc", 16, 0, &spro->pc, &sprn->pc);
	llsim_register_register("sp", "inst", 32, 0, &spro->inst, &sprn->inst);
	llsim_register_register("sp", "opcode", 5, 0, &spro->opcode, &sprn->opcode);
	llsim_register_register("sp", "dst", 3, 0, &spro->dst, &sprn->dst);
	llsim_register_register("sp", "src0", 3, 0, &spro->src0, &sprn->src0);
	llsim_register_register("sp", "src1", 3, 0, &spro->src1, &sprn->src1);
	llsim_register_register("sp", "alu0", 32, 0, &spro->alu0, &sprn->alu0);
	llsim_register_register("sp", "alu1", 32, 0, &spro->alu1, &sprn->alu1);
	llsim_register_register("sp", "aluout", 32, 0, &spro->aluout, &sprn->aluout);
	llsim_register_register("sp", "immediate", 32, 0, &spro->immediate, &sprn->immediate);
	llsim_register_register("sp", "cycle_counter", 32, 0, &spro->cycle_counter, &sprn->cycle_counter);
	llsim_register_register("sp", "ctl_state", 3, 0, &spro->ctl_state, &sprn->ctl_state);
}

void sp_init(char *program_name)
{
	llsim_unit_t *llsim_sp_unit;
	llsim_unit_registers_t *llsim_ur;
	sp_t *sp;

	llsim_printf("initializing sp unit\n");

	inst_trace_fp = fopen("inst_trace.txt", "w");
	if (inst_trace_fp == NULL) {
		printf("couldn't open file inst_trace.txt\n");
		exit(1);
	}

	cycle_trace_fp = fopen("cycle_trace.txt", "w");
	if (cycle_trace_fp == NULL) {
		printf("couldn't open file cycle_trace.txt\n");
		exit(1);
	}

	llsim_sp_unit = llsim_register_unit("sp", sp_run);
	llsim_ur = llsim_allocate_registers(llsim_sp_unit, "sp_registers", sizeof(sp_registers_t));
	sp = llsim_malloc(sizeof(sp_t));
	llsim_sp_unit->private = sp;
	sp->spro = llsim_ur->old;
	sp->sprn = llsim_ur->new;

	sp->sram = llsim_allocate_memory(llsim_sp_unit, "sram", 32, SP_SRAM_HEIGHT, 0);
	sp_generate_sram_memory_image(sp, program_name);

	sp->start = 1;

	sp_register_all_registers(sp);
	sp-> dma = (DMA_register_t*)calloc(sizeof(DMA_register_t), sizeof(char));
}

#pragma region Functions implementations 

/** add
 * -----
 * Computes addition of two integers and a constant.
 *
 * @param sp_t *sp
 *
 * @return - void.
 */
void add(sp_t* sp)
{
	sp->sprn->aluout = sp->spro->alu0 + sp->spro->alu1;
}

/** sub
 * -----
 * Computes subtruction of two integers and a constant.
 *
 * @param sp_t *sp
 *
 * @return - void.
 */
void sub(sp_t* sp) 
{
	sp->sprn->aluout = sp->spro->alu0 - sp->spro->alu1;
}

/** LSF
 * -----
 * Computes left shift of src0 of src1 places.
 *
 * @param sp_t *sp
 *
 * @return - void.
 */
void lsf(sp_t* sp) 
{
	sp->sprn->aluout = sp->spro->alu0 << sp->spro->alu1;/*R[dst]=lsf(R[src0],R[src1])*/
}

/** RSF
 * -----
 * Computes right shift of src1 of src0 places.
 *
 * @param sp_t *sp
 *
 * @return - void.
 */
void rsf(sp_t* sp) 
{
	sp->sprn->aluout = sp->spro->alu0 >> sp->spro->alu1; /*R[dst]=rsf(R[src0],R[src1])*/
}

/** and
 * -----
 * Computes bitwise and of two integers and a constant.
 *
 * @param sp_t *sp
 *
 * @return - void.
 */
void and(sp_t* sp) 
{
	sp->sprn->aluout = sp->spro->alu0 & sp->spro->alu1; /*R[dst]=rsf(R[src0],R[src1])*/
}

/** or
 * -----
 * Computes bitwise or of two integers and a constant.
 *
 * @param sp_t *sp
 *
 * @return - void.
 */
void or(sp_t * sp) 
{
	sp->sprn->aluout = sp->spro->alu0 | sp->spro->alu1;
}

/** xor
 * -----
 * Computes bitwise xor of two integers and a constant.
 *
 * @param sp_t *sp
 *
 * @return - void.
 */
void xor(sp_t* sp) 
{
	sp->sprn->aluout = sp->spro->alu0 ^ sp->spro->alu1;/*R[dst]=R[src0] XOR R[src1]*/
}

/** LHI
 * -----
 * Loads the 16 lowest bits of the immediate into the 16 highst bits of the dst register.
 *
 * @param sp_t *sp
 *
 * @return - void.
 */
void lhi(sp_t* sp) 
{
	sp->sprn->aluout = ((sp->spro->alu1) << 16) | sp->spro->alu0;
}

/** LD
 * -----
 * Loads memory content at address specified by R[src1].
 *
 * @param sp_t *sp
 *
 * @return - void.
 */
void ld(sp_t* sp) 
{
	llsim_mem_read(sp->sram, sp->spro->alu1);
}

/** JLT
 * -----
 * jumps to immediate[15:0] if R[src0]<R[src1].
 *
 * @param sp_t *sp
 *
 * @return - void.
 */
void jlt(sp_t* sp)
{
	sp->sprn->aluout = (sp->spro->alu0 < sp->spro->alu1) ? 1 : 0;
}

/** JLE
 * -----
 * jumps to immediate[15:0] if R[src0]<=R[src1].
 *
 * @param sp_t *sp
 *
 * @return - void.
 */
void jle(sp_t* sp)
{
	sp->sprn->aluout = (sp->spro->alu0 <= sp->spro->alu1) ? 1 : 0;
}

/** JEQ
 * -----
 * jumps to immediate[15:0] if R[src0]==R[src1].
 *
 * @param sp_t *sp
 *
 * @return - void.
 */
void jeq(sp_t* sp)
{
	sp->sprn->aluout = (sp->spro->alu0 == sp->spro->alu1) ? 1 : 0;
}

/** JNE
 * -----
 * jumps to immediate[15:0] if R[src0]!=R[src1].
 *
 * @param sp_t *sp
 *
 * @return - void.
 */
void jne(sp_t* sp) 
{
	sp->sprn->aluout = (sp->spro->alu0 != sp->spro->alu1) ? 1 : 0;
}

/** JNE
 * -----
 * empty function
 */
void none(sp_t* sp)
{

}

void* copy_dma(void* args)
{
	int i;
	sp_t* sp = (sp_t*)args;
	sp->dma->remaining_memory = sp->dma->length;

	for (i = 0; i < sp->dma->length; i++) {
		sp->sram->data[sp->dma->dest + i] =	sp->sram->data[sp->dma->source + i];
		sp->dma->remaining_memory -= 1;
	}
	if (sp->dma->remaining_memory == 0) {
		sp->dma->state = DMA_WAIT;
	}
    
	return (void*)0;
}

#pragma endregion


