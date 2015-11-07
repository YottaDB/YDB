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

#include "gtm_stdio.h"
#include "gtm_string.h"
#include <descrip.h>
#include <fab.h>
#include <rab.h>
#include <rmsdef.h>

#include "zcall.h"
#include <rtnhdr.h>
#include "compiler.h"
#include "obj_gen.h"
#include "objlangdefs.h"
#include "urx.h"
#include "vaxsym.h"
#include "op.h"
#include "incr_link.h"
#include "inst_flush.h"
#include "op_fgnlookup.h"
#include "min_max.h"

LITREF char		gtm_release_name[];
LITREF int4		gtm_release_name_len;

GBLREF mident_fixed	zlink_mname;
GBLREF unsigned char 	*gtm_main_address;
/* GBLREF unsigned char	*gtm_dyn_ch_address; */	/* if we need this variable, it should be defined in gtm$startup */

static short		linker_stack_depth;
static int4		stack_psect;
static char		loading_psect, *reloc_base, *stack;
static char		*load_base[GTM_LASTPSECT], *load_top[GTM_LASTPSECT];
static int		zlink_mname_len;

bool 			tir(char *buff, int size, urx_rtnref *urx_lcl_anchor);

/* TIR - process Alpha text information and relocation subrecords from a GT.M MUMPS object file record. */
#define GTMMAIN		"GTM$MAIN"
#define GTMDYNCH	"GTM$DYN_CH"

error_def(ERR_INVOBJ);
error_def(ERR_LOADRUNNING);

/* locc is defined here so that it may be inlined
 * locc - locate first occurrence of character in string
 */
static char *locc(char c, char *string, int length)
{
	while (0 < length--)
	{
		if (c == *string)
			return string;
		++string;
	}
	return 0;
}

/* ZL_ERROR - perform cleanup and signal errors found in zlinking a mumps object module. */
void zl_error(unsigned char *fab, bool libr, int4 err, int4 len, char *addr)
{
	if (load_base[GTM_LINKAGE])
		free(load_base[GTM_LINKAGE]);
	if (!libr)
		sys$close(fab);
	else
		lbr$close(fab);	/* close library */
	if (0 == len)
		rts_error(VARLSTCNT(1) err);
	else
		rts_error(VARLSTCNT(4) err, 2, len, addr);
}

