/****************************************************************
 *								*
 *	Copyright 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* New entries should be added at the end to maintain backward compatibility with previous journal files */
/* Note: This is an exception where we have 132+ characters in a line. It is needed so that from a
 *       particular number we can find record type. */

/* adding a new type of record may require a change in the following */
/*	--- jnl_output.c --> the way we get the "time" field in the align record */
/*      --- mur_read_file.c --> in mur_fopen, where we get the time for processing */

/*
JNL_TABLE_ENTRY (record type  extraction      label    sizeof fixed portion
		  enum,		routine
*/

JNL_TABLE_ENTRY (JRT_BAD,    NULL,	       "*BAD*",  0)			/* 0: Catch-all for invalid record types (must be first) */
JNL_TABLE_ENTRY (JRT_PINI,   mur_extract_pini, "PINI",   JRT_PINI_FIXED_SIZE)	/* 1: Process initialization */
JNL_TABLE_ENTRY (JRT_PFIN,   mur_extract_pfin, "PFIN",   JRT_PFIN_FIXED_SIZE)	/* 2: Process termination */
JNL_TABLE_ENTRY (JRT_ZTCOM,  mur_extract_tcom, "ZTCOM",  JRT_ZTCOM_FIXED_SIZE)	/* 3: End of "fenced" transaction */
JNL_TABLE_ENTRY (JRT_KILL,   mur_extract_set,  "KILL",   JRT_KILL_FIXED_SIZE)	/* 4: After-image logical journal transaction */
JNL_TABLE_ENTRY (JRT_FKILL,  mur_extract_set,  "FKILL",  JRT_FKILL_FIXED_SIZE)	/* 5: Like KILL, but the first in a "fenced" transaction */
JNL_TABLE_ENTRY (JRT_GKILL,  mur_extract_set,  "GKILL",  JRT_GKILL_FIXED_SIZE)	/* 6: Like FKILL, but not the first */
JNL_TABLE_ENTRY (JRT_SET,    mur_extract_set,  "SET",    JRT_SET_FIXED_SIZE)	/* 7: After-image logical journal transaction */
JNL_TABLE_ENTRY (JRT_FSET,   mur_extract_set,  "FSET",   JRT_FSET_FIXED_SIZE)	/* 8: Like SET, but the first in a "fenced" transaction */
JNL_TABLE_ENTRY (JRT_GSET,   mur_extract_set,  "GSET",   JRT_GSET_FIXED_SIZE)	/* 9: Like FSET, but not the first */
JNL_TABLE_ENTRY (JRT_PBLK,   mur_extract_pblk, "PBLK",   JRT_PBLK_FIXED_SIZE)	/* 10: Before-image physical journal transaction */
JNL_TABLE_ENTRY (JRT_EPOCH,  mur_extract_epoch,"EPOCH",  JRT_EPOCH_FIXED_SIZE)	/* 11: A "new epoch" */
JNL_TABLE_ENTRY (JRT_EOF,    mur_extract_eof,  "EOF",    JRT_EOF_FIXED_SIZE)	/* 12: End of file */
JNL_TABLE_ENTRY (JRT_TKILL,  mur_extract_set,  "TKILL",  JRT_TKILL_FIXED_SIZE)	/* 13: Like KILL, but the first in a TP transaction */
JNL_TABLE_ENTRY (JRT_UKILL,  mur_extract_set,  "UKILL",  JRT_UKILL_FIXED_SIZE)	/* 14: Like TKILL, but not the first */
JNL_TABLE_ENTRY (JRT_TSET,   mur_extract_set,  "TSET",   JRT_TSET_FIXED_SIZE)	/* 15: Like SET, but the first in a TP transaction */
JNL_TABLE_ENTRY (JRT_USET,   mur_extract_set,  "USET",   JRT_USET_FIXED_SIZE)	/* 16: Like TSET, but not the first */
JNL_TABLE_ENTRY (JRT_TCOM,   mur_extract_tcom, "TCOM",   JRT_TCOM_FIXED_SIZE)	/* 17: End of TP transaction */
JNL_TABLE_ENTRY (JRT_ALIGN,  mur_extract_align,"ALIGN",  JRT_ALIGN_FIXED_SIZE)	/* 18: Align record */
JNL_TABLE_ENTRY (JRT_NULL,   mur_extract_null, "NULL",   JRT_NULL_FIXED_SIZE)	/* 19: Null record */
JNL_TABLE_ENTRY (JRT_ZKILL,  mur_extract_set,  "ZKILL",  JRT_ZKILL_FIXED_SIZE)	/* 20: After-image logical journal transaction */
JNL_TABLE_ENTRY (JRT_FZKILL, mur_extract_set,  "FZKILL", JRT_FZKILL_FIXED_SIZE)	/* 21: Like ZKILL, but the first in a "fenced" transaction */
JNL_TABLE_ENTRY (JRT_GZKILL, mur_extract_set,  "GZKILL", JRT_GZKILL_FIXED_SIZE)	/* 22: Like FZKILL, but not the first */
JNL_TABLE_ENTRY (JRT_TZKILL, mur_extract_set,  "TZKILL", JRT_TZKILL_FIXED_SIZE)	/* 23: Like ZKILL, but the first in a TP transaction */
JNL_TABLE_ENTRY (JRT_UZKILL, mur_extract_set,  "UZKILL", JRT_UZKILL_FIXED_SIZE)	/* 24: Like TZKILL, but not the first */
JNL_TABLE_ENTRY (JRT_INCTN,  mur_extract_inctn,"INCTN",  JRT_INCTN_FIXED_SIZE)	/* 25: Increment curr_tn only, no logical update */
JNL_TABLE_ENTRY (JRT_AIMG,   mur_extract_aimg, "AIMG",   JRT_AIMG_FIXED_SIZE) 	/* 26: After-image physical journal transaction */

