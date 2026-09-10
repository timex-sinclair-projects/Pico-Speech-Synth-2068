/*
 * sp0256.c -- portable SP0256 Narrator Speech Processor emulation core.
 *
 * Derived from MAME's src/devices/sound/sp0256.cpp
 *   license: BSD-3-Clause
 *   copyright-holders: Joseph Zbiciak, Tim Lindner
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: (1) redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer; (2)
 * redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution; (3) neither the
 * names of the copyright holders nor the names of contributors may be used
 * to endorse or promote products derived from this software without
 * specific prior written permission.  THIS SOFTWARE IS PROVIDED "AS IS"
 * WITHOUT WARRANTY OF ANY KIND.
 *
 * Changes from MAME: removed device/stream/save-state plumbing, removed
 * the SPB640 FIFO path, linear output buffer instead of a scratch ring,
 * ROM accessed through a small window at SP0256_ROM_BASE.
 */

#include <string.h>
#include "sp0256.h"

#ifdef SP0256_TRACE
#include <stdio.h>
#define TRACE(...) fprintf(stderr, __VA_ARGS__)
#else
#define TRACE(...) ((void)0)
#endif

#define PER_PAUSE    64          /* Equiv timing period for pauses.  */
#define PER_NOISE    64          /* Equiv timing period for noise.   */

/* ------------------------------------------------------------------------ */
/*  ROM access.  Addresses are byte addresses in the SP0256's 64 KB map;   */
/*  the AL2 image occupies 0x1000..0x17FF.  Everything else reads as 0.    */
/* ------------------------------------------------------------------------ */
static inline uint32_t rom_rd(const sp0256_t *sp, uint32_t addr)
{
	addr &= 0xffff;
	addr -= SP0256_ROM_BASE;
	return (addr < sp->rom_len) ? sp->rom[addr] : 0;
}

/* ======================================================================== */
/*  LIMIT            -- Limiter function for digital sample output.         */
/* ======================================================================== */
static inline int16_t limit(int16_t s)
{
	if (s >  8191) return  8191;
	if (s < -8192) return -8192;
	return s;
}

/* Period register -> period in samples, with the pitch control applied.
 * 0 stays 0 (noise).  Higher pitch = shorter period.                     */
static inline uint32_t scaled_per(uint32_t r1, int pitch_pct)
{
	uint32_t p;
	if (!r1 || pitch_pct == 100) return r1;
	p = (r1 * 100u + (uint32_t)pitch_pct / 2) / (uint32_t)pitch_pct;
	return p ? p : 1;
}

/* ======================================================================== */
/*  LPC12_UPDATE     -- Update the 12-pole filter, outputting samples.      */
/*  Returns the number of samples actually written (may be < num_samp if   */
/*  the repeat counter expires).                                            */
/* ======================================================================== */
static int lpc12_update(sp0256_lpc12_t *f, int num_samp, int16_t *out)
{
	int i, j;

	for (i = 0; i < num_samp; i++)
	{
		int do_int = 0;
		uint16_t samp = 0;

		/* Periodic impulses, or random noise. */
		if (f->per)
		{
			if (f->cnt <= 0)
			{
				f->cnt += f->per;
				samp = (uint16_t)f->amp;
				f->rpt--;
				do_int = f->interp;

				for (j = 0; j < 6; j++)
					f->z_data[j][1] = f->z_data[j][0] = 0;
			}
			else
			{
				samp = 0;
				f->cnt--;
			}
		}
		else
		{
			int bit;

			if (--f->cnt <= 0)
			{
				do_int = f->interp;
				f->cnt = PER_NOISE;
				f->rpt--;
				for (j = 0; j < 6; j++)
					f->z_data[j][0] = f->z_data[j][1] = 0;
			}

			bit = f->rng & 1;
			f->rng = (f->rng >> 1) ^ (bit ? 0x4001 : 0);

			samp = (uint16_t)(bit ? f->amp : -f->amp);
		}

		/* Interpolation registers. */
		if (do_int)
		{
			f->r[0] += f->r[14];
			f->r[1] += f->r[15];

			f->amp = (f->r[0] & 0x1F) << (((f->r[0] & 0xE0) >> 5) + 0);
			f->per = scaled_per(f->r[1], f->pitch_pct);
		}

		/* Stop if the repeat counter expired. */
		if (f->rpt <= 0)
			break;

		/* Six cascaded second-order stages (App. Manual form). */
		for (j = 0; j < 6; j++)
		{
			samp += (uint16_t)(((int)f->b_coef[j] * (int)f->z_data[j][1]) >> 9);
			samp += (uint16_t)(((int)f->f_coef[j] * (int)f->z_data[j][0]) >> 8);

			f->z_data[j][1] = f->z_data[j][0];
			f->z_data[j][0] = (int16_t)samp;
		}

		out[i] = (int16_t)(limit((int16_t)samp) << 2);
	}

	return i;
}

