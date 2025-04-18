/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*	File modified by SNOVER on 10-APR-1986 08:04:39.51    */

#define VXI_HALT  (0x00)
#define VXI_NOP  (0x01)
#define VXI_REI  (0x02)
#define VXI_BPT  (0x03)
#define VXI_RET  (0x04)
#define VXI_RSB  (0x05)
#define VXI_LDPCTX  (0x06)
#define VXI_SVPCTX  (0x07)
#define VXI_CVTPS  (0x08)
#define VXI_CVTSP  (0x09)
#define VXI_INDEX  (0x0A)
#define VXI_CRC  (0x0B)
#define VXI_PROBER  (0x0C)
#define VXI_PROBEW  (0x0D)
#define VXI_INSQUE  (0x0E)
#define VXI_REMQUE  (0x0F)
#define VXI_BSBB  (0x10)
#define VXI_BRB  (0x11)
#define VXI_BNEQ  (0x12)
#define VXI_BEQL  (0x13)
#define VXI_BGTR  (0x14)
#define VXI_BLEQ  (0x15)
#define VXI_JSB  (0x16)
#define VXI_JMP  (0x17)
#define VXI_BGEQ  (0x18)
#define VXI_BLSS  (0x19)
#define VXI_BGTRU  (0x1A)
#define VXI_BLEQU  (0x1B)
#define VXI_BVC  (0x1C)
#define VXI_BVS  (0x1D)
#define VXI_BGEQU  (0x1E)
#define VXI_BLSSU  (0x1F)
#define VXI_ADDP4  (0x20)
#define VXI_ADDP6  (0x21)
#define VXI_SUBP4  (0x22)
#define VXI_SUBP6  (0x23)
#define VXI_CVTPT  (0x24)
#define VXI_MULP  (0x25)
#define VXI_CVTTP  (0x26)
#define VXI_DIVP  (0x27)
#define VXI_MOVC3  (0x28)
#define VXI_CMPC3  (0x29)
#define VXI_SCANC  (0x2A)
#define VXI_SPANC  (0x2B)
#define VXI_MOVC5  (0x2C)
#define VXI_CMPC5  (0x2D)
#define VXI_MOVTC  (0x2E)
#define VXI_MOVTUC  (0x2F)
#define VXI_BSBW  (0x30)
#define VXI_BRW  (0x31)
#define VXI_CVTWL  (0x32)
#define VXI_CVTWB  (0x33)
#define VXI_MOVP  (0x34)
#define VXI_CMPP3  (0x35)
#define VXI_CVTPL  (0x36)
#define VXI_CMPP4  (0x37)
#define VXI_EDITPC  (0x38)
#define VXI_MATCHC  (0x39)
#define VXI_LOCC  (0x3A)
#define VXI_SKPC  (0x3B)
#define VXI_MOVZWL  (0x3C)
#define VXI_ACBW  (0x3D)
#define VXI_MOVAW  (0x3E)
#define VXI_PUSHAW  (0x3F)
#define VXI_ADDF2  (0x40)
#define VXI_ADDF3  (0x41)
#define VXI_SUBF2  (0x42)
#define VXI_SUBF3  (0x43)
#define VXI_MULF2  (0x44)
#define VXI_MULF3  (0x45)
#define VXI_DIVF2  (0x46)
#define VXI_DIVF3  (0x47)
#define VXI_CVTFB  (0x48)
#define VXI_CVTFW  (0x49)
#define VXI_CVTFL  (0x4A)
#define VXI_CVTRFL  (0x4B)
#define VXI_CVTBF  (0x4C)
#define VXI_CVTWF  (0x4D)
#define VXI_CVTLF  (0x4E)
#define VXI_ACBF  (0x4F)
#define VXI_MOVF  (0x50)
#define VXI_CMPF  (0x51)
#define VXI_MNEGF  (0x52)
#define VXI_TSTF  (0x53)
#define VXI_EMODF  (0x54)
#define VXI_POLYF  (0x55)
#define VXI_CVTFD  (0x56)
#define VXI_ADAWI  (0x58)
#define VXI_INSQHI  (0x5C)
#define VXI_INSQTI  (0x5D)
#define VXI_REMQHI  (0x5E)
#define VXI_REMQTI  (0x5F)
#define VXI_ADDD2  (0x60)
#define VXI_ADDD3  (0x61)
#define VXI_SUBD2  (0x62)
#define VXI_SUBD3  (0x63)
#define VXI_MULD2  (0x64)
#define VXI_MULD3  (0x65)
#define VXI_DIVD2  (0x66)
#define VXI_DIVD3  (0x67)
#define VXI_CVTDB  (0x68)
#define VXI_CVTDW  (0x69)
#define VXI_CVTDL  (0x6A)
#define VXI_CVTRDL  (0x6B)
#define VXI_CVTBD  (0x6C)
#define VXI_CVTWD  (0x6D)
#define VXI_CVTLD  (0x6E)
#define VXI_ACBD  (0x6F)
#define VXI_MOVD  (0x70)
#define VXI_CMPD  (0x71)
#define VXI_MNEGD  (0x72)
#define VXI_TSTD  (0x73)
#define VXI_EMODD  (0x74)
#define VXI_POLYD  (0x75)
#define VXI_CVTDF  (0x76)
#define VXI_ASHL  (0x78)
#define VXI_ASHQ  (0x79)
#define VXI_EMUL  (0x7A)
#define VXI_EDIV  (0x7B)
#define VXI_CLRQ  (0x7C)
#define VXI_MOVQ  (0x7D)
#define VXI_MOVAQ  (0x7E)
#define VXI_PUSHAQ  (0x7F)
#define VXI_ADDB2  (0x80)
#define VXI_ADDB3  (0x81)
#define VXI_SUBB2  (0x82)
#define VXI_SUBB3  (0x83)
#define VXI_MULB2  (0x84)
#define VXI_MULB3  (0x85)
#define VXI_DIVB2  (0x86)
#define VXI_DIVB3  (0x87)
#define VXI_BISB2  (0x88)
#define VXI_BISB3  (0x89)
#define VXI_BICB2  (0x8A)
#define VXI_BICB3  (0x8B)
#define VXI_XORB2  (0x8C)
#define VXI_XORB3  (0x8D)
#define VXI_MNEGB  (0x8E)
#define VXI_CASEB  (0x8F)
#define VXI_MOVB  (0x90)
#define VXI_CMPB  (0x91)
#define VXI_MCOMB  (0x92)
#define VXI_BITB  (0x93)
#define VXI_CLRB  (0x94)
#define VXI_TSTB  (0x95)
#define VXI_INCB  (0x96)
#define VXI_DECB  (0x97)
#define VXI_CVTBL  (0x98)
#define VXI_CVTBW  (0x99)
#define VXI_MOVZBL  (0x9A)
#define VXI_MOVZBW  (0x9B)
#define VXI_ROTL  (0x9C)
#define VXI_ACBB  (0x9D)
#define VXI_MOVAB  (0x9E)
#define VXI_PUSHAB  (0x9F)
#define VXI_ADDW2  (0xA0)
#define VXI_ADDW3  (0xA1)
#define VXI_SUBW2  (0xA2)
#define VXI_SUBW3  (0xA3)
#define VXI_MULW2  (0xA4)
#define VXI_MULW3  (0xA5)
#define VXI_DIVW2  (0xA6)
#define VXI_DIVW3  (0xA7)
#define VXI_BISW2  (0xA8)
#define VXI_BISW3  (0xA9)
#define VXI_BICW2  (0xAA)
#define VXI_BICW3  (0xAB)
#define VXI_XORW2  (0xAC)
#define VXI_XORW3  (0xAD)
#define VXI_MNEGW  (0xAE)
#define VXI_CASEW  (0xAF)
#define VXI_MOVW  (0xB0)
#define VXI_CMPW  (0xB1)
#define VXI_MCOMW  (0xB2)
#define VXI_BITW  (0xB3)
#define VXI_CLRW  (0xB4)
#define VXI_TSTW  (0xB5)
#define VXI_INCW  (0xB6)
#define VXI_DECW  (0xB7)
#define VXI_BISPSW  (0xB8)
#define VXI_BICPSW  (0xB9)
#define VXI_POPR  (0xBA)
#define VXI_PUSHR  (0xBB)
#define VXI_CHMK  (0xBC)
#define VXI_CHME  (0xBD)
#define VXI_CHMS  (0xBE)
#define VXI_CHMU  (0xBF)
#define VXI_ADDL2  (0xC0)
#define VXI_ADDL3  (0xC1)
#define VXI_SUBL2  (0xC2)
#define VXI_SUBL3  (0xC3)
#define VXI_MULL2  (0xC4)
#define VXI_MULL3  (0xC5)
#define VXI_DIVL2  (0xC6)
#define VXI_DIVL3  (0xC7)
#define VXI_BISL2  (0xC8)
#define VXI_BISL3  (0xC9)
#define VXI_BICL2  (0xCA)
#define VXI_BICL3  (0xCB)
#define VXI_XORL2  (0xCC)
#define VXI_XORL3  (0xCD)
#define VXI_MNEGL  (0xCE)
#define VXI_CASEL  (0xCF)
#define VXI_MOVL  (0xD0)
#define VXI_CMPL  (0xD1)
#define VXI_MCOML  (0xD2)
#define VXI_BITL  (0xD3)
#define VXI_CLRL  (0xD4)
#define VXI_TSTL  (0xD5)
#define VXI_INCL  (0xD6)
#define VXI_DECL  (0xD7)
#define VXI_ADWC  (0xD8)
#define VXI_SBWC  (0xD9)
#define VXI_MTPR  (0xDA)
#define VXI_MFPR  (0xDB)
#define VXI_MOVPSL  (0xDC)
#define VXI_PUSHL  (0xDD)
#define VXI_MOVAL  (0xDE)
#define VXI_PUSHAL  (0xDF)
#define VXI_BBS  (0xE0)
#define VXI_BBC  (0xE1)
#define VXI_BBSS  (0xE2)
#define VXI_BBCS  (0xE3)
#define VXI_BBSC  (0xE4)
#define VXI_BBCC  (0xE5)
#define VXI_BBSSI  (0xE6)
#define VXI_BBCCI  (0xE7)
#define VXI_BLBS  (0xE8)
#define VXI_BLBC  (0xE9)
#define VXI_FFS  (0xEA)
#define VXI_FFC  (0xEB)
#define VXI_CMPV  (0xEC)
#define VXI_CMPZV  (0xED)
#define VXI_EXTV  (0xEE)
#define VXI_EXTZV  (0xEF)
#define VXI_INSV  (0xF0)
#define VXI_ACBL  (0xF1)
#define VXI_AOBLSS  (0xF2)
#define VXI_AOBLEQ  (0xF3)
#define VXI_SOBGEQ  (0xF4)
#define VXI_SOBGTR  (0xF5)
#define VXI_CVTLB  (0xF6)
#define VXI_CVTLW  (0xF7)
#define VXI_ASHP  (0xF8)
#define VXI_CVTLP  (0xF9)
#define VXI_CALLG  (0xFA)
#define VXI_CALLS  (0xFB)
#define VXI_XFC  (0xFC)
#define VXI_ESCD  (0xFD)
#define VXI_ESCE  (0xFE)
#define VXI_ESCF  (0xFF)
#define VXI_CVTDH  (0x132)
#define VXI_CVTGF  (0x133)
#define VXI_ADDG2  (0x140)
#define VXI_ADDG3  (0x141)
#define VXI_SUBG2  (0x142)
#define VXI_SUBG3  (0x143)
#define VXI_MULG2  (0x144)
#define VXI_MULG3  (0x145)
#define VXI_DIVG2  (0x146)
#define VXI_DIVG3  (0x147)
#define VXI_CVTGB  (0x148)
#define VXI_CVTGW  (0x149)
#define VXI_CVTGL  (0x14A)
#define VXI_CVTRGL  (0x14B)
#define VXI_CVTBG  (0x14C)
#define VXI_CVTWG  (0x14D)
#define VXI_CVTLG  (0x14E)
#define VXI_ACBG  (0x14F)
#define VXI_MOVG  (0x150)
#define VXI_CMPG  (0x151)
#define VXI_MNEGG  (0x152)
#define VXI_TSTG  (0x153)
#define VXI_EMODG  (0x154)
#define VXI_POLYG  (0x155)
#define VXI_CVTGH  (0x156)
#define VXI_ADDH2  (0x160)
#define VXI_ADDH3  (0x161)
#define VXI_SUBH2  (0x162)
#define VXI_SUBH3  (0x163)
#define VXI_MULH2  (0x164)
#define VXI_MULH3  (0x165)
#define VXI_DIVH2  (0x166)
#define VXI_DIVH3  (0x167)
#define VXI_CVTHB  (0x168)
#define VXI_CVTHW  (0x169)
#define VXI_CVTHL  (0x16A)
#define VXI_CVTRHL  (0x16B)
#define VXI_CVTBH  (0x16C)
#define VXI_CVTWH  (0x16D)
#define VXI_CVTLH  (0x16E)
#define VXI_ACBH  (0x16F)
#define VXI_MOVH  (0x170)
#define VXI_CMPH  (0x171)
#define VXI_MNEGH  (0x172)
#define VXI_TSTH  (0x173)
#define VXI_EMODH  (0x174)
#define VXI_POLYH  (0x175)
#define VXI_CVTHG  (0x176)
#define VXI_CLRO  (0x17C)
#define VXI_MOVO  (0x17D)
#define VXI_MOVAO  (0x17E)
#define VXI_PUSHAO  (0x17F)
#define VXI_CVTFH  (0x198)
#define VXI_CVTFG  (0x199)
#define VXI_CVTHF  (0x1F6)
#define VXI_CVTHD  (0x1F7)

extern const char vxi_opcode[][7];
