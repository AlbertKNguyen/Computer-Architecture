#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include "computer.h"
#include <string.h>
#undef mips			/* gcc already has a def for mips */

unsigned int endianSwap(unsigned int);

void PrintInfo (int changedReg, int changedMem);
unsigned int Fetch (int);
void Decode (unsigned int, DecodedInstr*, RegVals*);
int Execute (DecodedInstr*, RegVals*);
int Mem(DecodedInstr*, int, int *);
void RegWrite(DecodedInstr*, int, int *);
void UpdatePC(DecodedInstr*, int);
void PrintInstruction (DecodedInstr*);

/*Globally accessible Computer variable*/
Computer mips;
RegVals rVals;

// Bits location of instruction fields
static const unsigned int opcodeBits = 0xfc000000;
static const unsigned int rsBits = 0x03e00000;
static const unsigned int rtBits = 0x001f0000;
static const unsigned int rdBits = 0x0000f800;
static const unsigned int shamtBits = 0x000007c0;
static const unsigned int functBits = 0x0000003f;
static const unsigned int immediateBits = 0x0000ffff;
static const unsigned int addressBits = 0x03ffffff;

// Shift amount to extract instruction fields
static const unsigned int opcodeShift = 26;
static const unsigned int rsShift = 21;
static const unsigned int rtShift= 16;
static const unsigned int rdShift = 11;
static const unsigned int shamtShift = 6;

/*
 *  Return an initialized computer with the stack pointer set to the
 *  address of the end of data memory, the remaining registers initialized
 *  to zero, and the instructions read from the given file.
 *  The other arguments govern how the program interacts with the user.
 */
void InitComputer (FILE* filein, int printingRegisters, int printingMemory,
  int debugging, int interactive) {
    int k;
    unsigned int instr;

    /* Initialize registers and memory */

    for (k=0; k<32; k++) {
        mips.registers[k] = 0;
    }
    
    /* stack pointer - Initialize to highest address of data segment */
    mips.registers[29] = 0x00400000 + (MAXNUMINSTRS+MAXNUMDATA)*4;

    for (k=0; k<MAXNUMINSTRS+MAXNUMDATA; k++) {
        mips.memory[k] = 0;
    }

    k = 0;
    while (fread(&instr, 4, 1, filein)) {
	/*swap to big endian, convert to host byte order. Ignore this.*/
        mips.memory[k] = ntohl(endianSwap(instr));
        k++;
        if (k>MAXNUMINSTRS) {
            fprintf (stderr, "Program too big.\n");
            exit (1);
        }
    }

    mips.printingRegisters = printingRegisters;
    mips.printingMemory = printingMemory;
    mips.interactive = interactive;
    mips.debugging = debugging;
}

unsigned int endianSwap(unsigned int i) {
    return (i>>24)|(i>>8&0x0000ff00)|(i<<8&0x00ff0000)|(i<<24);
}

/*
 *  Run the simulation.
 */
void Simulate () {
    char s[40];  /* used for handling interactive input */
    unsigned int instr;
    int changedReg=-1, changedMem=-1, val;
    DecodedInstr d;
    
    /* Initialize the PC to the start of the code section */
    mips.pc = 0x00400000;
    while (1) {
        if (mips.interactive) {
            printf ("> ");
            fgets (s,sizeof(s),stdin);
            if (s[0] == 'q') {
                return;
            }
        }

        /* Fetch instr at mips.pc, returning it in instr */
        instr = Fetch (mips.pc);

        printf ("Executing instruction at %8.8x: %8.8x\n", mips.pc, instr);

        /* 
	 * Decode instr, putting decoded instr in d
	 * Note that we reuse the d struct for each instruction.
	 */
        Decode (instr, &d, &rVals);

        /*Print decoded instruction*/
        PrintInstruction(&d);

        /* 
	 * Perform computation needed to execute d, returning computed value 
	 * in val 
	 */
        val = Execute(&d, &rVals);

	    UpdatePC(&d,val);
        /* 
	 * Perform memory load or store. Place the
	 * address of any updated memory in *changedMem, 
	 * otherwise put -1 in *changedMem. 
	 * Return any memory value that is read, otherwise return -1.
         */
        val = Mem(&d, val, &changedMem);
        
        /* 
	 * Write back to register. If the instruction modified a register--
	 * (including jal, which modifies $ra) --
         * put the index of the modified register in *changedReg,
         * otherwise put -1 in *changedReg.
         */
        RegWrite(&d, val, &changedReg);

        PrintInfo (changedReg, changedMem);
    }
}