/* ======================================================================== */
/*  LPC12_REGDEC -- Decode the register set in the filter bank.             */
/* ======================================================================== */
static const int16_t qtbl[128] =
{
	0,      9,      17,     25,     33,     41,     49,     57,
	65,     73,     81,     89,     97,     105,    113,    121,
	129,    137,    145,    153,    161,    169,    177,    185,
	193,    201,    209,    217,    225,    233,    241,    249,
	257,    265,    273,    281,    289,    297,    301,    305,
	309,    313,    317,    321,    325,    329,    333,    337,
	341,    345,    349,    353,    357,    361,    365,    369,
	373,    377,    381,    385,    389,    393,    397,    401,
	405,    409,    413,    417,    421,    425,    427,    429,
	431,    433,    435,    437,    439,    441,    443,    445,
	447,    449,    451,    453,    455,    457,    459,    461,
	463,    465,    467,    469,    471,    473,    475,    477,
	479,    481,    482,    483,    484,    485,    486,    487,
	488,    489,    490,    491,    492,    493,    494,    495,
	496,    497,    498,    499,    500,    501,    502,    503,
	504,    505,    506,    507,    508,    509,    510,    511
};

static inline int16_t iq(uint8_t x)
{
	return (x & 0x80) ? qtbl[0x7F & (uint8_t)(-x)] : (int16_t)(-qtbl[x]);
}

static void lpc12_regdec(sp0256_lpc12_t *f, int pitch_pct)
{
	int i;

	/* Amplitude and period.  cnt forced to 0 for an initial impulse;     */
	/* compensated by the "repeat + 1" in the sequencer.                    */
	f->amp = (f->r[0] & 0x1F) << (((f->r[0] & 0xE0) >> 5) + 0);
	f->cnt = 0;
	f->per = scaled_per(f->r[1], pitch_pct);

	for (i = 0; i < 6; i++)
	{
		f->b_coef[i] = iq(f->r[2 + 2*i]);
		f->f_coef[i] = iq(f->r[3 + 2*i]);
	}

	f->interp = f->r[14] || f->r[15];
}

/* ======================================================================== */
/*  SP0256_DATAFMT   -- Data format table for the SP0256's microsequencer   */
/*                                                                          */
/*  len     4 bits      Length of field to extract                          */
/*  lshift  4 bits      Left-shift amount on field                          */
/*  param   4 bits      Parameter number being updated                      */
/*  delta   1 bit       This is a delta-update.  (Implies sign-extend)      */
/*  field   1 bit       This is a field replace.                            */
/*  clr5    1 bit       Clear F5, B5.                                       */
/*  clrall  1 bit       Clear all before doing this update                  */
/* ======================================================================== */
#define CR(l,s,p,d,f,c5,ca)         \
		(                           \
			(((l)  & 15) <<  0) |   \
			(((s)  & 15) <<  4) |   \
			(((p)  & 15) <<  8) |   \
			(((d)  &  1) << 12) |   \
			(((f)  &  1) << 13) |   \
			(((c5) &  1) << 14) |   \
			(((ca) &  1) << 15)     \
		)

#define CR_DELTA  CR(0,0,0,1,0,0,0)
#define CR_FIELD  CR(0,0,0,0,1,0,0)
#define CR_CLR5   CR(0,0,0,0,0,1,0)
#define CR_CLRA   CR(0,0,0,0,0,0,1)
#define CR_LEN(x) ((x) & 15)
#define CR_SHF(x) (((x) >> 4) & 15)
#define CR_PRM(x) (((x) >> 8) & 15)

enum { AM = 0, PR, B0, F0, B1, F1, B2, F2, B3, F3, B4, F4, B5, F5, IA, IP };

