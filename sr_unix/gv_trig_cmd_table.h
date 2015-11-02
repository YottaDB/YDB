/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

GV_TRIG_CMD_ENTRY(GVTR_CMDTYPE_SET,		"S",   0x01)	/* for SET  */
GV_TRIG_CMD_ENTRY(GVTR_CMDTYPE_KILL,		"K",   0x02)	/* for KILL */
GV_TRIG_CMD_ENTRY(GVTR_CMDTYPE_ZKILL,		"ZK",  0x04)	/* for ZKILL or ZWITHDRAW */
GV_TRIG_CMD_ENTRY(GVTR_CMDTYPE_ZTKILL,		"ZTK", 0x08)	/* for ZTKILL */
GV_TRIG_CMD_ENTRY(GVTR_CMDTYPE_ZTRIGGER,	"ZTR", 0x10)	/* For ZTRIGGER */
