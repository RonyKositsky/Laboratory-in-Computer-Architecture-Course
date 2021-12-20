#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>

#include "llsim.h"

#define sp_printf(a...)						\
	do {							\
		llsim_printf("sp: clock %d: ", llsim->clock);	\
		llsim_printf(a);				\
	} while (0)

int nr_simulated_instructions = 0;
FILE *inst_trace_fp = NULL, *cycle_trace_fp = NULL;

//BHT
#define BHT_SIZE 10
#define PREDICT_TAKEN_MAX 3
#define PREDICT_TAKEN_MIN 2
#define PREDICT_NOT_TAKEN_MAX 1
#define PREDICT_NOT_TAKEN_MIN 1

//DMA
#define DMA_IDLE 0
#define DMA_READ 1
#define DMA_WRITE 2

bool DMA_Finished = false;
bool memory_busy  = false;
bool DMA_active   = false;

typedef struct sp_registers_s {
	// 6 32 bit registers (r[0], r[1] don't exist)
	int r[8];

	// 32 bit cycle counter
	int cycle_counter;

	// fetch0
	int fetch0_active; // 1 bit
	int fetch0_pc; // 16 bits

	// fetch1
	int fetch1_active; // 1 bit
	int fetch1_pc; // 16 bits

	// dec0
	int dec0_active; // 1 bit
	int dec0_pc; // 16 bits
	int dec0_inst; // 32 bits

	// dec1
	int dec1_active; // 1 bit
	int dec1_pc; // 16 bits
	int dec1_inst; // 32 bits
	int dec1_opcode; // 5 bits
	int dec1_src0; // 3 bits
	int dec1_src1; // 3 bits
	int dec1_dst; // 3 bits
	int dec1_immediate; // 32 bits

	// exec0
	int exec0_active; // 1 bit
	int exec0_pc; // 16 bits
	int exec0_inst; // 32 bits
	int exec0_opcode; // 5 bits
	int exec0_src0; // 3 bits
	int exec0_src1; // 3 bits
	int exec0_dst; // 3 bits
	int exec0_immediate; // 32 bits
	int exec0_alu0; // 32 bits
	int exec0_alu1; // 32 bits

	// exec1
	int exec1_active; // 1 bit
	int exec1_pc; // 16 bits
	int exec1_inst; // 32 bits
	int exec1_opcode; // 5 bits
	int exec1_src0; // 3 bits
	int exec1_src1; // 3 bits
	int exec1_dst; // 3 bits
	int exec1_immediate; // 32 bits
	int exec1_alu0; // 32 bits
	int exec1_alu1; // 32 bits
	int exec1_aluout;

   int BHT[BHT_SIZE];

    //DMA
    bool DMA_busy;
    int DMA_state;
    int DMA_num_of_operations_left;
    int DMA_curr_src_addr;
    int DMA_curr_dest_addr;


} sp_registers_t;

/*
 * Master structure
 */
typedef struct sp_s {
	// local srams
#define SP_SRAM_HEIGHT	64 * 1024

	llsim_memory_t *srami, *sramd;

    //instruction counter
    int inst_cnt;

	unsigned int memory_image[SP_SRAM_HEIGHT];
	int memory_image_size;

	int start;

	sp_registers_t *spro, *sprn;
} sp_t;

static void sp_reset(sp_t *sp)
{
	sp_registers_t *sprn = sp->sprn;

	memset(sprn, 0, sizeof(*sprn));
}

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

int execute_exec0(llsim_memory_t *sramd, sp_registers_t *spro, int alu_0, int alu_1);
int exec_1_check_flush(sp_registers_t* spro, int next_pc);
void handle_dec_1_hazards_and_assign_alu0(llsim_memory_t *sramd, sp_registers_t *sprn, sp_registers_t *spro);
void handle_exec_0_hazards(llsim_memory_t *sramd,sp_registers_t *spro, int* alu_0, int* alu_1, int opcode);
void handle_exec_0_DMA(sp_registers_t* sprn, sp_registers_t* spro);
void exec_1_handle_flush(sp_registers_t* sprn, int next_pc);
void handle_DMA(sp_t *sp, int memory_busy);
void inst_trace_print(sp_t* sp);

static char opcode_name[32][4] = {"ADD", "SUB", "LSF", "RSF", "AND", "OR", "XOR", "LHI",
				 "LD", "ST", "U", "U", "U", "U", "U", "U",
				 "JLT", "JLE", "JEQ", "JNE", "JIN", "U", "U", "U",
				 "HLT", "CPY", "ASK", "U", "U", "U", "U", "U"};

static void dump_sram(sp_t *sp, char *name, llsim_memory_t *sram)
{
	FILE *fp;
	int i;

	fp = fopen(name, "w");
	if (fp == NULL) {
                printf("couldn't open file %s\n", name);
                exit(1);
	}
	for (i = 0; i < SP_SRAM_HEIGHT; i++)
		fprintf(fp, "%08x\n", llsim_mem_extract(sram, i, 31, 0));
	fclose(fp);
}