static const uint16_t sp0256_datafmt[] =
{
	/* OPCODE 1111: PAUSE */
	/*    0 */  CR( 0,  0,  0,  0,  0,  0,  1),     /*  Clear all   */

	/* Opcode 0001: LOADALL -- all modes */
	/*    1 */  CR( 8,  0,  AM, 0,  0,  0,  1),
	/*    2 */  CR( 8,  0,  PR, 0,  0,  0,  0),
	/*    3 */  CR( 8,  0,  B0, 0,  0,  0,  0),
	/*    4 */  CR( 8,  0,  F0, 0,  0,  0,  0),
	/*    5 */  CR( 8,  0,  B1, 0,  0,  0,  0),
	/*    6 */  CR( 8,  0,  F1, 0,  0,  0,  0),
	/*    7 */  CR( 8,  0,  B2, 0,  0,  0,  0),
	/*    8 */  CR( 8,  0,  F2, 0,  0,  0,  0),
	/*    9 */  CR( 8,  0,  B3, 0,  0,  0,  0),
	/*   10 */  CR( 8,  0,  F3, 0,  0,  0,  0),
	/*   11 */  CR( 8,  0,  B4, 0,  0,  0,  0),
	/*   12 */  CR( 8,  0,  F4, 0,  0,  0,  0),
	/*   13 */  CR( 8,  0,  B5, 0,  0,  0,  0),
	/*   14 */  CR( 8,  0,  F5, 0,  0,  0,  0),
	/* Mode 01 and 11 only */
	/*   15 */  CR( 8,  0,  IA, 0,  0,  0,  0),
	/*   16 */  CR( 8,  0,  IP, 0,  0,  0,  0),

	/* Opcode 0100: LOAD_4 -- Mode 00 and 01 */
	/*   17 */  CR( 6,  2,  AM, 0,  0,  0,  1),
	/*   18 */  CR( 8,  0,  PR, 0,  0,  0,  0),
	/*   19 */  CR( 4,  3,  B3, 0,  0,  0,  0),
	/*   20 */  CR( 6,  2,  F3, 0,  0,  0,  0),
	/*   21 */  CR( 7,  1,  B4, 0,  0,  0,  0),
	/*   22 */  CR( 6,  2,  F4, 0,  0,  0,  0),
	/* Mode 01 only */
	/*   23 */  CR( 8,  0,  B5, 0,  0,  0,  0),
	/*   24 */  CR( 8,  0,  F5, 0,  0,  0,  0),
	/* Mode 10 and 11 */
	/*   25 */  CR( 6,  2,  AM, 0,  0,  0,  1),
	/*   26 */  CR( 8,  0,  PR, 0,  0,  0,  0),
	/*   27 */  CR( 6,  1,  B3, 0,  0,  0,  0),
	/*   28 */  CR( 7,  1,  F3, 0,  0,  0,  0),
	/*   29 */  CR( 8,  0,  B4, 0,  0,  0,  0),
	/*   30 */  CR( 8,  0,  F4, 0,  0,  0,  0),
	/* Mode 11 only */
	/*   31 */  CR( 8,  0,  B5, 0,  0,  0,  0),
	/*   32 */  CR( 8,  0,  F5, 0,  0,  0,  0),

	/* Opcode 0110: SETMSB_6 -- Mode 00 only */
	/*   33 */  CR( 0,  0,  0,  0,  0,  1,  0),
	/* Mode 00 and 01 */
	/*   34 */  CR( 6,  2,  AM, 0,  0,  0,  0),
	/*   35 */  CR( 6,  2,  F3, 0,  1,  0,  0),
	/*   36 */  CR( 6,  2,  F4, 0,  1,  0,  0),
	/* Mode 01 only */
	/*   37 */  CR( 8,  0,  F5, 0,  1,  0,  0),
	/* Mode 10 only */
	/*   38 */  CR( 0,  0,  0,  0,  0,  1,  0),
	/* Mode 10 and 11 */
	/*   39 */  CR( 6,  2,  AM, 0,  0,  0,  0),
	/*   40 */  CR( 7,  1,  F3, 0,  1,  0,  0),
	/*   41 */  CR( 8,  0,  F4, 0,  1,  0,  0),
	/* Mode 11 only */
	/*   42 */  CR( 8,  0,  F5, 0,  1,  0,  0),

	/*   43 */  0,  /* unused */
	/*   44 */  0,  /* unused */

	/* Opcode 1001: DELTA_9 -- Mode 00 and 01 */
	/*   45 */  CR( 4,  2,  AM, 1,  0,  0,  0),
	/*   46 */  CR( 5,  0,  PR, 1,  0,  0,  0),
	/*   47 */  CR( 3,  4,  B0, 1,  0,  0,  0),
	/*   48 */  CR( 3,  3,  F0, 1,  0,  0,  0),
	/*   49 */  CR( 3,  4,  B1, 1,  0,  0,  0),
	/*   50 */  CR( 3,  3,  F1, 1,  0,  0,  0),
	/*   51 */  CR( 3,  4,  B2, 1,  0,  0,  0),
	/*   52 */  CR( 3,  3,  F2, 1,  0,  0,  0),
	/*   53 */  CR( 3,  3,  B3, 1,  0,  0,  0),
	/*   54 */  CR( 4,  2,  F3, 1,  0,  0,  0),
	/*   55 */  CR( 4,  1,  B4, 1,  0,  0,  0),
	/*   56 */  CR( 4,  2,  F4, 1,  0,  0,  0),
	/* Mode 01 only */
	/*   57 */  CR( 5,  0,  B5, 1,  0,  0,  0),
	/*   58 */  CR( 5,  0,  F5, 1,  0,  0,  0),
	/* Mode 10 and 11 */
	/*   59 */  CR( 4,  2,  AM, 1,  0,  0,  0),
	/*   60 */  CR( 5,  0,  PR, 1,  0,  0,  0),
	/*   61 */  CR( 4,  1,  B0, 1,  0,  0,  0),
	/*   62 */  CR( 4,  2,  F0, 1,  0,  0,  0),
	/*   63 */  CR( 4,  1,  B1, 1,  0,  0,  0),
	/*   64 */  CR( 4,  2,  F1, 1,  0,  0,  0),
	/*   65 */  CR( 4,  1,  B2, 1,  0,  0,  0),
	/*   66 */  CR( 4,  2,  F2, 1,  0,  0,  0),
	/*   67 */  CR( 4,  1,  B3, 1,  0,  0,  0),
	/*   68 */  CR( 5,  1,  F3, 1,  0,  0,  0),
	/*   69 */  CR( 5,  0,  B4, 1,  0,  0,  0),
	/*   70 */  CR( 5,  0,  F4, 1,  0,  0,  0),
	/* Mode 11 only */
	/*   71 */  CR( 5,  0,  B5, 1,  0,  0,  0),
	/*   72 */  CR( 5,  0,  F5, 1,  0,  0,  0),

	/* Opcode 1010: SETMSB_A -- Mode 00 only */
	/*   73 */  CR( 0,  0,  0,  0,  0,  1,  0),
	/* Mode 00 and 01 */
	/*   74 */  CR( 6,  2,  AM, 0,  0,  0,  0),
	/*   75 */  CR( 5,  3,  F0, 0,  1,  0,  0),
	/*   76 */  CR( 5,  3,  F1, 0,  1,  0,  0),
	/*   77 */  CR( 5,  3,  F2, 0,  1,  0,  0),
	/* Mode 10 only */
	/*   78 */  CR( 0,  0,  0,  0,  0,  1,  0),
	/* Mode 10 and 11 */
	/*   79 */  CR( 6,  2,  AM, 0,  0,  0,  0),
	/*   80 */  CR( 6,  2,  F0, 0,  1,  0,  0),
	/*   81 */  CR( 6,  2,  F1, 0,  1,  0,  0),
	/*   82 */  CR( 6,  2,  F2, 0,  1,  0,  0),

	/* Opcode 0010: LOAD_2 / 1100: LOAD_C -- Mode 00 */
	/*   83 */  CR( 6,  2,  AM, 0,  0,  0,  1),
	/*   84 */  CR( 8,  0,  PR, 0,  0,  0,  0),
	/*   85 */  CR( 3,  4,  B0, 0,  0,  0,  0),
	/*   86 */  CR( 5,  3,  F0, 0,  0,  0,  0),
	/*   87 */  CR( 3,  4,  B1, 0,  0,  0,  0),
	/*   88 */  CR( 5,  3,  F1, 0,  0,  0,  0),
	/*   89 */  CR( 3,  4,  B2, 0,  0,  0,  0),
	/*   90 */  CR( 5,  3,  F2, 0,  0,  0,  0),
	/*   91 */  CR( 4,  3,  B3, 0,  0,  0,  0),
	/*   92 */  CR( 6,  2,  F3, 0,  0,  0,  0),
	/*   93 */  CR( 7,  1,  B4, 0,  0,  0,  0),
	/*   94 */  CR( 6,  2,  F4, 0,  0,  0,  0),
	/* LOAD_2 only */
	/*   95 */  CR( 5,  0,  IA, 0,  0,  0,  0),
	/*   96 */  CR( 5,  0,  IP, 0,  0,  0,  0),
	/* LOAD_2, LOAD_C -- Mode 10 */
	/*   97 */  CR( 6,  2,  AM, 0,  0,  0,  1),
	/*   98 */  CR( 8,  0,  PR, 0,  0,  0,  0),
	/*   99 */  CR( 6,  1,  B0, 0,  0,  0,  0),
	/*  100 */  CR( 6,  2,  F0, 0,  0,  0,  0),
	/*  101 */  CR( 6,  1,  B1, 0,  0,  0,  0),
	/*  102 */  CR( 6,  2,  F1, 0,  0,  0,  0),
	/*  103 */  CR( 6,  1,  B2, 0,  0,  0,  0),
	/*  104 */  CR( 6,  2,  F2, 0,  0,  0,  0),
	/*  105 */  CR( 6,  1,  B3, 0,  0,  0,  0),
	/*  106 */  CR( 7,  1,  F3, 0,  0,  0,  0),
	/*  107 */  CR( 8,  0,  B4, 0,  0,  0,  0),
	/*  108 */  CR( 8,  0,  F4, 0,  0,  0,  0),
	/* LOAD_2 only */
	/*  109 */  CR( 5,  0,  IA, 0,  0,  0,  0),
	/*  110 */  CR( 5,  0,  IP, 0,  0,  0,  0),

	/* OPCODE 1101: DELTA_D -- Mode 00 and 01 */
	/*  111 */  CR( 4,  2,  AM, 1,  0,  0,  0),
	/*  112 */  CR( 5,  0,  PR, 1,  0,  0,  0),
	/*  113 */  CR( 3,  3,  B3, 1,  0,  0,  0),
	/*  114 */  CR( 4,  2,  F3, 1,  0,  0,  0),
	/*  115 */  CR( 4,  1,  B4, 1,  0,  0,  0),
	/*  116 */  CR( 4,  2,  F4, 1,  0,  0,  0),
	/* Mode 01 only */
	/*  117 */  CR( 5,  0,  B5, 1,  0,  0,  0),
	/*  118 */  CR( 5,  0,  F5, 1,  0,  0,  0),
	/* Mode 10 and 11 */
	/*  119 */  CR( 4,  2,  AM, 1,  0,  0,  0),
	/*  120 */  CR( 5,  0,  PR, 1,  0,  0,  0),
	/*  121 */  CR( 4,  1,  B3, 1,  0,  0,  0),
	/*  122 */  CR( 5,  1,  F3, 1,  0,  0,  0),
	/*  123 */  CR( 5,  0,  B4, 1,  0,  0,  0),
	/*  124 */  CR( 5,  0,  F4, 1,  0,  0,  0),
	/* Mode 11 only */
	/*  125 */  CR( 5,  0,  B5, 1,  0,  0,  0),
	/*  126 */  CR( 5,  0,  F5, 1,  0,  0,  0),

	/* OPCODE 1110: LOAD_E */
	/*  127 */  CR( 6,  2,  AM, 0,  0,  0,  0),
	/*  128 */  CR( 8,  0,  PR, 0,  0,  0,  0),

	/* Opcode 0010: LOAD_2 / 1100: LOAD_C -- Mode 01 */
	/*  129 */  CR( 6,  2,  AM, 0,  0,  0,  1),
	/*  130 */  CR( 8,  0,  PR, 0,  0,  0,  0),
	/*  131 */  CR( 3,  4,  B0, 0,  0,  0,  0),
	/*  132 */  CR( 5,  3,  F0, 0,  0,  0,  0),
	/*  133 */  CR( 3,  4,  B1, 0,  0,  0,  0),
	/*  134 */  CR( 5,  3,  F1, 0,  0,  0,  0),
	/*  135 */  CR( 3,  4,  B2, 0,  0,  0,  0),
	/*  136 */  CR( 5,  3,  F2, 0,  0,  0,  0),
	/*  137 */  CR( 4,  3,  B3, 0,  0,  0,  0),
	/*  138 */  CR( 6,  2,  F3, 0,  0,  0,  0),
	/*  139 */  CR( 7,  1,  B4, 0,  0,  0,  0),
	/*  140 */  CR( 6,  2,  F4, 0,  0,  0,  0),
	/*  141 */  CR( 8,  0,  B5, 0,  0,  0,  0),
	/*  142 */  CR( 8,  0,  F5, 0,  0,  0,  0),
	/* LOAD_2 only */
	/*  143 */  CR( 5,  0,  IA, 0,  0,  0,  0),
	/*  144 */  CR( 5,  0,  IP, 0,  0,  0,  0),
	/* LOAD_2, LOAD_C -- Mode 11 */
	/*  145 */  CR( 6,  2,  AM, 0,  0,  0,  1),
	/*  146 */  CR( 8,  0,  PR, 0,  0,  0,  0),
	/*  147 */  CR( 6,  1,  B0, 0,  0,  0,  0),
	/*  148 */  CR( 6,  2,  F0, 0,  0,  0,  0),
	/*  149 */  CR( 6,  1,  B1, 0,  0,  0,  0),
	/*  150 */  CR( 6,  2,  F1, 0,  0,  0,  0),
	/*  151 */  CR( 6,  1,  B2, 0,  0,  0,  0),
	/*  152 */  CR( 6,  2,  F2, 0,  0,  0,  0),
	/*  153 */  CR( 6,  1,  B3, 0,  0,  0,  0),
	/*  154 */  CR( 7,  1,  F3, 0,  0,  0,  0),
	/*  155 */  CR( 8,  0,  B4, 0,  0,  0,  0),
	/*  156 */  CR( 8,  0,  F4, 0,  0,  0,  0),
	/*  157 */  CR( 8,  0,  B5, 0,  0,  0,  0),
	/*  158 */  CR( 8,  0,  F5, 0,  0,  0,  0),
	/* LOAD_2 only */
	/*  159 */  CR( 5,  0,  IA, 0,  0,  0,  0),
	/*  160 */  CR( 5,  0,  IP, 0,  0,  0,  0),

	/* Opcode 0011: SETMSB_3 / 0101: SETMSB_5 -- Mode 00 only */
	/*  161 */  CR( 0,  0,  0,  0,  0,  1,  0),
	/* Mode 00 and 01 */
	/*  162 */  CR( 6,  2,  AM, 0,  0,  0,  0),
	/*  163 */  CR( 8,  0,  PR, 0,  0,  0,  0),
	/*  164 */  CR( 5,  3,  F0, 0,  1,  0,  0),
	/*  165 */  CR( 5,  3,  F1, 0,  1,  0,  0),
	/*  166 */  CR( 5,  3,  F2, 0,  1,  0,  0),
	/* SETMSB_3 only */
	/*  167 */  CR( 5,  0,  IA, 0,  0,  0,  0),
	/*  168 */  CR( 5,  0,  IP, 0,  0,  0,  0),
	/* Mode 10 only */
	/*  169 */  CR( 0,  0,  0,  0,  0,  1,  0),
	/* Mode 10 and 11 */
	/*  170 */  CR( 6,  2,  AM, 0,  0,  0,  0),
	/*  171 */  CR( 8,  0,  PR, 0,  0,  0,  0),
	/*  172 */  CR( 6,  2,  F0, 0,  1,  0,  0),
	/*  173 */  CR( 6,  2,  F1, 0,  1,  0,  0),
	/*  174 */  CR( 6,  2,  F2, 0,  1,  0,  0),
	/* SETMSB_3 only */
	/*  175 */  CR( 5,  0,  IA, 0,  0,  0,  0),
	/*  176 */  CR( 5,  0,  IP, 0,  0,  0,  0),
};

