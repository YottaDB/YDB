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

#include "gtm_unistd.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include <errno.h>

#include <rtnhdr.h>
#include "compiler.h"
#include "urx.h"
#include "objlabel.h"	/* needed for masscomp.h */
#include "masscomp.h"
#include "gtmio.h"
#include "incr_link.h"
#include "min_max.h"	/* MIDENT_CMP needs MIN */
#include "cmd_qlf.h"	/* needed for CQ_UTF8 */
#include "gtm_text_alloc.h"

/* INCR_LINK - read and process a mumps object module. Link said module to currently executing image */

LITREF char gtm_release_name[];
LITREF int4 gtm_release_name_len;

static char 		*code;
GBLREF mident_fixed 	zlink_mname;
GBLREF  boolean_t	gtm_utf8_mode;

error_def(ERR_INVOBJ);
error_def(ERR_LOADRUNNING);
error_def(ERR_TEXT);

#define RELREAD 50	/* number of relocation entries to buffer */
typedef struct res_list_struct
{
	struct res_list_struct *next, *list;
	unsigned int	addr, symnum;
} res_list;

void res_free(res_list *root);
bool addr_fix(int file, struct exec *fhead, urx_rtnref *urx_lcl, rhdtyp *code);
void zl_error(int4 file, int4 err, int4 err2, int4 len, char *addr);

bool incr_link(int file_desc)
{
	rhdtyp		*hdr, *old_rhead;
	int 		code_size, save_errno, cnt;
	int4 		rhd_diff, read_size;
	char		*literal_ptr;
	var_tabent	*curvar;
	char		module_name[SIZEOF(mident_fixed)];
	lab_tabent	*lbt_ent, *lbt_bot, *lbt_top, *olbt_ent, *olbt_bot, *olbt_top, *curlab;
	urx_rtnref	urx_lcl_anchor;
	int		order;
	struct exec 	file_hdr;

	urx_lcl_anchor.len = 0;
	urx_lcl_anchor.addr = 0;
	urx_lcl_anchor.lab = 0;
	urx_lcl_anchor.next = 0;
	code = NULL;
	DOREADRL(file_desc, &file_hdr, SIZEOF(file_hdr), read_size);
	if (read_size != SIZEOF(file_hdr))
	{
		if (-1 == read_size)
		{
			save_errno = errno;
			zl_error(file_desc, ERR_INVOBJ, ERR_TEXT, strlen(STRERROR(save_errno)),
					STRERROR(save_errno));
		} else
			zl_error(file_desc, ERR_INVOBJ, ERR_TEXT, RTS_ERROR_TEXT("reading file header"));
	} else if (OMAGIC != file_hdr.a_magic)
		zl_error(file_desc, ERR_INVOBJ, ERR_TEXT, RTS_ERROR_TEXT("bad magic"));
	else if (OBJ_LABEL != file_hdr.a_stamp)
		return FALSE;	/* wrong version */
	assert(0 == file_hdr.a_bss);
	code_size = file_hdr.a_text + file_hdr.a_data;
	code = GTM_TEXT_ALLOC(code_size);
	DOREADRL(file_desc, code, code_size, read_size);
	if (read_size != code_size)
	{
		if (-1 == read_size)
		{
			save_errno = errno;
			zl_error(file_desc, ERR_INVOBJ, ERR_TEXT, strlen(STRERROR(save_errno)), STRERROR(save_errno)); /* BYPASSOK */
		} else
			zl_error(file_desc, ERR_INVOBJ, ERR_TEXT, RTS_ERROR_TEXT("reading code"));
	}
	hdr = (rhdtyp *)code;
	if (memcmp(&hdr->jsb[0], "GTM_CODE", SIZEOF(hdr->jsb)))
		zl_error(file_desc, ERR_INVOBJ, ERR_TEXT, RTS_ERROR_TEXT("missing GTM_CODE"));
	if ((hdr->compiler_qlf & CQ_UTF8) && !gtm_utf8_mode)
		zl_error(file_desc, ERR_INVOBJ, ERR_TEXT,
			RTS_ERROR_TEXT("Object compiled with CHSET=UTF-8 which is different from $ZCHSET"));
	if (!(hdr->compiler_qlf & CQ_UTF8) && gtm_utf8_mode)
		zl_error(file_desc, ERR_INVOBJ, ERR_TEXT,
			RTS_ERROR_TEXT("Object compiled with CHSET=M which is different from $ZCHSET"));
	literal_ptr = code + file_hdr.a_text;
	for (cnt = hdr->vartab_len, curvar = VARTAB_ADR(hdr); cnt; --cnt, ++curvar)
	{ /* relocate the variable table */
		assert(0 < curvar->var_name.len);
		curvar->var_name.addr += (uint4)literal_ptr;
	}
	for (cnt = hdr->labtab_len, curlab = LABTAB_ADR(hdr); cnt; --cnt, ++curlab)
		/* relocate the label table */
		curlab->lab_name.addr += (uint4)literal_ptr;
	if (!addr_fix(file_desc, &file_hdr, &urx_lcl_anchor, hdr))
	{
		urx_free(&urx_lcl_anchor);
		zl_error(file_desc, ERR_INVOBJ, ERR_TEXT, RTS_ERROR_TEXT("address fixup failure"));
	}
	if (!zlput_rname(hdr))
	{
		urx_free(&urx_lcl_anchor);
		/* Copy routine name to local variable because zl_error free's it.  */
		memcpy(&module_name[0], hdr->routine_name.addr, hdr->routine_name.len);
		zl_error(file_desc, 0, ERR_LOADRUNNING, hdr->routine_name.len, &module_name[0]);
	}
	urx_add(&urx_lcl_anchor);
	old_rhead = (rhdtyp *)hdr->old_rhead_ptr;
	lbt_bot = (lab_tabent *)((char *)hdr + hdr->labtab_ptr);
	lbt_top = lbt_bot + hdr->labtab_len;
	while (old_rhead)
	{
		lbt_ent = lbt_bot;
		olbt_bot = (lab_tabent *)((char *)old_rhead + old_rhead->labtab_ptr);
		olbt_top = olbt_bot + old_rhead->labtab_len;
		for (olbt_ent = olbt_bot; olbt_ent < olbt_top; olbt_ent++)
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
		old_rhead = (rhdtyp *) old_rhead->old_rhead_ptr;
	}
	urx_resolve(hdr, lbt_bot, lbt_top);
	return TRUE;
}

