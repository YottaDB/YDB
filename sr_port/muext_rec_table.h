/****************************************************************
 *                                                              *
 *      Copyright 2010 Fidelity Information Services, Inc       *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

/* New entries should be added at the end to maintain backward compatibility with previous journal extract files */

/*
 * MUEXT_TABLE_ENTRY (muext_rectype,  code0  code1) : code0 and code1 are the 2-digit journal record type
 * in the journal extract file
 */
MUEXT_TABLE_ENTRY (MUEXT_NULL,     '0',	  '0')	/* 00 : NULL       record */
MUEXT_TABLE_ENTRY (MUEXT_PINI,     '0',	  '1')	/* 01 : PINI       record */
MUEXT_TABLE_ENTRY (MUEXT_PFIN,     '0',	  '2')	/* 02 : PFIN       record */
MUEXT_TABLE_ENTRY (MUEXT_EOF,      '0',	  '3')	/* 03 : EOF        record */
MUEXT_TABLE_ENTRY (MUEXT_KILL,     '0',	  '4')	/* 04 : KILL       record */
MUEXT_TABLE_ENTRY (MUEXT_SET,      '0',	  '5')	/* 05 : SET        record */
MUEXT_TABLE_ENTRY (MUEXT_ZTSTART,  '0',	  '6')	/* 06 : ZTSTART    record */
MUEXT_TABLE_ENTRY (MUEXT_ZTCOMMIT, '0',	  '7')	/* 07 : ZTCOMMIT   record */
MUEXT_TABLE_ENTRY (MUEXT_TSTART,   '0',	  '8')	/* 08 : TSTART     record */
MUEXT_TABLE_ENTRY (MUEXT_TCOMMIT,  '0',	  '9')	/* 09 : TCOMMIT    record */
MUEXT_TABLE_ENTRY (MUEXT_ZKILL,    '1',	  '0')	/* 10 : ZKILL      record */
MUEXT_TABLE_ENTRY (MUEXT_ZTWORM,   '1',	  '1')	/* 11 : ZTWORMHOLE record */
MUEXT_TABLE_ENTRY (MUEXT_ZTRIG,	   '1',   '2')  /* 12 : ZTRIGGER   record */
/* End of table (enforces last record has line end) */