static const int16_t sp0256_df_idx[16 * 8] =
{
	/*  OPCODE 0000 */      -1, -1,     -1, -1,     -1, -1,     -1, -1,
	/*  OPCODE 1000 */      -1, -1,     -1, -1,     -1, -1,     -1, -1,
	/*  OPCODE 0100 */      17, 22,     17, 24,     25, 30,     25, 32,
	/*  OPCODE 1100 */      83, 94,     129,142,    97, 108,    145,158,
	/*  OPCODE 0010 */      83, 96,     129,144,    97, 110,    145,160,
	/*  OPCODE 1010 */      73, 77,     74, 77,     78, 82,     79, 82,
	/*  OPCODE 0110 */      33, 36,     34, 37,     38, 41,     39, 42,
	/*  OPCODE 1110 */      127,128,    127,128,    127,128,    127,128,
	/*  OPCODE 0001 */      1,  14,     1,  16,     1,  14,     1,  16,
	/*  OPCODE 1001 */      45, 56,     45, 58,     59, 70,     59, 72,
	/*  OPCODE 0101 */      161,166,    162,166,    169,174,    170,174,
	/*  OPCODE 1101 */      111,116,    111,118,    119,124,    119,126,
	/*  OPCODE 0011 */      161,168,    162,168,    169,176,    170,176,
	/*  OPCODE 1011 */      -1, -1,     -1, -1,     -1, -1,     -1, -1,
	/*  OPCODE 0111 */      -1, -1,     -1, -1,     -1, -1,     -1, -1,
	/*  OPCODE 1111 */      0,  0,      0,  0,      0,  0,      0,  0
};

