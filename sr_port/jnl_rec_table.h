/****************************************************************
 *                                                              *
 *      Copyright 2001, 2012 Fidelity Information Services, Inc *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

/* New entries should be added at the end to maintain backward compatibility with previous journal files */
/* Note: This is an exception where we have 132+ characters in a line. It is needed so that from a
 *       particular number we can find record type. */

/*
JNL_TABLE_ENTRY (rectype,     extract_rtn,       label,     update,      fixed_size, is_replicated)
*/
JNL_TABLE_ENTRY (JRT_BAD,     NULL,              "*BAD*  ", NA,                FALSE, FALSE)  /* 0: Catch-all for invalid record types (must be first) */
JNL_TABLE_ENTRY (JRT_PINI,    mur_extract_pini,  "PINI   ", NA,                TRUE,  FALSE)  /* 1: Process initialization */
JNL_TABLE_ENTRY (JRT_PFIN,    mur_extract_pfin,  "PFIN   ", NA,                TRUE,  FALSE)  /* 2: Process termination */
JNL_TABLE_ENTRY (JRT_ZTCOM,   mur_extract_tcom,  "ZTCOM  ", ZTCOMREC,          TRUE,  FALSE)  /* 3: End of "fenced" transaction */
JNL_TABLE_ENTRY (JRT_KILL,    mur_extract_set,   "KILL   ", KILLREC,           FALSE, TRUE)   /* 4: After-image logical journal transaction */
JNL_TABLE_ENTRY (JRT_FKILL,   mur_extract_set,   "FKILL  ", KILLREC|FUPDREC,   FALSE, FALSE)  /* 5: Like KILL, but the first in a "fenced" transaction */
JNL_TABLE_ENTRY (JRT_GKILL,   mur_extract_set,   "GKILL  ", KILLREC|GUPDREC,   FALSE, FALSE)  /* 6: Like FKILL, but not the first */
JNL_TABLE_ENTRY (JRT_SET,     mur_extract_set,   "SET    ", SETREC,            FALSE, TRUE)   /* 7: After-image logical journal transaction */
JNL_TABLE_ENTRY (JRT_FSET,    mur_extract_set,   "FSET   ", SETREC|FUPDREC,    FALSE, FALSE)  /* 8: Like SET, but the first in a "fenced" transaction */
JNL_TABLE_ENTRY (JRT_GSET,    mur_extract_set,   "GSET   ", SETREC|GUPDREC,    FALSE, FALSE)  /* 9: Like FSET, but not the first */
JNL_TABLE_ENTRY (JRT_PBLK,    mur_extract_blk,   "PBLK   ", NA,                FALSE, FALSE)  /* 10: Before-image physical journal transaction */
JNL_TABLE_ENTRY (JRT_EPOCH,   mur_extract_epoch, "EPOCH  ", NA,                TRUE,  FALSE)  /* 11: A "new epoch" */
JNL_TABLE_ENTRY (JRT_EOF,     mur_extract_eof,   "EOF    ", NA,                TRUE,  FALSE)  /* 12: End of file */
JNL_TABLE_ENTRY (JRT_TKILL,   mur_extract_set,   "TKILL  ", KILLREC|TUPDREC,   FALSE, TRUE)   /* 13: Like KILL, but the first in a TP transaction */
JNL_TABLE_ENTRY (JRT_UKILL,   mur_extract_set,   "UKILL  ", KILLREC|UUPDREC,   FALSE, TRUE)   /* 14: Like TKILL, but not the first */
JNL_TABLE_ENTRY (JRT_TSET,    mur_extract_set,   "TSET   ", SETREC|TUPDREC,    FALSE, TRUE)   /* 15: Like SET, but the first in a TP transaction */
JNL_TABLE_ENTRY (JRT_USET,    mur_extract_set,   "USET   ", SETREC|UUPDREC,    FALSE, TRUE)   /* 16: Like TSET, but not the first */
JNL_TABLE_ENTRY (JRT_TCOM,    mur_extract_tcom,  "TCOM   ", TCOMREC,           TRUE,  TRUE)   /* 17: End of TP transaction */
JNL_TABLE_ENTRY (JRT_ALIGN,   mur_extract_align, "ALIGN  ", NA,                FALSE, FALSE)  /* 18: Align record */
JNL_TABLE_ENTRY (JRT_NULL,    mur_extract_null,  "NULL   ", NA,                TRUE,  TRUE)   /* 19: Null record */
JNL_TABLE_ENTRY (JRT_ZKILL,   mur_extract_set,   "ZKILL  ", ZKILLREC,          FALSE, TRUE)   /* 20: After-image logical journal transaction */
JNL_TABLE_ENTRY (JRT_FZKILL,  mur_extract_set,   "FZKILL ", ZKILLREC|FUPDREC,  FALSE, FALSE)  /* 21: Like ZKILL, but the first in a "fenced" transaction */
JNL_TABLE_ENTRY (JRT_GZKILL,  mur_extract_set,   "GZKILL ", ZKILLREC|GUPDREC,  FALSE, FALSE)  /* 22: Like FZKILL, but not the first */
JNL_TABLE_ENTRY (JRT_TZKILL,  mur_extract_set,   "TZKILL ", ZKILLREC|TUPDREC,  FALSE, TRUE)   /* 23: Like ZKILL, but the first in a TP transaction */
JNL_TABLE_ENTRY (JRT_UZKILL,  mur_extract_set,   "UZKILL ", ZKILLREC|UUPDREC,  FALSE, TRUE)   /* 24: Like TZKILL, but not the first */
JNL_TABLE_ENTRY (JRT_INCTN,   mur_extract_inctn, "INCTN  ", NA,                TRUE,  FALSE)  /* 25: Increment curr_tn only, no logical update */
JNL_TABLE_ENTRY (JRT_AIMG,    mur_extract_blk,   "AIMG   ", NA,                FALSE, FALSE)  /* 26: After-image physical journal transaction */
JNL_TABLE_ENTRY (JRT_TRIPLE,  NULL,              "TRIPLE ", NA,                TRUE,  TRUE)   /* 27: A REPL_OLD_TRIPLE message minus the 8-byte message
											       *     header ("type" and "len"). Only used in the
											       *     replication pipe. Never part of a journal file. */