/* INCR_LINK - read and process a mumps object module.  Link said module to currently executing image.  */
bool incr_link(unsigned char *fab, bool libr)
{
	struct RAB	rab;
	rhdtyp		*hdr, *old_rhead;
	lab_tabent	*lbt_ent, *lbt_bot, *lbt_top, *olbt_ent, *olbt_bot, *olbt_top;
	int		status, rec_count, n1;
	int4		linkage_size, lit_size, code_size;
	int4		rhd_diff;
	char		*bptr, *subrec, buff[OBJ_EMIT_BUF_SIZE], fake[SIZEOF(rtn_tabent)];
	unsigned char	*cp, *cp1, tmpch;
	unsigned short	rec_size;
	urx_rtnref	urx_lcl_anchor;
	char		module_name[SIZEOF(mident_fixed)];
	int		order;
	$DESCRIPTOR(buffdes, buff);

	bptr = buff;	/* initialize buffer pointer to first buffer */
	/* Safety initial values: */
	loading_psect = -1;
	reloc_base = -1;
	load_base[GTM_CODE]     = load_top[GTM_CODE]     = 0;
	load_base[GTM_LITERALS] = load_top[GTM_LITERALS] = 0;
	load_base[GTM_LINKAGE]  = load_top[GTM_LINKAGE]  = 0;
	if (!libr)
	{
		rab = cc$rms_rab;
		rab.rab$l_fab = fab;
		rab.rab$l_ubf = buff;
		rab.rab$w_usz = OBJ_EMIT_BUF_SIZE;
		status = sys$connect(&rab);
		if (RMS$_NORMAL != status)
			zl_error(fab, libr, status, 0, 0);
	}
	/* Although we process the linker commands relating to the GTM_RNAMESAAAAB PSECT,
	 * we never use the data, so we create the image on the stack and ignore it.
	 */
	load_base[GTM_RNAMESAAAAB] = &fake[0];
	load_top[GTM_RNAMESAAAAB]  = &fake[0] + SIZEOF(rtn_tabent);
	urx_lcl_anchor.len = 0;
	urx_lcl_anchor.addr = urx_lcl_anchor.lab = urx_lcl_anchor.next = 0;
	for (rec_count = 0; rec_count < 3; rec_count++)
	{
		if (!libr)
		{
			status = sys$get(&rab);
			rec_size = rab.rab$w_rsz;
		} else
		{
			status = lbr$get_record(fab, 0, &buffdes);
			rec_size = buffdes.dsc$w_length;
			bptr = buffdes.dsc$a_pointer;
		}
		if (RMS$_EOF == status)
			zl_error(fab, libr, ERR_INVOBJ, 0, 0);
		if (!(status & 1))
			zl_error(fab, libr, status, 0, 0);
		switch(*(short *)(&bptr[EOBJ$W_RECTYP]))
		{
		case EOBJ$C_EMH:
			if (0 == rec_count)
			{	/* First record must be module header record, subtype main module header.  */
				if (EMH$C_MHD != *(short *)(&bptr[EOBJ$W_SUBTYP]))
					zl_error(fab, libr, ERR_INVOBJ, 0, 0);
				zlink_mname_len = bptr[EMH$B_NAMLNG];
				if (zlink_mname_len > MAX_MIDENT_LEN)
					zl_error(fab, libr, ERR_INVOBJ, 0, 0);
				memcpy(&zlink_mname.c[0], &bptr[EMH$B_NAMLNG+1], zlink_mname_len);	/* copy module name */
				zlink_mname.c[zlink_mname_len] = 0;
				continue;
			} else if (1 == rec_count)
			{	/* Second record must be module header record, subtype language processor name header.  */
				if (EMH$C_LNM != *(short *)(&bptr[EOBJ$W_SUBTYP]))
					zl_error(fab, libr, ERR_INVOBJ, 0, 0);
				n1 = rec_size - 6;	/* record size minus bytes used up by header information */
				if (n1 > gtm_release_name_len)
					n1 = gtm_release_name_len;
				for (cp = &bptr[6], cp1 = gtm_release_name; n1 > 0; n1--)
				{
					if (*cp++ != (tmpch = *cp1++))		/* verify GT.M release name matches current name */
						return FALSE;
					if ('-' == tmpch)
						break;
				}
				/* VMS Linker looks at major and minor version numbers to see if recompile is necessary.
				 * That is, if the current GTM version is V5.2, any object file created using V5.1
				 * or lesser version will automatically be recompiled. But any object file created using
				 * V5.2* version will NOT be automatically recompiled. On the other hand, we want
				 * V5.2-000A to force unconditional recompile on all object files V5.2-000 (and previous)
				 * GTM versions. We want to do that because of C9C05-002003 causing OC_NAMECHK opcode as well
				 * as the xf_namechk/op_namechk transfer table entries to be removed.
				 *
				 * history of object file changes :
				 * 	code gen changed 	in V4.4-003A,
				 * 	dev params       	in V4.4-004,
				 * 	$increment/longnames	in V4.4-005
				 * 	OC_NAMECHK opcode nix	in V5.2-000A
				 */
				/* Example &bptr[6] = "GT.M V5.2-000 VMS AXP16-FEB-2007 16:04"
				 * We want the index into bptr where 5.2 starts which is bptr[12]
				 */
				if (0 > STRNCMP_LIT(&bptr[12], "5.2-000A"))	/* was compiled using V5.2-000 */
					return FALSE;
				continue;
			} else
				zl_error(fab, libr, ERR_INVOBJ, 0, 0);
		case EOBJ$C_EGSD:
			if (2 == rec_count)
			{
				/* Third record must be global symbol directory record.  */
				subrec = bptr + 8;	/* skip over record header to first subrecord header */
				/* GTM$CODE PSECT program section definition subrecord.  */
				if (EGSD$C_PSC != *(short *)(&subrec[EGPS$W_GSDTYP]))
					zl_error(fab, libr, ERR_INVOBJ, 0, 0);
				code_size = *(int4 *)(&subrec[EGPS$L_ALLOC]);
				subrec += *(short *)(&subrec[EGPS$W_SIZE]);	/* skip to next subrecord */
				/* GTM$LITERALS PSECT program section definition subrecord.  */
				if (EGSD$C_PSC != *(short *)(&subrec[EGPS$W_GSDTYP]))
					zl_error(fab, libr, ERR_INVOBJ, 0, 0);
				lit_size = *(int4 *)(&subrec[EGPS$L_ALLOC]);
				subrec += *(short *)(&subrec[EGPS$W_SIZE]);	/* skip to next subrecord */
				/* GTM$Rname PSECT program section definition subrecord.  */
				if (EGSD$C_PSC != *(short *)(&subrec[EGPS$W_GSDTYP]))
					zl_error(fab, libr, ERR_INVOBJ, 0, 0);
				subrec += *(short *)(&subrec[EGPS$W_SIZE]);	/* skip to next subrecord */
				/* $LINKAGE PSECT program section definition subrecord.  */
				if (EGSD$C_PSC != *(short *)(&subrec[EGPS$W_GSDTYP]))
					zl_error(fab, libr, ERR_INVOBJ, 0, 0);
				linkage_size = *(int4 *)(&subrec[EGPS$L_ALLOC]);
				subrec += *(short *)(&subrec[EGPS$W_SIZE]);	/* skip to next subrecord */
				load_base[GTM_LINKAGE]  = malloc(code_size + lit_size + linkage_size);
				load_base[GTM_LITERALS] = load_top[GTM_LINKAGE]  = load_base[GTM_LINKAGE]  + linkage_size;
				load_base[GTM_CODE]     = load_top[GTM_LITERALS] = load_base[GTM_LITERALS] + lit_size;
							  load_top[GTM_CODE]     = load_base[GTM_CODE]     + code_size;
				assert(load_top[GTM_CODE] - load_base[GTM_LINKAGE] == linkage_size + lit_size + code_size);
				continue;
			}
		/* caution : fall through */
		default:
			zl_error(fab, libr, ERR_INVOBJ, 0, 0);
		}
		break;
	}
	linker_stack_depth = 0;
	for(; ; rec_count++)
	{
		if (!libr)
		{
			status = sys$get(&rab);
			rec_size = rab.rab$w_rsz;
		} else
		{
			status = lbr$get_record(fab, 0, &buffdes);
			rec_size = buffdes.dsc$w_length;
			bptr = buffdes.dsc$a_pointer;
		}
		if (RMS$_EOF == status)
		{
			urx_free(&urx_lcl_anchor);
			zl_error(fab, libr, ERR_INVOBJ, 0, 0);
		}
		if (!(status & 1))
		{
			urx_free(&urx_lcl_anchor);
			zl_error(fab, libr, status, 0, 0);
		}
		switch(*(short *)(&bptr[EOBJ$W_RECTYP]))
		{
		case EOBJ$C_ETIR:
			assert(*(short *)(&bptr[EOBJ$W_SIZE]) == rec_size);
			subrec = bptr + 4;	/* skip over record header to first subrecord header */
			if (!tir(subrec, *(short *)(&bptr[EOBJ$W_SIZE]) - (subrec - bptr), &urx_lcl_anchor))
			{
				urx_free(&urx_lcl_anchor);
				zl_error(fab, libr, ERR_INVOBJ, 0, 0);
			}
			continue;
		case EOBJ$C_EGSD:
			continue;
		case EOBJ$C_EEOM:
			if (!libr)
			{
				if (RMS$_EOF != sys$get(&rab))
				{
					urx_free(&urx_lcl_anchor);
					zl_error(fab, libr, ERR_INVOBJ, 0, 0);
				}
			} else
			{	if (RMS$_EOF != lbr$get_record(fab, 0, &buffdes))
				{
					urx_free(&urx_lcl_anchor);
					zl_error(fab, libr, ERR_INVOBJ, 0, 0);
				}
			}
			break;
		default:
			urx_free(&urx_lcl_anchor);
			zl_error(fab, libr, ERR_INVOBJ, 0, 0);
		}
		break;
	}
	if (0 != linker_stack_depth)
	{
		urx_free(&urx_lcl_anchor);
		zl_error(fab, libr, ERR_INVOBJ, 0, 0);
	}
	hdr = load_base[GTM_CODE];
	if (!zlput_rname(hdr))
	{
		urx_free(&urx_lcl_anchor);
		/* Copy routine name to local variable because zl_error frees it. */
		memcpy(&module_name[0], hdr->routine_name.addr, hdr->routine_name.len);
		zl_error(fab, libr, ERR_LOADRUNNING, hdr->routine_name.len, &module_name[0]);
	}
	urx_add(&urx_lcl_anchor);
	old_rhead = hdr->old_rhead_ptr;
	lbt_bot = (lab_tabent *)((char *)hdr + hdr->labtab_ptr);
	lbt_top = lbt_bot + hdr->labtab_len;
	while (old_rhead)
	{
		lbt_ent = lbt_bot;
		olbt_bot = (lab_tabent *)((char *)old_rhead + old_rhead->labtab_ptr);
		olbt_top = olbt_bot + old_rhead->labtab_len;
		for (olbt_ent = olbt_bot;  olbt_ent < olbt_top;  ++olbt_ent)
		{
			for (; lbt_ent < lbt_top; lbt_ent++)
			{
				MIDENT_CMP(&olbt_ent->lab_name, &lbt_ent->lab_name, order);
				if (order <= 0)
					break;
			}
			if ((lbt_ent < lbt_top) && !order)
			{
				olbt_ent->lab_ln_ptr = lbt_ent->lab_ln_ptr;
				olbt_ent->has_parms = lbt_ent->has_parms;
			} else
				olbt_ent->lab_ln_ptr = 0;
		}
		rhd_diff = (char *)hdr - (char *)old_rhead;
		old_rhead->src_full_name = hdr->src_full_name;
		old_rhead->routine_name = hdr->routine_name;
		old_rhead->vartab_len = hdr->vartab_len;
		old_rhead->vartab_ptr = hdr->vartab_ptr + rhd_diff;
		old_rhead->ptext_ptr = hdr->ptext_ptr + rhd_diff;
		old_rhead->current_rhead_ptr = rhd_diff;
		old_rhead->temp_mvals = hdr->temp_mvals;
		old_rhead->temp_size = hdr->temp_size;
		old_rhead->linkage_ptr = hdr->linkage_ptr;
		old_rhead->literal_ptr = hdr->literal_ptr;
		old_rhead = (rhdtyp *)old_rhead->old_rhead_ptr;
	}
	urx_resolve (load_base[GTM_CODE], lbt_bot, lbt_top);
	inst_flush(NULL, 0); /* flush instruction cache for resolved references on VMS, this flushes whole pipe */
	return TRUE;
}