void handle_branch_prediction(sp_registers_t* spro, sp_registers_t* sprn)
{
    int opcode = (spro->dec0_inst >> 25) & 31;

    //branch prediction
    if (opcode == JLT || opcode == JLE || opcode == JEQ || opcode == JNE)
    {
        int pc = spro->dec0_inst & 65535;
        if (spro->BHT[pc % BHT_SIZE] > PREDICT_NOT_TAKEN_MIN) {//need to flush the pipeline
            sprn->fetch0_pc = pc;
            sprn->dec0_active = 0;
            sprn->fetch1_active = 0;
            sprn->fetch0_active = 1;
        }
    }

    //The only possible structural hazard - current is load and adjacent older operation is store
    //stall next and previous adjacent stages:
    if (opcode == LD && spro->dec1_opcode == ST && spro->dec1_active)
    {
        sprn->fetch1_active = 0;
        sprn->dec1_active = 0;
        
        //revert stage fetch1 to stage fetch0:
        sprn->fetch0_active = spro->fetch1_active;
        sprn->fetch0_pc = spro->fetch1_pc;

        //redo the current stage:
        sprn->dec0_pc = spro->dec0_pc;
        sprn->dec0_inst = spro->dec0_inst;
        sprn->dec0_active = spro->dec0_active;

    }
    else
    {
        // no structural hazard
        sprn->dec1_dst = (spro->dec0_inst >> 22) & 7;
        sprn->dec1_src0 = (spro->dec0_inst >> 19) & 7;
        sprn->dec1_src1 = (spro->dec0_inst >> 16) & 7;
        sprn->dec1_immediate = spro->dec0_inst & 65535;
        sprn->dec1_opcode = (spro->dec0_inst >> 25) & 31;
        sprn->dec1_inst = spro->dec0_inst;
        sprn->dec1_pc = spro->dec0_pc;
        sprn->dec1_active = 1;
    }
}

bool is_branch_operaion(int opcode)
{
    return opcode == JLT || opcode == JLE || opcode == JEQ || opcode == JNE || opcode == JIN;
}

