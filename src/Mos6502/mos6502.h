/*
6502 emulation adaptations made by beachviking for the ESP32
02/25/2024
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

#ifndef C_H
#define C_H

/* This ifdef allows the header to be used from both C and C++. */
#ifdef __cplusplus
extern "C" {
#endif
extern unsigned char mem[];
// extern unsigned int cpucycles;
extern unsigned int pc;
// extern uint16_t pc;
void initcpu(unsigned short newpc, unsigned char newa, unsigned char newx, unsigned char newy);
int runcpu(void);
#ifdef __cplusplus
}
#endif

#endif

