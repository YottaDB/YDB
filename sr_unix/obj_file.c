/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include <errno.h>

#include "compiler.h"
#include "rtnhdr.h"
#include "obj_gen.h"
#include "cgp.h"
#include "mdq.h"
#include "objlabel.h"
#include "stringpool.h"
#include "parse_file.h"
#include "gtmio.h"
#include "mmemory.h"
#include "obj_file.h"

GBLDEF char		object_file_name[MAX_FBUFF + 1];
GBLDEF short		object_name_len;
GBLDEF int		object_file_des;
DEBUG_ONLY(GBLDEF int	obj_bytes_written;)

GBLREF boolean_t	run_time;
GBLREF int4		lits_text_size, lits_mval_size;
GBLREF unsigned char	*runtime_base;
GBLREF mliteral		literal_chain;
GBLREF char		source_file_name[];
GBLREF unsigned short	source_name_len;
GBLREF mident		routine_name;
GBLREF spdesc		stringpool;
GBLREF int4		linkage_size;
GBLREF uint4		lnkrel_cnt;	/* number of entries in linkage Psect to relocate */
GBLREF int4		sym_table_size;

static char			emit_buff[OBJ_EMIT_BUF_SIZE];	/* buffer for emit output */
static int			emit_buff_used;		/* number of chars in emit_buff */
static struct rel_table		*link_rel, *link_rel_end; /* Linkage relocation entries.  */
static struct linkage_entry	*linkage_first, *linkage_last;
static struct sym_table		*symbols;
void	emit_link_reference(int4 refoffset, mstr *name);
int	output_symbol_size(void);

void	drop_object_file(void)
{
        if (0 < object_file_des)
        {
		UNLINK(object_file_name);
		close(object_file_des);
        }
}


/*
 *	emit_link_reference
 *
 *	Description: If not already defined, create relocation entry for
 *                   a linkage table entry to be resolved later.
 */
void	emit_link_reference(int4 refoffset, mstr *name)
{
	struct sym_table	*sym;
	struct rel_table	*newrel;

	sym = define_symbol(GTM_LINKAGE, name);
	assert(sym);
	if ((N_TEXT | N_EXT) != sym->n.n_type)
	{
		newrel = (struct rel_table *)mcalloc(sizeof(struct rel_table));
		newrel->next = (struct rel_table *)0;
		newrel->resolve = 0;
		newrel->r.r_address = refoffset;
		newrel->r.r_symbolnum = 0;
		if (NULL == link_rel)
			link_rel = newrel;
		else
			link_rel_end->next = newrel;
		link_rel_end = newrel;
		if (sym->resolve)
			newrel->resolve = sym->resolve;
		sym->resolve = newrel;
	}
}


/*
 *	emit_immed
 *
 *	Args:  buffer of executable code, and byte count to be output.
 */
void	emit_immed(char *source, uint4 size)
{
	short int 	write;
	error_def(ERR_STRINGOFLOW);

	if (run_time)
	{
		if (stringpool.free + size > stringpool.top)
			rts_error(VARLSTCNT(1) ERR_STRINGOFLOW);
		memcpy(stringpool.free, source, size);
		stringpool.free += size;
	} else
	{
		DEBUG_ONLY(obj_bytes_written += size);
		while (size > 0)
		{
			write = sizeof(emit_buff) - emit_buff_used;
			write = size < write ? size : write;
			memcpy(emit_buff + emit_buff_used, source, write);
			size -= write;
			source += write;
			emit_buff_used += write;
			if (0 != size)
				buff_emit();
		}
	}
}


/*
 *	buff_emit
 *
 *	Output partially or completely filled buffer
 */
void	buff_emit(void)
{
	int	stat;
	error_def(ERR_OBJFILERR);

	DOWRITERC(object_file_des, emit_buff, emit_buff_used, stat);
	if (0 != stat)
		rts_error(VARLSTCNT(5) ERR_OBJFILERR, 2, object_name_len, object_file_name, stat);
	emit_buff_used = 0;
}

