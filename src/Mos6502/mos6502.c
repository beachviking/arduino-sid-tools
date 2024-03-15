/*
6502 emulation adaptations made by beachviking 02/25/2024
Based on original works from SIDDump, author and copyright details provided below.
_______________________
SIDDump V1.08
by Lasse Oorni (loorni@gmail.com) and Stein Pedersen

Version history:

V1.0    - Original
V1.01   - Fixed BIT instruction
V1.02   - Added incomplete illegal opcode support, enough for John Player
V1.03   - Some CPU bugs fixed
V1.04   - Improved command line handling, added illegal NOP instructions, fixed
          illegal LAX to work again
V1.05   - Partial support for multispeed tunes
V1.06   - Added CPU cycle profiling functionality by Stein Pedersen
V1.07   - Support rudimentary line counting for SID detection routines
V1.08   - CPU bugfixes

Copyright (C) 2005-2020 by the authors. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.
*/

#include <stdio.h>
#include <stdlib.h>
#include "mos6502.h"

#define V1 1    // works pretty well, no support for undocumented opcodes though...
// #define V2 1 // does not work well... Usage not recommended.
// #define V3 1 // does not work well... Usage not recommended.

#ifdef V1
#define FN 0x80
#define FV 0x40
#define FB 0x10
#define FD 0x08
#define FI 0x04
#define FZ 0x02
#define FC 0x01

#define MEM(address) (mem[address])
#define LO() (MEM(pc))
#define HI() (MEM(pc+1))
#define FETCH() (MEM(pc++))
#define SETpc(newpc) (pc = (newpc))
#define PUSH(data) (MEM(0x100 + (sp--)) = (data))
#define POP() (MEM(0x100 + (++sp)))

#define IMMEDIATE() (LO())
#define ABSOLUTE() (LO() | (HI() << 8))
#define ABSOLUTEX() (((LO() | (HI() << 8)) + x) & 0xffff)
#define ABSOLUTEY() (((LO() | (HI() << 8)) + y) & 0xffff)
#define ZEROPAGE() (LO() & 0xff)
#define ZEROPAGEX() ((LO() + x) & 0xff)
#define ZEROPAGEY() ((LO() + y) & 0xff)
#define INDIRECTX() (MEM((LO() + x) & 0xff) | (MEM((LO() + x + 1) & 0xff) << 8))
#define INDIRECTY() (((MEM(LO()) | (MEM((LO() + 1) & 0xff) << 8)) + y) & 0xffff)
#define INDIRECTZP() (((MEM(LO()) | (MEM((LO() + 1) & 0xff) << 8)) + 0) & 0xffff)

#define WRITE(address)                  \
{                                       \
  /* cpuwritemap[(address) >> 6] = 1; */  \
}

#define EVALPAGECROSSING(baseaddr, realaddr) ((((baseaddr) ^ (realaddr)) & 0xff00) ? 1 : 0)
#define EVALPAGECROSSING_ABSOLUTEX() (EVALPAGECROSSING(ABSOLUTE(), ABSOLUTEX()))
#define EVALPAGECROSSING_ABSOLUTEY() (EVALPAGECROSSING(ABSOLUTE(), ABSOLUTEY()))
#define EVALPAGECROSSING_INDIRECTY() (EVALPAGECROSSING(INDIRECTZP(), INDIRECTY()))

#define BRANCH()                                          \
{                                                         \
  ++cpucycles;                                            \
  temp = FETCH();                                         \
  if (temp < 0x80)                                        \
  {                                                       \
    cpucycles += EVALPAGECROSSING(pc, pc + temp);         \
    SETpc(pc + temp);                                     \
  }                                                       \
  else                                                    \
  {                                                       \
    cpucycles += EVALPAGECROSSING(pc, pc + temp - 0x100); \
    SETpc(pc + temp - 0x100);                             \
  }                                                       \
}

#define SETFLAGS(data)                  \
{                                       \
  if (!(data))                          \
    flags = (flags & ~FN) | FZ;         \
  else                                  \
    flags = (flags & ~(FN|FZ)) |        \
    ((data) & FN);                      \
}

#define ASSIGNSETFLAGS(dest, data)      \
{                                       \
  dest = data;                          \
  if (!dest)                            \
    flags = (flags & ~FN) | FZ;         \
  else                                  \
    flags = (flags & ~(FN|FZ)) |        \
    (dest & FN);                        \
}

#define ADC(data)                                                        \
{                                                                        \
    unsigned tempval = data;                                             \
                                                                         \
    if (flags & FD)                                                      \
    {                                                                    \
        temp = (a & 0xf) + (tempval & 0xf) + (flags & FC);               \
        if (temp > 0x9)                                                  \
            temp += 0x6;                                                 \
        if (temp <= 0x0f)                                                \
            temp = (temp & 0xf) + (a & 0xf0) + (tempval & 0xf0);         \
        else                                                             \
            temp = (temp & 0xf) + (a & 0xf0) + (tempval & 0xf0) + 0x10;  \
        if (!((a + tempval + (flags & FC)) & 0xff))                      \
            flags |= FZ;                                                 \
        else                                                             \
            flags &= ~FZ;                                                \
        if (temp & 0x80)                                                 \
            flags |= FN;                                                 \
        else                                                             \
            flags &= ~FN;                                                \
        if (((a ^ temp) & 0x80) && !((a ^ tempval) & 0x80))              \
            flags |= FV;                                                 \
        else                                                             \
            flags &= ~FV;                                                \
        if ((temp & 0x1f0) > 0x90) temp += 0x60;                         \
        if ((temp & 0xff0) > 0xf0)                                       \
            flags |= FC;                                                 \
        else                                                             \
            flags &= ~FC;                                                \
    }                                                                    \
    else                                                                 \
    {                                                                    \
        temp = tempval + a + (flags & FC);                               \
        SETFLAGS(temp & 0xff);                                           \
        if (!((a ^ tempval) & 0x80) && ((a ^ temp) & 0x80))              \
            flags |= FV;                                                 \
        else                                                             \
            flags &= ~FV;                                                \
        if (temp > 0xff)                                                 \
            flags |= FC;                                                 \
        else                                                             \
            flags &= ~FC;                                                \
    }                                                                    \
    a = temp;                                                            \
}

#define SBC(data)                                                        \
{                                                                        \
    unsigned tempval = data;                                             \
    temp = a - tempval - ((flags & FC) ^ FC);                            \
                                                                         \
    if (flags & FD)                                                      \
    {                                                                    \
        unsigned tempval2;                                               \
        tempval2 = (a & 0xf) - (tempval & 0xf) - ((flags & FC) ^ FC);    \
        if (tempval2 & 0x10)                                             \
            tempval2 = ((tempval2 - 6) & 0xf) | ((a & 0xf0) - (tempval   \
            & 0xf0) - 0x10);                                             \
        else                                                             \
            tempval2 = (tempval2 & 0xf) | ((a & 0xf0) - (tempval         \
            & 0xf0));                                                    \
        if (tempval2 & 0x100)                                            \
            tempval2 -= 0x60;                                            \
        if (temp < 0x100)                                                \
            flags |= FC;                                                 \
        else                                                             \
            flags &= ~FC;                                                \
        SETFLAGS(temp & 0xff);                                           \
        if (((a ^ temp) & 0x80) && ((a ^ tempval) & 0x80))               \
            flags |= FV;                                                 \
        else                                                             \
            flags &= ~FV;                                                \
        a = tempval2;                                                    \
    }                                                                    \
    else                                                                 \
    {                                                                    \
        SETFLAGS(temp & 0xff);                                           \
        if (temp < 0x100)                                                \
            flags |= FC;                                                 \
        else                                                             \
            flags &= ~FC;                                                \
        if (((a ^ temp) & 0x80) && ((a ^ tempval) & 0x80))               \
            flags |= FV;                                                 \
        else                                                             \
            flags &= ~FV;                                                \
        a = temp;                                                        \
    }                                                                    \
}

#define CMP(src, data)                  \
{                                       \
  temp = (src - data) & 0xff;           \
                                        \
  flags = (flags & ~(FC|FN|FZ)) |       \
          (temp & FN);                  \
                                        \
  if (!temp) flags |= FZ;               \
  if (src >= data) flags |= FC;         \
}

#define ASL(data)                       \
{                                       \
  temp = data;                          \
  temp <<= 1;                           \
  if (temp & 0x100) flags |= FC;        \
  else flags &= ~FC;                    \
  ASSIGNSETFLAGS(data, temp);           \
}

#define LSR(data)                       \
{                                       \
  temp = data;                          \
  if (temp & 1) flags |= FC;            \
  else flags &= ~FC;                    \
  temp >>= 1;                           \
  ASSIGNSETFLAGS(data, temp);           \
}

#define ROL(data)                       \
{                                       \
  temp = data;                          \
  temp <<= 1;                           \
  if (flags & FC) temp |= 1;            \
  if (temp & 0x100) flags |= FC;        \
  else flags &= ~FC;                    \
  ASSIGNSETFLAGS(data, temp);           \
}

#define ROR(data)                       \
{                                       \
  temp = data;                          \
  if (flags & FC) temp |= 0x100;        \
  if (temp & 1) flags |= FC;            \
  else flags &= ~FC;                    \
  temp >>= 1;                           \
  ASSIGNSETFLAGS(data, temp);           \
}

#define DEC(data)                       \
{                                       \
  temp = data - 1;                      \
  ASSIGNSETFLAGS(data, temp);           \
}

#define INC(data)                       \
{                                       \
  temp = data + 1;                      \
  ASSIGNSETFLAGS(data, temp);           \
}

