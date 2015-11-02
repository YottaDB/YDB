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

#include "gtm_string.h"

#include <errno.h>

#include "compiler.h"
#include <rtnhdr.h>
#include "obj_gen.h"
#include "cgp.h"
#include "mdq.h"
#include "cmd_qlf.h"
#include "objlabel.h"	/* needed for masscomp.h */
#include "masscomp.h"
#include "stringpool.h"
#include "parse_file.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtmio.h"
#include "mmemory.h"
#include "obj_file.h"

LITREF char gtm_release_name[];
LITREF int4 gtm_release_name_len;

GBLREF mliteral 	literal_chain;
GBLREF char 		source_file_name[];
GBLREF unsigned short 	source_name_len;

GBLREF command_qualifier cmd_qlf;
GBLREF mident		routine_name;
GBLREF mident		module_name;
GBLREF boolean_t	run_time;
GBLREF int4		mlmax, mvmax;
GBLREF int4		code_size, lit_addrs, lits_size;

GBLDEF int4	psect_use_tab[GTM_LASTPSECT];	/* bytes of each psect in this module */
GBLREF char	object_file_name[];
GBLREF short	object_name_len;
GBLREF int	object_file_des;

static short int current_psect;
static char emit_buff[OBJ_EMIT_BUF_SIZE];	/* buffer for emit output */
static short int emit_buff_used;		/* number of chars in emit_buff */

GBLREF uint4 txtrel_cnt;
static uint4 cdlits;
static struct rel_table *data_rel, *data_rel_end;
static struct rel_table *text_rel, *text_rel_end;
DEBUG_ONLY(static uint4 		txtrel_cnt_in_hdr;)

error_def(ERR_OBJFILERR);

void create_object_file(rhdtyp *rhead)
{
	int status;
	unsigned char rout_len;
	uint4 stat;
	char		obj_name[SIZEOF(mident_fixed) + 5];
	mstr		fstr;
	parse_blk	pblk;
	struct exec	hdr;
	error_def(ERR_FILEPARSE);

	assert(!run_time);

	memset(&pblk, 0, SIZEOF(pblk));
	pblk.buffer = object_file_name;
	pblk.buff_size = MAX_FBUFF;
	/* create the object file */
	fstr.len = (MV_DEFINED(&cmd_qlf.object_file) ? cmd_qlf.object_file.str.len : 0);
	fstr.addr = cmd_qlf.object_file.str.addr;
	rout_len = module_name.len;
	memcpy(&obj_name[0], module_name.addr, rout_len);
	obj_name[rout_len] = '.';
	obj_name[rout_len + 1] = 'o';
	obj_name[rout_len + 2] = 0;
	pblk.def1_size = rout_len + 2;
	pblk.def1_buf = obj_name;
	status = parse_file(&fstr, &pblk);
	if (!(status & 1))
		rts_error(VARLSTCNT(5) ERR_FILEPARSE, 2, fstr.len, fstr.addr, status);

	object_name_len = pblk.b_esl;
	object_file_name[object_name_len] = 0;

	OPEN_OBJECT_FILE(object_file_name, O_CREAT | O_RDWR, object_file_des);
	if (FD_INVALID == object_file_des)
		rts_error(VARLSTCNT(5) ERR_OBJFILERR, 2, object_name_len, object_file_name, errno);
	memcpy(&rhead->jsb[0], "GTM_CODE", SIZEOF(rhead->jsb));
	emit_addr((char *)&rhead->src_full_name.addr - (char *)rhead,
		(int4)rhead->src_full_name.addr, (int4 *)&rhead->src_full_name.addr);
	emit_addr((char *)&rhead->routine_name.addr - (char *)rhead,
		(int4)rhead->routine_name.addr, (int4 *)&rhead->routine_name.addr);
	txtrel_cnt += 2;
	DEBUG_ONLY(txtrel_cnt_in_hdr = txtrel_cnt;)

	set_psect(GTM_CODE, 0);
	hdr.a_magic = OMAGIC;
	hdr.a_stamp = OBJ_LABEL;
	hdr.a_entry = 0;
	hdr.a_bss = 0;
	hdr.a_text = code_size;
	assert(0 == PADLEN(lits_size, NATIVE_WSIZE));
	hdr.a_data = lits_size;		/* and pad to even # */
	hdr.a_syms = (mlmax + cdlits) * SIZEOF(struct nlist);
	hdr.a_trsize = txtrel_cnt * SIZEOF(struct relocation_info);
	hdr.a_drsize = lit_addrs * SIZEOF(struct relocation_info);
	emit_immed((char *)&hdr, SIZEOF(hdr));
	memset(psect_use_tab, 0, SIZEOF(psect_use_tab));
	emit_immed((char *)rhead, SIZEOF(*rhead));
}

