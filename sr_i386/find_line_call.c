/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "xfer_enum.h"
#include "i386.h"
#include "rtnhdr.h"	/* Needed by zbreak.h */
#include "zbreak.h"

zb_code  *find_line_call(void *addr)
{
	unsigned char *call_addr;
	union
	{
		ModR_M		modrm;
		unsigned char	byte;
	} modrm_byte;

	call_addr = (unsigned char *)addr;
	modrm_byte.byte = *(call_addr + 1);
	if (*call_addr == I386_INS_Grp5_Prefix  &&  modrm_byte.modrm.reg_opcode == I386_INS_CALL_Ev)
	{
		call_addr++;
		assert (modrm_byte.modrm.r_m == I386_REG_EBX);
		call_addr++;
		if (modrm_byte.modrm.mod == I386_MOD32_BASE_DISP_8)
		{
			if (*call_addr == xf_linestart * SIZEOF(int4) ||
			    *call_addr == xf_zbstart * SIZEOF(int4))
				return (zb_code *)call_addr;

			call_addr++;
		}
		else
		{
			assert (modrm_byte.modrm.mod == I386_MOD32_BASE_DISP_32);
			if (*((int4 *)call_addr) != xf_isformal * SIZEOF(int4))
				return (zb_code *)addr;

			call_addr += SIZEOF(int4);
		}
	}

	modrm_byte.byte = *(call_addr + 1);
	if (*call_addr == I386_INS_PUSH_Ib  ||  *call_addr == I386_INS_PUSH_Iv)
	{
		while (*call_addr == I386_INS_PUSH_Ib  ||  *call_addr == I386_INS_PUSH_Iv)
		{
			if (*call_addr == I386_INS_PUSH_Ib)
				call_addr += 1 + SIZEOF(unsigned char);
			else
			{
				assert (*call_addr == I386_INS_PUSH_Iv);
				call_addr += 1 + SIZEOF(int4);
			}
		}
		modrm_byte.byte = *(call_addr + 1);
		if (*call_addr++ != I386_INS_Grp5_Prefix  ||  modrm_byte.modrm.reg_opcode != I386_INS_CALL_Ev)
			return (zb_code *)addr;

		assert (modrm_byte.modrm.mod == I386_MOD32_BASE_DISP_8  ||  modrm_byte.modrm.mod == I386_MOD32_BASE_DISP_32);
		assert (modrm_byte.modrm.r_m == I386_REG_EBX);
		call_addr++;
		if (modrm_byte.modrm.mod == I386_MOD32_BASE_DISP_8)
		{
			if (*call_addr != xf_linefetch * SIZEOF(int4) &&
			    *call_addr != xf_zbfetch * SIZEOF(int4))
				return (zb_code *)addr;
		}
	}
	else if (*call_addr == I386_INS_Grp5_Prefix  &&  modrm_byte.modrm.reg_opcode == I386_INS_CALL_Ev)
	{
		call_addr++;
		assert (modrm_byte.modrm.mod == I386_MOD32_BASE_DISP_8  ||  modrm_byte.modrm.mod == I386_MOD32_BASE_DISP_32);
		assert (modrm_byte.modrm.r_m == I386_REG_EBX);
		call_addr++;
		if (modrm_byte.modrm.mod == I386_MOD32_BASE_DISP_8)
		{
			if (*call_addr != xf_linestart * SIZEOF(int4) &&
			    *call_addr != xf_zbstart * SIZEOF(int4))
				return (zb_code *)addr;
		}
	}
	return (zb_code *)call_addr;
}