/* ======================================================================== */
/*  BITREV32       -- Bit-reverse a 32-bit number.                          */
/* ======================================================================== */
static inline uint32_t bitrev32(uint32_t val)
{
	val = ((val & 0xFFFF0000u) >> 16) | ((val & 0x0000FFFFu) << 16);
	val = ((val & 0xFF00FF00u) >>  8) | ((val & 0x00FF00FFu) <<  8);
	val = ((val & 0xF0F0F0F0u) >>  4) | ((val & 0x0F0F0F0Fu) <<  4);
	val = ((val & 0xCCCCCCCCu) >>  2) | ((val & 0x33333333u) <<  2);
	val = ((val & 0xAAAAAAAAu) >>  1) | ((val & 0x55555555u) <<  1);
	return val;
}

/* ======================================================================== */
/*  GETB  -- Get up to 8 bits at the current PC.                            */
/* ======================================================================== */
static uint32_t getb(sp0256_t *sp, int len)
{
	uint32_t d0 = rom_rd(sp, (uint32_t)(sp->pc    ) >> 3);
	uint32_t d1 = rom_rd(sp, (uint32_t)(sp->pc + 8) >> 3);
	uint32_t data = ((d1 << 8) | d0) >> (sp->pc & 7);

	sp->pc += len;
	return data & ((1u << len) - 1);
}