bool addr_fix(int file, struct exec *fhead, urx_rtnref *urx_lcl, rhdtyp *code)
{
	res_list 	*res_root, *new_res, *res_temp, *res_temp1;
	char 		*symbols, *sym_temp, *sym_temp1, *symtop, *res_addr;
	struct relocation_info rel[RELREAD];
	int		numrel, rel_read, i, string_size, sym_size;
	size_t		status;
	mident_fixed	rtnid, labid;
	mstr		rtn_str;
	rhdtyp		*rtn;
	lab_tabent	*label, *labtop;
	bool		labsym;
	urx_rtnref	*urx_rp;
	urx_addr	*urx_tmpaddr;

	res_root = 0;
	numrel = (fhead->a_trsize + fhead->a_drsize) / SIZEOF(struct relocation_info);
	if (numrel * SIZEOF(struct relocation_info) != fhead->a_trsize + fhead->a_drsize)
		return FALSE;
	for ( ; numrel;)
	{
		rel_read = numrel < RELREAD ? numrel : RELREAD;
		DOREADRC(file, rel, rel_read * SIZEOF(struct relocation_info), status);
                if (0 != status)
		{
			res_free(res_root);
			return FALSE;
		}
		numrel -= rel_read;
		for (i = 0; i < rel_read; i++)
		{
			if (rel[i].r_extern)
			{
				new_res = (res_list *)malloc(SIZEOF(*new_res));
				new_res->symnum = rel[i].r_symbolnum;
				new_res->addr = rel[i].r_address;
				new_res->next = new_res->list = 0;
				/* Insert the relocation entry in symbol number order on the unresolved chain */
				if (!res_root)
					res_root = new_res;
				else
				{	res_temp = res_root;
					res_temp1 = 0;
					while (res_temp)
					{
						if (res_temp->symnum >= new_res->symnum)
							break;
						res_temp1 = res_temp;
						res_temp = res_temp->next;
					}
					if (res_temp)
					{	if (res_temp->symnum == new_res->symnum)
						{
							new_res->list = res_temp->list;
							res_temp->list = new_res;
						} else
						{	if (res_temp1)
							{
								new_res->next = res_temp1->next;
								res_temp1->next = new_res;
							} else
							{
								assert(res_temp == res_root);
								new_res->next = res_root;
								res_root = new_res;
							}
						}
					} else
						res_temp1->next = new_res;
				}
			} else
				*(unsigned int *)(((char *)code) + rel[i].r_address) += (unsigned int)code;
		}
	}
	/* All relocations within the routine should have been done, so copy the routine_name */
	assert(code->routine_name.len < SIZEOF(zlink_mname.c));
	memcpy(&zlink_mname.c[0], code->routine_name.addr, code->routine_name.len);
	zlink_mname.c[code->routine_name.len] = 0;
	if (!res_root)
		return TRUE;
	if ((off_t)-1 == lseek(file, (off_t)fhead->a_syms, SEEK_CUR))
	{	res_free(res_root);
		return FALSE;
	}
	DOREADRC(file, &string_size, SIZEOF(string_size), status);
	if (0 != status)
	{
		res_free(res_root);
		return FALSE;
	}
	string_size -= SIZEOF(string_size);
	symbols = malloc(string_size);
	DOREADRC(file, symbols, string_size, status);
	if (0 != status)
	{
		free(symbols);
		res_free(res_root);
		return FALSE;
	}
	/* Match up unresolved entries with the null terminated symbol name entries from the
	 * symbol text pool we just read in.
	 */
	sym_temp = sym_temp1 = symbols;
	symtop = symbols + string_size;
	for (i = 0; res_root; i++)
	{
		while (i < res_root->symnum)
		{	/* Forward symbol space until our symnum index (i) matches the symbol we are processing in res_root */
			while (*sym_temp)
			{
				if (sym_temp >= symtop)
				{
					free(symbols);
					res_free(res_root);
					return FALSE;
				}
				sym_temp++;
			}
			sym_temp++;
			sym_temp1 = sym_temp;
			i++;
		}
		assert (i == res_root->symnum);
		/* Find end of routine name that we care about */
		while (('.' != *sym_temp1) && *sym_temp1)
		{	if (sym_temp1 >= symtop)
			{
				free(symbols);
				res_free(res_root);
				return FALSE;
			}
			sym_temp1++;
		}
		sym_size = sym_temp1 - sym_temp;
		assert(sym_size <= MAX_MIDENT_LEN);
		memcpy(&rtnid.c[0], sym_temp, sym_size);
		rtnid.c[sym_size] = 0;
		if ('_' == rtnid.c[0])
			rtnid.c[0] = '%';
		assert((sym_size != mid_len(&zlink_mname)) || (0 != memcmp(&zlink_mname.c[0], &rtnid.c[0], sym_size)));
		rtn_str.addr = &rtnid.c[0];
		rtn_str.len = sym_size;
		rtn = find_rtn_hdr(&rtn_str); /* Routine already resolved? */
		sym_size = 0;
		labsym = FALSE;
		if (*sym_temp1 == '.')
		{	/* If symbol is for a label, find the end of the label name */
			sym_temp1++;
			sym_temp = sym_temp1;
			while (*sym_temp1)
			{
				if (sym_temp1 >= symtop)
				{
					free(symbols);
					res_free(res_root);
					return FALSE;
				}
				sym_temp1++;
			}
			sym_size = sym_temp1 - sym_temp;
			assert(sym_size <= MAX_MIDENT_LEN);
			memcpy(&labid.c[0], sym_temp, sym_size);
			labid.c[sym_size] = 0;
			if ('_' == labid.c[0])
				labid.c[0] = '%';
			labsym = TRUE;
		}
		sym_temp1++;
		sym_temp = sym_temp1;
		if (rtn)
		{	/* The routine part at least is known */
			if (labsym)
			{	/* Look our target label up in the routines label table */
				label = (lab_tabent *)((char *)rtn + rtn->labtab_ptr);
				labtop = label + rtn->labtab_len;
				for (; label < labtop && ((sym_size != label->lab_name.len)
					|| memcmp(&labid.c[0], label->lab_name.addr, sym_size)); label++)
					;
				if (label < labtop)
					res_addr = (char *)&label->LABENT_LNR_OFFSET;
				else
					res_addr = 0;
			} else
				res_addr = (char *)rtn;
			if (res_addr)
			{	/* The external symbol definition is available. Resolve all references to it */
				res_temp = res_root->next;
				while (res_root)
				{
					*(uint4 *)(((char *)code) + res_root->addr) = (unsigned int)res_addr;
					res_temp1 = res_root->list;
					free(res_root);
					res_root = res_temp1;
				}
				res_root = res_temp;
				continue;
			}
		}
		/* This symbol is unknown. Put on the (local) unresolved extern chain -- either for labels or routines */
		urx_rp = urx_putrtn(rtn_str.addr, rtn_str.len, urx_lcl);
		res_temp = res_root->next;
		while (res_root)
		{
			if (labsym)
				urx_putlab(&labid.c[0], sym_size, urx_rp, ((char *)code) + res_root->addr);
			else
			{	urx_tmpaddr = (urx_addr *)malloc(SIZEOF(urx_addr));
				urx_tmpaddr->next = urx_rp->addr;
				urx_tmpaddr->addr = (INTPTR_T *)(((char *)code) + res_root->addr);
				urx_rp->addr = urx_tmpaddr;
			}
			res_temp1 = res_root->list;
			free(res_root);
			res_root = res_temp1;
		}
		res_root = res_temp;
	}
	free(symbols);
	return TRUE;
}

void res_free(res_list *root)
{
	res_list *temp;

	while (root)
	{	while (root->list)
		{	temp = root->list->list;
			free(root->list);
			root->list = temp;
		}
		temp = root->next;
		free(root);
		root = temp;
	}
}

/* ZL_ERROR - perform cleanup and signal errors found in zlinking a mumps object module
 * err - an error code that accepts no arguments and
 * err2 - an error code that accepts two arguments (!AD)
 */
void zl_error(int4 file, int4 err, int4 err2, int4 len, char *addr)
{
	int rc;

	if (code)
	{
		GTM_TEXT_FREE(code);
		code = NULL;
	}
	CLOSEFILE_RESET(file, rc);	/* resets "file" to FD_INVALID */
	if ((0 != err) && (0 != err2))
		rts_error(VARLSTCNT(6) err, 0, err2, 2, len, addr);
	else if (0 != err)
		rts_error(VARLSTCNT(1) err);
	else
	{
		assert(0 != err2);
		rts_error(VARLSTCNT(4) err2, 2, len, addr);
	}
}