static void sp_ctl(sp_t *sp)
{
	sp_registers_t *spro = sp->spro;
	sp_registers_t *sprn = sp->sprn;
	int i;

	fprintf(cycle_trace_fp, "cycle %d\n", spro->cycle_counter);
	fprintf(cycle_trace_fp, "cycle_counter %08x\n", spro->cycle_counter);
	for (i = 2; i <= 7; i++)
		fprintf(cycle_trace_fp, "r%d %08x\n", i, spro->r[i]);

	fprintf(cycle_trace_fp, "fetch0_active %08x\n", spro->fetch0_active);
	fprintf(cycle_trace_fp, "fetch0_pc %08x\n", spro->fetch0_pc);

	fprintf(cycle_trace_fp, "fetch1_active %08x\n", spro->fetch1_active);
	fprintf(cycle_trace_fp, "fetch1_pc %08x\n", spro->fetch1_pc);

	fprintf(cycle_trace_fp, "dec0_active %08x\n", spro->dec0_active);
	fprintf(cycle_trace_fp, "dec0_pc %08x\n", spro->dec0_pc);
	fprintf(cycle_trace_fp, "dec0_inst %08x\n", spro->dec0_inst); // 32 bits

	fprintf(cycle_trace_fp, "dec1_active %08x\n", spro->dec1_active);
	fprintf(cycle_trace_fp, "dec1_pc %08x\n", spro->dec1_pc); // 16 bits
	fprintf(cycle_trace_fp, "dec1_inst %08x\n", spro->dec1_inst); // 32 bits
	fprintf(cycle_trace_fp, "dec1_opcode %08x\n", spro->dec1_opcode); // 5 bits
	fprintf(cycle_trace_fp, "dec1_src0 %08x\n", spro->dec1_src0); // 3 bits
	fprintf(cycle_trace_fp, "dec1_src1 %08x\n", spro->dec1_src1); // 3 bits
	fprintf(cycle_trace_fp, "dec1_dst %08x\n", spro->dec1_dst); // 3 bits
	fprintf(cycle_trace_fp, "dec1_immediate %08x\n", spro->dec1_immediate); // 32 bits

	fprintf(cycle_trace_fp, "exec0_active %08x\n", spro->exec0_active);
	fprintf(cycle_trace_fp, "exec0_pc %08x\n", spro->exec0_pc); // 16 bits
	fprintf(cycle_trace_fp, "exec0_inst %08x\n", spro->exec0_inst); // 32 bits
	fprintf(cycle_trace_fp, "exec0_opcode %08x\n", spro->exec0_opcode); // 5 bits
	fprintf(cycle_trace_fp, "exec0_src0 %08x\n", spro->exec0_src0); // 3 bits
	fprintf(cycle_trace_fp, "exec0_src1 %08x\n", spro->exec0_src1); // 3 bits
	fprintf(cycle_trace_fp, "exec0_dst %08x\n", spro->exec0_dst); // 3 bits
	fprintf(cycle_trace_fp, "exec0_immediate %08x\n", spro->exec0_immediate); // 32 bits
	fprintf(cycle_trace_fp, "exec0_alu0 %08x\n", spro->exec0_alu0); // 32 bits
	fprintf(cycle_trace_fp, "exec0_alu1 %08x\n", spro->exec0_alu1); // 32 bits

	fprintf(cycle_trace_fp, "exec1_active %08x\n", spro->exec1_active);
	fprintf(cycle_trace_fp, "exec1_pc %08x\n", spro->exec1_pc); // 16 bits
	fprintf(cycle_trace_fp, "exec1_inst %08x\n", spro->exec1_inst); // 32 bits
	fprintf(cycle_trace_fp, "exec1_opcode %08x\n", spro->exec1_opcode); // 5 bits
	fprintf(cycle_trace_fp, "exec1_src0 %08x\n", spro->exec1_src0); // 3 bits
	fprintf(cycle_trace_fp, "exec1_src1 %08x\n", spro->exec1_src1); // 3 bits
	fprintf(cycle_trace_fp, "exec1_dst %08x\n", spro->exec1_dst); // 3 bits
	fprintf(cycle_trace_fp, "exec1_immediate %08x\n", spro->exec1_immediate); // 32 bits
	fprintf(cycle_trace_fp, "exec1_alu0 %08x\n", spro->exec1_alu0); // 32 bits
	fprintf(cycle_trace_fp, "exec1_alu1 %08x\n", spro->exec1_alu1); // 32 bits
	fprintf(cycle_trace_fp, "exec1_aluout %08x\n", spro->exec1_aluout);

	fprintf(cycle_trace_fp, "\n");

	sp_printf("cycle_counter %08x\n", spro->cycle_counter);
	sp_printf("r2 %08x, r3 %08x\n", spro->r[2], spro->r[3]);
	sp_printf("r4 %08x, r5 %08x, r6 %08x, r7 %08x\n", spro->r[4], spro->r[5], spro->r[6], spro->r[7]);
	sp_printf("fetch0_active %d, fetch1_active %d, dec0_active %d, dec1_active %d, exec0_active %d, exec1_active %d\n",
		  spro->fetch0_active, spro->fetch1_active, spro->dec0_active, spro->dec1_active, spro->exec0_active, spro->exec1_active);
	sp_printf("fetch0_pc %d, fetch1_pc %d, dec0_pc %d, dec1_pc %d, exec0_pc %d, exec1_pc %d\n",
		  spro->fetch0_pc, spro->fetch1_pc, spro->dec0_pc, spro->dec1_pc, spro->exec0_pc, spro->exec1_pc);

	sprn->cycle_counter = spro->cycle_counter + 1;

	if (sp->start)
		sprn->fetch0_active = 1;

    // fetch0
    sprn->fetch1_active = 0;
    if (spro->fetch0_active) {
		if(!DMA_Finished)
		{
	        llsim_mem_read(sp->srami, spro->fetch0_pc);
	        sprn->fetch0_pc = (spro->fetch0_pc + 1) & 65535;
	        sprn->fetch1_pc = spro->fetch0_pc;
	        sprn->fetch1_active = 1;
    	}
	    sprn->fetch1_active = 1;
	}

    // fetch1
    if (spro->fetch1_active) 
    {
		if(!DMA_Finished)
        {
            sprn->dec0_pc = spro->fetch1_pc;
            sprn->dec0_inst = llsim_mem_extract_dataout(sp->srami, 31, 0);
        }
        sprn->dec0_active = 1;
    } 
    else
    {
        sprn->dec0_active = 0;
    }

    // dec0
    if (spro->dec0_active) 
    {
		if(!DMA_Finished)
        {
            handle_branch_prediction(spro, sprn);
		}
		else 
        {
		    sprn->dec1_active = 1;
        }
    } 
    else
    {
        sprn->dec1_active = 0;
    }


    // dec1
    if (spro->dec1_active) 
    {
		if(!DMA_Finished)
        {
            handle_dec_1_hazards_and_assign_alu0(sp->sramd, sprn, spro);

            //transfer other registers to next step
            sprn->exec0_pc = spro->dec1_pc;
            sprn->exec0_inst = spro->dec1_inst;
            sprn->exec0_opcode = spro->dec1_opcode;
            sprn->exec0_dst = spro->dec1_dst;
            sprn->exec0_src0 = spro->dec1_src0;
            sprn->exec0_src1 = spro->dec1_src1;
            sprn->exec0_immediate = spro->dec1_immediate;
        }
        sprn->exec0_active = 1;
    }
    else
    {
        sprn->exec0_active = 0;
    }

    // exec0
    if (spro->exec0_active) 
    {
        int alu_0 = spro->exec0_alu0;
        int alu_1 = spro->exec0_alu1;

        handle_exec_0_hazards(sp->sramd, spro, &alu_0, &alu_1, spro->exec1_opcode);

        if (spro->exec0_opcode != CPY)
        {
            sprn->exec1_aluout = execute_exec0(sp->sramd, spro, alu_0, alu_1);
        }

        handle_exec_0_DMA(sprn, spro);


        sprn->exec1_inst = spro->exec0_inst;
        sprn->exec1_alu0 = alu_0;
        sprn->exec1_alu1 = alu_1;
        sprn->exec1_opcode = spro->exec0_opcode;
        sprn->exec1_dst = spro->exec0_dst;
        sprn->exec1_src0 = spro->exec0_src0;
        sprn->exec1_src1 = spro->exec0_src1;
        sprn->exec1_immediate = spro->exec0_immediate;
        sprn->exec1_pc = spro->exec0_pc;
        sprn->exec1_active = 1;

    }
    else 
    {
        sprn->exec1_active = 0;
    }


    // exec1
    if (spro->exec1_active)
    {
        sp_printf("exec1: pc %d, inst %08x, opcode %d, aluout %d\n", spro->exec1_pc, spro->exec1_inst,
                  spro->exec1_opcode, spro->exec1_aluout);
        inst_trace_print(sp);

        sp->inst_cnt = sp->inst_cnt + 1;

        if ((spro->exec1_opcode == HLT)||(DMA_Finished)) 
        {
			if(spro->DMA_num_of_operations_left > 0)
            {
				DMA_Finished=true;
			}
			else
            {
				DMA_Finished = false;
				fprintf(inst_trace_fp, "sim finished at pc %d, %d instructions", sp->spro->exec1_pc, sp->inst_cnt);
				llsim_stop();
				dump_sram(sp, "srami_out.txt", sp->srami);
				dump_sram(sp, "sramd_out.txt", sp->sramd);
            }
        }
        else if (spro->exec1_opcode == ST)
        {
            llsim_mem_set_datain(sp->sramd, spro->exec1_alu0, 31, 0);
            llsim_mem_write(sp->sramd, spro->exec1_alu1);
        }
        else if (spro->exec1_opcode == LD)
        {
            if (spro->exec1_dst != 0 && spro->exec1_dst != 1)
                sprn->r[spro->exec1_dst] = llsim_mem_extract_dataout(sp->sramd, 31, 0);
        }
        else if (is_branch_operaion(spro->exec1_opcode)) 
        {
            int branch_taken = 0;
            int next_pc;

            if(spro->exec1_opcode == JIN)
            {
                next_pc = spro->exec1_alu0 & 65535;
                branch_taken = 1;
            }
            else 
            {
                if (spro->exec1_aluout) 
                {
                    next_pc = spro->exec1_immediate & 65535;
                    branch_taken = 1;
                }
                else
                    next_pc = (spro->exec1_pc + 1) & 65535;
            }

            // update Branch History Table and R[7]
            if (branch_taken) 
            {
                sprn->r[7] = spro->exec1_pc;
                if (spro->BHT[spro->exec1_pc % BHT_SIZE] + 1 < BHT_SIZE)
                {
                    sprn->BHT[spro->exec1_pc % BHT_SIZE] = spro->BHT[spro->exec1_pc % BHT_SIZE] + 1;
                }
            }
            else if (spro->BHT[spro->exec1_pc % BHT_SIZE] - 1 > 0)
            {
                sprn->BHT[spro->exec1_pc % BHT_SIZE] = spro->BHT[spro->exec1_pc % BHT_SIZE] - 1;
            }

            //check if earlier stages need flush
            if (exec_1_check_flush(spro, next_pc))
            {
                exec_1_handle_flush(sprn, next_pc);
            }

        }
        else if (spro->exec1_dst != 0 && spro->exec1_dst != 1) // ALU or ASK operation
        {
            sprn->r[spro->exec1_dst] = spro->exec1_aluout;
        }

    }

    if (spro->exec1_opcode == CPY && !DMA_active)
    {
        DMA_active = true;
    }

    if(!DMA_Finished)
    {
        int memory_busy = 1;
        if (sprn->dec1_opcode != LD && sprn->exec0_opcode != LD && sprn->exec1_opcode != LD &&
            sprn->exec0_opcode != ST && sprn->dec1_opcode != ST && sprn->exec1_opcode != ST) 
        {
            memory_busy = 0;
        }
	    handle_DMA(sp, memory_busy);
    }
	else
    {
        handle_DMA(sp, 0);
    }
    

}

