/* sp0256_names.h -- SP0256-AL2 allophone names, shared by harness and firmware. */
#pragma once
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/* Allophone codes, in ROM order. */
enum sp0256_allophone {
	AL_PA1 = 0, AL_PA2 = 1, AL_PA3 = 2, AL_PA4 = 3, AL_PA5 = 4, AL_OY = 5, AL_AY = 6, AL_EH = 7,
	AL_KK3 = 8, AL_PP = 9, AL_JH = 10, AL_NN1 = 11, AL_IH = 12, AL_TT2 = 13, AL_RR1 = 14, AL_AX = 15,
	AL_MM = 16, AL_TT1 = 17, AL_DH1 = 18, AL_IY = 19, AL_EY = 20, AL_DD1 = 21, AL_UW1 = 22, AL_AO = 23,
	AL_AA = 24, AL_YY2 = 25, AL_AE = 26, AL_HH1 = 27, AL_BB1 = 28, AL_TH = 29, AL_UH = 30, AL_UW2 = 31,
	AL_AW = 32, AL_DD2 = 33, AL_GG3 = 34, AL_VV = 35, AL_GG1 = 36, AL_SH = 37, AL_ZH = 38, AL_RR2 = 39,
	AL_FF = 40, AL_KK2 = 41, AL_KK1 = 42, AL_ZZ = 43, AL_NG = 44, AL_LL = 45, AL_WW = 46, AL_XR = 47,
	AL_WH = 48, AL_YY1 = 49, AL_CH = 50, AL_ER1 = 51, AL_ER2 = 52, AL_OW = 53, AL_DH2 = 54, AL_SS = 55,
	AL_NN2 = 56, AL_HH2 = 57, AL_OR = 58, AL_AR = 59, AL_YR = 60, AL_GG2 = 61, AL_EL = 62, AL_BB2 = 63,
};

static const char *const sp0256_names[64] = {
	"PA1","PA2","PA3","PA4","PA5","OY","AY","EH","KK3","PP","JH","NN1","IH","TT2","RR1","AX",
	"MM","TT1","DH1","IY","EY","DD1","UW1","AO","AA","YY2","AE","HH1","BB1","TH","UH","UW2",
	"AW","DD2","GG3","VV","GG1","SH","ZH","RR2","FF","KK2","KK1","ZZ","NG","LL","WW","XR",
	"WH","YY1","CH","ER1","ER2","OW","DH2","SS","NN2","HH2","OR","AR","YR","GG2","EL","BB2"
};

/* Datasheet Table 6 durations, ms (nominal, not sequencer time). */
static const int sp0256_table6_ms[64] = {
	 10, 30, 50,100,200,420,260, 70,120,210,140,140, 70,140,170, 70,
	180,100,290,250,280, 70,100,100,100,180,120,130, 80,180,100,260,
	370,160,140,190, 80,160,190,120,150,190,160,210,220,110,180,360,
	200,130,190,160,300,240,240, 90,190,180,330,290,350, 40,190, 50
};

/* Name, alias (NN, RR, TT, DH, DD, UW, HH, GG, KK, YY, ER, BB) or number
 * -> allophone index, or -1. */
static inline int sp0256_lookup(const char *tok)
{
	static const struct { const char *a; int n; } alias[] = {
		{"NN",11},{"RR",14},{"TT",17},{"DH",18},{"DD",21},{"UW",22},{"HH",27},
		{"GG",36},{"KK",42},{"YY",49},{"ER",51},{"BB",28}
	};
	char up[8];
	int i;
	for (i = 0; tok[i] && i < 7; i++) up[i] = (char)toupper((unsigned char)tok[i]);
	up[i] = 0;
	if (isdigit((unsigned char)up[0])) {
		int v = atoi(up);
		return (v >= 0 && v < 64) ? v : -1;
	}
	for (i = 0; i < 64; i++) if (!strcmp(up, sp0256_names[i])) return i;
	for (i = 0; i < (int)(sizeof alias / sizeof alias[0]); i++)
		if (!strcmp(up, alias[i].a)) return alias[i].n;
	return -1;
}