void close_object_file(void)
{
	assert(0 == PADLEN(lits_size, NATIVE_WSIZE));
	resolve_sym();
	output_relocation();
	output_symbol();
	if (emit_buff_used)
		buff_emit();
	if ((off_t)-1 == lseek(object_file_des, (off_t)0, SEEK_SET))
		rts_error(VARLSTCNT(5) ERR_OBJFILERR, 2, object_name_len, object_file_name, errno);
}


void drop_object_file(void)
{
	int	rc;

        if (FD_INVALID != object_file_des)
        {
		UNLINK(object_file_name);
		CLOSEFILE_RESET(object_file_des, rc);	/* resets "object_file_des" to FD_INVALID */
        }
}

GBLREF spdesc stringpool;

void emit_addr(int4 refaddr, int4 offset, int4 *result)
{
	struct rel_table *newrel;

	if (run_time)
	{
		unsigned char *ptr;
		ptr = stringpool.free;
		*result = offset - (int4) ptr;
	} else
	{	*result = offset + code_size;
		newrel = (struct rel_table *) mcalloc(SIZEOF(struct rel_table));
		newrel->next = (struct rel_table *) 0;
		newrel->resolve = 0;
		newrel->r.r_address = refaddr;
		newrel->r.r_symbolnum = N_DATA;
		newrel->r.r_pcrel = 0;
		newrel->r.r_length = 2;
		newrel->r.r_extern = 0;
		newrel->r.r_pad = 0;
		if (!text_rel)
			text_rel = text_rel_end = newrel;
		else
		{	text_rel_end->next = newrel;
			text_rel_end = newrel;
		}
	}
	return;
}


void emit_pidr(int4 refoffset, int4 data_offset, int4 *result)
{
	struct rel_table *newrel;

	assert(!run_time);
	refoffset += code_size;
	data_offset += code_size;
	*result = data_offset;
	newrel = (struct rel_table *) mcalloc(SIZEOF(struct rel_table));
	newrel->next = (struct rel_table *)0;
	newrel->resolve = 0;
	newrel->r.r_address = refoffset;
	newrel->r.r_symbolnum = N_DATA;
	newrel->r.r_pcrel = 0;
	newrel->r.r_length = 2;
	newrel->r.r_extern = 0;
	newrel->r.r_pad = 0;
	if (!data_rel)
		data_rel = data_rel_end = newrel;
	else
	{	data_rel_end->next = newrel;
		data_rel_end = newrel;
	}
}


void emit_reference(uint4 refaddr, mstr *name, uint4 *result)
{
	struct sym_table *sym;
	struct rel_table *newrel;

	sym = define_symbol(0, name, 0);
	assert(sym);
	if (sym->n.n_type == (N_TEXT | N_EXT))
		*result = sym->n.n_value;
	else
	{
		newrel = (struct rel_table *) mcalloc(SIZEOF(struct rel_table));
		newrel->next = (struct rel_table *)0;
		newrel->resolve = 0;
		newrel->r.r_address = refaddr;
		newrel->r.r_symbolnum = 0;
		newrel->r.r_pcrel = 0;
		newrel->r.r_length = 2;
		newrel->r.r_extern = 1;
		newrel->r.r_pad = 0;
		if (!text_rel)
			text_rel = text_rel_end = newrel;
		else
		{	text_rel_end->next = newrel;
			text_rel_end = newrel;
		}
		if (sym->resolve)
			newrel->resolve = sym->resolve;
		sym->resolve = newrel;
		*result = 0;
	}
}


/*
 *	emit_immed
 *
 *	Args:  buffer of executable code, and byte count to be output.
 */

error_def(ERR_STRINGOFLOW);
void emit_immed(char *source, uint4 size)
{
	short int write;

	if (run_time)
	{
		if (stringpool.free + size > stringpool.top)
			rts_error(VARLSTCNT(1) ERR_STRINGOFLOW);
		memcpy(stringpool.free, source, size);
		stringpool.free += size;
	} else
	{	while(size > 0)
		{
			write = SIZEOF(emit_buff) - emit_buff_used;
			write = size < write ? size : write;
			memcpy(emit_buff + emit_buff_used, source, write);
			size -= write;
			source += write;
			emit_buff_used += write;
			psect_use_tab[current_psect] += write;
			if (size)
				buff_emit();
		}
	}
}


/*
 *	buff_emit
 *
 *	Args:  buffer pointer, number of bytes to emit
 */

void buff_emit(void)
{
	uint4 stat;

	if (-1 == write(object_file_des, emit_buff, emit_buff_used))
		rts_error(VARLSTCNT(5) ERR_OBJFILERR, 2, object_name_len, object_file_name, errno);
	emit_buff_used = 0;
}


void set_psect(unsigned char psect,unsigned char offset)
{
	current_psect = psect;
	return;
}

/*
 *	define_symbol
 *
 *	Args:  psect index, symbol name, symbol value.
 *
 *	Description:  Buffers a definition of a global symbol with the
 *		given name and value in the given psect.
 */

static struct sym_table *symbols;
struct sym_table *define_symbol(unsigned char psect, mstr *name, int4 value)
{
	int cmp;
	struct sym_table *sym, *sym1, *newsym;