#define EOR(data)                       \
{                                       \
  a ^= data;                            \
  SETFLAGS(a);                          \
}

#define ORA(data)                       \
{                                       \
  a |= data;                            \
  SETFLAGS(a);                          \
}

#define AND(data)                       \
{                                       \
  a &= data;                            \
  SETFLAGS(a)                           \
}

#define BIT(data)                       \
{                                       \
  flags = (flags & ~(FN|FV)) |          \
          (data & (FN|FV));             \
  if (!(data & a)) flags |= FZ;         \
  else flags &= ~FZ;                    \
}

void initcpu(unsigned short newpc, unsigned char newa, unsigned char newx, unsigned char newy);
int runcpu(void);
// void setpc(unsigned short newpc);

unsigned int pc;
unsigned char a;
unsigned char x;
unsigned char y;
unsigned char flags;
unsigned char sp;
unsigned char mem[0x10000];
unsigned int cpucycles;

static const int cpucycles_table[] = 
{
  7,  6,  0,  8,  3,  3,  5,  5,  3,  2,  2,  2,  4,  4,  6,  6, 
  2,  5,  0,  8,  4,  4,  6,  6,  2,  4,  2,  7,  4,  4,  7,  7, 
  6,  6,  0,  8,  3,  3,  5,  5,  4,  2,  2,  2,  4,  4,  6,  6, 
  2,  5,  0,  8,  4,  4,  6,  6,  2,  4,  2,  7,  4,  4,  7,  7, 
  6,  6,  0,  8,  3,  3,  5,  5,  3,  2,  2,  2,  3,  4,  6,  6, 
  2,  5,  0,  8,  4,  4,  6,  6,  2,  4,  2,  7,  4,  4,  7,  7, 
  6,  6,  0,  8,  3,  3,  5,  5,  4,  2,  2,  2,  5,  4,  6,  6, 
  2,  5,  0,  8,  4,  4,  6,  6,  2,  4,  2,  7,  4,  4,  7,  7, 
  2,  6,  2,  6,  3,  3,  3,  3,  2,  2,  2,  2,  4,  4,  4,  4, 
  2,  6,  0,  6,  4,  4,  4,  4,  2,  5,  2,  5,  5,  5,  5,  5, 
  2,  6,  2,  6,  3,  3,  3,  3,  2,  2,  2,  2,  4,  4,  4,  4, 
  2,  5,  0,  5,  4,  4,  4,  4,  2,  4,  2,  4,  4,  4,  4,  4, 
  2,  6,  2,  8,  3,  3,  5,  5,  2,  2,  2,  2,  4,  4,  6,  6, 
  2,  5,  0,  8,  4,  4,  6,  6,  2,  4,  2,  7,  4,  4,  7,  7, 
  2,  6,  2,  8,  3,  3,  5,  5,  2,  2,  2,  2,  4,  4,  6,  6, 
  2,  5,  0,  8,  4,  4,  6,  6,  2,  4,  2,  7,  4,  4,  7,  7
};

void initcpu(unsigned short newpc, unsigned char newa, unsigned char newx, unsigned char newy)
{
  pc = newpc;
  a = newa;
  x = newx;
  y = newy;
  flags = 0;
  sp = 0xff;
  cpucycles = 0;
}

