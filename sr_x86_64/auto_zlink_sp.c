/****************************************************************
 *                                                              *
 *      Copyright 2008, 2010 Fidelity Information Services, Inc *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"

#include "opcode.h"
#include <mdefsp.h>
#include "x86_64.h"
#include <auto_zlink_sp.h>
#include "i386.h"

GBLREF int4	rtnhdr_off, labaddr_off;

union
{
	ModR_M	modrm;
	unsigned char	byte;
} modrm_byte_byte, modrm_byte_long, modrm_byte_actual;


short opcode_correct(unsigned char *curr_pc, short opcode, short reg_opcode, short is_rm, short r_m)
{
	if (*(curr_pc - MOD_BYTE_SZ) == opcode)
        {
		modrm_byte_actual.byte = *(curr_pc - MOD_BYTE_SZ + 1);
		if ((modrm_byte_actual.modrm.reg_opcode == reg_opcode) && (modrm_byte_actual.modrm.mod == I386_MOD32_BASE_DISP_8))
		{
			if ((is_rm) && (modrm_byte_actual.modrm.r_m == r_m))
				return MOD_BYTE_SZ;
			else if (!is_rm)
				return MOD_BYTE_SZ;
			else
				return FALSE;
		}
	} else if (*(curr_pc - MOD_LONG_SZ) == opcode)
	{
		modrm_byte_actual.byte = *(curr_pc - MOD_LONG_SZ + 1);
		if ((modrm_byte_actual.modrm.reg_opcode == reg_opcode) && (modrm_byte_actual.modrm.mod == I386_MOD32_BASE_DISP_32))
		{
			if (is_rm && (modrm_byte_actual.modrm.r_m == r_m))
				return MOD_LONG_SZ;
			else if (!is_rm)
				return MOD_LONG_SZ;
			else
				return FALSE;
		}
	} else if (*(curr_pc - MOD_NONE_SZ) == opcode)
	{
		modrm_byte_actual.byte = *(curr_pc - MOD_NONE_SZ + 1);
		if ((modrm_byte_actual.modrm.reg_opcode == reg_opcode) && (modrm_byte_actual.modrm.mod == I386_MOD32_BASE))
		{
			if (is_rm && modrm_byte_actual.modrm.r_m == r_m)
				return MOD_NONE_SZ;
			else if (!is_rm)
				return MOD_NONE_SZ;
			else
				return FALSE;
		}
	}
	return FALSE;
}


short valid_calling_sequence(unsigned char *pc)
{
	short curr_offset = 0, inst_sz;

	modrm_byte_byte.modrm.reg_opcode = I386_INS_CALL_Ev;
	modrm_byte_byte.modrm.mod = I386_MOD32_BASE_DISP_8;
	modrm_byte_byte.modrm.r_m = GTM_REG_XFER_TABLE;

	modrm_byte_long.modrm.reg_opcode = I386_INS_CALL_Ev;
	modrm_byte_long.modrm.mod = I386_MOD32_BASE_DISP_32;
	modrm_byte_long.modrm.r_m = GTM_REG_XFER_TABLE;

			/*	 inst = call off(%reg_xfer) 	*/
	if (curr_offset += opcode_correct(pc, I386_INS_Grp5_Prefix, I386_INS_CALL_Ev, TRUE, GTM_REG_XFER_TABLE))
	{
			/* 	inst is :: mov R0 = MEM 	*/
		inst_sz = 1 + opcode_correct((pc - curr_offset),I386_INS_MOV_Gv_Ev,I386_REG_RDI,TRUE,GTM_REG_PV & 0x7);
		switch(inst_sz - 1)
		{
			case FALSE :
				return FALSE;
			case MOD_NONE_SZ :
				rtnhdr_off = 0;
				break;
			case MOD_BYTE_SZ :
				rtnhdr_off = (int4) *((char *)pc - curr_offset - 1);
				break;
			case MOD_LONG_SZ :
				rtnhdr_off = (int4) *((int4 *)(pc - curr_offset - 4));
				break;
			default :
				GTMASSERT;
		}
		curr_offset += inst_sz;
		/* if invalid, would've returned before... */
			/*	inst is :: mov R1 = MEM		*/
		inst_sz = 1 + opcode_correct((pc - curr_offset),I386_INS_MOV_Gv_Ev,I386_REG_RSI,TRUE,GTM_REG_PV & 0x7);
		switch(inst_sz - 1)
		{
			case FALSE :
				return FALSE;
			case MOD_NONE_SZ :
				labaddr_off = 0;
				break;
			case MOD_BYTE_SZ :
				labaddr_off = (int4) *((char *)pc - curr_offset - 1);
				break;
			case MOD_LONG_SZ :
				labaddr_off = (int4) *((int4 *)(pc - curr_offset - 4));
				break;
			default :
				GTMASSERT;
		}
		return TRUE;
	}
	return FALSE;
}