static void sp_run(llsim_unit_t *unit)
{
	sp_t *sp = (sp_t *) unit->private;
	
	if (llsim->reset) {
		sp_reset(sp);
		return;
	}

	sp->srami->read = 0;
	sp->srami->write = 0;
	sp->sramd->read = 0;
	sp->sramd->write = 0;

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
        while (addr < SP_SRAM_HEIGHT) 
        {
                fscanf(fp, "%08x\n", &sp->memory_image[addr]);
                addr++;
                if (feof(fp))
                        break;
        }
	sp->memory_image_size = addr;

    fprintf(inst_trace_fp, "program %s loaded, %d lines\n", program_name, addr);

	for (i = 0; i < sp->memory_image_size; i++) 
    {
		llsim_mem_inject(sp->srami, i, sp->memory_image[i], 31, 0);
		llsim_mem_inject(sp->sramd, i, sp->memory_image[i], 31, 0);
	}
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

	sp->srami = llsim_allocate_memory(llsim_sp_unit, "srami", 32, SP_SRAM_HEIGHT, 0);
	sp->sramd = llsim_allocate_memory(llsim_sp_unit, "sramd", 32, SP_SRAM_HEIGHT, 0);
	sp_generate_sram_memory_image(sp, program_name);

	sp->start = 1;
	
	// c2v_translate_end
}


