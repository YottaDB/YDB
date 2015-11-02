/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "i386.h"
#include "urx.h"
#include <rtnhdr.h>
#include "op.h"
#include <auto_zlink.h>

#define PEA_SZ		5
#define XFER_BYTE_SZ	3
#define XFER_LONG_SZ	6
#define INST_SZ		1

error_def(ERR_LABELUNKNOWN);
error_def(ERR_ROUTINEUNKNOWN);

rhdtyp *auto_zlink (unsigned char *pc, int4 **line)
{
	char		*adj_pc;	/* address of PEA rtnref offset */
	mstr		rname;
	mident_fixed	rname_local;
	urx_rtnref	*rtnurx;
	mval		rtn;
	rhdtyp		*rhead;
	union
	{
		ModR_M		modrm;
		unsigned char	byte;
	} modrm_byte_byte, modrm_byte_long;

	/* ASSUMPTION -- The instruction previous to the current mpc is a transfer table jump.
	 *		This is either a byte or a int4 displacement off of ebx, instruction
	 *		size either 3 or 6 (prefix byte, ModR/M byte, 8- or 32-bit offset).
	 */
	modrm_byte_byte.modrm.reg_opcode = I386_INS_CALL_Ev;
	modrm_byte_byte.modrm.mod = I386_MOD32_BASE_DISP_8;
	modrm_byte_byte.modrm.r_m = I386_REG_EBX;
	modrm_byte_long.modrm.reg_opcode = I386_INS_CALL_Ev;
	modrm_byte_long.modrm.mod = I386_MOD32_BASE_DISP_32;
	modrm_byte_long.modrm.r_m = I386_REG_EBX;
	if ((*(pc - XFER_BYTE_SZ) == I386_INS_Grp5_Prefix) && (*(pc - XFER_BYTE_SZ + 1) == modrm_byte_byte.byte))
	{
		assert(*(pc - XFER_BYTE_SZ - PEA_SZ) == I386_INS_PUSH_Iv);
		adj_pc = (char *)pc - XFER_BYTE_SZ - PEA_SZ;
	} else if ((*(pc - XFER_LONG_SZ) == I386_INS_Grp5_Prefix) && (*(pc - XFER_LONG_SZ + 1) == modrm_byte_long.byte))
	{
		assert(*(pc - XFER_LONG_SZ - PEA_SZ) == I386_INS_PUSH_Iv);
		adj_pc = (char *)pc - XFER_LONG_SZ - PEA_SZ;
	} else
		GTMASSERT;
	if (azl_geturxrtn(adj_pc + INST_SZ, &rname, &rtnurx))
	{
		assert((0 <= rname.len) && (MAX_MIDENT_LEN >= rname.len));
		assert(rname.addr);
		/* Copy rname into local storage because azl_geturxrtn sets rname.addr to an address that is
		 * freed during op_zlink and before the call to find_rtn_hdr.
		 */
		memcpy(rname_local.c, rname.addr, rname.len);
		rname.addr = rname_local.c;
		assert(rtnurx);
		assert(*(adj_pc - PEA_SZ) == I386_INS_PUSH_Iv);
		assert(azl_geturxlab(adj_pc - PEA_SZ + INST_SZ, rtnurx));
		assert(!find_rtn_hdr(&rname));
		rtn.mvtype = MV_STR;
		rtn.str.len = rname.len;
		rtn.str.addr = rname.addr;
		op_zlink (&rtn, 0);
		if (0 != (rhead = find_rtn_hdr(&rname)))	/* note the assignment */
		{
			*line = *(int4 **)(adj_pc - PEA_SZ + INST_SZ);
			if (!(*line))
				rts_error(VARLSTCNT(1) ERR_LABELUNKNOWN);
			return rhead;
		}
	}
	rts_error(VARLSTCNT(1) ERR_ROUTINEUNKNOWN);
	return NULL;
}
