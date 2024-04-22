#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define	TRUE (1)
#define FALSE (0)

//clock cycles
int64_t cycles = 0;

// registers
uint64_t 	regs[32];

// program counter
uint64_t	pc = 0;
uint64_t	exit_pc;

// memory
#define INST_MEM_SIZE 32*1024
#define DATA_MEM_SIZE 32*1024
uint32_t	inst_mem[INST_MEM_SIZE]; //instruction memory
uint64_t	data_mem[DATA_MEM_SIZE]; //data memory

//misc. function
int init(char* filename);

void fetch();//fetch an instruction from a instruction memory
void decode();//decode the instruction and read data from register file
void exe();//perform the appropriate operation 
void mem();//access the data memory
void wb();//write result of arithmetic operation or data read from the data memory if required
void cycle_end();

void print_cycles();
void print_reg();
void print_pc();

int main(int ac, char *av[])
{
	(void)ac;

	char done = FALSE;
	if(init(av[1])!=0)
            return -1;

	while (!done)
	{
		wb();
		fetch();
		decode();
		exe();
		mem();

		cycle_end();

		cycles++;    //increase clock cycle
	
     		// check the exit condition, do not delete!! 
		if (regs[9] == 10)  //if value in $t1 is 10, finish the simulation
		{
			done = TRUE;
			pc = exit_pc;
		}

	}


        print_cycles();  //print clock cycles
        print_pc();		 //print pc
        print_reg();	 //print registers
    

	return 0;
}

// implementation

typedef struct s_r_type
{
	uint64_t	opcode: 7;
	uint64_t	rd: 5;
	uint64_t	funct3: 3;
	uint64_t	rs1: 5;
	uint64_t	rs2: 5;
	uint64_t	funct7: 7;
}	t_r_type;

typedef struct s_i_type
{
	uint64_t	opcode: 7;
	uint64_t	rd: 5;
	uint64_t	funct3: 3;
	uint64_t	rs1: 5;
	uint64_t	imm0_11: 12;
}	t_i_type;

typedef struct s_s_type
{
	uint64_t	opcode: 7;
	uint64_t	imm0_4: 5;
	uint64_t	funct3: 3;
	uint64_t	rs1: 5;
	uint64_t	rs2: 5;
	uint64_t	imm5_11: 7;
}	t_s_type;

typedef struct s_sb_type
{
	uint64_t	opcode: 7;
	uint64_t	imm11: 1;
	uint64_t	imm1_4: 4;
	uint64_t	funct3: 3;
	uint64_t	rs1: 5;
	uint64_t	rs2: 5;
	uint64_t	imm5_10: 6;
	uint64_t	imm12: 1;
}	t_sb_type;

typedef struct s_uj_type
{
	uint64_t	opcode: 7;
	uint64_t	rd: 5;
	uint64_t	imm12_19: 8;
	uint64_t	imm11: 1;
	uint64_t	imm1_10: 10;
	uint64_t	imm20: 1;
}	t_uj_type;

typedef union
{
	t_r_type	r_inst;
	t_i_type	i_inst;
	t_s_type	s_inst;
	t_sb_type	sb_inst;
	t_uj_type	uj_inst;
}	t_inst;

typedef struct s_control_unit
{
	uint16_t	branch: 1;
	uint16_t	mem_read: 1;
	uint16_t	mem_to_reg: 1;
	uint16_t	alu_op: 4;
	uint16_t	mem_write: 1;
	uint16_t	alu_src: 1;
	uint16_t	reg_write: 1;
	uint16_t	link: 1;
	uint16_t	funct3: 1;
	uint16_t	funct7: 1;
}	t_control_unit;

struct
{
	uint16_t	wb_ex: 3;
	uint16_t	mem_ex: 3;
	uint16_t	mem_id: 3;
	uint16_t	ex_id: 3;
}	forwarding_unit;

typedef struct s_pipeline_reg
{
	t_inst		inst;	// instruction
	uint64_t	pc;		// program counter
	int64_t		imm;	// immediate
	uint64_t	data;
	uint64_t	d1;		// read data 1
	uint64_t	d2;		// read data 2
	uint16_t	rs1: 5;
	uint16_t	rs2: 5;
	uint16_t	rd: 5;
	uint64_t	alu_res;// alu result
	uint8_t		alu_zero: 1;
	t_control_unit	cu;		// control unit
}	t_pipeline_reg;