/*
 *  Print relevant information about the state of the computer.
 *  changedReg is the index of the register changed by the instruction
 *  being simulated, otherwise -1.
 *  changedMem is the address of the memory location changed by the
 *  simulated instruction, otherwise -1.
 *  Previously initialized flags indicate whether to print all the
 *  registers or just the one that changed, and whether to print
 *  all the nonzero memory or just the memory location that changed.
 */
void PrintInfo ( int changedReg, int changedMem) {
    int k, addr;
    printf ("New pc = %8.8x\n", mips.pc);
    if (!mips.printingRegisters && changedReg == -1) {
        printf ("No register was updated.\n");
    } else if (!mips.printingRegisters) {
        printf ("Updated r%2.2d to %8.8x\n",
        changedReg, mips.registers[changedReg]);
    } else {
        for (k=0; k<32; k++) {
            printf ("r%2.2d: %8.8x  ", k, mips.registers[k]);
            if ((k+1)%4 == 0) {
                printf ("\n");
            }
        }
    }
    if (!mips.printingMemory && changedMem == -1) {
        printf ("No memory location was updated.\n");
    } else if (!mips.printingMemory) {
        printf ("Updated memory at address %8.8x to %8.8x\n",
        changedMem, Fetch (changedMem));
    } else {
        printf ("Nonzero memory\n");
        printf ("ADDR	  CONTENTS\n");
        for (addr = 0x00400000+4*MAXNUMINSTRS;
             addr < 0x00400000+4*(MAXNUMINSTRS+MAXNUMDATA);
             addr = addr+4) {
            if (Fetch (addr) != 0) {
                printf ("%8.8x  %8.8x\n", addr, Fetch (addr));
            }
        }
    }
}

/*
 *  Return the contents of memory at the given address. Simulates
 *  instruction fetch. 
 */
unsigned int Fetch ( int addr) {
    return mips.memory[(addr-0x00400000)/4];
}

/* Decode instr, returning decoded instruction. */
void Decode ( unsigned int instr, DecodedInstr* d, RegVals* rVals) {
    // Get opcode
    d->op = (instr & opcodeBits) >> opcodeShift;
    // Determine instruction type and fill RegVals struct and corresponding Regs struct
    if (d->op == 0) {
        d->type = R;
        // Fill RRegs struct
        d->regs.r.rs = (instr & rsBits) >> rsShift;
        d->regs.r.rt = (instr & rtBits) >> rtShift;
        d->regs.r.rd = (instr & rdBits) >> rdShift;
        d->regs.r.shamt = (instr & shamtBits) >> shamtShift;
        d->regs.r.funct = instr & functBits;

        // Fill RegVals struct with register reads
        rVals->R_rs = mips.registers[d->regs.r.rs];
        rVals->R_rt = mips.registers[d->regs.r.rt];
        rVals->R_rd = mips.registers[d->regs.r.rd];
    } else if (d->op == 2 || d->op == 3) {
        d->type = J;
        // Fill JRegs struct/calculate target address
        d->regs.j.target = ((instr & addressBits) << 2) | (mips.pc & 0xf0000000);
    } else if (d->op == 4 || d->op == 5 || d->op == 8 || d->op == 9 || d->op == 12 || d->op == 13 || d->op == 15 || d->op == 35 || d->op == 43) {
        d->type = I;
        // Fill IRegs struct
        d->regs.i.rs = (instr & rsBits) >> rsShift;
        d->regs.i.rt = (instr & rtBits) >> rtShift;
        d->regs.i.addr_or_immed = instr & immediateBits; // zero extend
        if (d->op == 4 || d->op == 5) { // bne/beq shift immediate left by 2 + PC + 4 for exact PC address for branch jump
            d->regs.i.addr_or_immed = (d->regs.i.addr_or_immed << 2) + mips.pc + 4;
        } else if (d->regs.i.addr_or_immed & 0x00008000 && d->op != 12 && d->op != 13) { // negative integer. dont do for andi and ori
            d->regs.i.addr_or_immed =  d->regs.i.addr_or_immed | 0xffff0000; // sign extend
	    }
        // Fill RegVals struct with register reads
        rVals->R_rs = mips.registers[d->regs.r.rs];
        rVals->R_rt = mips.registers[d->regs.r.rt];
    } else {
        exit(0);
    }
}