int runcpu(void)
{
  unsigned temp;

  unsigned char op = FETCH();
  /* printf("pc: %04x OP: %02x A:%02x X:%02x Y:%02x\n", pc-1, op, a, x, y); */
  cpucycles += cpucycles_table[op];
  switch(op)
  {
    case 0xa7:
    ASSIGNSETFLAGS(a, MEM(ZEROPAGE()));
    x = a;
    pc++;
    break;

    case 0xb7:
    ASSIGNSETFLAGS(a, MEM(ZEROPAGEY()));
    x = a;
    pc++;
    break;

    case 0xaf:
    ASSIGNSETFLAGS(a, MEM(ABSOLUTE()));
    x = a;
    pc += 2;
    break;

    case 0xa3:
    ASSIGNSETFLAGS(a, MEM(INDIRECTX()));
    x = a;
    pc++;
    break;

    case 0xb3:
    cpucycles += EVALPAGECROSSING_INDIRECTY();
    ASSIGNSETFLAGS(a, MEM(INDIRECTY()));
    x = a;
    pc++;
    break;
    
    case 0x1a:
    case 0x3a:
    case 0x5a:
    case 0x7a:
    case 0xda:
    case 0xfa:
    break;
    
    case 0x80:
    case 0x82:
    case 0x89:
    case 0xc2:
    case 0xe2:
    case 0x04:
    case 0x44:
    case 0x64:
    case 0x14:
    case 0x34:
    case 0x54:
    case 0x74:
    case 0xd4:
    case 0xf4:
    pc++;
    break;
    
    case 0x0c:
    case 0x1c:
    case 0x3c:
    case 0x5c:
    case 0x7c:
    case 0xdc:
    case 0xfc:
    cpucycles += EVALPAGECROSSING_ABSOLUTEX();
    pc += 2;
    break;

    case 0x69:
    ADC(IMMEDIATE());
    pc++;
    break;

    case 0x65:
    ADC(MEM(ZEROPAGE()));
    pc++;
    break;

    case 0x75:
    ADC(MEM(ZEROPAGEX()));
    pc++;
    break;

    case 0x6d:
    ADC(MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0x7d:
    cpucycles += EVALPAGECROSSING_ABSOLUTEX();
    ADC(MEM(ABSOLUTEX()));
     pc += 2;
    break;

    case 0x79:
    cpucycles += EVALPAGECROSSING_ABSOLUTEY();
    ADC(MEM(ABSOLUTEY()));
    pc += 2;
    break;

    case 0x61:
    ADC(MEM(INDIRECTX()));
    pc++;
    break;

    case 0x71:
    cpucycles += EVALPAGECROSSING_INDIRECTY();
    ADC(MEM(INDIRECTY()));
    pc++;
    break;

    case 0x29:
    AND(IMMEDIATE());
    pc++;
    break;

    case 0x25:
    AND(MEM(ZEROPAGE()));
    pc++;
    break;

    case 0x35:
    AND(MEM(ZEROPAGEX()));
    pc++;
    break;

    case 0x2d:
    AND(MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0x3d:
    cpucycles += EVALPAGECROSSING_ABSOLUTEX();
    AND(MEM(ABSOLUTEX()));
    pc += 2;
    break;

    case 0x39:
    cpucycles += EVALPAGECROSSING_ABSOLUTEY();
    AND(MEM(ABSOLUTEY()));
    pc += 2;
    break;

    case 0x21:
    AND(MEM(INDIRECTX()));
    pc++;
    break;

    case 0x31:
    cpucycles += EVALPAGECROSSING_INDIRECTY();
    AND(MEM(INDIRECTY()));
    pc++;
    break;

    case 0x0a:
    ASL(a);
    break;

    case 0x06:
    ASL(MEM(ZEROPAGE()));
    pc++;
    break;

    case 0x16:
    ASL(MEM(ZEROPAGEX()));
    pc++;
    break;

    case 0x0e:
    ASL(MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0x1e:
    ASL(MEM(ABSOLUTEX()));
    pc += 2;
    break;

    case 0x90:
    if (!(flags & FC)) BRANCH()
    else pc++;
    break;

    case 0xb0:
    if (flags & FC) BRANCH()
    else pc++;
    break;

    case 0xf0:
    if (flags & FZ) BRANCH()
    else pc++;
    break;

    case 0x24:
    BIT(MEM(ZEROPAGE()));
    pc++;
    break;

    case 0x2c:
    BIT(MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0x30:
    if (flags & FN) BRANCH()
    else pc++;
    break;

    case 0xd0:
    if (!(flags & FZ)) BRANCH()
    else pc++;
    break;

    case 0x10:
    if (!(flags & FN)) BRANCH()
    else pc++;
    break;

    case 0x50:
    if (!(flags & FV)) BRANCH()
    else pc++;
    break;

    case 0x70:
    if (flags & FV) BRANCH()
    else pc++;
    break;

    case 0x18:
    flags &= ~FC;
    break;

    case 0xd8:
    flags &= ~FD;
    break;

    case 0x58:
    flags &= ~FI;
    break;

    case 0xb8:
    flags &= ~FV;
    break;

    case 0xc9:
    CMP(a, IMMEDIATE());
    pc++;
    break;

    case 0xc5:
    CMP(a, MEM(ZEROPAGE()));
    pc++;
    break;

    case 0xd5:
    CMP(a, MEM(ZEROPAGEX()));
    pc++;
    break;

    case 0xcd:
    CMP(a, MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0xdd:
    cpucycles += EVALPAGECROSSING_ABSOLUTEX();
    CMP(a, MEM(ABSOLUTEX()));
    pc += 2;
    break;

    case 0xd9:
    cpucycles += EVALPAGECROSSING_ABSOLUTEY();
    CMP(a, MEM(ABSOLUTEY()));
    pc += 2;
    break;

    case 0xc1:
    CMP(a, MEM(INDIRECTX()));
    pc++;
    break;

    case 0xd1:
    cpucycles += EVALPAGECROSSING_INDIRECTY();
    CMP(a, MEM(INDIRECTY()));
    pc++;
    break;

    case 0xe0:
    CMP(x, IMMEDIATE());
    pc++;
    break;

    case 0xe4:
    CMP(x, MEM(ZEROPAGE()));
    pc++;
    break;

    case 0xec:
    CMP(x, MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0xc0:
    CMP(y, IMMEDIATE());
    pc++;
    break;

    case 0xc4:
    CMP(y, MEM(ZEROPAGE()));
    pc++;
    break;

    case 0xcc:
    CMP(y, MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0xc6:
    DEC(MEM(ZEROPAGE()));
    WRITE(ZEROPAGE());
    pc++;
    break;

    case 0xd6:
    DEC(MEM(ZEROPAGEX()));
    WRITE(ZEROPAGEX());
    pc++;
    break;

    case 0xce:
    DEC(MEM(ABSOLUTE()));
    WRITE(ABSOLUTE());
    pc += 2;
    break;

    case 0xde:
    DEC(MEM(ABSOLUTEX()));
    WRITE(ABSOLUTEX());
    pc += 2;
    break;

    case 0xca:
    x--;
    SETFLAGS(x);
    break;

    case 0x88:
    y--;
    SETFLAGS(y);
    break;

    case 0x49:
    EOR(IMMEDIATE());
    pc++;
    break;

    case 0x45:
    EOR(MEM(ZEROPAGE()));
    pc++;
    break;

    case 0x55:
    EOR(MEM(ZEROPAGEX()));
    pc++;
    break;

    case 0x4d:
    EOR(MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0x5d:
    cpucycles += EVALPAGECROSSING_ABSOLUTEX();
    EOR(MEM(ABSOLUTEX()));
    pc += 2;
    break;

    case 0x59:
    cpucycles += EVALPAGECROSSING_ABSOLUTEY();
    EOR(MEM(ABSOLUTEY()));
    pc += 2;
    break;

    case 0x41:
    EOR(MEM(INDIRECTX()));
    pc++;
    break;

    case 0x51:
    cpucycles += EVALPAGECROSSING_INDIRECTY();
    EOR(MEM(INDIRECTY()));
    pc++;
    break;

    case 0xe6:
    INC(MEM(ZEROPAGE()));
    WRITE(ZEROPAGE());
    pc++;
    break;

    case 0xf6:
    INC(MEM(ZEROPAGEX()));
    WRITE(ZEROPAGEX());
    pc++;
    break;

    case 0xee:
    INC(MEM(ABSOLUTE()));
    WRITE(ABSOLUTE());
    pc += 2;
    break;

    case 0xfe:
    INC(MEM(ABSOLUTEX()));
    WRITE(ABSOLUTEX());
    pc += 2;
    break;

    case 0xe8:
    x++;
    SETFLAGS(x);
    break;

    case 0xc8:
    y++;
    SETFLAGS(y);
    break;

    case 0x20:
    PUSH((pc+1) >> 8);
    PUSH((pc+1) & 0xff);
    pc = ABSOLUTE();
    break;

    case 0x4c:
    pc = ABSOLUTE();
    break;

    case 0x6c:
    {
      unsigned short adr = ABSOLUTE();
      pc = (MEM(adr) | (MEM(((adr + 1) & 0xff) | (adr & 0xff00)) << 8));
    }
    break;

    case 0xa9:
    ASSIGNSETFLAGS(a, IMMEDIATE());
    pc++;
    break;

    case 0xa5:
    ASSIGNSETFLAGS(a, MEM(ZEROPAGE()));
    pc++;
    break;

    case 0xb5:
    ASSIGNSETFLAGS(a, MEM(ZEROPAGEX()));
    pc++;
    break;

    case 0xad:
    ASSIGNSETFLAGS(a, MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0xbd:
    cpucycles += EVALPAGECROSSING_ABSOLUTEX();
    ASSIGNSETFLAGS(a, MEM(ABSOLUTEX()));
    pc += 2;
    break;

    case 0xb9:
    cpucycles += EVALPAGECROSSING_ABSOLUTEY();
    ASSIGNSETFLAGS(a, MEM(ABSOLUTEY()));
    pc += 2;
    break;

    case 0xa1:
    ASSIGNSETFLAGS(a, MEM(INDIRECTX()));
    pc++;
    break;

    case 0xb1:
    cpucycles += EVALPAGECROSSING_INDIRECTY();
    ASSIGNSETFLAGS(a, MEM(INDIRECTY()));
    pc++;
    break;

    case 0xa2:
    ASSIGNSETFLAGS(x, IMMEDIATE());
    pc++;
    break;

    case 0xa6:
    ASSIGNSETFLAGS(x, MEM(ZEROPAGE()));
    pc++;
    break;

    case 0xb6:
    ASSIGNSETFLAGS(x, MEM(ZEROPAGEY()));
    pc++;
    break;

    case 0xae:
    ASSIGNSETFLAGS(x, MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0xbe:
    cpucycles += EVALPAGECROSSING_ABSOLUTEY();
    ASSIGNSETFLAGS(x, MEM(ABSOLUTEY()));
    pc += 2;
    break;

    case 0xa0:
    ASSIGNSETFLAGS(y, IMMEDIATE());
    pc++;
    break;

    case 0xa4:
    ASSIGNSETFLAGS(y, MEM(ZEROPAGE()));
    pc++;
    break;

    case 0xb4:
    ASSIGNSETFLAGS(y, MEM(ZEROPAGEX()));
    pc++;
    break;

    case 0xac:
    ASSIGNSETFLAGS(y, MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0xbc:
    cpucycles += EVALPAGECROSSING_ABSOLUTEX();
    ASSIGNSETFLAGS(y, MEM(ABSOLUTEX()));
    pc += 2;
    break;

    case 0x4a:
    LSR(a);
    break;

    case 0x46:
    LSR(MEM(ZEROPAGE()));
    WRITE(ZEROPAGE());
    pc++;
    break;

    case 0x56:
    LSR(MEM(ZEROPAGEX()));
    WRITE(ZEROPAGEX());
    pc++;
    break;

    case 0x4e:
    LSR(MEM(ABSOLUTE()));
    WRITE(ABSOLUTE());
    pc += 2;
    break;

    case 0x5e:
    LSR(MEM(ABSOLUTEX()));
    WRITE(ABSOLUTEX());
    pc += 2;
    break;

    case 0xea:
    break;

    case 0x09:
    ORA(IMMEDIATE());
    pc++;
    break;

    case 0x05:
    ORA(MEM(ZEROPAGE()));
    pc++;
    break;

    case 0x15:
    ORA(MEM(ZEROPAGEX()));
    pc++;
    break;

    case 0x0d:
    ORA(MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0x1d:
    cpucycles += EVALPAGECROSSING_ABSOLUTEX();
    ORA(MEM(ABSOLUTEX()));
    pc += 2;
    break;

    case 0x19:
    cpucycles += EVALPAGECROSSING_ABSOLUTEY();
    ORA(MEM(ABSOLUTEY()));
    pc += 2;
    break;

    case 0x01:
    ORA(MEM(INDIRECTX()));
    pc++;
    break;

    case 0x11:
    cpucycles += EVALPAGECROSSING_INDIRECTY();
    ORA(MEM(INDIRECTY()));
    pc++;
    break;

    case 0x48:
    PUSH(a);
    break;

    case 0x08:
    PUSH(flags | 0x30);
    break;

    case 0x68:
    ASSIGNSETFLAGS(a, POP());
    break;

    case 0x28:
    flags = POP();
    break;

    case 0x2a:
    ROL(a);
    break;

    case 0x26:
    ROL(MEM(ZEROPAGE()));
    WRITE(ZEROPAGE());
    pc++;
    break;

    case 0x36:
    ROL(MEM(ZEROPAGEX()));
    WRITE(ZEROPAGEX());
    pc++;
    break;

    case 0x2e:
    ROL(MEM(ABSOLUTE()));
    WRITE(ABSOLUTE());
    pc += 2;
    break;

    case 0x3e:
    ROL(MEM(ABSOLUTEX()));
    WRITE(ABSOLUTEX());
    pc += 2;
    break;

    case 0x6a:
    ROR(a);
    break;

    case 0x66:
    ROR(MEM(ZEROPAGE()));
    WRITE(ZEROPAGE());
    pc++;
    break;

    case 0x76:
    ROR(MEM(ZEROPAGEX()));
    WRITE(ZEROPAGEX());
    pc++;
    break;

    case 0x6e:
    ROR(MEM(ABSOLUTE()));
    WRITE(ABSOLUTE());
    pc += 2;
    break;

    case 0x7e:
    ROR(MEM(ABSOLUTEX()));
    WRITE(ABSOLUTEX());
    pc += 2;
    break;

    case 0x40:
    if (sp == 0xff) return 0;
    flags = POP();
    pc = POP();
    pc |= POP() << 8;
    break;

    case 0x60:
    if (sp == 0xff) return 0;
    pc = POP();
    pc |= POP() << 8;
    pc++;
    break;

    case 0xe9:
    case 0xeb:
    SBC(IMMEDIATE());
    pc++;
    break;

    case 0xe5:
    SBC(MEM(ZEROPAGE()));
    pc++;
    break;

    case 0xf5:
    SBC(MEM(ZEROPAGEX()));
    pc++;
    break;

    case 0xed:
    SBC(MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0xfd:
    cpucycles += EVALPAGECROSSING_ABSOLUTEX();
    SBC(MEM(ABSOLUTEX()));
    pc += 2;
    break;

    case 0xf9:
    cpucycles += EVALPAGECROSSING_ABSOLUTEY();
    SBC(MEM(ABSOLUTEY()));
    pc += 2;
    break;

    case 0xe1:
    SBC(MEM(INDIRECTX()));
    pc++;
    break;

    case 0xf1:
    cpucycles += EVALPAGECROSSING_INDIRECTY();
    SBC(MEM(INDIRECTY()));
    pc++;
    break;

    case 0x38:
    flags |= FC;
    break;

    case 0xf8:
    flags |= FD;
    break;

    case 0x78:
    flags |= FI;
    break;

    case 0x85:
    MEM(ZEROPAGE()) = a;
    WRITE(ZEROPAGE());
    pc++;
    break;

    case 0x95:
    MEM(ZEROPAGEX()) = a;
    WRITE(ZEROPAGEX());
    pc++;
    break;

    case 0x8d:
    MEM(ABSOLUTE()) = a;
    WRITE(ABSOLUTE());
    pc += 2;
    break;

    case 0x9d:
    MEM(ABSOLUTEX()) = a;
    WRITE(ABSOLUTEX());
    pc += 2;
    break;

    case 0x99:
    MEM(ABSOLUTEY()) = a;
    WRITE(ABSOLUTEY());
    pc += 2;
    break;

    case 0x81:
    MEM(INDIRECTX()) = a;
    WRITE(INDIRECTX());
    pc++;
    break;

    case 0x91:
    MEM(INDIRECTY()) = a;
    WRITE(INDIRECTY());
    pc++;
    break;

    case 0x86:
    MEM(ZEROPAGE()) = x;
    WRITE(ZEROPAGE());
    pc++;
    break;

    case 0x96:
    MEM(ZEROPAGEY()) = x;
    WRITE(ZEROPAGEY());
    pc++;
    break;

    case 0x8e:
    MEM(ABSOLUTE()) = x;
    WRITE(ABSOLUTE());
    pc += 2;
    break;

    case 0x84:
    MEM(ZEROPAGE()) = y;
    WRITE(ZEROPAGE());
    pc++;
    break;

    case 0x94:
    MEM(ZEROPAGEX()) = y;
    WRITE(ZEROPAGEX());
    pc++;
    break;

    case 0x8c:
    MEM(ABSOLUTE()) = y;
    WRITE(ABSOLUTE());
    pc += 2;
    break;

    case 0xaa:
    ASSIGNSETFLAGS(x, a);
    break;

    case 0xba:
    ASSIGNSETFLAGS(x, sp);
    break;

    case 0x8a:
    ASSIGNSETFLAGS(a, x);
    break;

    case 0x9a:
    sp = x;
    break;

    case 0x98:
    ASSIGNSETFLAGS(a, y);
    break;

    case 0xa8:
    ASSIGNSETFLAGS(y, a);
    break;

    case 0x00:
    return 0;

    case 0x02:
    printf("Error: CPU halt at %04X\n", pc-1);
    // exit(1);
    break;
          
    default:
    printf("Error: Unknown opcode $%02X at $%04X\n", op, pc-1);
    // exit(1);
    break;
  }
  return 1;
}

// void setpc(unsigned short newpc)
// {
//   pc = newpc;
// }

#endif

#ifdef V2
void initcpu(unsigned short newpc, unsigned char newa, unsigned char newx, unsigned char newy);
int runcpu(void);
// void setpc(unsigned short newpc);

// unsigned short pc;
// unsigned char a;
// unsigned char x;
// unsigned char y;
unsigned char flags;
// unsigned char sp;
unsigned char mem[0x10000];
// unsigned int cpucycles;

//CPU (and CIA/VIC-IRQ) emulation constants and variables - avoiding internal/automatic variables to retain speed
const uint8_t flagsw[]={0x01,0x21,0x04,0x24,0x00,0x40,0x08,0x28}, branchflag[]={0x80,0x40,0x01,0x02};

unsigned int pc=0, addr=0, storadd=0;

short int A=0, T=0, SP=0xFF; 

uint8_t X=0, Y=0, IR=0, ST=0x00;  //STATUS-flags: N V - B D I Z C
// uint8_t IR=0, ST=0x00;  //STATUS-flags: N V - B D I Z C

float CPUtime=0.0;
char cycles=0, finished=0, dynCIA=0;


static const int cpucycles_table[] = 
{
  7,  6,  0,  8,  3,  3,  5,  5,  3,  2,  2,  2,  4,  4,  6,  6, 
  2,  5,  0,  8,  4,  4,  6,  6,  2,  4,  2,  7,  4,  4,  7,  7, 
  6,  6,  0,  8,  3,  3,  5,  5,  4,  2,  2,  2,  4,  4,  6,  6, 
  2,  5,  0,  8,  4,  4,  6,  6,  2,  4,  2,  7,  4,  4,  7,  7, 
  6,  6,  0,  8,  3,  3,  5,  5,  3,  2,  2,  2,  3,  4,  6,  6, 
  2,  5,  0,  8,  4,  4,  6,  6,  2,  4,  2,  7,  4,  4,  7,  7, 
  6,  6,  0,  8,  3,  3,  5,  5,  4,  2,  2,  2,  5,  4,  6,  6, 
  2,  5,  0,  8,  4,  4,  6,  6,  2,  4,  2,  7,  4,  4,  7,  7, 
  2,  6,  2,  6,  3,  3,  3,  3,  2,  2,  2,  2,  4,  4,  4,  4, 
  2,  6,  0,  6,  4,  4,  4,  4,  2,  5,  2,  5,  5,  5,  5,  5, 
  2,  6,  2,  6,  3,  3,  3,  3,  2,  2,  2,  2,  4,  4,  4,  4, 
  2,  5,  0,  5,  4,  4,  4,  4,  2,  4,  2,  4,  4,  4,  4,  4, 
  2,  6,  2,  8,  3,  3,  5,  5,  2,  2,  2,  2,  4,  4,  6,  6, 
  2,  5,  0,  8,  4,  4,  6,  6,  2,  4,  2,  7,  4,  4,  7,  7, 
  2,  6,  2,  8,  3,  3,  5,  5,  2,  2,  2,  2,  4,  4,  6,  6, 
  2,  5,  0,  8,  4,  4,  6,  6,  2,  4,  2,  7,  4,  4,  7,  7
};

void initcpu(unsigned short newpc, unsigned char newa, unsigned char newx, unsigned char newy)
{
  pc = newpc;
  A = newa;
  X = newx;
  Y = newy;
  flags = 0;
  SP = 0xff;
  ST = 0;
  // cpucycles = 0;
  // cycles = 0;
}



// //  void initCPU (unsigned int mempos) { pc=mempos; A=0; X=0; Y=0; ST=0; SP=0xFF; } 

//  //My CPU implementation is based on the instruction table by Graham at codebase64.
//  //After some examination of the table it was clearly seen that columns of the table (instructions' 2nd nybbles)
//  // mainly correspond to addressing modes, and double-rows usually have the same instructions.
//  //The code below is laid out like this, with some exceptions present.
//  //Thanks to the hardware being in my mind when coding this, the illegal instructions could be added fairly easily...
//  byte CPU() //the CPU emulation for SID/PRG playback (ToDo: CIA/VIC-IRQ/NMI/RESET vectors, BCD-mode)
//  { //'IR' is the instruction-register, naming after the hardware-equivalent
int runcpu(void)
{
  IR=mem[pc]; cycles=2; storadd=0; //'cycle': ensure smallest 6510 runtime (for implied/register instructions)
  if(IR&1) {  //nybble2:  1/5/9/D:accu.instructions, 3/7/B/F:illegal opcodes
   switch (IR&0x1F) { //addressing modes (begin with more complex cases), pc wraparound not handled inside to save codespace
    case 1: case 3: pc++; addr = mem[mem[pc]+X] + mem[mem[pc]+X+1]*256; cycles=6; break; //(zp,x)
    case 0x11: case 0x13: pc++; addr = mem[mem[pc]] + mem[mem[pc]+1]*256 + Y; cycles=6; break; //(zp),y (5..6 cycles, 8 for R-M-W)
    case 0x19: case 0x1B: pc++; addr=mem[pc]; pc++; addr+=mem[pc]*256 + Y; cycles=5; break; //abs,y //(4..5 cycles, 7 cycles for R-M-W)
    case 0x1D: pc++; addr=mem[pc]; pc++; addr+=mem[pc]*256 + X; cycles=5; break; //abs,x //(4..5 cycles, 7 cycles for R-M-W)
    case 0xD: case 0xF: pc++; addr=mem[pc]; pc++; addr+=mem[pc]*256; cycles=4; break; //abs
    case 0x15: pc++; addr = mem[pc] + X; cycles=4; break; //zp,x
    case 5: case 7: pc++; addr = mem[pc]; cycles=3; break; //zp
    case 0x17: pc++; if ((IR&0xC0)!=0x80) { addr = mem[pc] + X; cycles=4; } //zp,x for illegal opcodes
               else { addr = mem[pc] + Y; cycles=4; }  break; //zp,y for LAX/SAX illegal opcodes
    case 0x1F: pc++; if ((IR&0xC0)!=0x80) { addr = mem[pc] + mem[++pc]*256 + X; cycles=5; } //abs,x for illegal opcodes
               else { addr = mem[pc] + mem[++pc]*256 + Y; cycles=5; }  break; //abs,y for LAX/SAX illegal opcodes
    case 9: case 0xB: pc++; addr = pc; cycles=2;  //immediate
   }
   addr&=0xFFFF;
   switch (IR&0xE0) {
    case 0x60: if ((IR&0x1F)!=0xB) { if((IR&3)==3) {T=(mem[addr]>>1)+(ST&1)*128; ST&=124; ST|=(T&1); mem[addr]=T; cycles+=2;}   //ADC / RRA (ROR+ADC)
                T=A; A+=mem[addr]+(ST&1); ST&=60; ST|=(A&128)|(A>255); A&=0xFF; ST |= (!A)<<1 | ( !((T^mem[addr])&0x80) & ((T^A)&0x80) ) >> 1; }
               else { A&=mem[addr]; T+=mem[addr]+(ST&1); ST&=60; ST |= (T>255) | ( !((A^mem[addr])&0x80) & ((T^A)&0x80) ) >> 1; //V-flag set by intermediate ADC mechanism: (A&mem)+mem
                T=A; A=(A>>1)+(ST&1)*128; ST|=(A&128)|(T>127); ST|=(!A)<<1; }  break; // ARR (AND+ROR, bit0 not going to C, but C and bit7 get exchanged.)
    case 0xE0: if((IR&3)==3 && (IR&0x1F)!=0xB) {mem[addr]++;cycles+=2;}  T=A; A-=mem[addr]+!(ST&1); //SBC / ISC(ISB)=INC+SBC
               ST&=60; ST|=(A&128)|(A>=0); A&=0xFF; ST |= (!A)<<1 | ( ((T^mem[addr])&0x80) & ((T^A)&0x80) ) >> 1; break; 
    case 0xC0: if((IR&0x1F)!=0xB) { if ((IR&3)==3) {mem[addr]--; cycles+=2;}  T=A-mem[addr]; } // CMP / DCP(DEC+CMP)
               else {X=T=(A&X)-mem[addr];} /*SBX(AXS)*/  ST&=124;ST|=(!(T&0xFF))<<1|(T&128)|(T>=0);  break;  //SBX (AXS) (CMP+DEX at the same time)
    case 0x00: if ((IR&0x1F)!=0xB) { if ((IR&3)==3) {ST&=124; ST|=(mem[addr]>127); mem[addr]<<=1; cycles+=2;}  
                A|=mem[addr]; ST&=125;ST|=(!A)<<1|(A&128); } //ORA / SLO(ASO)=ASL+ORA
               else {A&=mem[addr]; ST&=124;ST|=(!A)<<1|(A&128)|(A>127);}  break; //ANC (AND+Carry=bit7)
    case 0x20: if ((IR&0x1F)!=0xB) { if ((IR&3)==3) {T=(mem[addr]<<1)+(ST&1); ST&=124; ST|=(T>255); T&=0xFF; mem[addr]=T; cycles+=2;}  
                A&=mem[addr]; ST&=125; ST|=(!A)<<1|(A&128); }  //AND / RLA (ROL+AND)
               else {A&=mem[addr]; ST&=124;ST|=(!A)<<1|(A&128)|(A>127);}  break; //ANC (AND+Carry=bit7)
    case 0x40: if ((IR&0x1F)!=0xB) { if ((IR&3)==3) {ST&=124; ST|=(mem[addr]&1); mem[addr]>>=1; cycles+=2;}
                A^=mem[addr]; ST&=125;ST|=(!A)<<1|(A&128); } //EOR / SRE(LSE)=LSR+EOR
                else {A&=mem[addr]; ST&=124; ST|=(A&1); A>>=1; A&=0xFF; ST|=(A&128)|((!A)<<1); }  break; //ALR(ASR)=(AND+LSR)
    case 0xA0: if ((IR&0x1F)!=0x1B) { A=mem[addr]; if((IR&3)==3) X=A; } //LDA / LAX (illegal, used by my 1 rasterline player) 
               else {A=X=SP=mem[addr]&SP;} /*LAS(LAR)*/  ST&=125; ST|=((!A)<<1) | (A&128); break;  // LAS (LAR)
    case 0x80: if ((IR&0x1F)==0xB) { A = X & mem[addr]; ST&=125; ST|=(A&128) | ((!A)<<1); } //XAA (TXA+AND), highly unstable on real 6502!
               else if ((IR&0x1F)==0x1B) { SP=A&X; mem[addr]=SP&((addr>>8)+1); } //TAS(SHS) (SP=A&X, mem=S&H} - unstable on real 6502
               else {mem[addr]=A & (((IR&3)==3)?X:0xFF); storadd=addr;}  break; //STA / SAX (at times same as AHX/SHX/SHY) (illegal) 
   }
  }
  
  else if(IR&2) {  //nybble2:  2:illegal/LDX, 6:A/X/INC/DEC, A:Accu-shift/reg.transfer/NOP, E:shift/X/INC/DEC
   switch (IR&0x1F) { //addressing modes
    case 0x1E: pc++; addr=mem[pc]; pc++; addr+=mem[pc]*256 + ( ((IR&0xC0)!=0x80) ? X:Y ); cycles=5; break; //abs,x / abs,y
    case 0xE: pc++; addr=mem[pc]; pc++; addr+=mem[pc]*256; cycles=4; break; //abs
    case 0x16: pc++; addr = mem[pc] + ( ((IR&0xC0)!=0x80) ? X:Y ); cycles=4; break; //zp,x / zp,y
    case 6: pc++; addr = mem[pc]; cycles=3; break; //zp
    case 2: pc++; addr = pc; cycles=2;  //imm.
   }  
   addr&=0xFFFF; 
   switch (IR&0xE0) {
    case 0x00: ST&=0xFE; case 0x20: if((IR&0xF)==0xA) { A=(A<<1)+(ST&1); ST&=124;ST|=(A&128)|(A>255); A&=0xFF; ST|=(!A)<<1; } //ASL/ROL (Accu)
      else { T=(mem[addr]<<1)+(ST&1); ST&=124;ST|=(T&128)|(T>255); T&=0xFF; ST|=(!T)<<1; mem[addr]=T; cycles+=2; }  break; //RMW (Read-Write-Modify)
    case 0x40: ST&=0xFE; case 0x60: if((IR&0xF)==0xA) { T=A; A=(A>>1)+(ST&1)*128; ST&=124;ST|=(A&128)|(T&1); A&=0xFF; ST|=(!A)<<1; } //LSR/ROR (Accu)
      else { T=(mem[addr]>>1)+(ST&1)*128; ST&=124;ST|=(T&128)|(mem[addr]&1); T&=0xFF; ST|=(!T)<<1; mem[addr]=T; cycles+=2; }  break; //mem (RMW)
    case 0xC0: if(IR&4) { mem[addr]--; ST&=125;ST|=(!mem[addr])<<1|(mem[addr]&128); cycles+=2; } //DEC
      else {X--; X&=0xFF; ST&=125;ST|=(!X)<<1|(X&128);}  break; //DEX
    case 0xA0: if((IR&0xF)!=0xA) X=mem[addr];  else if(IR&0x10) {X=SP;break;}  else X=A;  ST&=125;ST|=(!X)<<1|(X&128);  break; //LDX/TSX/TAX
    case 0x80: if(IR&4) {mem[addr]=X;storadd=addr;}  else if(IR&0x10) SP=X;  else {A=X; ST&=125;ST|=(!A)<<1|(A&128);}  break; //STX/TXS/TXA
    case 0xE0: if(IR&4) { mem[addr]++; ST&=125;ST|=(!mem[addr])<<1|(mem[addr]&128); cycles+=2; } //INC/NOP
   }
  }
  
  else if((IR&0xC)==8) {  //nybble2:  8:register/status
   switch (IR&0xF0) {
    case 0x60: SP++; SP&=0xFF; A=mem[0x100+SP]; ST&=125;ST|=(!A)<<1|(A&128); cycles=4; break; //PLA
    case 0xC0: Y++; Y&=0xFF; ST&=125;ST|=(!Y)<<1|(Y&128); break; //INY
    case 0xE0: X++; X&=0xFF; ST&=125;ST|=(!X)<<1|(X&128); break; //INX
    case 0x80: Y--; Y&=0xFF; ST&=125;ST|=(!Y)<<1|(Y&128); break; //DEY
    case 0x00: mem[0x100+SP]=ST; SP--; SP&=0xFF; cycles=3; break; //PHP
    case 0x20: SP++; SP&=0xFF; ST=mem[0x100+SP]; cycles=4; break; //PLP
    case 0x40: mem[0x100+SP]=A; SP--; SP&=0xFF; cycles=3; break; //PHA
    case 0x90: A=Y; ST&=125;ST|=(!A)<<1|(A&128); break; //TYA
    case 0xA0: Y=A; ST&=125;ST|=(!Y)<<1|(Y&128); break; //TAY
    default: if(flagsw[IR>>5]&0x20) ST|=(flagsw[IR>>5]&0xDF); else ST&=255-(flagsw[IR>>5]&0xDF);  //CLC/SEC/CLI/SEI/CLV/CLD/SED
   }
  }
  
  else {  //nybble2:  0: control/branch/Y/compare  4: Y/compare  C:Y/compare/JMP
   if ((IR&0x1F)==0x10) { pc++; T=mem[pc]; if(T&0x80) T-=0x100; //BPL/BMI/BVC/BVS/BCC/BCS/BNE/BEQ  relative branch 
    if(IR&0x20) {if (ST&branchflag[IR>>6]) {pc+=T;cycles=3;}} else {if (!(ST&branchflag[IR>>6])) {pc+=T;cycles=3;}}  } 
   else {  //nybble2:  0:Y/control/Y/compare  4:Y/compare  C:Y/compare/JMP
    switch (IR&0x1F) { //addressing modes
     case 0: pc++; addr = pc; cycles=2; break; //imm. (or abs.low for JSR/BRK)
     case 0x1C: pc++; addr=mem[pc]; pc++; addr+=mem[pc]*256 + X; cycles=5; break; //abs,x
     case 0xC: pc++; addr=mem[pc]; pc++; addr+=mem[pc]*256; cycles=4; break; //abs
     case 0x14: pc++; addr = mem[pc] + X; cycles=4; break; //zp,x
     case 4: pc++; addr = mem[pc]; cycles=3;  //zp
    }  
    addr&=0xFFFF;  
    switch (IR&0xE0) {
     case 0x00: mem[0x100+SP]=pc%256; SP--;SP&=0xFF; mem[0x100+SP]=pc/256;  SP--;SP&=0xFF; mem[0x100+SP]=ST; SP--;SP&=0xFF; 
       pc = mem[0xFFFE]+mem[0xFFFF]*256-1; cycles=7; break; //BRK
     case 0x20: if(IR&0xF) { ST &= 0x3D; ST |= (mem[addr]&0xC0) | ( !(A&mem[addr]) )<<1; } //BIT
      else { mem[0x100+SP]=(pc+2)%256; SP--;SP&=0xFF; mem[0x100+SP]=(pc+2)/256;  SP--;SP&=0xFF; pc=mem[addr]+mem[addr+1]*256-1; cycles=6; }  break; //JSR
     case 0x40: if(IR&0xF) { pc = addr-1; cycles=3; } //JMP
      // else { if(SP>=0xFF) return 0xFE; SP++;SP&=0xFF; ST=mem[0x100+SP]; SP++;SP&=0xFF; T=mem[0x100+SP]; SP++;SP&=0xFF; pc=mem[0x100+SP]+T*256-1; cycles=6; }  break; //RTI
      else { if(SP>=0xFF) return 0x0; SP++;SP&=0xFF; ST=mem[0x100+SP]; SP++;SP&=0xFF; T=mem[0x100+SP]; SP++;SP&=0xFF; pc=mem[0x100+SP]+T*256-1; cycles=6; }  break; //RTI
     case 0x60: if(IR&0xF) { pc = mem[addr]+mem[addr+1]*256-1; cycles=5; } //JMP() (indirect)
      // else { if(SP>=0xFF) return 0xFF; SP++;SP&=0xFF; T=mem[0x100+SP]; SP++;SP&=0xFF; pc=mem[0x100+SP]+T*256-1; cycles=6; }  break; //RTS
      else { if(SP>=0xFF) return 0x0; SP++;SP&=0xFF; T=mem[0x100+SP]; SP++;SP&=0xFF; pc=mem[0x100+SP]+T*256-1; cycles=6; }  break; //RTS
     case 0xC0: T=Y-mem[addr]; ST&=124;ST|=(!(T&0xFF))<<1|(T&128)|(T>=0); break; //CPY
     case 0xE0: T=X-mem[addr]; ST&=124;ST|=(!(T&0xFF))<<1|(T&128)|(T>=0); break; //CPX
     case 0xA0: Y=mem[addr]; ST&=125;ST|=(!Y)<<1|(Y&128); break; //LDY
     case 0x80: mem[addr]=Y; storadd=addr;  //STY
    }
   }
  }
 
  pc++; //pc&=0xFFFF; 
  // return 0;
  return 1; 
 } 
#endif

#ifdef V3
//6502 defines
#define UNDOCUMENTED //when this is defined, undocumented opcodes are handled.
                     //otherwise, they're simply treated as NOPs.

#define FLAG_CARRY     0x01
#define FLAG_ZERO      0x02
#define FLAG_INTERRUPT 0x04
#define FLAG_DECIMAL   0x08
#define FLAG_BREAK     0x10
#define FLAG_CONSTANT  0x20
#define FLAG_OVERFLOW  0x40
#define FLAG_SIGN      0x80

#define BASE_STACK     0x100

#define saveaccum(n) a = (uint8_t)((n) & 0x00FF)


//flag modifier macros
#define setcarry() status |= FLAG_CARRY
#define clearcarry() status &= (~FLAG_CARRY)
#define setzero() status |= FLAG_ZERO
#define clearzero() status &= (~FLAG_ZERO)
#define setinterrupt() status |= FLAG_INTERRUPT
#define clearinterrupt() status &= (~FLAG_INTERRUPT)
#define setdecimal() status |= FLAG_DECIMAL
#define cleardecimal() status &= (~FLAG_DECIMAL)
#define setoverflow() status |= FLAG_OVERFLOW
#define clearoverflow() status &= (~FLAG_OVERFLOW)
#define setsign() status |= FLAG_SIGN
#define clearsign() status &= (~FLAG_SIGN)


//flag calculation macros
#define zerocalc(n) {\
    if ((n) & 0x00FF) clearzero();\
        else setzero();\
}

#define signcalc(n) {\
    if ((n) & 0x0080) setsign();\
        else clearsign();\
}

#define carrycalc(n) {\
    if ((n) & 0xFF00) setcarry();\
        else clearcarry();\
}

#define overflowcalc(n, m, o) { /* n = result, m = accumulator, o = memory */ \
    if (((n) ^ (uint16_t)(m)) & ((n) ^ (o)) & 0x0080) setoverflow();\
        else clearoverflow();\
}


//6502 CPU registers
uint16_t pc;
uint8_t sp, a, x, y, status;


// unsigned int pc;
// unsigned char a;
// unsigned char x;
// unsigned char y;
// unsigned char flags;
// unsigned char sp;
unsigned char mem[0x10000];

//helper variables
uint32_t instructions = 0; //keep track of total instructions executed
uint32_t clockticks6502 = 0, clockgoal6502 = 0;
uint16_t oldpc, ea, reladdr, value, result;
uint8_t opcode, oldstatus;

uint8_t read6502(uint16_t address) { return(mem[address]); }
void write6502(uint16_t address, uint8_t value) { mem[address] = value; }

//a few general functions used by various other functions
void push16(uint16_t pushval) {
    write6502(BASE_STACK + sp, (pushval >> 8) & 0xFF);
    write6502(BASE_STACK + ((sp - 1) & 0xFF), pushval & 0xFF);
    sp -= 2;
}

void push8(uint8_t pushval) {
    write6502(BASE_STACK + sp--, pushval);
}

uint16_t pull16() {
    uint16_t temp16;
    temp16 = read6502(BASE_STACK + ((sp + 1) & 0xFF)) | ((uint16_t)read6502(BASE_STACK + ((sp + 2) & 0xFF)) << 8);
    sp += 2;
    return(temp16);
}

uint8_t pull8() {
    return (read6502(BASE_STACK + ++sp));
}

void reset6502() {
    pc = (uint16_t)read6502(0xFFFC) | ((uint16_t)read6502(0xFFFD) << 8);
    a = 0;
    x = 0;
    y = 0;
    sp = 0xFD;
    status |= FLAG_CONSTANT;
}


static void (*addrtable[256])();
static void (*optable[256])();
uint8_t penaltyop, penaltyaddr;

//addressing mode functions, calculates effective addresses
static void imp() { //implied
}

static void acc() { //accumulator
}

static void imm() { //immediate
    ea = pc++;
}

static void zp() { //zero-page
    ea = (uint16_t)read6502((uint16_t)pc++);
}

static void zpx() { //zero-page,X
    ea = ((uint16_t)read6502((uint16_t)pc++) + (uint16_t)x) & 0xFF; //zero-page wraparound
}

static void zpy() { //zero-page,Y
    ea = ((uint16_t)read6502((uint16_t)pc++) + (uint16_t)y) & 0xFF; //zero-page wraparound
}

static void rel() { //relative for branch ops (8-bit immediate value, sign-extended)
    reladdr = (uint16_t)read6502(pc++);
    if (reladdr & 0x80) reladdr |= 0xFF00;
}

static void abso() { //absolute
    ea = (uint16_t)read6502(pc) | ((uint16_t)read6502(pc+1) << 8);
    pc += 2;
}

static void absx() { //absolute,X
    uint16_t startpage;
    ea = ((uint16_t)read6502(pc) | ((uint16_t)read6502(pc+1) << 8));
    startpage = ea & 0xFF00;
    ea += (uint16_t)x;

    if (startpage != (ea & 0xFF00)) { //one cycle penlty for page-crossing on some opcodes
        penaltyaddr = 1;
    }

    pc += 2;
}

static void absy() { //absolute,Y
    uint16_t startpage;
    ea = ((uint16_t)read6502(pc) | ((uint16_t)read6502(pc+1) << 8));
    startpage = ea & 0xFF00;
    ea += (uint16_t)y;

    if (startpage != (ea & 0xFF00)) { //one cycle penlty for page-crossing on some opcodes
        penaltyaddr = 1;
    }

    pc += 2;
}

static void ind() { //indirect
    uint16_t eahelp, eahelp2;
    eahelp = (uint16_t)read6502(pc) | (uint16_t)((uint16_t)read6502(pc+1) << 8);
    eahelp2 = (eahelp & 0xFF00) | ((eahelp + 1) & 0x00FF); //replicate 6502 page-boundary wraparound bug
    ea = (uint16_t)read6502(eahelp) | ((uint16_t)read6502(eahelp2) << 8);
    pc += 2;
}

static void indx() { // (indirect,X)
    uint16_t eahelp;
    eahelp = (uint16_t)(((uint16_t)read6502(pc++) + (uint16_t)x) & 0xFF); //zero-page wraparound for table pointer
    ea = (uint16_t)read6502(eahelp & 0x00FF) | ((uint16_t)read6502((eahelp+1) & 0x00FF) << 8);
}

static void indy() { // (indirect),Y
    uint16_t eahelp, eahelp2, startpage;
    eahelp = (uint16_t)read6502(pc++);
    eahelp2 = (eahelp & 0xFF00) | ((eahelp + 1) & 0x00FF); //zero-page wraparound
    ea = (uint16_t)read6502(eahelp) | ((uint16_t)read6502(eahelp2) << 8);
    startpage = ea & 0xFF00;
    ea += (uint16_t)y;

    if (startpage != (ea & 0xFF00)) { //one cycle penlty for page-crossing on some opcodes
        penaltyaddr = 1;
    }
}

static uint16_t getvalue() {
    if (addrtable[opcode] == acc) return((uint16_t)a);
        else return((uint16_t)read6502(ea));
}

static uint16_t getvalue16() {
    return((uint16_t)read6502(ea) | ((uint16_t)read6502(ea+1) << 8));
}

static void putvalue(uint16_t saveval) {
    if (addrtable[opcode] == acc) a = (uint8_t)(saveval & 0x00FF);
        else write6502(ea, (saveval & 0x00FF));
}


//instruction handler functions
static void adc() {
    penaltyop = 1;
    value = getvalue();
    result = (uint16_t)a + value + (uint16_t)(status & FLAG_CARRY);
   
    carrycalc(result);
    zerocalc(result);
    overflowcalc(result, a, value);
    signcalc(result);
    
    #ifndef NES_CPU
    if (status & FLAG_DECIMAL) {
        clearcarry();
        
        if ((a & 0x0F) > 0x09) {
            a += 0x06;
        }
        if ((a & 0xF0) > 0x90) {
            a += 0x60;
            setcarry();
        }
        
        clockticks6502++;
    }
    #endif
   
    saveaccum(result);
}

static void and() {
    penaltyop = 1;
    value = getvalue();
    result = (uint16_t)a & value;
   
    zerocalc(result);
    signcalc(result);
   
    saveaccum(result);
}

static void asl() {
    value = getvalue();
    result = value << 1;

    carrycalc(result);
    zerocalc(result);
    signcalc(result);
   
    putvalue(result);
}

static void bcc() {
    if ((status & FLAG_CARRY) == 0) {
        oldpc = pc;
        pc += reladdr;
        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
            else clockticks6502++;
    }
}

static void bcs() {
    if ((status & FLAG_CARRY) == FLAG_CARRY) {
        oldpc = pc;
        pc += reladdr;
        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
            else clockticks6502++;
    }
}

static void beq() {
    if ((status & FLAG_ZERO) == FLAG_ZERO) {
        oldpc = pc;
        pc += reladdr;
        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
            else clockticks6502++;
    }
}

static void bit() {
    value = getvalue();
    result = (uint16_t)a & value;
   
    zerocalc(result);
    status = (status & 0x3F) | (uint8_t)(value & 0xC0);
}

static void bmi() {
    if ((status & FLAG_SIGN) == FLAG_SIGN) {
        oldpc = pc;
        pc += reladdr;
        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
            else clockticks6502++;
    }
}

static void bne() {
    if ((status & FLAG_ZERO) == 0) {
        oldpc = pc;
        pc += reladdr;
        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
            else clockticks6502++;
    }
}

static void bpl() {
    if ((status & FLAG_SIGN) == 0) {
        oldpc = pc;
        pc += reladdr;
        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
            else clockticks6502++;
    }
}

static void brk() {
    pc++;
    push16(pc); //push next instruction address onto stack
    push8(status | FLAG_BREAK); //push CPU status to stack
    setinterrupt(); //set interrupt flag
    pc = (uint16_t)read6502(0xFFFE) | ((uint16_t)read6502(0xFFFF) << 8);
}

static void bvc() {
    if ((status & FLAG_OVERFLOW) == 0) {
        oldpc = pc;
        pc += reladdr;
        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
            else clockticks6502++;
    }
}

static void bvs() {
    if ((status & FLAG_OVERFLOW) == FLAG_OVERFLOW) {
        oldpc = pc;
        pc += reladdr;
        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
            else clockticks6502++;
    }
}

static void clc() {
    clearcarry();
}

static void cld() {
    cleardecimal();
}

static void cli() {
    clearinterrupt();
}

static void clv() {
    clearoverflow();
}

static void cmp() {
    penaltyop = 1;
    value = getvalue();
    result = (uint16_t)a - value;
   
    if (a >= (uint8_t)(value & 0x00FF)) setcarry();
        else clearcarry();
    if (a == (uint8_t)(value & 0x00FF)) setzero();
        else clearzero();
    signcalc(result);
}

static void cpx() {
    value = getvalue();
    result = (uint16_t)x - value;
   
    if (x >= (uint8_t)(value & 0x00FF)) setcarry();
        else clearcarry();
    if (x == (uint8_t)(value & 0x00FF)) setzero();
        else clearzero();
    signcalc(result);
}

static void cpy() {
    value = getvalue();
    result = (uint16_t)y - value;
   
    if (y >= (uint8_t)(value & 0x00FF)) setcarry();
        else clearcarry();
    if (y == (uint8_t)(value & 0x00FF)) setzero();
        else clearzero();
    signcalc(result);
}

static void dec() {
    value = getvalue();
    result = value - 1;
   
    zerocalc(result);
    signcalc(result);
   
    putvalue(result);
}

static void dex() {
    x--;
   
    zerocalc(x);
    signcalc(x);
}

static void dey() {
    y--;
   
    zerocalc(y);
    signcalc(y);
}

static void eor() {
    penaltyop = 1;
    value = getvalue();
    result = (uint16_t)a ^ value;
   
    zerocalc(result);
    signcalc(result);
   
    saveaccum(result);
}

static void inc() {
    value = getvalue();
    result = value + 1;
   
    zerocalc(result);
    signcalc(result);
   
    putvalue(result);
}

static void inx() {
    x++;
   
    zerocalc(x);
    signcalc(x);
}

static void iny() {
    y++;
   
    zerocalc(y);
    signcalc(y);
}

static void jmp() {
    pc = ea;
}

static void jsr() {
    push16(pc - 1);
    pc = ea;
}

static void lda() {
    penaltyop = 1;
    value = getvalue();
    a = (uint8_t)(value & 0x00FF);
   
    zerocalc(a);
    signcalc(a);
}

static void ldx() {
    penaltyop = 1;
    value = getvalue();
    x = (uint8_t)(value & 0x00FF);
   
    zerocalc(x);
    signcalc(x);
}

static void ldy() {
    penaltyop = 1;
    value = getvalue();
    y = (uint8_t)(value & 0x00FF);
   
    zerocalc(y);
    signcalc(y);
}

static void lsr() {
    value = getvalue();
    result = value >> 1;
   
    if (value & 1) setcarry();
        else clearcarry();
    zerocalc(result);
    signcalc(result);
   
    putvalue(result);
}

static void nop() {
    switch (opcode) {
        case 0x1C:
        case 0x3C:
        case 0x5C:
        case 0x7C:
        case 0xDC:
        case 0xFC:
            penaltyop = 1;
            break;
    }
}

static void ora() {
    penaltyop = 1;
    value = getvalue();
    result = (uint16_t)a | value;
   
    zerocalc(result);
    signcalc(result);
   
    saveaccum(result);
}

static void pha() {
    push8(a);
}

static void php() {
    push8(status | FLAG_BREAK);
}

static void pla() {
    a = pull8();
   
    zerocalc(a);
    signcalc(a);
}

static void plp() {
    status = pull8() | FLAG_CONSTANT;
}

static void rol() {
    value = getvalue();
    result = (value << 1) | (status & FLAG_CARRY);
   
    carrycalc(result);
    zerocalc(result);
    signcalc(result);
   
    putvalue(result);
}

static void ror() {
    value = getvalue();
    result = (value >> 1) | ((status & FLAG_CARRY) << 7);
   
    if (value & 1) setcarry();
        else clearcarry();
    zerocalc(result);
    signcalc(result);
   
    putvalue(result);
}

static void rti() {
    status = pull8();
    value = pull16();
    pc = value;
}

static void rts() {
    value = pull16();
    pc = value + 1;
}

static void sbc() {
    penaltyop = 1;
    value = getvalue() ^ 0x00FF;
    result = (uint16_t)a + value + (uint16_t)(status & FLAG_CARRY);
   
    carrycalc(result);
    zerocalc(result);
    overflowcalc(result, a, value);
    signcalc(result);

    #ifndef NES_CPU
    if (status & FLAG_DECIMAL) {
        clearcarry();
        
        a -= 0x66;
        if ((a & 0x0F) > 0x09) {
            a += 0x06;
        }
        if ((a & 0xF0) > 0x90) {
            a += 0x60;
            setcarry();
        }
        
        clockticks6502++;
    }
    #endif
   
    saveaccum(result);
}

static void sec() {
    setcarry();
}

static void sed() {
    setdecimal();
}

static void sei() {
    setinterrupt();
}

static void sta() {
    putvalue(a);
}

static void stx() {
    putvalue(x);
}

static void sty() {
    putvalue(y);
}

static void tax() {
    x = a;
   
    zerocalc(x);
    signcalc(x);
}

static void tay() {
    y = a;
   
    zerocalc(y);
    signcalc(y);
}

static void tsx() {
    x = sp;
   
    zerocalc(x);
    signcalc(x);
}

static void txa() {
    a = x;
   
    zerocalc(a);
    signcalc(a);
}

static void txs() {
    sp = x;
}

static void tya() {
    a = y;
   
    zerocalc(a);
    signcalc(a);
}

//undocumented instructions
#ifdef UNDOCUMENTED
    static void lax() {
        lda();
        ldx();
    }

    static void sax() {
        sta();
        stx();
        putvalue(a & x);
        if (penaltyop && penaltyaddr) clockticks6502--;
    }

    static void dcp() {
        dec();
        cmp();
        if (penaltyop && penaltyaddr) clockticks6502--;
    }

    static void isb() {
        inc();
        sbc();
        if (penaltyop && penaltyaddr) clockticks6502--;
    }

    static void slo() {
        asl();
        ora();
        if (penaltyop && penaltyaddr) clockticks6502--;
    }

    static void rla() {
        rol();
        and();
        if (penaltyop && penaltyaddr) clockticks6502--;
    }

    static void sre() {
        lsr();
        eor();
        if (penaltyop && penaltyaddr) clockticks6502--;
    }

    static void rra() {
        ror();
        adc();
        if (penaltyop && penaltyaddr) clockticks6502--;
    }
#else
    #define lax nop
    #define sax nop
    #define dcp nop
    #define isb nop
    #define slo nop
    #define rla nop
    #define sre nop
    #define rra nop
#endif


static void (*addrtable[256])() = {
/*        |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  A  |  B  |  C  |  D  |  E  |  F  |     */
/* 0 */     imp, indx,  imp, indx,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imm, abso, abso, abso, abso, /* 0 */
/* 1 */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx, /* 1 */
/* 2 */    abso, indx,  imp, indx,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imm, abso, abso, abso, abso, /* 2 */
/* 3 */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx, /* 3 */
/* 4 */     imp, indx,  imp, indx,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imm, abso, abso, abso, abso, /* 4 */
/* 5 */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx, /* 5 */
/* 6 */     imp, indx,  imp, indx,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imm,  ind, abso, abso, abso, /* 6 */
/* 7 */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx, /* 7 */
/* 8 */     imm, indx,  imm, indx,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imm, abso, abso, abso, abso, /* 8 */
/* 9 */     rel, indy,  imp, indy,  zpx,  zpx,  zpy,  zpy,  imp, absy,  imp, absy, absx, absx, absy, absy, /* 9 */
/* A */     imm, indx,  imm, indx,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imm, abso, abso, abso, abso, /* A */
/* B */     rel, indy,  imp, indy,  zpx,  zpx,  zpy,  zpy,  imp, absy,  imp, absy, absx, absx, absy, absy, /* B */
/* C */     imm, indx,  imm, indx,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imm, abso, abso, abso, abso, /* C */
/* D */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx, /* D */
/* E */     imm, indx,  imm, indx,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imm, abso, abso, abso, abso, /* E */
/* F */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx  /* F */
};

static void (*optable[256])() = {
/*        |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  A  |  B  |  C  |  D  |  E  |  F  |      */
/* 0 */      brk,  ora,  nop,  slo,  nop,  ora,  asl,  slo,  php,  ora,  asl,  nop,  nop,  ora,  asl,  slo, /* 0 */
/* 1 */      bpl,  ora,  nop,  slo,  nop,  ora,  asl,  slo,  clc,  ora,  nop,  slo,  nop,  ora,  asl,  slo, /* 1 */
/* 2 */      jsr,  and,  nop,  rla,  bit,  and,  rol,  rla,  plp,  and,  rol,  nop,  bit,  and,  rol,  rla, /* 2 */
/* 3 */      bmi,  and,  nop,  rla,  nop,  and,  rol,  rla,  sec,  and,  nop,  rla,  nop,  and,  rol,  rla, /* 3 */
/* 4 */      rti,  eor,  nop,  sre,  nop,  eor,  lsr,  sre,  pha,  eor,  lsr,  nop,  jmp,  eor,  lsr,  sre, /* 4 */
/* 5 */      bvc,  eor,  nop,  sre,  nop,  eor,  lsr,  sre,  cli,  eor,  nop,  sre,  nop,  eor,  lsr,  sre, /* 5 */
/* 6 */      rts,  adc,  nop,  rra,  nop,  adc,  ror,  rra,  pla,  adc,  ror,  nop,  jmp,  adc,  ror,  rra, /* 6 */
/* 7 */      bvs,  adc,  nop,  rra,  nop,  adc,  ror,  rra,  sei,  adc,  nop,  rra,  nop,  adc,  ror,  rra, /* 7 */
/* 8 */      nop,  sta,  nop,  sax,  sty,  sta,  stx,  sax,  dey,  nop,  txa,  nop,  sty,  sta,  stx,  sax, /* 8 */
/* 9 */      bcc,  sta,  nop,  nop,  sty,  sta,  stx,  sax,  tya,  sta,  txs,  nop,  nop,  sta,  nop,  nop, /* 9 */
/* A */      ldy,  lda,  ldx,  lax,  ldy,  lda,  ldx,  lax,  tay,  lda,  tax,  nop,  ldy,  lda,  ldx,  lax, /* A */
/* B */      bcs,  lda,  nop,  lax,  ldy,  lda,  ldx,  lax,  clv,  lda,  tsx,  lax,  ldy,  lda,  ldx,  lax, /* B */
/* C */      cpy,  cmp,  nop,  dcp,  cpy,  cmp,  dec,  dcp,  iny,  cmp,  dex,  nop,  cpy,  cmp,  dec,  dcp, /* C */
/* D */      bne,  cmp,  nop,  dcp,  nop,  cmp,  dec,  dcp,  cld,  cmp,  nop,  dcp,  nop,  cmp,  dec,  dcp, /* D */
/* E */      cpx,  sbc,  nop,  isb,  cpx,  sbc,  inc,  isb,  inx,  sbc,  nop,  sbc,  cpx,  sbc,  inc,  isb, /* E */
/* F */      beq,  sbc,  nop,  isb,  nop,  sbc,  inc,  isb,  sed,  sbc,  nop,  isb,  nop,  sbc,  inc,  isb  /* F */
};

static const uint32_t ticktable[256] = {
/*        |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  A  |  B  |  C  |  D  |  E  |  F  |     */
/* 0 */      7,    6,    2,    8,    3,    3,    5,    5,    3,    2,    2,    2,    4,    4,    6,    6,  /* 0 */
/* 1 */      2,    5,    2,    8,    4,    4,    6,    6,    2,    4,    2,    7,    4,    4,    7,    7,  /* 1 */
/* 2 */      6,    6,    2,    8,    3,    3,    5,    5,    4,    2,    2,    2,    4,    4,    6,    6,  /* 2 */
/* 3 */      2,    5,    2,    8,    4,    4,    6,    6,    2,    4,    2,    7,    4,    4,    7,    7,  /* 3 */
/* 4 */      6,    6,    2,    8,    3,    3,    5,    5,    3,    2,    2,    2,    3,    4,    6,    6,  /* 4 */
/* 5 */      2,    5,    2,    8,    4,    4,    6,    6,    2,    4,    2,    7,    4,    4,    7,    7,  /* 5 */
/* 6 */      6,    6,    2,    8,    3,    3,    5,    5,    4,    2,    2,    2,    5,    4,    6,    6,  /* 6 */
/* 7 */      2,    5,    2,    8,    4,    4,    6,    6,    2,    4,    2,    7,    4,    4,    7,    7,  /* 7 */
/* 8 */      2,    6,    2,    6,    3,    3,    3,    3,    2,    2,    2,    2,    4,    4,    4,    4,  /* 8 */
/* 9 */      2,    6,    2,    6,    4,    4,    4,    4,    2,    5,    2,    5,    5,    5,    5,    5,  /* 9 */
/* A */      2,    6,    2,    6,    3,    3,    3,    3,    2,    2,    2,    2,    4,    4,    4,    4,  /* A */
/* B */      2,    5,    2,    5,    4,    4,    4,    4,    2,    4,    2,    4,    4,    4,    4,    4,  /* B */
/* C */      2,    6,    2,    8,    3,    3,    5,    5,    2,    2,    2,    2,    4,    4,    6,    6,  /* C */
/* D */      2,    5,    2,    8,    4,    4,    6,    6,    2,    4,    2,    7,    4,    4,    7,    7,  /* D */
/* E */      2,    6,    2,    8,    3,    3,    5,    5,    2,    2,    2,    2,    4,    4,    6,    6,  /* E */
/* F */      2,    5,    2,    8,    4,    4,    6,    6,    2,    4,    2,    7,    4,    4,    7,    7   /* F */
};


void nmi6502() {
    push16(pc);
    push8(status);
    status |= FLAG_INTERRUPT;
    pc = (uint16_t)read6502(0xFFFA) | ((uint16_t)read6502(0xFFFB) << 8);
}

void irq6502() {
    push16(pc);
    push8(status);
    status |= FLAG_INTERRUPT;
    pc = (uint16_t)read6502(0xFFFE) | ((uint16_t)read6502(0xFFFF) << 8);
}

uint8_t callexternal = 0;
void (*loopexternal)();

void exec6502(uint32_t tickcount) {
    clockgoal6502 += tickcount;
   
    while (clockticks6502 < clockgoal6502) {
        opcode = read6502(pc++);
        status |= FLAG_CONSTANT;

        penaltyop = 0;
        penaltyaddr = 0;

        (*addrtable[opcode])();
        (*optable[opcode])();
        clockticks6502 += ticktable[opcode];
        if (penaltyop && penaltyaddr) clockticks6502++;

        instructions++;

        if (callexternal) (*loopexternal)();
    }

}

void step6502() {
    opcode = read6502(pc++);
    status |= FLAG_CONSTANT;

    penaltyop = 0;
    penaltyaddr = 0;

    (*addrtable[opcode])();
    (*optable[opcode])();
    clockticks6502 += ticktable[opcode];
    if (penaltyop && penaltyaddr) clockticks6502++;
    clockgoal6502 = clockticks6502;

    instructions++;

    if (callexternal) (*loopexternal)();
}

void hookexternal(void *funcptr) {
    if (funcptr != (void *)NULL) {
        loopexternal = funcptr;
        callexternal = 1;
    } else callexternal = 0;
}

void initcpu(unsigned short newpc, unsigned char newa, unsigned char newx, unsigned char newy) { 
    pc = newpc, a = newa; x = newx, y = newy; sp = 0xFF; 
}

int runcpu(void) {
    step6502();

    if (opcode == 0x00) {
        return 0;
    }

    if (opcode == 0x40) {
        if (sp == 0xff) return 0;
    }

    if (opcode == 0x60) {
        if (sp == 0xff) return 0;
    }

    return 1;
}

#endif