static t_pipeline_reg	preg_IF_w;

static t_pipeline_reg	preg_ID_r;
static t_pipeline_reg	preg_ID_w;

static t_pipeline_reg	preg_EX_r;
static t_pipeline_reg	preg_EX_w;

static t_pipeline_reg	preg_MEM_r;
static t_pipeline_reg	preg_MEM_w;

static t_pipeline_reg	preg_WB_r;

void fetch()
{
	// reset write part of IF/ID pipeline register to 0
	preg_IF_w = (t_pipeline_reg){0,};

	// fetch instrction from instruction memory with pc
	*(uint32_t*)&preg_IF_w.inst = *(uint32_t*)((uint8_t*)inst_mem + pc);

	// store pc to pipeline registers
	preg_IF_w.pc = pc;
}

void decode()
{
	preg_ID_w = preg_ID_r;

	t_inst			inst = preg_ID_r.inst;
	uint8_t	opcode = inst.r_inst.opcode;
	uint8_t	compressed_opcode;

	compressed_opcode = ((opcode & 0x70) >> 4) + ((opcode & 0xC) >> 2);
	preg_ID_w.rs1 = inst.r_inst.rs1;
	preg_ID_w.rs2 = inst.r_inst.rs2;
	preg_ID_w.rd = inst.r_inst.rd;
	preg_ID_w.cu.branch = (inst.r_inst.opcode & 0x40) >> 6;
	preg_ID_w.cu.link = (inst.r_inst.opcode & 0x4) >> 2;
	preg_ID_w.d1
		= (forwarding_unit.ex_id & 0x2) ? preg_ID_w.d1 : regs[inst.r_inst.rs1];
	preg_ID_w.d2
		= (forwarding_unit.ex_id & 0x4) ? preg_ID_w.d2 : regs[inst.r_inst.rs2];
	switch (compressed_opcode)
	{
		// R-type
		case 3:
			preg_ID_w.cu.reg_write = 1;
			preg_ID_w.cu.alu_op = 2;
			break;
		// I-type
		case 0:
			preg_ID_w.cu.mem_read = 1;
			preg_ID_w.cu.mem_to_reg = 1; // fall through
		case 1: case 7:
			preg_ID_w.cu.reg_write = 1;
			preg_ID_w.cu.alu_src = 1;
			preg_ID_w.cu.alu_op = (compressed_opcode & 1) << 1;
			preg_ID_w.imm = inst.i_inst.imm0_11;
			preg_ID_w.imm = (preg_ID_w.imm << 52) >> 52;
			break;
		// S-type
		case 2:
			preg_ID_w.cu.mem_write = 1;
			preg_ID_w.cu.alu_op = 0;
			preg_ID_w.cu.alu_src = 1;
			preg_ID_w.imm = inst.s_inst.imm0_4;
			preg_ID_w.imm |= inst.s_inst.imm5_11 << 5;
			preg_ID_w.imm = (preg_ID_w.imm << 52) >> 52;
			break;
		// SB-type
		case 6:
			preg_ID_w.cu.alu_op = 1;
			preg_ID_w.cu.alu_src = 1;
			preg_ID_w.imm = inst.sb_inst.imm1_4;
			preg_ID_w.imm |= inst.sb_inst.imm5_10 << 4;
			preg_ID_w.imm |= inst.sb_inst.imm11 << 10;
			preg_ID_w.imm |= inst.sb_inst.imm12 << 11;
			preg_ID_w.imm = (preg_ID_w.imm << 52) >> 52;
			break;
		// U-type
		case 5:
			break;
		// UJ-type
		case 9:
			preg_ID_w.cu.reg_write = 1;
			preg_ID_w.cu.alu_op = 0;
			preg_ID_w.cu.alu_src = 1;
			preg_ID_w.imm = inst.uj_inst.imm1_10;
			preg_ID_w.imm |= inst.uj_inst.imm11 << 10;
			preg_ID_w.imm |= inst.uj_inst.imm12_19 << 11;
			preg_ID_w.imm |= inst.uj_inst.imm20 << 19;
			preg_ID_w.imm = (preg_ID_w.imm << 44) >> 44;
			break;
	}
	preg_ID_w.cu.funct7 = compressed_opcode == 3 || (compressed_opcode == 1 && (inst.i_inst.funct3 & 0x3) == 1);
	preg_ID_w.pc = preg_ID_w.cu.branch == 1 ?
		((compressed_opcode == 7 ? preg_ID_w.d1 : preg_ID_w.pc) + (preg_ID_w.imm << 1)) : preg_ID_w.pc;
}