/* ======================================================================== */
/*  MICRO -- Emulate the microsequencer.  Executes instructions until the   */
/*           repeat count != 0 or the sequencer halts.                      */
/* ======================================================================== */
static void micro(sp0256_t *sp)
{
	uint8_t immed4, opcode;
	uint16_t cr;
	int ctrl_xfer, repeat, i, idx0, idx1;

	while (sp->filt.rpt <= 0)
	{
		/* Halted: pick up a pending ALD, if any. */
		if (sp->halted && !sp->lrq)
		{
			sp->pc     = sp->ald | (SP0256_ROM_BASE << 3);
			sp->halted = 0;
			sp->lrq    = 1;
			sp->ald    = 0;
			sp->started++;
			for (i = 0; i < 16; i++)
				sp->filt.r[i] = 0;
		}

		/* Still halted: nothing to do. */
		if (sp->halted)
		{
			sp->filt.rpt = 1;
			sp->lrq      = 1;
			sp->ald      = 0;
			for (i = 0; i < 16; i++)
				sp->filt.r[i] = 0;
			sp->sby = 1;
			return;
		}

		immed4    = (uint8_t)getb(sp, 4);
		opcode    = (uint8_t)getb(sp, 4);
		repeat    = 0;
		ctrl_xfer = 0;

		switch (opcode)
		{
			/* OPCODE 0000: RTS / SETPAGE */
			case 0x0:
			{
				if (immed4)
				{
					sp->page = bitrev32(immed4) >> 13;
				}
				else
				{
					uint32_t btrg = (uint32_t)sp->stack;
					sp->stack = 0;
					if (!btrg)
					{
						sp->halted = 1;
						sp->pc     = 0;
					}
					else
					{
						sp->pc = (int)btrg;
					}
					ctrl_xfer = 1;
				}
				break;
			}

			/* OPCODE 0111: JMP, OPCODE 1011: JSR */
			case 0xE:
			case 0xD:
			{
				uint32_t btrg = sp->page
				              | (bitrev32(immed4) >> 17)
				              | (bitrev32(getb(sp, 8)) >> 21);
				ctrl_xfer = 1;
				if (opcode == 0xD)
					sp->stack = (sp->pc + 7) & ~7;
				sp->pc = (int)btrg;
				break;
			}

			/* OPCODE 1000: SETMODE */
			case 0x1:
			{
				sp->mode = ((immed4 & 8) >> 2) | (immed4 & 4) | ((immed4 & 3) << 4);
				break;
			}

			/* All data-loading opcodes and PAUSE. */
			default:
			{
				repeat = immed4 | (sp->mode & 0x30);
				break;
			}
		}
		if (opcode != 1) sp->mode &= 0xF;

		if (ctrl_xfer)
			continue;

		if (!repeat) continue;

		sp->filt.rpt = repeat + 1;

		i    = (opcode << 3) | (sp->mode & 6);
		idx0 = sp0256_df_idx[i++];
		idx1 = sp0256_df_idx[i  ];

		if (idx0 < 0 || idx1 < 0)
			continue;   /* undefined opcode/mode combination */

		for (i = idx0; i <= idx1; i++)
		{
			int len, shf, delta, field, prm, clra, clr5, j;
			int8_t value;

			cr = sp0256_datafmt[i];

			len   = CR_LEN(cr);
			shf   = CR_SHF(cr);
			prm   = CR_PRM(cr);
			clra  = cr & CR_CLRA;
			clr5  = cr & CR_CLR5;
			delta = cr & CR_DELTA;
			field = cr & CR_FIELD;
			value = 0;

			if (clra)
			{
				for (j = 0; j < 16; j++)
					sp->filt.r[j] = 0;
				sp->silent = 1;
			}

			if (clr5)
				sp->filt.r[B5] = sp->filt.r[F5] = 0;

			if (len)
				value = (int8_t)getb(sp, len);
			else
				continue;

			if (delta)
			{
				if (value & (1 << (len - 1)))
					value = (int8_t)(value | (int8_t)(0xFFu << len));
			}

			if (shf)
				value = (int8_t)(value << shf);

			sp->silent = 0;

			if (field)
			{
				sp->filt.r[prm] &= (uint8_t)~(~0u << shf);   /* keep low bits  */
				sp->filt.r[prm] |= (uint8_t)value;           /* merge new MSBs */
				continue;
			}

			if (delta)
			{
				sp->filt.r[prm] = (uint8_t)(sp->filt.r[prm] + value);
				continue;
			}

			sp->filt.r[prm] = (uint8_t)value;
		}

		/* PAUSE: silent, with an equivalent period. */
		if (opcode == 0xF)
		{
			sp->silent = 1;
			sp->filt.r[1] = PER_PAUSE;
		}

		lpc12_regdec(&sp->filt, sp->pitch_pct);

		/* Apply the speed control, and for voiced frames also the pitch     */
		/* control so that a pitch change leaves the duration unchanged.     */
		if (sp->speed_pct != 100 || (sp->pitch_pct != 100 && sp->filt.per))
		{
			int num = repeat * 100 * (sp->filt.per ? sp->pitch_pct : 100);
			int den = sp->speed_pct * 100;
			int r = (num + den / 2) / den;
			sp->filt.rpt = (r < 1 ? 1 : r) + 1;
		}
		TRACE("op=%X mode=%lu rpt=%d amp=%d per=%lu IA=%u IP=%u r0=%02X r1=%02X\n",
		      opcode, (unsigned long)sp->mode, sp->filt.rpt, sp->filt.amp,
		      (unsigned long)sp->filt.per, sp->filt.r[14], sp->filt.r[15],
		      sp->filt.r[0], sp->filt.r[1]);
		break;
	}
}