/*
 * 	buff_flush
 *
 * 	Flush the contents of emit_buff to the file.
 */
void	buff_flush(void)
{
	if (emit_buff_used)
		buff_emit();
}

/*
 *	define_symbol
 *
 *	Args:  psect index, symbol name.
 *
 *	Description:  Buffers a definition of a global symbol with the
 *		given name in the given psect.
 */
struct sym_table *define_symbol(unsigned char psect, mstr *name)
{
	int4			cmp;
	struct sym_table	*sym, *sym1, *newsym;

	sym = symbols;
	sym1 = 0;
	while (sym)
	{
		if ((cmp = memvcmp(name->addr, (int)name->len, &sym->name[0], sym->name_len - 1)) <= 0)
			break;
		sym1 = sym;
		sym = sym->next;
	}

	if (cmp || !sym)
	{
		/* Didn't find it in existing symbols; create new symbol.  */
		newsym = (struct sym_table *)mcalloc(USIZEOF(struct sym_table) + name->len);
		newsym->name_len = name->len + 1;
		memcpy(&newsym->name[0], name->addr, name->len);
		newsym->name[name->len] = 0;
		newsym->n.n_type = N_EXT;
		if (GTM_CODE == psect)
			newsym->n.n_type |= N_TEXT;	/* if symbol is in GTM_CODE, it is defined */
		else
			lnkrel_cnt++;	/* otherwise it's external (only one reference in linkage Psect) */
		newsym->resolve = 0;
		newsym->linkage_offset = -1;	/* don't assign linkage Psect offset unless it's used */
		newsym->next = sym;
		if (sym1)
			sym1->next = newsym;
		else
			symbols = newsym;
		sym_table_size += newsym->name_len;
		sym = newsym;
	}

	return sym;
}


void	resolve_sym (void)
{
	uint4			symnum;
	struct sym_table	*sym;
	struct rel_table	*rel;

	for (sym = symbols, symnum = 0; sym; sym = sym->next, symnum++)
	{
		if (sym->resolve)
		{
			rel = sym->resolve;
			while (rel)
			{
				rel->r.r_symbolnum = symnum;
				rel = rel->resolve;
			}
		}
	}
}


/* Output the relocation table for the linkage section */
void	output_relocation (void)
{
	struct rel_table	*rel;

	for (rel = link_rel; NULL != rel;  rel = rel->next)
		emit_immed((char *)&rel->r, sizeof(rel->r));
}


/* Size that output_symbol() below will eventually generate */
int	output_symbol_size(void)
{
	uint4			string_length;
	struct sym_table	*sym;

	string_length = sizeof(int4);
	sym = symbols;
	while (sym)
	{
		string_length += sym->name_len;
		sym = sym->next;
	}
	return string_length;
}


/* Symbol string table output. Consists of a series of null terminated
   strings in symbol number order.
*/
void	output_symbol(void)
{
	uint4			string_length;
	struct sym_table	*sym;

	string_length = USIZEOF(int4) + sym_table_size;
	assert(string_length == output_symbol_size());
	emit_immed((char *)&string_length, sizeof(string_length));
	sym = symbols;
	while (sym)
	{
		emit_immed((char *)&sym->name[0], sym->name_len);
		sym = sym->next;
	}
}


/*
 *	obj_init
 *
 *	Description:	Initialize symbol list, relocation information, linkage Psect list, linkage_size.
 */
void	obj_init(void)
{
	lnkrel_cnt = 0;
	link_rel = link_rel_end = NULL;
	symbols = 0;
	sym_table_size = 0;

	linkage_first = linkage_last = NULL;
	linkage_size = MIN_LINK_PSECT_SIZE;	/* minimum size of linkage Psect, assuming no references from generated code */

	return;
}