uint8_t	set_alu_control_lines(uint8_t alu_op, t_inst inst)
{
	uint8_t	funct3 = inst.r_inst.funct3;
	uint8_t	funct7_30 = (inst.r_inst.funct7 >> 5) & preg_EX_r.cu.funct7;

	switch (alu_op)
	{
		case 0:
			return 2;
		case 1:
			return 6;
		case 2:
			break;
	}
	switch (funct3)
	{
		case 0:	// add, sub
			return 2 + ((funct7_30) << 2);
//		case 1:	// sll
//			return;
//		case 4:	// xor
//			return;
//		case 5:	// sra, srl
//			return;
		case 6:	// or
			return 1;
		case 7:	// and
			return 0;
	}
	return 0;
}

void exe()
{
	preg_EX_w = preg_EX_r;

	int64_t			d1, d2;
	int64_t			res;
	uint8_t	control_lines = set_alu_control_lines(preg_EX_r.cu.alu_op, preg_EX_r.inst);

	preg_EX_w.data = preg_EX_r.d2;
	d1 = preg_EX_r.d1;
	d2 = preg_EX_r.cu.alu_src == 1 ? preg_EX_r.imm : (int64_t)preg_EX_r.d2;
	switch (control_lines)
	{
		case 0:
			res = d1 & d2;
			break;
		case 1:
			res = d1 | d2;
			break;
		case 2:
			res = d1 + d2;
			break;
		case 6:
			res = d1 - d2;
			break;
	};
	preg_EX_w.alu_res = res;
	preg_EX_w.alu_zero = !res;
}

void mem()
{
	preg_MEM_w = preg_MEM_r;

	if (preg_MEM_r.cu.mem_write)
	{
		data_mem[preg_MEM_r.alu_res] = preg_MEM_r.data;
	}
	if (preg_MEM_r.cu.mem_read)
	{
		preg_MEM_w.data = data_mem[preg_MEM_r.alu_res];
	}
}

void wb()
{
	if (preg_WB_r.cu.reg_write != 1 || preg_WB_r.rd == 0)
		return;
	regs[preg_WB_r.rd]
		= preg_WB_r.cu.mem_to_reg == 1 ? preg_WB_r.data : preg_WB_r.alu_res;
}

void forwarding()
{
	uint8_t	temp;

	temp = preg_WB_r.cu.reg_write & (preg_WB_r.rd != 0);
	temp = ((temp & (preg_EX_r.rs1 == preg_WB_r.rd)) << 1)
		+ ((temp & (preg_EX_r.rs2 == preg_WB_r.rd)) << 2);
	forwarding_unit.wb_ex = temp;
	temp = preg_MEM_r.cu.reg_write & (preg_MEM_r.rd != 0);
	temp = ((temp & (preg_EX_r.rs1 == preg_MEM_r.rd)) << 1)
		+ ((temp & (preg_EX_r.rs2 == preg_MEM_r.rd)) << 2);
	forwarding_unit.mem_ex = temp;
	temp = preg_MEM_r.cu.reg_write & (preg_MEM_r.rd != 0);
	temp = (temp & (preg_ID_r.rs1 == preg_MEM_r.rd)) << 1;
	forwarding_unit.mem_id = temp;
	temp = preg_EX_r.cu.reg_write & (preg_EX_r.rd != 0);
	temp = ((temp & (preg_ID_r.rs1 == preg_EX_r.rd)) << 1)
		+ ((temp & (preg_ID_r.rs2 == preg_EX_r.rd)) << 2);
	forwarding_unit.ex_id = temp;
	if (forwarding_unit.wb_ex & 0x2)
		preg_EX_r.d1 = preg_WB_r.alu_res;
	if (forwarding_unit.wb_ex & 0x4)
		preg_EX_r.d2 = preg_WB_r.alu_res;
	if (forwarding_unit.mem_ex & 0x2)
		preg_EX_r.d1 = preg_MEM_r.alu_res;
	if (forwarding_unit.mem_ex & 0x4)
		preg_EX_r.d2 = preg_MEM_r.alu_res;
}