/* ======================================================================== */
/*  Public interface                                                        */
/* ======================================================================== */
void sp0256_init(sp0256_t *sp, const uint8_t *rom, uint32_t rom_len)
{
	memset(sp, 0, sizeof(*sp));
	sp->rom     = rom;
	sp->rom_len = rom_len;
	sp0256_reset(sp);
}

static int clamp_pct(int v)
{
	return v < SP0256_PCT_MIN ? SP0256_PCT_MIN : v > SP0256_PCT_MAX ? SP0256_PCT_MAX : v;
}

void sp0256_set_speed(sp0256_t *sp, int percent) { sp->speed_pct = clamp_pct(percent); }
void sp0256_set_pitch(sp0256_t *sp, int percent) { sp->pitch_pct = clamp_pct(percent); sp->filt.pitch_pct = sp->pitch_pct; }

void sp0256_reset(sp0256_t *sp)
{
	int speed = sp->speed_pct ? sp->speed_pct : 100;
	int pitch = sp->pitch_pct ? sp->pitch_pct : 100;
	memset(&sp->filt, 0, sizeof(sp->filt));
	sp->speed_pct = speed;
	sp->pitch_pct = pitch;
	sp->filt.pitch_pct = pitch;
	sp->halted   = 1;
	sp->filt.rpt = -1;
	sp->filt.rng = 1;
	sp->lrq      = 1;
	sp->ald      = 0;
	sp->pc       = 0;
	sp->stack    = 0;
	sp->mode     = 0;
	sp->page     = SP0256_ROM_BASE << 3;
	sp->silent   = 1;
	sp->sby      = 1;
}