void habdle_src0(llsim_memory_t* sramd, sp_registers_t* sprn, sp_registers_t* spro)
{
    int opcode = spro->exec1_opcode;

    if (spro->dec1_src0 == 1)
    { 
        //immediate reg treatment
        sprn->exec0_alu0 = spro->dec1_immediate;
    }
    else if (spro->dec1_src0 == 0)
    {
        //zero reg treatment
        sprn->exec0_alu0 = 0;
    }
    else if (spro->exec1_opcode == LD && spro->exec1_active &&
        spro->exec1_dst == spro->dec1_src0)
    {
        // read after write MEMORY bypass
        sprn->exec0_alu0 = llsim_mem_extract_dataout(sramd, 31, 0);
    }
    else if (spro->exec1_active && spro->dec1_src0 == spro->exec1_dst &&
        (opcode == ADD || opcode == SUB || opcode == LSF || opcode == RSF || opcode == AND || opcode == OR ||
            opcode == XOR || opcode == LHI || opcode == ASK || opcode == CPY))
    {
        // read after write ALU bypass
        sprn->exec0_alu0 = spro->exec1_aluout;
    }
    else if (spro->dec1_src0 == 7 && spro->exec1_active && spro->exec1_aluout == 1 &&
        (spro->exec1_opcode == JLT || spro->exec1_opcode == JLE || spro->exec1_opcode == JEQ ||
            spro->exec1_opcode == JNE || spro->exec1_opcode == JIN))
    {
        // branch is taken in next stage - need to flush current write to R[7]
        sprn->exec0_alu0 = spro->exec1_pc;
    }
    else
    {
        sprn->exec0_alu0 = spro->r[spro->dec1_src0];
    }
}

void habdle_src1(llsim_memory_t* sramd, sp_registers_t* sprn, sp_registers_t* spro)
{
    int opcode = spro->exec1_opcode;

    if (spro->dec1_src1 == 1)
    {
        //immediate reg treatment
        sprn->exec0_alu1 = spro->dec1_immediate;
    }
    else if (spro->dec1_src1 == 0)
    {
        //zero reg treatment
        sprn->exec0_alu1 = 0;
    }
    else if (spro->exec1_opcode == LD && spro->exec1_active &&
        spro->dec1_src1 == spro->exec1_dst)
    {
        // read after write MEMORY bypass
        sprn->exec0_alu1 = llsim_mem_extract_dataout(sramd, 31, 0);
    }
    else if (spro->exec1_active && spro->exec1_dst == spro->dec1_src1 &&
        (opcode == ADD || opcode == SUB || opcode == LSF || opcode == RSF || opcode == AND || opcode == OR ||
            opcode == XOR || opcode == LHI || opcode == ASK || opcode == CPY)) 
    {
        // read after write ALU bypass
        sprn->exec0_alu1 = spro->exec1_aluout;
    }
    else if (spro->dec1_src1 == 7 && spro->exec1_active && spro->exec1_aluout == 1 &&
        (spro->exec1_opcode == JLT || spro->exec1_opcode == JLE || spro->exec1_opcode == JEQ ||
            spro->exec1_opcode == JNE || spro->exec1_opcode == JIN))
    {
        // branch is taken in next stage - need to flush current write to R[7]
        sprn->exec0_alu1 = spro->exec1_pc;
    }
    else
    {
        sprn->exec0_alu1 = spro->r[spro->dec1_src1];
    }
}