/*
 *	comp_linkages
 *
 *	Description: Define the symbols and relocation entries we will need to create
 *		     the linkage section when this module is linked in.
 */
void	comp_linkages(void)
{
	mstr			name;
	struct linkage_entry	*linkagep;

	for (linkagep = linkage_first; NULL != linkagep; linkagep = linkagep->next)
	{
		name.len = linkagep->symbol->name_len - 1;	/* don't count '\0' terminator */
		name.addr = (char *)&linkagep->symbol->name[0];
		emit_link_reference(linkagep->symbol->linkage_offset, &name);
	}

	return;
}


void	emit_literals(void)
{
	uint4		offset, padsize;
	mliteral	*p;

	/* emit the literal text pool which includes the source file path and routine name */
	offset = (uint4)(stringpool.free - stringpool.base);
	emit_immed((char *)stringpool.base, offset);
	padsize = (uint4)(PADLEN(offset, NATIVE_WSIZE)); /* comp_lits aligns the start of source path on NATIVE_WSIZE boundary.*/
	if (padsize)
	{
		emit_immed(PADCHARS, padsize);
		offset += padsize;
	}
	emit_immed(source_file_name, source_name_len);
	offset += source_name_len;
	padsize = (uint4)(PADLEN(offset, NATIVE_WSIZE)); /* comp_lits aligns the start of routine_name on NATIVE_WSIZE boundary.*/
	if (padsize)
	{
		emit_immed(PADCHARS, padsize);
		offset += padsize;
	}
	emit_immed(routine_name.addr, routine_name.len);
	offset += routine_name.len;
	padsize = (uint4)(PADLEN(offset, NATIVE_WSIZE)); /* comp_lits aligns the start of literal area on NATIVE_WSIZE boundary.*/
	if (padsize)
	{
		emit_immed(PADCHARS, padsize);
		offset += padsize;
	}
	assert(offset == lits_text_size);

	/* emit the literal mval list */
	offset = 0;
 	dqloop(&literal_chain, que, p)
	{
		assert(p->rt_addr == offset);
		MV_FORCE_NUMD(&p->v);
		if (p->v.str.len)
			p->v.str.addr = (char *)(p->v.str.addr - (char *)stringpool.base); /* Initial offset */
		else
			p->v.str.addr = NULL;
		p->v.fnpc_indx = (unsigned char)-1;
		emit_immed((char *)&p->v, sizeof(p->v));
		offset += sizeof(p->v);
	}
	assert (offset == lits_mval_size);
}


/*
 *	find_linkage
 *
 *	Argument:	the name of a global symbol
 *
 *	Description:	Returns the offset into the linkage Psect of a global symbol.
 */
int4	find_linkage(mstr* name)
{
	struct linkage_entry	*newlnk;
	struct sym_table	*sym;

	sym = define_symbol(GTM_LITERALS, name);

	if (-1 == sym->linkage_offset)
	{
		/* Add new linkage psect entry at end of list.  */
		sym ->linkage_offset = linkage_size;

		newlnk = (struct linkage_entry *)mcalloc(sizeof(struct linkage_entry));
		newlnk->symbol = sym;
		newlnk->next = NULL;
		if (NULL == linkage_first)
			linkage_first = newlnk;
		if (NULL != linkage_last)
			linkage_last->next = newlnk;
		linkage_last = newlnk;

		linkage_size += sizeof(lnk_tabent);
	}
	return sym->linkage_offset;
}


/*
 *	literal_offset
 *
 *	Argument:	Offset of literal in literal Psect.
 *
 *	Description:	Return offset to literal from literal Psect base register.
 */
int	literal_offset(UINTPTR_T offset)
{
	/* If we have no offset assigned yet, assume a really big offset. */
	if ((unsigned int)-1 == offset)
		offset = MAXPOSINT4;
	return (int)((run_time ? (offset - (UINTPTR_T)runtime_base) : offset));
}