int sp0256_ald(sp0256_t *sp, uint8_t addr)
{
	if (!sp->lrq)
		return 0;                     /* buffer full: write dropped */

	sp->lrq = 0;
	sp->ald = addr << 4;              /* 2-byte entries, PC in bits */
	sp->sby = 0;
	return 1;
}

int sp0256_lrq_pin(const sp0256_t *sp) { return sp->lrq ? 0 : 1; }
int sp0256_sby_pin(const sp0256_t *sp) { return sp->sby; }
int sp0256_halted (const sp0256_t *sp) { return sp->halted; }

int sp0256_render(sp0256_t *sp, int16_t *out, int n)
{
	int done = 0;
	int stall = 0;

	while (done < n)
	{
		int got;

		if (sp->filt.rpt <= 0)
			micro(sp);

		if (sp->silent && sp->filt.rpt <= 0)
		{
			out[done++] = 0;
			continue;
		}

		got = lpc12_update(&sp->filt, n - done, out + done);
		done += got;

		/* The filter can legitimately return 0 when its repeat count     */
		/* expires on the first sample; the sequencer then advances.  Guard */
		/* against any pathological state by emitting silence.             */
		if (got == 0)
		{
			if (++stall > 8)
			{
				out[done++] = 0;
				stall = 0;
			}
		}
		else
			stall = 0;
	}
	return done;
}