void handle_dec_1_hazards_and_assign_alu0(llsim_memory_t *sramd, sp_registers_t *sprn, sp_registers_t *spro){

    habdle_src0(sramd, sprn, spro);
    habdle_src1(sramd, sprn, spro);

    if (spro->dec1_opcode == LHI) // LHI opcode treatment
        sprn->exec0_alu1 = spro->dec1_immediate;
}


void handle_exec_0_hazards(llsim_memory_t *sramd, sp_registers_t *spro, int* alu_0, int* alu_1, int opcode){
    //Handle alu_0
    if (spro->exec0_src0 != 0 && spro->exec0_src0 != 1) 
    {
        if (opcode == LD && spro->exec1_active && spro->exec0_src0 == spro->exec1_dst)
        {
            // read after write MEMORY bypass
            *alu_0 = llsim_mem_extract_dataout(sramd, 31, 0);
        }
        else if (spro->exec1_active && spro->exec1_dst == spro->exec0_src0 &&
            (opcode == ADD || opcode == SUB || opcode == AND || opcode == OR || opcode == XOR ||
                opcode == LSF || opcode == RSF || opcode == LHI || opcode == CPY || opcode == ASK))
        {
            // read after write ALU bypass
            *alu_0 = spro->exec1_aluout;
        }
        else if (spro->exec0_src0 == 7 && spro->exec1_active && (opcode == JLT || opcode == JLE ||
            opcode == JEQ || opcode == JNE || opcode == JIN))
        {
            // branch is taken in next stage - need to flush current write to R[7]
            *alu_0 = spro->exec1_pc;
        }
    }

    //Handle alu_1 
    if (spro->exec0_src1 != 0 && spro->exec0_src1 != 1) 
    {
        if (opcode == LD && spro->exec1_active && spro->exec1_dst == spro->exec0_src1)
        {
            // read after write MEMORY bypass
            *alu_1 = llsim_mem_extract_dataout(sramd, 31, 0);
        }
        else if (spro->exec1_active && spro->exec1_dst == spro->exec0_src1 &&
            (opcode == ADD || opcode == SUB || opcode == AND || opcode == OR || opcode == XOR ||
                opcode == LSF || opcode == RSF || opcode == LHI || opcode == CPY || opcode == ASK))
        {
            // read after write ALU bypass
            *alu_1 = spro->exec1_aluout;
        }
        else if (spro->exec0_src1 == 7 && spro->exec1_active && (opcode == JLT || opcode == JLE ||
            opcode == JEQ || opcode == JNE || opcode == JIN))
        {
            // branch is taken in next stage - need to flush current write to R[7]
            *alu_1 = spro->exec1_pc;
        }
    }
}

int execute_exec0(llsim_memory_t *sramd, sp_registers_t *spro, int alu_0, int alu_1){
    int alu_out = 0;

    if (spro->exec0_opcode == ADD)
        alu_out = alu_0 + alu_1;
    else if (spro->exec0_opcode == SUB)
        alu_out = alu_0 - alu_1;
    else if (spro->exec0_opcode == LSF)
        alu_out = alu_0 << alu_1;
    else if (spro->exec0_opcode == RSF)
        alu_out = alu_0 >> alu_1;
    else if (spro->exec0_opcode == AND)
        alu_out = alu_0 & alu_1;
    else if (spro->exec0_opcode == OR)
        alu_out = alu_0 | alu_1;
    else if (spro->exec0_opcode == XOR)
        alu_out = alu_0 ^ alu_1;
    else if (spro->exec0_opcode ==LHI)
        alu_out = (alu_0 & 65535)| (alu_1<<16);
    else if (spro->exec0_opcode == LD)
        llsim_mem_read(sramd, alu_1 & 65535);
    else if (spro->exec0_opcode == JLT)
        alu_out = (alu_0 < alu_1) ? 1 : 0;
    else if (spro->exec0_opcode == JLE)
        alu_out = (alu_0 <= alu_1) ? 1 : 0;
    else if (spro->exec0_opcode == JEQ)
        alu_out = (alu_0 == alu_1) ? 1 : 0;
    else if (spro->exec0_opcode == JNE)
        alu_out = (alu_0 != alu_1) ? 1 : 0;
    else if (spro->exec0_opcode == ASK)
        alu_out = spro->DMA_num_of_operations_left;

    return alu_out;
}

