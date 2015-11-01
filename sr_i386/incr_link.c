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

#include "mdef.h"

#include <unistd.h>
#include "gtm_stdio.h"
#include "gtm_string.h"
#include <errno.h>
#include <string.h>

#include "rtnhdr.h"
#include "compiler.h"
#include "urx.h"
#include "masscomp.h"
#include "gtmio.h"
#include "incr_link.h"

/* INCR_LINK - read and process a mumps object module.  Link said module to
 	currently executing image */

LITREF char gtm_release_name[];
LITREF int4 gtm_release_name_len;

static char *code;
GBLDEF mident zlink_mname;
GBLREF unsigned char *zl_lab_err;

bool addr_fix(int file, struct exec *fhead, urx_rtnref *urx_lcl, unsigned char *code);
void zl_error(int4 file, int4 err, int4 len, char *addr);

bool incr_link(int file_desc)
{
	rhdtyp	*hdr, *old_rhead;
	int code_size, save_errno;
	int4 rhd_diff,lab_miss_off,*olnt_ent,*olnt_top, read_size;
	mident		module_name;
	lbl_tables *lbt_ent, *lbt_bot, *lbt_top, *olbt_ent, *olbt_bot, *olbt_top;
	urx_rtnref	urx_lcl_anchor;
	bool	more, order;
	struct exec file_hdr;
	error_def(ERR_INVOBJ);
	error_def(ERR_LOADRUNNING);

	urx_lcl_anchor.len = 0;
	urx_lcl_anchor.addr = 0;
	urx_lcl_anchor.lab = 0;
	urx_lcl_anchor.next = 0;
	code = NULL;

/*
	read_size = read(file_desc, &file_hdr, sizeof(file_hdr));
	if (read_size != sizeof(file_hdr) || file_hdr.a_magic != OMAGIC || file_hdr.a_stamp != STAMP13)
		zl_error(file_desc, ERR_INVOBJ, 0, 0);
*/
	DOREADRL(file_desc, &file_hdr, sizeof(file_hdr), read_size);
	if (read_size != sizeof(file_hdr))
		if (-1 == read_size)
		{
			save_errno = errno < sys_nerr ? (0 <= errno ? errno : 0) : 0;
			zl_error(file_desc, ERR_INVOBJ, strlen(STRERROR(save_errno)),
					STRERROR(save_errno));
		}
		else
			zl_error(file_desc, ERR_INVOBJ, RTS_ERROR_TEXT("reading file header"));
	else if (OMAGIC != file_hdr.a_magic)
		zl_error(file_desc, ERR_INVOBJ, RTS_ERROR_TEXT("bad magic"));
	else if (STAMP13 != file_hdr.a_stamp)
		return FALSE;	/* wrong version */

	assert (file_hdr.a_bss == 0);
	code_size = file_hdr.a_text + file_hdr.a_data;
	code = malloc(code_size);
/*
	read_size = read(file_desc, code, code_size);
	if (read_size != code_size)
		zl_error(file_desc, ERR_INVOBJ, 0, 0);
*/
	DOREADRL(file_desc, code, code_size, read_size);
	if (read_size != code_size)
		if (-1 == read_size)
		{
			save_errno = errno < sys_nerr ? (0 <= errno ? errno : 0) : 0;
			zl_error(file_desc, ERR_INVOBJ, strlen(STRERROR(save_errno)),
					STRERROR(save_errno));
		}
		else
			zl_error(file_desc, ERR_INVOBJ, RTS_ERROR_TEXT("reading code"));

	hdr = (rhdtyp *)code;
	if (memcmp(&hdr->jsb[0], "GTM_CODE", sizeof(hdr->jsb)))
		zl_error(file_desc, ERR_INVOBJ, RTS_ERROR_TEXT("missing GTM_CODE"));

	memcpy(&zlink_mname.c[0], &hdr->routine_name, sizeof(mident));
	if (!addr_fix(file_desc, &file_hdr, &urx_lcl_anchor, code))
	{
		urx_free(&urx_lcl_anchor);
		zl_error(file_desc, ERR_INVOBJ, RTS_ERROR_TEXT("address fixup failure"));
	}

	if (!zlput_rname (hdr))
	{
		urx_free(&urx_lcl_anchor);
		/* Copy routine name to local variable because zl_error free's it.  */
		memcpy(module_name.c, hdr->routine_name.c, sizeof(mident));
		zl_error(file_desc, ERR_LOADRUNNING, mid_len(&module_name), module_name.c);
	}
	urx_add (&urx_lcl_anchor);

	old_rhead = (rhdtyp *) hdr->old_rhead_ptr;
	lbt_bot = (lbl_tables *) ((char *)hdr + hdr->labtab_ptr);
	lbt_top = lbt_bot + hdr->labtab_len;
	while (old_rhead)
	{
		rhd_diff = (char *) hdr - (char *) old_rhead;
		lab_miss_off = (char *)(&zl_lab_err) - rhd_diff - (char *) old_rhead;
		lbt_ent = lbt_bot;
		olnt_ent = (int4 *)((char *) old_rhead + old_rhead->lnrtab_ptr);
		olnt_top = olnt_ent + old_rhead->lnrtab_len;
		for ( ; olnt_ent < olnt_top ;olnt_ent++)
			*olnt_ent = lab_miss_off;
		olbt_bot = (lbl_tables *) ((char *) old_rhead + old_rhead->labtab_ptr);
		olbt_top = olbt_bot + old_rhead->labtab_len;
		for (olbt_ent = olbt_bot; olbt_ent < olbt_top ;olbt_ent++)
		{
			while((more = lbt_ent < lbt_top)
				&& (order = memcmp(&olbt_ent->lab_name.c[0],&lbt_ent->lab_name.c[0],sizeof(mident))) > 0)
				lbt_ent++;
			if (more && !order)
			{
				olnt_ent = (int4 *)((char *) old_rhead + olbt_ent->lab_ln_ptr);
				assert(*olnt_ent == lab_miss_off);
				*olnt_ent = *((int4 *) (code + lbt_ent->lab_ln_ptr));
			}
		}
		old_rhead->src_full_name = hdr->src_full_name;
		old_rhead->vartab_len = hdr->vartab_len;
		old_rhead->vartab_ptr = hdr->vartab_ptr + rhd_diff;
		old_rhead->ptext_ptr = hdr->ptext_ptr + rhd_diff;
		old_rhead->current_rhead_ptr = rhd_diff;
		old_rhead->temp_mvals = hdr->temp_mvals;
		old_rhead->temp_size = hdr->temp_size;
		old_rhead = (rhdtyp *) old_rhead->old_rhead_ptr;
	}
	urx_resolve (hdr, lbt_bot, lbt_top);
	return TRUE;
}