bool tir(				/* TRUE if no errors encountered;  FALSE upon encountering any error  */
	 char *buff,			/* start of buffer containing TIR commands */
	 int size,			/* size of buff */
	 urx_rtnref *urx_lcl_anchor)	/* unresovled external local anchor */
{
	rhdtyp		*rtn;
	lab_tabent	*label, *labtop;
	mident_fixed	rtnid, labid;
	mstr		str;
	int4		sto_imm_length;
	unsigned char	y;
	char		*top, *loc;
	int		len, len1, lab_len, n;
	urx_rtnref	*urx_rp;
	urx_addr	*urx_tmpaddr;
	unsigned char	*cp1, *cp2, ch;
	bool		now_lower;

	top = buff + size;
	for(;  buff < top;)
	{
		switch (loading_psect)
		{
		case -1:
			assert(-1 == reloc_base);
			break;
		case GTM_CODE:
		case GTM_LITERALS:
		case GTM_RNAMESAAAAB:
		case GTM_LINKAGE:
			assert((load_base[loading_psect] <= reloc_base) && (reloc_base <= load_top[loading_psect]));
			break;
		default:
			return FALSE;
		}
		/* Note: although the following code uses the term "stack", it should be noted this only works if the
		 *	 maximum depth of the stack is one (1).
		 */
		switch(*(short *)(&buff[ETIR$W_RECTYP]))
		{
		case ETIR$C_STA_PQ:	/* stack PSECT base plus byte offset */
			if ((0 != linker_stack_depth) || (0 != *(int4 *)(&buff[12])))	/* high-order 32 bits of quadword address */
				return FALSE;
			stack_psect = *(int4 *)(&buff[4]);
			stack = load_base[stack_psect] + *(int4 *)(&buff[8]);
			linker_stack_depth++;
			buff += *(short *)(&buff[ETIR$W_SIZE]);
			continue;
		case ETIR$C_CTL_SETRB:	/* set relocation base */
			if (0 >= linker_stack_depth)
				return FALSE;
			loading_psect = stack_psect;
			reloc_base = stack;
			linker_stack_depth--;
			buff += *(short *)(&buff[ETIR$W_SIZE]);
			continue;
		case ETIR$C_STC_BOH_GBL:	/* store conditional BSR or hint at global address */
		case ETIR$C_STC_LDA_GBL:	/* store conditional LDA at global address */
		case ETIR$C_STC_NOP_GBL:	/* store conditional NOP at global address */
			/* These linker commands are used by the OpenVMS Alpha Linker to replace instructions in the
			 * current image with alternative, faster instructions if certain conditions about those
			 * instructions and the displacement from them to other addresses are true.  The GT.M linker
			 * does not use the same mechanisms, so these instructions are not generated except in the
			 * routine header JSB prologue (which is never executed for a GT.M module that is dynamically
			 * linked and can therefore be ignored by the GT.M linker).
			 */
			buff += *(short *)(&buff[ETIR$W_SIZE]);
			continue;
		case ETIR$C_STC_LP_PSB:		/* store conditional linkage pair plus signature */
			if (reloc_base + 4 * SIZEOF(int4) > load_top[loading_psect])
				return FALSE;
			/* Store dummy values for now (GTM$MAIN and GTM$DYN_CH aren't really used) [lidral] */
			*((int4 *)reloc_base)++ = 0;
			*((int4 *)reloc_base)++ = 0;
			*((int4 *)reloc_base)++ = 0;
			*((int4 *)reloc_base)++ = 0;
			buff += *(short *)(&buff[ETIR$W_SIZE]);
			continue;
		case ETIR$C_STO_GBL:	/* store global */
			if (reloc_base + 2 * SIZEOF(int4) > load_top[loading_psect])
				return FALSE;
			len = buff[4];
			if ((len > SIZEOF(ZCSYM_PREFIX)) && (0 == MEMCMP_LIT(&buff[5], ZCSYM_PREFIX)))
			{
				mval	package, extent;

				len1 = len;
				package.mvtype = extent.mvtype = MV_STR;
				cp1 = &buff[5] + SIZEOF(ZCSYM_PREFIX) - 1;
				len1 -= SIZEOF(ZCSYM_PREFIX) - 1;
				package.str.addr = cp1;
				loc = locc('.', cp1, len1);
				assert(0 < loc);
				package.str.len = (unsigned char *)loc - cp1;
				len1 -= package.str.len + 1;		/* take off package and . */
				extent.str.len = len1;
				extent.str.addr = cp1 + 1;
				if (0 == extent.str.len)
					return FALSE;
				if ((package.str.len > 0) && ('_' == *package.str.addr))
					*package.str.addr = '%';
				if ('_' == *extent.str.addr)
					*extent.str.addr = '%';
				*((int4 *)reloc_base)++ = (int4)op_fgnlookup(&package, &extent);
				*((int4 *)reloc_base)++ = 0;	/* high-order 32 bits of address */
			} else  if (0 != (loc = locc('.', &buff[5], len)))	/* global name contains a '.' */
			{
				len1 = loc - &buff[5]; /* length of the routine part before the '.' */
				assert(MAX_MIDENT_LEN >= len1);
				memcpy(&rtnid.c[0], &buff[5], len1);
				rtnid.c[len1] = 0;
				if ('_' == rtnid.c[0])
					rtnid.c[0] = '%';
				cp1 = loc + 1;
				lab_len = len - ((char *)cp1 - &buff[5]); /* length of the label part following the '.' */
				assert(MAX_MIDENT_LEN >= lab_len);
				memcpy(&labid.c[0], cp1, lab_len);
				labid.c[lab_len] = 0;
				if ('_' == labid.c[0])
					labid.c[0] = '%';
				str.addr = &rtnid.c[0];
				str.len = len1;
				if (0 != (rtn = find_rtn_hdr(&str))) /* Routine already resolved? */
				{
					label = (lab_tabent *)((char *)rtn + rtn->labtab_ptr);
					labtop = label + rtn->labtab_len;
					for (; label < labtop && ((lab_len != label->lab_name.len)
						|| memcmp(&labid.c[0], label->lab_name.addr, lab_len)); label++)
						;
					if (label < labtop)
					{
						*((int4 *)reloc_base)++ = (char *)&label->lab_ln_ptr;
						*((int4 *)reloc_base)++ = 0;	/* high-order 32 bits of address */
						buff += *(short *)(&buff[ETIR$W_SIZE]);
						continue;
					}
				}
				/* This symbol is unknown. Put on the (local) unresolved extern chain --
				 * either for labels or routines
				 */
				urx_rp = urx_putrtn(&rtnid.c[0], len1, urx_lcl_anchor);
				urx_putlab(&labid.c[0], lab_len, urx_rp, reloc_base);
				*((int4 *)reloc_base)++ = 0;
				*((int4 *)reloc_base)++ = 0;
			} else if (0 != (loc = locc('$', &buff[5], len)))	/* global name contains a '$' */
			{
				if ((SIZEOF(GTMMAIN) - 1 == len) && (0 == memcmp(GTMMAIN, &buff[5], len)))
					*((int4 *)reloc_base)++ = gtm_main_address;
				else  if ((SIZEOF(GTMDYNCH) - 1 == len) && (0 == memcmp(GTMDYNCH, &buff[5], len)))
				{
					/* *((int4 *)reloc_base)++ = gtm_dyn_ch_address; */	/* don't need */
					*((int4 *)reloc_base)++ = 0;	/* dummy value; GT.M frames are not machine frames */
				} else
					return FALSE;
				*((int4 *)reloc_base)++ = 0;	/* high-order 32 bits of address */
			} else	/* It's a bona fide global name. */
			{
				memcpy(&rtnid.c[0], &buff[5], len);
				rtnid.c[len] = 0;
				assert(zlink_mname_len > 0 && zlink_mname_len <= MAX_MIDENT_LEN);
				if (zlink_mname_len == len && !memcmp(&zlink_mname.c[0], &rtnid.c[0], len))	/* program name */
				{
					*((int4 *)reloc_base)++ = load_base[GTM_CODE];
					*((int4 *)reloc_base)++ = 0;	/* high-order 32 bits of address */
				} else
				{
					if ('_' == rtnid.c[0])
						rtnid.c[0] = '%';
					str.addr = &rtnid.c[0];
					str.len = len;
					if (0 != (rtn = find_rtn_hdr(&str)))
						*((int4 *)reloc_base)++ = rtn->linkage_ptr;
					else
					{
						urx_rp = urx_putrtn(&rtnid.c[0], len, urx_lcl_anchor);
						urx_tmpaddr = malloc(SIZEOF(urx_addr));
						urx_tmpaddr->next = urx_rp->addr;
						urx_tmpaddr->addr = reloc_base;
						urx_rp->addr = urx_tmpaddr;
						*((int4 *)reloc_base)++ = 0;
					}
					*((int4 *)reloc_base)++ = 0;
				}
			}
			buff += *(short *)(&buff[ETIR$W_SIZE]);
			continue;
		case ETIR$C_STO_IMM:	/* store immediate */
			sto_imm_length = *(int4 *)(&buff[4]);
			if (reloc_base + sto_imm_length > load_top[loading_psect])
				return FALSE;
			memcpy(reloc_base, &buff[8], sto_imm_length);
			reloc_base += sto_imm_length;
			buff += *(short *)(&buff[ETIR$W_SIZE]);
			continue;
		case ETIR$C_STO_LW:	/* store longword */
			if ((0 >= linker_stack_depth) || (reloc_base + SIZEOF(int4) > load_top[loading_psect]))
				return FALSE;
			*((int4 *)reloc_base)++ = stack;
			linker_stack_depth--;
			buff += *(short *)(&buff[ETIR$W_SIZE]);
			continue;
		case ETIR$C_STO_OFF:	/* store offset to PSECT */
			if ((0 >= linker_stack_depth) || (reloc_base + 2 * SIZEOF(int4) > load_top[loading_psect]))
				return FALSE;
			*((int4 *)reloc_base)++ = stack;	/* low-order 32 bits of quadword address */
			*((int4 *)reloc_base)++ = 0;		/* high-order 32 bits always zero */
			linker_stack_depth--;
			buff += *(short *)(&buff[ETIR$W_SIZE]);
			continue;
		default:
			return FALSE;
		}
	}
	return TRUE;
}
