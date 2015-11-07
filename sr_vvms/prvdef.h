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

#define	PRV$M_CMKRNL	1
#define	PRV$M_CMEXEC	2
#define	PRV$M_SYSNAM	4
#define	PRV$M_GRPNAM	8
#define	PRV$M_ALLSPOOL	16
#define	PRV$M_DETACH	32
#define	PRV$M_DIAGNOSE	64
#define	PRV$M_LOG_IO	128
#define	PRV$M_GROUP	256
#define	PRV$M_NOACNT	512
#define	PRV$M_PRMCEB	1024
#define	PRV$M_PRMMBX	2048
#define	PRV$M_PSWAPM	4096
#define	PRV$M_SETPRI	8192
#define	PRV$M_SETPRV	16384
#define	PRV$M_TMPMBX	32768
#define	PRV$M_WORLD	65536
#define	PRV$M_MOUNT	131072
#define	PRV$M_OPER	262144
#define	PRV$M_EXQUOTA	524288
#define	PRV$M_NETMBX	1048576
#define	PRV$M_VOLPRO	2097152
#define	PRV$M_PHY_IO	4194304
#define	PRV$M_BUGCHK	8388608
#define	PRV$M_PRMGBL	16777216
#define	PRV$M_SYSGBL	33554432
#define	PRV$M_PFNMAP	67108864
#define	PRV$M_SHMEM	134217728
#define	PRV$M_SYSPRV	268435456
#define	PRV$M_BYPASS	536870912
#define	PRV$M_SYSLCK	1073741824
#define	PRV$M_SHARE	-2147483648
#define	PRV$M_ACNT	512
#define	PRV$M_ALTPRI	8192
#define	PRV$S_PRVDEF	5
#define	PRV$V_CMKRNL	0
#define	PRV$V_CMEXEC	1
#define	PRV$V_SYSNAM	2
#define	PRV$V_GRPNAM	3
#define	PRV$V_ALLSPOOL	4
#define	PRV$V_DETACH	5
#define	PRV$V_DIAGNOSE	6
#define	PRV$V_LOG_IO	7
#define	PRV$V_GROUP	8
#define	PRV$V_NOACNT	9
#define	PRV$V_PRMCEB	10
#define	PRV$V_PRMMBX	11
#define	PRV$V_PSWAPM	12
#define	PRV$V_SETPRI	13
#define	PRV$V_SETPRV	14
#define	PRV$V_TMPMBX	15
#define	PRV$V_WORLD	16
#define	PRV$V_MOUNT	17
#define	PRV$V_OPER	18
#define	PRV$V_EXQUOTA	19
#define	PRV$V_NETMBX	20
#define	PRV$V_VOLPRO	21
#define	PRV$V_PHY_IO	22
#define	PRV$V_BUGCHK	23
#define	PRV$V_PRMGBL	24
#define	PRV$V_SYSGBL	25
#define	PRV$V_PFNMAP	26
#define	PRV$V_SHMEM	27
#define	PRV$V_SYSPRV	28
#define	PRV$V_BYPASS	29
#define	PRV$V_SYSLCK	30
#define	PRV$V_SHARE	31
#define	PRV$V_UPGRADE	32
#define	PRV$V_DOWNGRADE	33
#define	PRV$V_GRPPRV	34
#define	PRV$V_READALL	35
#define	PRV$V_TMPJNL	36
#define	PRV$V_PRMJNL	37
#define	PRV$V_SECURITY	38
#define	PRV$V_ACNT	9
#define	PRV$V_ALTPRI	13