/*
 *  Print the disassembled version of the given instruction
 *  followed by a newline.
 */
void PrintInstruction ( DecodedInstr* d) {
    if (d->type == R) {
        switch (d->regs.r.funct) {
            // sll
            case 0:
                printf("sll\t$%d, $%d, %d\n", d->regs.r.rd, d->regs.r.rt, d->regs.r.shamt);
                break;
            // srl
            case 2:
                printf("srl\t$%d, $%d, %d\n", d->regs.r.rd, d->regs.r.rt, d->regs.r.shamt);
                break;
            // jr
            case 8:
                printf("jr\t$%d\n", d->regs.r.rs);
                break;
            // addu
            case 33:
                printf("addu\t$%d, $%d, $%d\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.rt);
                break;
            // subu
            case 35:
                printf("subu\t$%d, $%d, $%d\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.rt);
                break;
            // and
            case 36:
                printf("and\t$%d, $%d, $%d\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.rt);
                break;
            // or
            case 37:
                printf("or\t$%d, $%d, $%d\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.rt);
                break;
            // slt
            case 42:
                printf("slt\t$%d, $%d, $%d\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.rt);
                break;
            default:
                exit(0);
        }
    } else if (d->type == I) {
        switch (d->op) {
            // beq
            case 4:
                printf("beq\t$%d, $%d, 0x%8.8x\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
                break;
            // bne
            case 5:
                printf("bne\t$%d, $%d, 0x%8.8x\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
                break;
            // addiu
            case 9:
                printf("addiu\t$%d, $%d, %d\n", d->regs.i.rt, d->regs.i.rs, d->regs.i.addr_or_immed);
                break;
            // andi
            case 12:
                printf("andi\t$%d, $%d, 0x%x\n", d->regs.i.rt, d->regs.i.rs, d->regs.i.addr_or_immed);
                break;
            // ori
            case 13:
                printf("ori\t$%d, $%d, 0x%x\n", d->regs.i.rt, d->regs.i.rs, d->regs.i.addr_or_immed);
                break;
            // lui
            case 15:
                printf("lui\t$%d, 0x%x\n", d->regs.i.rt, d->regs.i.addr_or_immed);
                break;
            // lw    
            case 35:
                printf("lw\t$%d, %d($%d)\n", d->regs.i.rt, d->regs.i.addr_or_immed, d->regs.i.rs);
                break;
            // sw
            case 43:
                printf("sw\t$%d, %d($%d)\n", d->regs.i.rt, d->regs.i.addr_or_immed, d->regs.i.rs);
                break;
            default:
                exit(0);
        } 
    } else {
        // j
        if (d->op == 2) {
            printf("j\t0x%8.8x\n", d->regs.j.target);
        } else if (d->op == 3) { // jal
            printf("jal\t0x%8.8x\n", d->regs.j.target);
        } else {
            exit(0);
        }
    }
}

/* Perform computation needed to execute d, returning computed value */
int Execute ( DecodedInstr* d, RegVals* rVals) {
    if (d->type == R) {
        switch (d->regs.r.funct) {
            // sll
            case 0:
                return (unsigned int)rVals->R_rt << d->regs.r.shamt;
            // srl
            case 2:
                return (unsigned int)rVals->R_rt >> d->regs.r.shamt;
            // addu
            case 33:
                return (unsigned int)rVals->R_rs + (unsigned int)rVals->R_rt;
            // subu
            case 35:
                return (unsigned int)rVals->R_rs - (unsigned int)rVals->R_rt;
            // and
            case 36:
                return rVals->R_rs & rVals->R_rt;
            // or
            case 37:
                return rVals->R_rs | rVals->R_rt;
            // slt
            case 42:
                return rVals->R_rs < rVals->R_rt;
            default:
                return 0;
        }
    } else if (d->type == I) {
        switch (d->op) {
            // beq
            case 4:
                return rVals->R_rs == rVals->R_rt;
            // bne
            case 5:
		        return rVals->R_rs != rVals->R_rt;
            // addiu
            case 9:
                return rVals->R_rs + d->regs.i.addr_or_immed; 
            // andi
            case 12:
                return rVals->R_rs & d->regs.i.addr_or_immed;
            // ori
            case 13:
                return rVals->R_rs | d->regs.i.addr_or_immed;
            // lui
            case 15:
                return d->regs.i.addr_or_immed << 16;
            // lw    
            case 35:
            // sw
            case 43:
                return rVals->R_rs + d->regs.i.addr_or_immed;
            default:
                return 0;
        } 
    } else {
        // jal
        if (d->op == 3) { 
            return mips.pc + 4;
        } else {
            return 0;
        }
    } 
    return 0;
}

/* 
 * Update the program counter based on the current instruction. For
 * instructions other than branches and jumps, for example, the PC
 * increments by 4 (which we have provided).
 */
void UpdatePC ( DecodedInstr* d, int val) {
    // j/jal
    if (d->op == 2 || d->op == 3) {
        mips.pc = d->regs.j.target;
    } else if ((d->op == 4 || d->op == 5) && val == 1) { // beq/bne
        mips.pc = d->regs.i.addr_or_immed;
    } else if (d->regs.r.funct== 8) { // jr
        mips.pc = mips.registers[31];
    } else {
        mips.pc += 4;
    }
}

/*
 * Perform memory load or store. Place the address of any updated memory 
 * in *changedMem, otherwise put -1 in *changedMem. Return any memory value 
 * that is read, otherwise return -1. 
 *
 * Remember that we're mapping MIPS addresses to indices in the mips.memory 
 * array. mips.memory[0] corresponds with address 0x00400000, mips.memory[1] 
 * with address 0x00400004, and so forth.
 *
 */
int Mem( DecodedInstr* d, int val, int *changedMem) {
    if (d->type == I) { 
        // lw
        if (d->op == 35) {
            if (val >= 0x00401000 && val < 0x00404000 && val % 4 == 0) {
                *changedMem = -1;
                return mips.memory[(val - 0x00400000) / 4];
            } else {
                printf("Memory Access Exception at 0x%8.8x: address 0x%8.8x\n", mips.pc, val);
                exit(0);
            }
        } else if (d->op == 43){ // sw
            if (val >= 0x00401000 && val < 0x00404000 && val % 4 == 0) {
                mips.memory[(val - 0x00400000) / 4] = mips.registers[d->regs.r.rt];
                *changedMem = val;
                return -1;
            } else {
                printf("Memory Access Exception at 0x%8.8x: address 0x%8.8x\n", mips.pc, val);
                exit(0);
            }
        } else {
            *changedMem = -1;
            return val;
        }
    } else {
        *changedMem = -1;
        return val;
    }
}

/* 
 * Write back to register. If the instruction modified a register--
 * (including jal, which modifies $ra) --
 * put the index of the modified register in *changedReg,
 * otherwise put -1 in *changedReg.
 */
void RegWrite( DecodedInstr* d, int val, int *changedReg) {
    if (d->type == R) {
        // sll/srl/addu/subu/and/or/slt
        if (d->regs.r.funct != 8) {
            mips.registers[d->regs.r.rd] = val;
            *changedReg = d->regs.r.rd;
        } else { // jr
            *changedReg = -1;
        }
    } else if (d->type == I) {
        switch (d->op) {
            // addiu
            case 9:
            // andi
            case 12:
            // ori
            case 13:
            // lui
            case 15:
            // lw
            case 35:
                mips.registers[d->regs.r.rt] = val;
                *changedReg = d->regs.r.rt;
                break;
            default:
                *changedReg = -1;
        } 
    } else { 
        // jal
        if (d->op == 3) {
            mips.registers[31] = val;
            *changedReg = 31;
        } else {
            *changedReg = -1;
        }
    }
}
