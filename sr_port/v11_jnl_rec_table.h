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

JNL_TABLE_ENTRY (JRT_BAD,    NULL,	       "*BAD*",  0)	/* 0: Catch-all for invalid record types (must be first) */
JNL_TABLE_ENTRY (JRT_PINI,   mur_extract_pini, "PINI",   sizeof(struct_jrec_pini))		/* 1: Process initialization */
JNL_TABLE_ENTRY (JRT_PFIN,   mur_extract_pfin, "PFIN",   sizeof(struct_jrec_pfin))		/* 2: Process termination */
JNL_TABLE_ENTRY (JRT_ZTCOM,  mur_extract_tcom, "ZTCOM",  sizeof(struct_jrec_tcom))		/* 3: End of "fenced" transaction */
JNL_TABLE_ENTRY (JRT_KILL,   mur_extract_set,  "KILL",   sizeof(fixed_jrec_kill_set))	/* 4: After-image logical journal transaction */
JNL_TABLE_ENTRY (JRT_FKILL,  mur_extract_set,  "FKILL",  sizeof(fixed_jrec_tp_kill_set))	/* 5: Like KILL, but the first in a "fenced" transaction */
JNL_TABLE_ENTRY (JRT_GKILL,  mur_extract_set,  "GKILL",  sizeof(fixed_jrec_tp_kill_set))	/* 6: Like FKILL, but not the first */
JNL_TABLE_ENTRY (JRT_SET,    mur_extract_set,  "SET",    sizeof(fixed_jrec_kill_set))	/* 7: After-image logical journal transaction */
JNL_TABLE_ENTRY (JRT_FSET,   mur_extract_set,  "FSET",   sizeof(fixed_jrec_tp_kill_set))	/* 8: Like SET, but the first in a "fenced" transaction */
JNL_TABLE_ENTRY (JRT_GSET,   mur_extract_set,  "GSET",   sizeof(fixed_jrec_tp_kill_set))	/* 9: Like FSET, but not the first */
JNL_TABLE_ENTRY (JRT_PBLK,   mur_extract_pblk, "PBLK",   sizeof(struct_jrec_pblk) - 1 - PADDED)	/* 10: Before-image physical journal transaction */
JNL_TABLE_ENTRY (JRT_EPOCH,  mur_extract_epoch,"EPOCH",  sizeof(struct_jrec_epoch))	/* 11: A "new epoch" */
JNL_TABLE_ENTRY (JRT_EOF,    mur_extract_eof,  "EOF",    sizeof(struct_jrec_eof))		/* 12: End of file */
JNL_TABLE_ENTRY (JRT_TKILL,  mur_extract_set,  "TKILL",  sizeof(fixed_jrec_tp_kill_set))	/* 13: Like KILL, but the first in a TP transaction */
JNL_TABLE_ENTRY (JRT_UKILL,  mur_extract_set,  "UKILL",  sizeof(fixed_jrec_tp_kill_set))	/* 14: Like TKILL, but not the first */
JNL_TABLE_ENTRY (JRT_TSET,   mur_extract_set,  "TSET",   sizeof(fixed_jrec_tp_kill_set))	/* 15: Like SET, but the first in a TP transaction */
JNL_TABLE_ENTRY (JRT_USET,   mur_extract_set,  "USET",   sizeof(fixed_jrec_tp_kill_set))	/* 16: Like TSET, but not the first */
JNL_TABLE_ENTRY (JRT_TCOM,   mur_extract_tcom, "TCOM",   sizeof(struct_jrec_tcom))		/* 17: End of TP transaction */
JNL_TABLE_ENTRY (JRT_ALIGN,  mur_extract_align,"ALIGN",  sizeof(struct_jrec_align))	/* 18: Align record */
JNL_TABLE_ENTRY (JRT_NULL,   mur_extract_null, "NULL",   sizeof(struct_jrec_null))		/* 19: Null record */
JNL_TABLE_ENTRY (JRT_ZKILL,  mur_extract_set,  "ZKILL",  sizeof(fixed_jrec_kill_set))	/* 20: After-image logical journal transaction */
JNL_TABLE_ENTRY (JRT_FZKILL, mur_extract_set,  "FZKILL", sizeof(fixed_jrec_tp_kill_set))	/* 21: Like ZKILL, but the first in a "fenced" transaction */
JNL_TABLE_ENTRY (JRT_GZKILL, mur_extract_set,  "GZKILL", sizeof(fixed_jrec_tp_kill_set))	/* 22: Like FZKILL, but not the first */
JNL_TABLE_ENTRY (JRT_TZKILL, mur_extract_set,  "TZKILL", sizeof(fixed_jrec_tp_kill_set))	/* 23: Like ZKILL, but the first in a TP transaction */
JNL_TABLE_ENTRY (JRT_UZKILL, mur_extract_set,  "UZKILL", sizeof(fixed_jrec_tp_kill_set))	/* 24: Like TZKILL, but not the first */
JNL_TABLE_ENTRY (JRT_INCTN,  mur_extract_inctn,"INCTN",  sizeof(struct_jrec_inctn))	/* 25: Increment curr_tn only, no logical update */
JNL_TABLE_ENTRY (JRT_AIMG,   mur_extract_aimg, "AIMG",   sizeof(struct_jrec_after_image) - 1 - PADDED) /* 26: After-image physical journal transaction */
