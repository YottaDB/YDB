/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

struct exec {
	short	a_magic;	/* magic number */
	short	a_stamp;	/* version stamp - RTU 2.0+ uses this - see below */
uint4	a_text;		/* size of text segment */
uint4	a_data;		/* size of initialized data */
uint4	a_bss;		/* size of uninitialized data */
uint4	a_syms;		/* size of symbol table */
uint4	a_entry;	/* entry point */
uint4	a_trsize;	/* size of text relocation */
uint4	a_drsize;	/* size of data relocation */
};

/*
 * Format of a relocation datum.
 */
struct relocation_info {
	int	r_address;	/* address which is relocated */
unsigned int	r_symbolnum:24,	/* local symbol ordinal */
		r_pcrel:1, 	/* was relocated pc relative already */
		r_length:2,	/* 0=byte, 1=word, 2=int4 */
		r_extern:1,	/* does not include value of sym referenced */
		r_pad:4;		/* nothing, yet */
};

struct rel_table {
	struct rel_table	*next, *resolve;
	struct relocation_info r;
};

/*
 * Format of a symbol table entry; this file is included by <a.out.h>
 * and should be used if you aren't interested the a.out header
 * or relocation information.
 */
struct	nlist {
	int4	n_strx;		/* index into file string table */
unsigned char	n_type;		/* type flag, i.e. N_TEXT etc; see below */
	char	n_other;	/* unused */
	short	n_desc;		/* see <stab.h> */
uint4	n_value;	/* value of this symbol (or sdb offset) */
};

struct sym_table {
	struct sym_table *next;
	struct nlist n;
	struct rel_table *resolve;
	unsigned short name_len;
	unsigned char name[1];
};

/*
 * Simple values for n_type.
 */
#define	N_UNDF	0x0		/* undefined */
#define	N_ABS	0x2		/* absolute */
#define	N_TEXT	0x4		/* text */
#define	N_DATA	0x6		/* data */
#define	N_BSS	0x8		/* bss */
#define	N_COMM	0x12		/* common (internal to ld) */
#define	N_IPCOMM 0x16		/* initialized private */
#define	N_PCOMM	0x18		/* uninitialized private */
#define	N_FN	0x1f		/* file name symbol */

#define	N_EXT	01		/* external bit, or'ed in */
#define	N_TYPE	0x1e		/* mask for all the type bits */