void forwarding_extended()
{
	if (forwarding_unit.mem_id & 0x2)
		preg_ID_w.d1 = preg_MEM_r.alu_res;
	if (forwarding_unit.ex_id & 0x2)
		preg_ID_w.d1 = preg_EX_w.alu_res;
	if (forwarding_unit.ex_id & 0x4)
		preg_ID_w.d2 = preg_EX_w.alu_res;
	if (preg_ID_w.cu.link)
	{
		preg_ID_w.d1 = preg_ID_r.pc + 4;
		preg_ID_w.imm = 0;
	}
	else
	{
		preg_ID_w.alu_zero = preg_ID_w.d1 == preg_ID_w.d2;
	}
}

void cycle_end()
{
	forwarding_extended();

	preg_ID_r = preg_IF_w;
	preg_EX_r = preg_ID_w;
	preg_MEM_r = preg_EX_w;
	preg_WB_r = preg_MEM_w;

	if ((preg_EX_r.alu_zero | preg_EX_r.cu.link) & (preg_EX_r.cu.branch))
	{
		pc = preg_EX_r.pc;
		preg_ID_r = (t_pipeline_reg){0,};
	}
	else
		pc += 4;

	forwarding();
	exit_pc = preg_WB_r.pc;
}

// implementation end

/* initialize all datapath elements
//fill the instruction and data memory
//reset the registers
*/
int init(char* filename)
{
	FILE* fp = fopen(filename, "r");
	int i;
	int	inst;

	if (fp == NULL)
	{
		fprintf(stderr, "Error opening file.\n");
		return -1;
	}

	/* fill instruction memory */
	i = 0;
	while (fscanf(fp, "%x", &inst) == 1)
	{
		inst_mem[i++] = inst;
	}


	/*reset the registers*/
	for (i = 0; i<32; i++)
	{
		regs[i] = 0;
	}

	/*reset pc*/
	pc = 0;
    /*reset clock cycles*/
    cycles=0;

	fclose(fp);
    return 0;
}

void print_cycles()
{
	printf("---------------------------------------------------\n");

	printf("Clock cycles = %ld\n", cycles);
}

void print_pc()
{
	printf("PC	   = %ld\n\n", pc);
}

void print_reg()
{
	printf("x0   = %lu\n", regs[0]);
	printf("x1   = %lu\n", regs[1]);
	printf("x2   = %lu\n", regs[2]);
	printf("x3   = %lu\n", regs[3]);
	printf("x4   = %lu\n", regs[4]);
	printf("x5   = %lu\n", regs[5]);
	printf("x6   = %lu\n", regs[6]);
	printf("x7   = %lu\n", regs[7]);
	printf("x8   = %lu\n", regs[8]);
	printf("x9   = %lu\n", regs[9]);
	printf("x10  = %lu\n", regs[10]);
	printf("x11  = %lu\n", regs[11]);
	printf("x12  = %lu\n", regs[12]);
	printf("x13  = %lu\n", regs[13]);
	printf("x14  = %lu\n", regs[14]);
	printf("x15  = %lu\n", regs[15]);
	printf("x16  = %lu\n", regs[16]);
	printf("x17  = %lu\n", regs[17]);
	printf("x18  = %lu\n", regs[18]);
	printf("x19  = %lu\n", regs[19]);
	printf("x20  = %lu\n", regs[20]);
	printf("x21  = %lu\n", regs[21]);
	printf("x22  = %lu\n", regs[22]);
	printf("x23  = %lu\n", regs[23]);
	printf("x24  = %lu\n", regs[24]);
	printf("x25  = %lu\n", regs[25]);
	printf("x26  = %lu\n", regs[26]);
	printf("x27  = %lu\n", regs[27]);
	printf("x28  = %lu\n", regs[28]);
	printf("x29  = %lu\n", regs[29]);
	printf("x30  = %lu\n", regs[30]);
	printf("x31  = %lu\n", regs[31]);
	printf("\n");
}