void handle_exec_0_DMA(sp_registers_t* sprn, sp_registers_t* spro)
{
    int pol_status = (spro->DMA_busy || (spro->exec1_opcode == CPY && spro->exec1_active));
    int opcode = spro->exec1_opcode;
    int next_is_content_changing_opcode = (opcode == ADD || opcode == SUB || opcode == AND || opcode == OR ||
            opcode == XOR || opcode == LSF || opcode == RSF || opcode == LHI || opcode == CPY || opcode == ASK);

    if (spro->exec0_opcode == CPY && pol_status == 0)
    {
        //handle Read After Write Hazard for src0
        if (spro->exec1_active && spro->exec1_dst == spro->exec0_src0 && next_is_content_changing_opcode)
        {
            sprn->DMA_curr_src_addr = spro->exec1_aluout;
        }
        else
        {
            sprn->DMA_curr_src_addr = spro->r[spro->exec0_src0];
        }
        if (spro->exec1_dst == spro->exec0_src1 && spro->exec1_active && next_is_content_changing_opcode)
        {  
            //handle Read After Write Hazard for src1
            sprn->DMA_curr_dest_addr = spro->exec1_aluout;
        }
        else
        {
            sprn->DMA_curr_dest_addr = spro->r[spro->exec0_src1];
        }

        sprn->DMA_num_of_operations_left = spro->exec0_immediate;
    }
}


int exec_1_check_flush(sp_registers_t* spro, int next_pc)
{
    if (spro->exec0_active && spro->exec0_pc != next_pc)
        return 1;
    else if (spro->fetch1_active && spro->fetch1_pc != next_pc)
        return 1;
    else if (spro->dec1_active && spro->dec1_pc != next_pc)
        return 1;
    else if (spro->dec0_active && spro->dec0_pc != next_pc)
        return 1;
    else if (spro->fetch0_active && spro->fetch0_pc != next_pc)
        return 1;
    else if (spro->dec0_active && spro->dec0_pc != next_pc)
        return 1;
    else
        return 0;
}


void exec_1_handle_flush(sp_registers_t* sprn, int next_pc){
    sprn->fetch0_active = 1;
    sprn->dec0_active = 0;
    sprn->exec0_active = 0;
    sprn->fetch1_active = 0;
    sprn->dec1_active = 0;
    sprn->exec1_active = 0;
    sprn->fetch0_pc = next_pc;
}


static void handle_DMA(sp_t *sp, int memory_busy){

    sp_registers_t *spro = sp->spro;
    sp_registers_t *sprn = sp->sprn;

    switch (spro->DMA_state) 
    {
        case DMA_IDLE:
            if (DMA_active && !memory_busy) {
                sprn->DMA_state = DMA_READ;
                sprn->DMA_busy = 1;
            }
            else
                sprn->DMA_state = DMA_IDLE;
            break;

        case DMA_READ:
            llsim_mem_read(sp->sramd, spro->DMA_curr_src_addr);
            sprn->DMA_state = DMA_WRITE;
            break;

        case DMA_WRITE:
            llsim_mem_set_datain(sp->sramd, llsim_mem_extract_dataout(sp->sramd, 31, 0), 31, 0);
            llsim_mem_write(sp->sramd, spro->DMA_curr_dest_addr);

            sprn->DMA_num_of_operations_left = spro->DMA_num_of_operations_left - 1;
            sprn->DMA_curr_dest_addr = spro->DMA_curr_dest_addr + 1;
            sprn->DMA_curr_src_addr = spro->DMA_curr_src_addr + 1;

            if (spro->DMA_num_of_operations_left - 1 == 0) 
            {
                sprn->DMA_busy = 0;
                sprn->DMA_state = DMA_IDLE;
                DMA_active = 0;
            }
            else 
            {
                if (memory_busy)
                    sprn->DMA_state = DMA_IDLE;
                else
                    sprn->DMA_state = DMA_READ;
            }
            break;
    }
}


