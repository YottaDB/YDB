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

/* adding a new type of record may require a change in the following */
/*	--- jnl_output.c --> the way we get the "time" field in the align record */
/*      --- mur_read_file.c --> in mur_fopen, where we get the time for processing */

/*
JNL_TABLE_ENTRY (record type  extraction      label    sizeof fixed portion
		  enum,		routine
*/

JNL_TABLE_ENTRY (JRT_BAD,   NULL,	      "*BAD*", 0)	/* 0: Catch-all for invalid record types (must be first) */
JNL_TABLE_ENTRY (JRT_PINI,  mur_extract_pini, "PINI",  sizeof(struct jrec_pini_struct))		/* 1: Process initialization */
JNL_TABLE_ENTRY (JRT_PFIN,  mur_extract_pfin, "PFIN",  sizeof(struct jrec_pfin_struct))		/* 2: Process termination */
JNL_TABLE_ENTRY (JRT_ZTCOM, mur_extract_tcom, "ZTCOM", sizeof(struct jrec_tcom_struct))		/* 3: End of "fenced" transaction */
JNL_TABLE_ENTRY (JRT_KILL,  mur_extract_set,  "KILL",  sizeof(struct fixed_jrec_kill_set_struct))	/* 4: After-image logical journal transaction */
JNL_TABLE_ENTRY (JRT_FKILL, mur_extract_set,  "FKILL", sizeof(struct fixed_jrec_tp_kill_set_struct))	/* 5: Like KILL, but the first in a "fenced" transaction */
JNL_TABLE_ENTRY (JRT_GKILL, mur_extract_set,  "GKILL", sizeof(struct fixed_jrec_tp_kill_set_struct))	/* 6: Like FKILL, but not the first */
JNL_TABLE_ENTRY (JRT_SET,   mur_extract_set,  "SET",   sizeof(struct fixed_jrec_kill_set_struct))	/* 7: After-image logical journal transaction */
JNL_TABLE_ENTRY (JRT_FSET,  mur_extract_set,  "FSET",  sizeof(struct fixed_jrec_tp_kill_set_struct))	/* 8: Like SET, but the first in a "fenced" transaction */
JNL_TABLE_ENTRY (JRT_GSET,  mur_extract_set,  "GSET",  sizeof(struct fixed_jrec_tp_kill_set_struct))	/* 9: Like FSET, but not the first */
JNL_TABLE_ENTRY (JRT_PBLK,  mur_extract_pblk, "PBLK",  sizeof(struct jrec_pblk_struct) - 1 - PADDED)	/* 10: Before-image physical journal transaction */
JNL_TABLE_ENTRY (JRT_EPOCH, mur_extract_epoch,"EPOCH", sizeof(struct jrec_epoch_struct))	/* 11: A "new epoch" */
JNL_TABLE_ENTRY (JRT_EOF,   mur_extract_eof,  "EOF",   sizeof(struct jrec_eof_struct))		/* 12: End of file */

/* The following were added here at the end to maintain backward compatibility with previous journal files */
JNL_TABLE_ENTRY (JRT_TKILL, mur_extract_set,  "TKILL", sizeof(struct fixed_jrec_tp_kill_set_struct))	/* 13: Like KILL, but the first in a TP transaction */
JNL_TABLE_ENTRY (JRT_UKILL, mur_extract_set,  "UKILL", sizeof(struct fixed_jrec_tp_kill_set_struct))	/* 14: Like TKILL, but not the first */
JNL_TABLE_ENTRY (JRT_TSET,  mur_extract_set,  "TSET",  sizeof(struct fixed_jrec_tp_kill_set_struct))	/* 15: Like SET, but the first in a TP transaction */
JNL_TABLE_ENTRY (JRT_USET,  mur_extract_set,  "USET",  sizeof(struct fixed_jrec_tp_kill_set_struct))	/* 16: Like TSET, but not the first */
JNL_TABLE_ENTRY (JRT_TCOM,  mur_extract_tcom, "TCOM",  sizeof(struct jrec_tcom_struct))		/* 17: End of TP transaction */
JNL_TABLE_ENTRY (JRT_ALIGN, mur_extract_align,"ALIGN", sizeof(struct jrec_align_struct))	/* 18: Align record */
JNL_TABLE_ENTRY (JRT_NULL,  mur_extract_null, "NULL",  sizeof(struct jrec_null_struct))		/* 19: Null record */
JNL_TABLE_ENTRY (JRT_ZKILL,  mur_extract_set,  "ZKILL",  sizeof(struct fixed_jrec_kill_set_struct))	/* 20: After-image logical journal transaction */
JNL_TABLE_ENTRY (JRT_FZKILL, mur_extract_set,  "FZKILL", sizeof(struct fixed_jrec_tp_kill_set_struct))	/* 21: Like ZKILL, but the first in a "fenced" transaction */
JNL_TABLE_ENTRY (JRT_GZKILL, mur_extract_set,  "GZKILL", sizeof(struct fixed_jrec_tp_kill_set_struct))	/* 22: Like FZKILL, but not the first */
JNL_TABLE_ENTRY (JRT_TZKILL, mur_extract_set,  "TZKILL", sizeof(struct fixed_jrec_tp_kill_set_struct))	/* 23: Like ZKILL, but the first in a TP transaction */
JNL_TABLE_ENTRY (JRT_UZKILL, mur_extract_set,  "UZKILL", sizeof(struct fixed_jrec_tp_kill_set_struct))	/* 24: Like TZKILL, but not the first */