typedef struct res_list_struct {
	struct res_list_struct *next, *list;
	unsigned int	addr, symnum;
	} res_list;

void res_free(res_list *root);

#define RELREAD 50	/* number of relocation entries to buffer */

bool addr_fix(int file, struct exec *fhead, urx_rtnref *urx_lcl, unsigned char *code)
{
	res_list *res_root, *new_res, *res_temp, *res_temp1;
	char *symbols, *sym_temp, *sym_temp1, *symtop, *res_addr;
	struct relocation_info rel[RELREAD];
	struct nlist syms[10];
	int	numrel, rel_read, i, string_size, sym_size;
	size_t	status;
	mident	rtnid, labid;
	mstr	rtn_str;
	rhdtyp	*rtn;
	lent	*label, *labtop;
	bool	labsym;
	urx_rtnref	*urx_rp;
	urx_addr	*urx_tmpaddr;

	res_root = 0;
	numrel = (fhead->a_trsize + fhead->a_drsize) / sizeof(struct relocation_info);
	if (numrel * sizeof(struct relocation_info) != fhead->a_trsize + fhead->a_drsize)
		return FALSE;

	for ( ; numrel;)
	{
		rel_read = numrel < RELREAD ? numrel : RELREAD;
		DOREADRC(file, rel, rel_read * sizeof(struct relocation_info), status);
                if (0 != status)
		{
			res_free(res_root);
			return FALSE;
		}
		numrel -= rel_read;
		for (i = 0; i < rel_read; i++)
		{	if (rel[i].r_extern)
			{	new_res = (res_list *) malloc(sizeof(*new_res));
				new_res->symnum = rel[i].r_symbolnum;
				new_res->addr = rel[i].r_address;
				new_res->next = new_res->list = 0;
				if (!res_root)
					res_root = new_res;
				else
				{	res_temp = res_root;
					res_temp1 = 0;
					while (res_temp)
					{	if (res_temp->symnum >= new_res->symnum)
							break;
						res_temp1 = res_temp;
						res_temp = res_temp->next;
					}
					if (res_temp)
					{	if (res_temp->symnum == new_res->symnum)
						{	new_res->list = res_temp->list;
							res_temp->list = new_res;
						}
						else
						{	if (res_temp1)
							{	new_res->next = res_temp1->next;
								res_temp1->next = new_res;
							}
							else
							{	assert(res_temp == res_root);
								new_res->next = res_root;
								res_root = new_res;
							}
						}
					}
					else
						res_temp1->next = new_res;
				}
			}
			else
				*(unsigned int *)(code + rel[i].r_address) += (unsigned int) code;
		}
	}
	if (!res_root)
		return TRUE;

	if ((off_t)-1 == lseek(file, (off_t)fhead->a_syms, SEEK_CUR))
	{	res_free(res_root);
		return FALSE;
	}
	DOREADRC(file, &string_size, sizeof(string_size), status);
	if (0 != status)
	{
		res_free(res_root);
		return FALSE;
	}
	string_size -= sizeof(string_size);
	symbols = malloc(string_size);
	DOREADRC(file, symbols, string_size, status);
	if (0 != status)
	{
		free(symbols);
		res_free(res_root);
		return FALSE;
	}
	sym_temp = sym_temp1 = symbols;
	symtop = symbols + string_size;
	for (i = 0; res_root; i++)
	{	while (i < res_root->symnum)
		{ 	while (*sym_temp)
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
		while (*sym_temp1 != '.' && *sym_temp1)
		{	if (sym_temp1 >= symtop)
			{
				free(symbols);
				res_free(res_root);
				return FALSE;
			}
			sym_temp1++;
		}
		memset(&rtnid.c[0], 0, sizeof(rtnid));
		sym_size = sym_temp1 - sym_temp;
		assert (sym_size <= sizeof(mident));
		memcpy(&rtnid.c[0], sym_temp, sym_size);
		if (rtnid.c[0] == '_')
			rtnid.c[0] = '%';
		assert (memcmp(&zlink_mname.c[0], &rtnid.c[0], sizeof(mident)));
		rtn_str.addr = &rtnid.c[0];
		rtn_str.len = sym_size;
		rtn = find_rtn_hdr(&rtn_str);
		sym_size = 0;
		labsym = FALSE;
		if (*sym_temp1 == '.')
		{	sym_temp1++;
			sym_temp = sym_temp1;
			while (*sym_temp1)
			{	if (sym_temp1 >= symtop)
				{
					free(symbols);
					res_free(res_root);
					return FALSE;
				}
				sym_temp1++;
			}
			sym_size = sym_temp1 - sym_temp;
			assert (sym_size <= sizeof(mident));
			memset(&labid.c[0], 0, sizeof(labid));
			memcpy(&labid.c[0], sym_temp, sym_size);
			if (labid.c[0] == '_')
				labid.c[0] = '%';
			labsym = TRUE;
		}
		sym_temp1++;
		sym_temp = sym_temp1;
		if (rtn)
		{	if (labsym)
			{
				label = (lent *)((char *) rtn + rtn->labtab_ptr);
				labtop = label + rtn->labtab_len;
				for (; label < labtop && memcmp(&labid.c[0], &label->lname.c[0], sizeof(mident)); label++)
					;

				if (label < labtop)
					res_addr = (char *)(label->laddr + (char *) rtn);
				else
					res_addr = 0;
			}
			else
				res_addr = (char *)rtn;
			if (res_addr)
			{
				res_temp = res_root->next;
				while(res_root)
				{	*(uint4 *)(code + res_root->addr) = (unsigned int) res_addr;
					res_temp1 = res_root->list;
					free(res_root);
					res_root = res_temp1;
				}
				res_root = res_temp;
				continue;
			}
		}
		urx_rp = urx_putrtn(rtn_str.addr, rtn_str.len, urx_lcl);
		res_temp = res_root->next;
		while(res_root)
		{
			if (labsym)
				urx_putlab(&labid.c[0], sym_size, urx_rp, code + res_root->addr);
			else
			{	urx_tmpaddr = (urx_addr *) malloc(sizeof(urx_addr));
				urx_tmpaddr->next = urx_rp->addr;
				urx_tmpaddr->addr = (int4 *)(code + res_root->addr);
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


/* ZL_ERROR - perform cleanup and signal errors found in zlinking a mumps
	object module */

void zl_error(int4 file, int4 err, int4 len, char *addr)
{
	if (code)
		free(code);

	close(file);
	if (!len)
		rts_error(VARLSTCNT(1) err);
	else
		rts_error(VARLSTCNT(4) err, 2, len, addr);
}