void inst_trace_print(sp_t* sp)
{
    fprintf(inst_trace_fp, "\n");
    fprintf(inst_trace_fp, "--- instruction %d (%04x) @ PC %d (%04i) -----------------------------------------------------------\n",
            sp->inst_cnt, sp->inst_cnt, sp->spro->exec1_pc, sp->spro->exec1_pc);
    fprintf(inst_trace_fp, "pc = %04d, inst = %08x, opcode = %d (%s), dst = %d, src0 = %d, src1 = %d, immediate = %08x\n",
            sp->spro->exec1_pc, sp->spro->exec1_inst, sp->spro->exec1_opcode, opcode_name[sp->spro->exec1_opcode],
            sp->spro->exec1_dst, sp->spro->exec1_src0, sp->spro->exec1_src1, sbs(sp->spro->exec1_inst, 15, 0));
    fprintf(inst_trace_fp, "r[0] = %08x r[1] = %08x r[2] = %08x r[3] = %08x \n",
            0, sp->spro->exec1_immediate, sp->spro->r[2], sp->spro->r[3]);
    fprintf(inst_trace_fp, "r[4] = %08x r[5] = %08x r[6] = %08x r[7] = %08x \n",
            sp->spro->r[4], sp->spro->r[5], sp->spro->r[6], sp->spro->r[7]);
    fprintf(inst_trace_fp, "\n");

    switch (sp->spro->exec1_opcode)
    {
        case ADD:
            fprintf(inst_trace_fp, ">>>> EXEC: R[%d] = %d ADD %d <<<<\n", sp->spro->exec1_dst, sp->spro->exec1_alu0, sp->spro->exec1_alu1);
            break;
        case SUB:
            fprintf(inst_trace_fp, ">>>> EXEC: R[%d] = %d SUB %d <<<<\n", sp->spro->exec1_dst, sp->spro->exec1_alu0, sp->spro->exec1_alu1);
            break;
        case AND:
            fprintf(inst_trace_fp, ">>>> EXEC: R[%d] = %d AND %d <<<<\n", sp->spro->exec1_dst, sp->spro->exec1_alu0, sp->spro->exec1_alu1);
            break;
        case OR:
            fprintf(inst_trace_fp, ">>>> EXEC: R[%d] = %d OR %d <<<<\n", sp->spro->exec1_dst, sp->spro->exec1_alu0, sp->spro->exec1_alu1);
            break;
        case XOR:
            fprintf(inst_trace_fp, ">>>> EXEC: R[%d] = %d XOR %d <<<<\n", sp->spro->exec1_dst, sp->spro->exec1_alu0, sp->spro->exec1_alu1);
            break;
        case LHI:
            fprintf(inst_trace_fp, ">>>> EXEC: R[%d][31:16] = 0x%04x <<<<\n", sp->spro->exec1_dst, sp->spro->exec1_immediate & 65535);
            break;
        case LSF:
            fprintf(inst_trace_fp, ">>>> EXEC: R[%d] = %d LSF %d <<<<\n", sp->spro->exec1_dst, sp->spro->exec1_alu0, sp->spro->exec1_alu1);
            break;
        case RSF:
            fprintf(inst_trace_fp, ">>>> EXEC: R[%d] = %d RSF %d <<<<\n", sp->spro->exec1_dst, sp->spro->exec1_alu0, sp->spro->exec1_alu1);
            break;
        case LD:
            fprintf(inst_trace_fp, ">>>> EXEC: R[%d] = MEM[%d] = %08x <<<<\n", sp->spro->exec1_dst, sp->spro->exec1_alu1,
                    llsim_mem_extract_dataout(sp->sramd, 31, 0));
            break;
        case ST:
            fprintf(inst_trace_fp, ">>>> EXEC: MEM[%d] = R[%d] = %08x <<<<\n", sp->spro->exec1_alu1, sp->spro->exec1_src0, sp->spro->exec1_alu0);
            break;
        case JIN:
            fprintf(inst_trace_fp, ">>>> EXEC: JIN %d <<<<\n", sp->spro->exec1_alu0 & 65535);
            break;
        case HLT:
            fprintf(inst_trace_fp, ">>>> EXEC: HALT at PC %04x<<<<\n", sp->spro->exec1_pc);
            break;
        case JLT:
            fprintf(inst_trace_fp, ">>>> EXEC: JLT %d, %d, %d <<<<\n", sp->spro->exec1_alu0, sp->spro->exec1_alu1, sp->spro->exec1_aluout ? sp->spro->exec1_immediate & 65535 : sp->spro->exec1_pc + 1);
            break;
        case JLE:
            fprintf(inst_trace_fp, ">>>> EXEC: JLE %d, %d, %d <<<<\n", sp->spro->exec1_alu0, sp->spro->exec1_alu1, sp->spro->exec1_aluout ? sp->spro->exec1_immediate & 65535 : sp->spro->exec1_pc + 1);
            break;
        case JEQ:
            fprintf(inst_trace_fp, ">>>> EXEC: JEQ %d, %d, %d <<<<\n", sp->spro->exec1_alu0, sp->spro->exec1_alu1, sp->spro->exec1_aluout ? sp->spro->exec1_immediate & 65535 : sp->spro->exec1_pc + 1);
            break;
        case JNE:
            fprintf(inst_trace_fp, ">>>> EXEC: JNE %d, %d, %d <<<<\n", sp->spro->exec1_alu0, sp->spro->exec1_alu1, sp->spro->exec1_aluout ? sp->spro->exec1_immediate & 65535 : sp->spro->exec1_pc + 1);
            break;
        case CPY: 
            fprintf(inst_trace_fp, ">>>> EXEC: CPY from address %04x to adress %04x with length of %d words <<<", sp->spro->DMA_curr_src_addr, sp->spro->DMA_curr_dest_addr, sp->spro->DMA_num_of_operations_left);
            break;
        case ASK: 
            fprintf(inst_trace_fp, ">>>> EXEC: ASK result saved to register %d <<<<", sp->spro->exec1_dst);
            break;
        default:
            break;
    }
}
