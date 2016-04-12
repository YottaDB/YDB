/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef FNPC_INCLUDED
#define FNPC_INCLUDED

/* Note, FNPC_MAX should never exceed 254 since the value 255 is used to flag "invalid entry" */
#define FNPC_STRLEN_MIN 15
#define FNPC_MAX 50
#define FNPC_ELEM_MAX 81

#ifdef DEBUG
GBLREF	uint4	process_id;
/* $[Z]PIECE() statistics */
GBLREF	int	c_miss;				/* cache misses (debug) */
GBLREF	int	c_hit;				/* cache hits (debug) */
GBLREF	int	c_small;			/* scanned small string brute force */
GBLREF	int	c_small_pcs;			/* chars scanned by small scan */
GBLREF	int	c_pskip;			/* number of pieces "skipped" */
GBLREF	int	c_pscan;			/* number of pieces "scanned" */
GBLREF	int	c_parscan;			/* number of partial scans (partial cache hits) */
/* Flag we are doing SET $[Z]PIECE() and its statistics fields */
GBLREF	boolean_t	setp_work;		/* The work we are doing is for set $piece */
GBLREF	int	cs_miss;			/* cache misses (debug) */
GBLREF	int	cs_hit;				/* cache hits (debug) */
GBLREF	int	cs_small;			/* scanned small string brute force */
GBLREF	int	cs_small_pcs;			/* chars scanned by small scan */
GBLREF	int	cs_pskip;			/* number of pieces "skipped" */
GBLREF	int	cs_pscan;			/* number of pieces "scanned" */
GBLREF	int	cs_parscan;			/* number of partial scans (partial cache hits) */
GBLREF	int	c_clear;			/* cleared due to (possible) value change */
#  define COUNT_EVENT(x) if (setp_work) ++cs_##x; else ++c_##x;
#  define INCR_COUNT(x,y) if (setp_work) cs_##x += y; else c_##x += y;
#else
#  define COUNT_EVENT(x)
#  define INCR_COUNT(x,y)
#endif

/* The delimiter argument to op_fnp1, opfnzp1, op_setp1, and op_setzp1 is
 * passed as an integer but contains 1-4 chars (zero filled). The unicode
 * versions are interested in all of them but the non-unicode versions are
 * only interested in the first char.
 */
typedef union
{
	int		unichar_val;
	unsigned char	unibytes_val[4];
} delimfmt;

typedef struct fnpc_struct
{
	mstr		last_str;			/* The last string (addr/len) we used in cache */
	int		delim;				/* delimiter used in $[z]piece */
	int		npcs;				/* Number of pieces for which values are filled in */
	int		indx;				/* The index of this piece */
	boolean_t	byte_oriented;			/* True if byte oriented; False if (unicode) char oriented */
	unsigned int	pstart[FNPC_ELEM_MAX + 1];	/* Where each piece starts (last elem holds end of last piece) */
} fnpc;

typedef struct
{
	fnpc		*fnpcsteal;			/* Last stolen cache element */
	fnpc		*fnpcmax;			/* (use addrs to avoid array indexing) */
	fnpc		fnpcs[FNPC_MAX];
} fnpc_area;

#ifdef DEBUG
void	fnpc_stats(void);
#endif


#endif
