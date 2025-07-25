/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved. *
 *								*
 * Copyright (c) 2017-2025 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
/* Defines opcodes and their associated octypes.
 * New opcodes should go at the end to preserve ABI. */



/* start at line 20 to make arithmetic (add 20) easy for those debugging compilation */
OPCODE_DEF(OC_NOOP, (OCT_CGSKIP))
OPCODE_DEF(OC_PARAMETER, (OCT_CGSKIP))
OPCODE_DEF(OC_VAR, (OCT_MVADDR | OCT_MVAL | OCT_CGSKIP | OCT_EXPRLEAF))
OPCODE_DEF(OC_LIT, (OCT_MVAL | OCT_CGSKIP | OCT_EXPRLEAF))
OPCODE_DEF(OC_ADD, (OCT_MVAL | OCT_ARITH))
OPCODE_DEF(OC_SUB, (OCT_MVAL | OCT_ARITH))
OPCODE_DEF(OC_MUL, (OCT_MVAL | OCT_ARITH))
OPCODE_DEF(OC_DIV, (OCT_MVAL | OCT_ARITH))
OPCODE_DEF(OC_IDIV, (OCT_MVAL | OCT_ARITH))
OPCODE_DEF(OC_MOD, (OCT_MVAL | OCT_ARITH))
OPCODE_DEF(OC_NEG, (OCT_MVAL | OCT_UNARY))
OPCODE_DEF(OC_FORCENUM, (OCT_MVAL | OCT_UNARY))
OPCODE_DEF(OC_CAT, (OCT_MVAL))
OPCODE_DEF(OC_SRCHINDX, (OCT_MVADDR | OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_GETINDX, (OCT_MVADDR | OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_PUTINDX, (OCT_MVADDR | OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_GVNAME, (OCT_NULL))
OPCODE_DEF(OC_GVNAKED, (OCT_NULL))
OPCODE_DEF(OC_ZALLOCATE, (OCT_NULL))
OPCODE_DEF(OC_ZDEALLOCATE, (OCT_NULL))
OPCODE_DEF(OC_GVGET, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_GVINCR, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_GVPUT, (OCT_NULL))
OPCODE_DEF(OC_GVKILL, (OCT_NULL))
OPCODE_DEF(OC_GVORDER, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_GVNEXT, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_GVDATA, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNASCII, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNCHAR, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNDATA, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNEXTRACT, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNFIND, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNFNUMBER, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNGET, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNGVGET, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNINCR, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNJ2, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNJ3, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNLENGTH, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNPOPULATION, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNNEXT, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNORDER, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNPIECE, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNRANDOM, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNTEXT, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZFILE, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZGETDVI, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZGETJPI, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZGETSYI, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZM, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZPARSE, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZPID, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZPRIV, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZSEA, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZSETPRV, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_SVGET, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_SVPUT, (OCT_NULL))
OPCODE_DEF(OC_LT, (OCT_BOOL | OCT_REL))
OPCODE_DEF(OC_NLT, (OCT_BOOL | OCT_REL | OCT_NEGATED))
OPCODE_DEF(OC_GT, (OCT_BOOL | OCT_REL))
OPCODE_DEF(OC_NGT, (OCT_BOOL | OCT_REL | OCT_NEGATED))
OPCODE_DEF(OC_EQU, (OCT_BOOL | OCT_REL))
OPCODE_DEF(OC_NEQU, (OCT_BOOL | OCT_REL | OCT_NEGATED))
OPCODE_DEF(OC_CONTAIN, (OCT_BOOL | OCT_REL))
OPCODE_DEF(OC_NCONTAIN, (OCT_BOOL | OCT_REL | OCT_NEGATED))
OPCODE_DEF(OC_FOLLOW, (OCT_BOOL | OCT_REL))
OPCODE_DEF(OC_NFOLLOW, (OCT_BOOL | OCT_REL | OCT_NEGATED))
OPCODE_DEF(OC_PATTERN, (OCT_BOOL | OCT_REL))
OPCODE_DEF(OC_NPATTERN, (OCT_BOOL | OCT_REL | OCT_NEGATED))
OPCODE_DEF(OC_AND, (OCT_BOOL))
OPCODE_DEF(OC_NAND, (OCT_BOOL | OCT_NEGATED))
OPCODE_DEF(OC_OR, (OCT_BOOL))
OPCODE_DEF(OC_NOR, (OCT_BOOL | OCT_NEGATED))
OPCODE_DEF(OC_COM, (OCT_BOOL | OCT_UNARY))
OPCODE_DEF(OC_BREAK, (OCT_NULL))
OPCODE_DEF(OC_CLOSE, (OCT_NULL))
OPCODE_DEF(OC_HANG, (OCT_NULL))
OPCODE_DEF(OC_JOB, (OCT_NULL))
OPCODE_DEF(OC_KILL, (OCT_NULL))
OPCODE_DEF(OC_KILLALL, (OCT_NULL))
OPCODE_DEF(OC_LKNAME, (OCT_NULL))
OPCODE_DEF(OC_LCKINCR, (OCT_NULL))
OPCODE_DEF(OC_LCKDECR, (OCT_NULL))
OPCODE_DEF(OC_LOCK, (OCT_NULL))
OPCODE_DEF(OC_UNLOCK, (OCT_NULL))
OPCODE_DEF(OC_XNEW, (OCT_NULL))
OPCODE_DEF(OC_NEWVAR, (OCT_NULL))
OPCODE_DEF(OC_CDADDR, (OCT_CDADDR))
OPCODE_DEF(OC_OPEN, (OCT_NULL))
OPCODE_DEF(OC_RET, (OCT_NULL))
OPCODE_DEF(OC_READ, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_RDONE, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_READFL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_STO, (OCT_NULL))
OPCODE_DEF(OC_SETPIECE, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_USE, (OCT_NULL))
OPCODE_DEF(OC_VIEW, (OCT_NULL))
OPCODE_DEF(OC_WRITE, (OCT_NULL))
OPCODE_DEF(OC_WTONE, (OCT_NULL))
OPCODE_DEF(OC_WTTAB, (OCT_NULL))
OPCODE_DEF(OC_WTFF, (OCT_NULL))
OPCODE_DEF(OC_WTEOL, (OCT_NULL))
OPCODE_DEF(OC_SETZBRK, (OCT_NULL))
OPCODE_DEF(OC_ZCONT, (OCT_NULL))
OPCODE_DEF(OC_ZEDIT, (OCT_NULL))
OPCODE_DEF(OC_ZGOTO, (OCT_NULL))
OPCODE_DEF(OC_ZLINK, (OCT_NULL))
OPCODE_DEF(OC_ZMESS, (OCT_NULL))
OPCODE_DEF(OC_ZPRINT, (OCT_NULL))
OPCODE_DEF(OC_ZSHOW, (OCT_NULL))
OPCODE_DEF(OC_ZSYSTEM, (OCT_NULL))
OPCODE_DEF(OC_WATCHMOD, (OCT_NULL))
OPCODE_DEF(OC_WATCHREF, (OCT_NULL))
OPCODE_DEF(OC_LVZWRITE, (OCT_NULL))
OPCODE_DEF(OC_GVZWRITE, (OCT_NULL))
OPCODE_DEF(OC_RTERROR, (OCT_NULL))
OPCODE_DEF(OC_CALL, (OCT_JUMP))  /* do f */
OPCODE_DEF(OC_EXTCALL, (OCT_NULL))
OPCODE_DEF(OC_JMP, (OCT_JUMP))
OPCODE_DEF(OC_EXTJMP, (OCT_NULL))
OPCODE_DEF(OC_LABADDR, (ARLINK_ONLY(OCT_MINT) NON_ARLINK_ONLY(OCT_CDADDR)))
OPCODE_DEF(OC_RHDADDR, (ARLINK_ONLY(OCT_MINT) NON_ARLINK_ONLY(OCT_CDADDR)))
OPCODE_DEF(OC_CURRHD, (ARLINK_ONLY(OCT_MINT) NON_ARLINK_ONLY(OCT_CDADDR)))
OPCODE_DEF(OC_CURRTN, (OCT_MVAL))
OPCODE_DEF(OC_LDADDR, (OCT_NULL))
OPCODE_DEF(OC_INDGLVN, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_INDPAT, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_INDFUN, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_COMMARG, (OCT_NULL))
OPCODE_DEF(OC_INDLVADR, (OCT_MVADDR | OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_INDSET, (OCT_NULL))
OPCODE_DEF(OC_GVEXTNAM, (OCT_NULL))
OPCODE_DEF(OC_INDNAME, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_INDTEXT, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_COMVAL, (OCT_MVAL | OCT_COERCE | OCT_UNARY))
OPCODE_DEF(OC_GVSAVTARG, (OCT_MVAL))
OPCODE_DEF(OC_GVRECTARG, (OCT_NULL))
OPCODE_DEF(OC_COMINT, (OCT_MINT | OCT_COERCE))
OPCODE_DEF(OC_FNTRANSLATE, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_COBOOL, (OCT_BOOL | OCT_COERCE | OCT_UNARY))
OPCODE_DEF(OC_BOOLINIT, (OCT_BOOL | OCT_EXPRLEAF))
OPCODE_DEF(OC_BOOLFINI, (OCT_NULL))
OPCODE_DEF(OC_SETTEST, (OCT_NULL))
OPCODE_DEF(OC_CLRTEST, (OCT_NULL))
OPCODE_DEF(OC_FORLOOP, (OCT_NULL))
OPCODE_DEF(OC_NUMCMP, (OCT_BOOL | OCT_REL))
OPCODE_DEF(OC_JMPEQU, (OCT_JUMP))
OPCODE_DEF(OC_JMPNEQ, (OCT_JUMP))
OPCODE_DEF(OC_JMPGTR, (OCT_JUMP))
OPCODE_DEF(OC_JMPLEQ, (OCT_JUMP))
OPCODE_DEF(OC_JMPLSS, (OCT_JUMP))
OPCODE_DEF(OC_JMPGEQ, (OCT_JUMP))
OPCODE_DEF(OC_STOTEMP, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_PASSTHRU, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_LVPATWRITE, (OCT_NULL))
OPCODE_DEF(OC_JMPAT, (OCT_NULL))
OPCODE_DEF(OC_IRETMVAL, (OCT_NULL))
OPCODE_DEF(OC_FNZDATE, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_IRETMVAD, (OCT_NULL))
OPCODE_DEF(OC_HARDRET, (OCT_NULL))
OPCODE_DEF(OC_GETTRUTH, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_LINESTART, (OCT_NULL))
OPCODE_DEF(OC_FORINIT, (OCT_NULL))
OPCODE_DEF(OC_FETCH, (OCT_NULL))
OPCODE_DEF(OC_LINEFETCH, (OCT_NULL))
OPCODE_DEF(OC_ILIT, (OCT_MINT | OCT_CGSKIP | OCT_EXPRLEAF))
OPCODE_DEF(OC_CDLIT, (OCT_CDADDR | OCT_CGSKIP | OCT_EXPRLEAF))
OPCODE_DEF(OC_IGETSRC, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_XKILL, (OCT_NULL))
OPCODE_DEF(OC_ZATTACH, (OCT_NULL))
OPCODE_DEF(OC_FNZCALL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_VXCMPL, (OCT_BOOL))
OPCODE_DEF(OC_INDLVARG, (OCT_MVAL))
OPCODE_DEF(OC_FORCHK1, (OCT_NULL))
OPCODE_DEF(OC_CVTPARM, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_ZPREVIOUS, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZPREVIOUS, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_GVQUERY, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNQUERY, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_BINDPARM, (OCT_NULL))
OPCODE_DEF(OC_RETARG, (OCT_NULL))
OPCODE_DEF(OC_EXFUN, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_EXTEXFUN, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_EXCAL, (OCT_NULL))
OPCODE_DEF(OC_EXTEXCAL, (OCT_NULL))
OPCODE_DEF(OC_ZHELP, (OCT_NULL))
OPCODE_DEF(OC_FNP1, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_ZG1, (OCT_NULL))
OPCODE_DEF(OC_NEWINTRINSIC, (OCT_NULL))
OPCODE_DEF(OC_GVZWITHDRAW, (OCT_NULL))
OPCODE_DEF(OC_LVZWITHDRAW, (OCT_NULL))
OPCODE_DEF(OC_NULLEXP, (OCT_MVADDR | OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_EXFUNRET, (OCT_NULL))
OPCODE_DEF(OC_FNLVNAME, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_LKEXTNAME, (OCT_NULL))
OPCODE_DEF(OC_FORLCLDO, (OCT_JUMP))    /* CALL inside a FOR loop that has locals */
OPCODE_DEF(OC_INDRZSHOW, (OCT_NULL))
OPCODE_DEF(OC_ZSHOWLOC, (OCT_NULL))
OPCODE_DEF(OC_ZSTEP, (OCT_NULL))
OPCODE_DEF(OC_ZSTEPACT, (OCT_NULL))
OPCODE_DEF(OC_CONUM, (OCT_NULL))
OPCODE_DEF(OC_LKINIT, (OCT_NULL))
OPCODE_DEF(OC_RESTARTPC, (OCT_NULL))
OPCODE_DEF(OC_FNZTRNLNM, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_ZTSTART, (OCT_NULL))
OPCODE_DEF(OC_ZTCOMMIT, (OCT_NULL))
OPCODE_DEF(OC_FNVIEW, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_STOLIT, (OCT_NULL))
OPCODE_DEF(OC_FNZGETLKI, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZLKID, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_INDLVNAMADR, (OCT_MVADDR | OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_CALLSP, (OCT_JUMP))
OPCODE_DEF(OC_TIMTRU, (OCT_NULL))
OPCODE_DEF(OC_IOCONTROL, (OCT_NULL))
OPCODE_DEF(OC_FGNCAL, (OCT_NULL))
OPCODE_DEF(OC_FNFGNCAL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_RHDADDR1, (ARLINK_ONLY(OCT_MINT) NON_ARLINK_ONLY(OCT_CDADDR)))
OPCODE_DEF(OC_ZCOMPILE, (OCT_NULL))
OPCODE_DEF(OC_TSTART, (OCT_NULL))
OPCODE_DEF(OC_TROLLBACK, (OCT_NULL))
OPCODE_DEF(OC_TRESTART, (OCT_NULL))
OPCODE_DEF(OC_TCOMMIT, (OCT_NULL))
OPCODE_DEF(OC_EXP, (OCT_MVAL | OCT_ARITH))
OPCODE_DEF(OC_FNGET2, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_INDINCR, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNNAME, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_INDFNNAME, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNLVPRVNAME, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_GVO2, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNLVNAMEO2, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNO2, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_INDO2, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZBITSTR, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZBITLEN, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZBITGET, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZBITSET, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZBITCOUN, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZBITFIND, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZBITNOT, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZBITAND, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZBITOR, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZBITXOR, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FGNLOOKUP, (OCT_CDADDR))
OPCODE_DEF(OC_SORTSAFTER, (OCT_BOOL | OCT_REL))
OPCODE_DEF(OC_NSORTSAFTER, (OCT_BOOL | OCT_REL | OCT_NEGATED))
OPCODE_DEF(OC_FNGVGET1, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNGET1, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_SETP1, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZQGBLMOD, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_SETEXTRACT, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_INDDEVPARMS, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_INDMERGE, (OCT_NULL))
OPCODE_DEF(OC_MERGE, (OCT_NULL))
OPCODE_DEF(OC_MERGE_GVARG, (OCT_NULL))
OPCODE_DEF(OC_MERGE_LVARG, (OCT_MVADDR | OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_M_SRCHINDX, (OCT_MVADDR | OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNSTACK1, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNSTACK2, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNQLENGTH, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNQSUBSCR, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNREVERSE, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_PSVPUT, (OCT_NULL))
OPCODE_DEF(OC_FNZJOBEXAM, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZSIGPROC, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_TRIPSIZE, (OCT_MVAL | OCT_CGSKIP | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZASCII, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZCHAR, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZEXTRACT, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_SETZEXTRACT, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZFIND, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZJ2, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZLENGTH, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZPOPULATION, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZPIECE, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_SETZPIECE, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZP1, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_SETZP1, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZTRANSLATE, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZCONVERT2, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZCONVERT3, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZWIDTH, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZWRITE, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZSUBSTR, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_SETALS2ALS, (OCT_NULL))
OPCODE_DEF(OC_SETALSIN2ALSCT, (OCT_NULL))
OPCODE_DEF(OC_SETALSCTIN2ALS, (OCT_NULL))
OPCODE_DEF(OC_SETALSCT2ALSCT, (OCT_NULL))
OPCODE_DEF(OC_KILLALIAS, (OCT_NULL))
OPCODE_DEF(OC_KILLALIASALL, (OCT_NULL))
OPCODE_DEF(OC_FNZDATA, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_CLRALSVARS, (OCT_NULL))
OPCODE_DEF(OC_FNZAHANDLE, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZTRIGGER, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_EXFUNRETALS, (OCT_NULL))
OPCODE_DEF(OC_SETFNRETIN2ALS, (OCT_NULL))
OPCODE_DEF(OC_SETFNRETIN2ALSCT, (OCT_NULL))
OPCODE_DEF(OC_ZTRIGGER, (OCT_NULL))
OPCODE_DEF(OC_ZWRITESVN, (OCT_EXPRLEAF))
OPCODE_DEF(OC_RFRSHINDX, (OCT_MVADDR | OCT_MVAL))
OPCODE_DEF(OC_SAVPUTINDX, (OCT_MVADDR | OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FORFREEINDX, (OCT_NULL))
OPCODE_DEF(OC_FORNESTLVL, (OCT_NULL))
OPCODE_DEF(OC_ZHALT, (OCT_NULL))
OPCODE_DEF(OC_IGETDST, (OCT_MVADDR | OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_INDGET1, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_GLVNPOP, (OCT_NULL))
OPCODE_DEF(OC_GLVNSLOT, (OCT_MVADDR | OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_INDSAVGLVN, (OCT_MVADDR | OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_INDSAVLVN, (OCT_MVADDR | OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_RFRSHLVN, (OCT_MVADDR | OCT_MVAL))
OPCODE_DEF(OC_SAVGVN, (OCT_MVADDR | OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_SAVLVN, (OCT_MVADDR | OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_SHARESLOT, (OCT_NULL))
OPCODE_DEF(OC_STOGLVN, (OCT_NULL))
OPCODE_DEF(OC_RFRSHGVN, (OCT_NULL))
OPCODE_DEF(OC_INDFNNAME2, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_INDGET2, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_INDMERGE2, (OCT_NULL))
OPCODE_DEF(OC_LITC, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_STOLITC, (OCT_NULL))
OPCODE_DEF(OC_FNZPEEK, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZSOCKET, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZSYSLOG, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_ZRUPDATE, (OCT_NULL))
OPCODE_DEF(OC_CDIDX, (OCT_MINT | OCT_CGSKIP | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZCOLLATE, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZATRANSFORM, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZTRANSLATE_FAST, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNTRANSLATE_FAST, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZAUDITLOG, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNREVERSEQUERY, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_GVREVERSEQUERY, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNQ2, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_GVQ2, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_INDQ2, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZYHASH, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZYISSQLNULL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FNZYSUFFIX, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_ANDOR, (OCT_BOOL | OCT_EXPRLEAF))
OPCODE_DEF(OC_BOOLEXPRSTART, (OCT_BOOL | OCT_EXPRLEAF))
OPCODE_DEF(OC_BOOLEXPRFINISH, (OCT_BOOL | OCT_EXPRLEAF))
OPCODE_DEF(OC_EQUNUL_RETMVAL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_NEQUNUL_RETMVAL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_EQUNUL_RETBOOL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_NEQUNUL_RETBOOL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_EQU_RETBOOL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_NEQU_RETBOOL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_EQU_RETMVAL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_NEQU_RETMVAL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_CONTAIN_RETBOOL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_NCONTAIN_RETBOOL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_CONTAIN_RETMVAL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_NCONTAIN_RETMVAL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FOLLOW_RETBOOL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_NFOLLOW_RETBOOL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_FOLLOW_RETMVAL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_NFOLLOW_RETMVAL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_PATTERN_RETBOOL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_NPATTERN_RETBOOL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_PATTERN_RETMVAL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_NPATTERN_RETMVAL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_SORTSAFTER_RETBOOL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_NSORTSAFTER_RETBOOL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_SORTSAFTER_RETMVAL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_NSORTSAFTER_RETMVAL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_GT_RETBOOL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_NGT_RETBOOL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_GT_RETMVAL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_NGT_RETMVAL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_LT_RETBOOL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_NLT_RETBOOL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_LT_RETMVAL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_NLT_RETMVAL, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_BXRELOP_EQU, (OCT_BOOL | OCT_REL))
OPCODE_DEF(OC_BXRELOP_NEQU, (OCT_BOOL | OCT_REL))
OPCODE_DEF(OC_BXRELOP_CONTAIN, (OCT_BOOL | OCT_REL))
OPCODE_DEF(OC_BXRELOP_NCONTAIN, (OCT_BOOL | OCT_REL))
OPCODE_DEF(OC_BXRELOP_SORTSAFTER, (OCT_BOOL | OCT_REL))
OPCODE_DEF(OC_BXRELOP_NSORTSAFTER, (OCT_BOOL | OCT_REL))
OPCODE_DEF(OC_BXRELOP_PATTERN, (OCT_BOOL | OCT_REL))
OPCODE_DEF(OC_BXRELOP_NPATTERN, (OCT_BOOL | OCT_REL))
OPCODE_DEF(OC_BXRELOP_FOLLOW, (OCT_BOOL | OCT_REL))
OPCODE_DEF(OC_BXRELOP_NFOLLOW, (OCT_BOOL | OCT_REL))
OPCODE_DEF(OC_BXRELOP_GT, (OCT_BOOL | OCT_REL))
OPCODE_DEF(OC_BXRELOP_NGT, (OCT_BOOL | OCT_REL))
OPCODE_DEF(OC_BXRELOP_LT, (OCT_BOOL | OCT_REL))
OPCODE_DEF(OC_BXRELOP_NLT, (OCT_BOOL | OCT_REL))
OPCODE_DEF(OC_FNZYCOMPILE, (OCT_MVAL | OCT_EXPRLEAF))
OPCODE_DEF(OC_GVNAMENAKED, (OCT_NULL))
OPCODE_DEF(OC_LASTOPCODE, (OCT_CGSKIP))
/* insert new opcodes before OC_LASTOPCODE */