JNL_TABLE_ENTRY (JRT_TZTWORM, mur_extract_set,   "TZTWORM", ZTWORMREC|TUPDREC, FALSE, TRUE)   /* 28: If $ZTWORMHOLE is first record in TP */
JNL_TABLE_ENTRY (JRT_UZTWORM, mur_extract_set,   "UZTWORM", ZTWORMREC|UUPDREC, FALSE, TRUE)   /* 29: Like TZTWORM but not the first record in TP */
JNL_TABLE_ENTRY (JRT_TZTRIG,  mur_extract_set,	 "TZTRIG ", ZTRIGREC|TUPDREC,  FALSE, TRUE)   /* 30: If ZTRIGGER is first record in TP */
JNL_TABLE_ENTRY (JRT_UZTRIG,  mur_extract_set,	 "UZTRIG ", ZTRIGREC|UUPDREC,  FALSE, TRUE)   /* 31: Like TZTRIG but not the first record in TP */
JNL_TABLE_ENTRY (JRT_HISTREC, NULL,              "HISTREC", NA,                TRUE,  TRUE)   /* 32: A REPL_HISTREC message minus the 8-byte message
											       *     header ("type" and "len"). Only used in the
											       *     replication pipe. Never part of a journal file. */
JNL_TABLE_ENTRY (JRT_TRUNC,   mur_extract_trunc, "TRUNC  ", NA,                TRUE,  FALSE)  /* 33: Record DB file truncate details */