	sym = symbols;
	sym1 = 0;
	while(sym)
	{
		if ((cmp = memvcmp(name->addr, name->len, &sym->name[0], sym->name_len - 1)) <= 0)
			break;
		sym1 = sym;
		sym = sym->next;
	}
	if (cmp || !sym)
	{	newsym = (struct sym_table *) mcalloc(SIZEOF(struct sym_table) + name->len);
		newsym->name_len = name->len + 1;
		memcpy(&newsym->name[0], name->addr, name->len);
		newsym->name[ name->len ] = 0;
		newsym->n.n_strx = 0;
		newsym->n.n_type = N_EXT;
		if (psect == GTM_CODE)
			newsym->n.n_type |= N_TEXT;	/* if symbol is in GTM_CODE, it is defined */
		else
			txtrel_cnt++;
		newsym->n.n_other = 0;
		newsym->n.n_desc = 0;
		newsym->n.n_value = value;
		newsym->resolve = 0;
		newsym->next = sym;
		if (sym1)
			sym1->next = newsym;
		else
			symbols = newsym;
		cdlits++;
		return 0;
	}
	if (!(sym->n.n_type & N_TEXT))
		txtrel_cnt++;
	return sym;
}

void resolve_sym(void)
{
	uint4 symnum;
	struct sym_table *sym;
	struct rel_table *rel;

	symnum = 0;
	sym = symbols;
	while (sym)
	{	if (sym->resolve)
		{	rel = sym->resolve;
			while (rel)
			{	rel->r.r_symbolnum = symnum;
				rel = rel->resolve;
			}
		}
		symnum++;
		sym = sym->next;
	}
}


void output_relocation(void)
{
	struct rel_table 	*rel;
	DEBUG_ONLY(int	cnt;)

	DEBUG_ONLY(cnt = 0;)
	rel = text_rel;
	while (rel)
	{
		emit_immed((char *)&rel->r, SIZEOF(rel->r));
		rel = rel->next;
		DEBUG_ONLY(cnt++;)
	}
	assert(cnt == txtrel_cnt_in_hdr);

	DEBUG_ONLY(cnt = 0;)
	rel = data_rel;
	while (rel)
	{
		emit_immed((char *)&rel->r, SIZEOF(rel->r));
		rel = rel->next;
		DEBUG_ONLY(cnt++;)
	}
	assert(cnt == lit_addrs);
}


void output_symbol(void)
{
	uint4 string_length;
	struct sym_table *sym;

	string_length = SIZEOF(int4);
	sym = symbols;
	while (sym)
	{
		sym->n.n_strx = string_length;
		emit_immed((char *)&sym->n, SIZEOF(sym->n));
		string_length += sym->name_len;
		sym = sym->next;
	}
	emit_immed((char *)&string_length, SIZEOF(string_length));
	sym = symbols;
	while (sym)
	{
		emit_immed((char *)&sym->name[0], sym->name_len);
		sym = sym->next;
	}
}


void obj_init(void)
{
	cdlits = txtrel_cnt = 0;
	data_rel = text_rel = data_rel_end = text_rel_end = 0;
	symbols = 0;
}



void emit_literals(void)
{
	uint4 offset, padsize;
	mliteral *p;

	set_psect(GTM_LITERALS, 0);
	offset = stringpool.free - stringpool.base;
	emit_immed((char *)stringpool.base, offset);
	/* comp_lits aligns the start of source path on a NATIVE_WSIZE boundary.*/
	padsize = PADLEN(offset, NATIVE_WSIZE);
	if (padsize)
	{
		emit_immed(PADCHARS, padsize);
		offset += padsize;
	}
	emit_immed(source_file_name, source_name_len);
	offset += source_name_len;
	/* comp_lits aligns the start of routine_name on a NATIVE_WSIZE boundary.*/
	padsize = PADLEN(offset, NATIVE_WSIZE);
	if (padsize)
	{
		emit_immed(PADCHARS, padsize);
		offset += padsize;
	}
	emit_immed(routine_name.addr, routine_name.len);
	offset += routine_name.len;
	/* comp_lits aligns the start of the literal area on a NATIVE_WSIZE boundary.*/
	padsize = PADLEN(offset, NATIVE_WSIZE);
	if (padsize)
	{
		emit_immed(PADCHARS, padsize);
		offset += padsize;
	}

	dqloop(&literal_chain, que, p)
	{
		assert (p->rt_addr == offset);
		MV_FORCE_NUMD(&p->v);
		if (p->v.str.len)
			emit_pidr(p->rt_addr + ((char *) &p->v.str.addr - (char *)&p->v),
				 p->v.str.addr - (char *) stringpool.base, (int4 *)&p->v.str.addr);
		else
			p->v.str.addr = 0;
		emit_immed((char *)&p->v, SIZEOF(p->v));
		offset += SIZEOF(p->v);
	}
	assert(lits_size == offset);
}
