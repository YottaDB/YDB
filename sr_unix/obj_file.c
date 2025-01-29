/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2023 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
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
#include "obj_gen.h"
#include "cgp.h"
#include "mdq.h"
#include "hashtab_str.h"
#include "objlabel.h"
#include "stringpool.h"
#include "parse_file.h"
#include "gtmio.h"
#include "mmemory.h"
#include "obj_file.h"
#include "mmrhash.h"

GBLREF unsigned char	object_file_name[];
GBLREF unsigned short	object_name_len;
GBLREF int		object_file_des;
DEBUG_ONLY(GBLDEF int	obj_bytes_written;)

GBLREF boolean_t	run_time;
GBLREF int4		lits_text_size, lits_mval_size;
GBLREF unsigned char	*runtime_base;
GBLREF mliteral		literal_chain;
GBLREF unsigned char 	source_file_name[];
GBLREF unsigned short	source_name_len;
GBLREF mident		routine_name;
GBLREF spdesc		rts_stringpool, stringpool;
GBLREF int4		linkage_size;
GBLREF uint4		lnkrel_cnt;	/* number of entries in linkage Psect to relocate */
GBLREF int4		sym_table_size;
GBLREF hash_table_str	*compsyms_hashtab;

static char			emit_buff[OBJ_EMIT_BUF_SIZE];	/* buffer for emit output */
static int			emit_buff_used;			/* number of chars in emit_buff */
static int			symcnt;
static struct rel_table		*link_rel, *link_rel_end;	/* Linkage relocation entries.  */

error_def(ERR_OBJFILERR);
error_def(ERR_STRINGOFLOW);

STATICFNDCL void emit_link_reference(int4 refoffset, mstr *name);

/* Routine to clear/delete the existing object file (used to delete existing temporary object file name
 * on an error.
 */
void drop_object_file(void)
{
	int	rc;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (0 < object_file_des)
        {
		rc = UNLINK((const char *)object_file_name);
		assert(!rc);
		PRO_ONLY(UNUSED(rc));
		CLOSE_OBJECT_FILE(object_file_des, rc);		/* Resets "object_file_des" to FD_INVALID */
		assert(!rc);
		rc = UNLINK(TADR(tmp_object_file_name));	/* Just in case the temp file was in play */
		assert(!rc);
		PRO_ONLY(UNUSED(rc));
	}
}

/*
 *	emit_link_reference
 *
 *	Description: If not already defined, create relocation entry for
 *                   a linkage table entry to be resolved later.
 */
STATICFNDEF void emit_link_reference(int4 refoffset, mstr *name)
{
	struct sym_table	*sym;
	struct rel_table	*newrel;

	sym = define_symbol(GTM_LINKAGE, name);
	assert(sym);
	if ((N_TEXT | N_EXT) != sym->n.n_type)
	{
		newrel = (struct rel_table *)mcalloc(sizeof(struct rel_table));
		newrel->next = NULL;
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
void emit_immed(char *source, uint4 size)
{
	int4 	write;

	if (run_time)
	{
		assert(rts_stringpool.base == stringpool.base);
		if (!IS_STP_SPACE_AVAILABLE_PRO(size))
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_STRINGOFLOW);
		memcpy(stringpool.free, source, size);
		stringpool.free += size;
	} else
	{
		DEBUG_ONLY(obj_bytes_written += size);
		while (0 < size)
		{
			write = SIZEOF(emit_buff) - emit_buff_used;
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
void buff_emit(void)
{
	int	stat;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Accumulate object code piece in the object hash with progressive murmurhash call */
	ydb_mmrhash_128_ingest(TADR(objhash_state), emit_buff, emit_buff_used);
	DOWRITERC(object_file_des, emit_buff, emit_buff_used, stat);
	if (0 != stat)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(5) ERR_OBJFILERR, 2, object_name_len, object_file_name, stat);
	emit_buff_used = 0;
}

/*
 * 	buff_flush
 *
 * 	Flush the contents of emit_buff to the file.
 */
void buff_flush(void)
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
	boolean_t		usehtab, added;
	int4			cmp;
	struct sym_table	*sym, *sym1 = NULL, *newsym;
	stringkey		symkey;
	ht_ent_str		*syment;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	usehtab = (SYM_HASH_CUTOVER < symcnt);
	DEBUG_ONLY(syment = NULL);
	if (!usehtab)
	{	/* "Brute force" version of lookup for now (until SYM_HASH_CUTOVER symbols defined) */
		assert(NULL == compsyms_hashtab || NULL == compsyms_hashtab->base);
		sym = TREF(defined_symbols);
		while (sym)
		{	/* Consider this a match only if type is N_EXT. If we are inserting an external reference symbol
			 * for the current routine name, we may find a N_TEXT entry. But in this case, we want to add another
			 * (N_EXT) entry so that the symbol table is correctly formed.
			 */
			if ((0 >= (cmp = memvcmp(name->addr, (int)name->len, &sym->name[0], sym->name_len - 1)))
			    && (N_EXT == sym->n.n_type))
				break;
			sym1 = sym;
			sym = sym->next;
		}
		if (sym && !cmp)
			return sym;
	} else
	{	/* Hashtable lookup  */
		if (!compsyms_hashtab)
		{	/* Allocate if not allocated yet */
			compsyms_hashtab = (hash_table_str *)malloc(sizeof(hash_table_str));
			assert(NULL != compsyms_hashtab);
			compsyms_hashtab->base = NULL;
		}
		if (!compsyms_hashtab->base)
		{	/* Need to initialize hash table and load it with the elements so far */
			init_hashtab_str(compsyms_hashtab, SYM_HASH_CUTOVER * 2, HASHTAB_NO_COMPACT, HASHTAB_NO_SPARE_TABLE);
			assert(compsyms_hashtab->base);
			for (sym = TREF(defined_symbols); sym; sym = sym->next)
			{
				if (N_EXT != sym->n.n_type)	/* Consider this a match only if type is N_EXT. See comment above */
					continue;
				symkey.str.addr = (char *)&sym->name[0];
				symkey.str.len = sym->name_len - 1;
				COMPUTE_HASH_STR(&symkey);
				added = add_hashtab_str(compsyms_hashtab, &symkey, sym, &syment);
				assert(added);
				PRO_ONLY(UNUSED(added));
				assert(syment->value);
				assert(syment->key.str.addr == (char *)&((struct sym_table *)syment->value)->name[0]);
			}
		}
		symkey.str = *name;	/* Copy of the key */
		COMPUTE_HASH_STR(&symkey);
		added = add_hashtab_str(compsyms_hashtab, &symkey, NULL, &syment);
		if (!added)
		{	/* Hash entry exists for this symbol */
			sym = (struct sym_table *)syment->value;
			assert(sym);
			assert(0 == memvcmp(name->addr, (int)name->len, &sym->name[0], sym->name_len - 1));
			return sym;
		}
	}
	/* Didn't find it in existing symbols; create new symbol.  */
	newsym = (struct sym_table *)mcalloc(sizeof(struct sym_table) + name->len);
	newsym->name_len = name->len + 1;
	memcpy(&newsym->name[0], name->addr, name->len);
	newsym->name[name->len] = 0;
	newsym->n.n_type = N_EXT;
	if (GTM_CODE == psect)
		newsym->n.n_type |= N_TEXT;	/* If symbol is in GTM_CODE, it is defined */
	else
		lnkrel_cnt++;		/* Otherwise it's external (only one reference in linkage Psect) */
	newsym->resolve = 0;
	newsym->linkage_offset = -1;	/* Don't assign linkage Psect offset unless it's used */
	if (!usehtab)
	{	/* Brute force -- add into the queue where the test failed (keeping sorted) */
		newsym->next = sym;
		if (sym1)
			sym1->next = newsym;
		else
			TREF(defined_symbols) = newsym;
	} else
	{	/* Hashtab usage -- add at beginning .. easiest since sorting doesn't matter for future lookups */
		newsym->next = TREF(defined_symbols);
		TREF(defined_symbols) = newsym;
		assert(syment);
		syment->value = newsym;
	}
	++symcnt;
	sym_table_size += newsym->name_len;
	return newsym;
}

/* Routine to resolve all the symbols of a given name to the same symbol number to associate relocation table
 * entries together properly so all references for a given name associate to the same linkage table entry.
 */
void resolve_sym (void)
{
	uint4			symnum;
	struct sym_table	*sym;
	struct rel_table	*rel;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	for (sym = TREF(defined_symbols), symnum = 0; sym; sym = sym->next, symnum++)
	{
		for (rel = sym->resolve; rel; rel = rel->resolve)
			rel->r.r_symbolnum = symnum;
	}
}

/* Output the relocation table for the linkage section */
void output_relocation (void)
{
	struct rel_table	*rel;

	for (rel = link_rel; NULL != rel;  rel = rel->next)
		emit_immed((char *)&rel->r, sizeof(rel->r));
}

#ifdef DEBUG
/* Size that output_symbol() below will eventually generate -- used to verify validity of OUTPUT_SYMBOL_SIZE macro */
int output_symbol_size(void)
{
	uint4			string_length;
	struct sym_table	*sym;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	string_length = sizeof(int4);
	sym = TREF(defined_symbols);
	while (sym)
	{
		string_length += sym->name_len;
		sym = sym->next;
	}
	return string_length;
}
#endif

/* Symbol string table output. Consists of a series of null terminated
 * strings in symbol number order.
 */
void output_symbol(void)
{
	uint4			string_length;
	struct sym_table	*sym;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	string_length = sizeof(int4) + sym_table_size;
	assert(string_length == output_symbol_size());
	emit_immed((char *)&string_length, sizeof(string_length));
	sym = TREF(defined_symbols);
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
void obj_init(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	lnkrel_cnt = symcnt = 0;
	link_rel = link_rel_end = NULL;
	TREF(defined_symbols) = NULL;
	TREF(linkage_first) = NULL;
	TREF(linkage_last) = NULL;
	sym_table_size = 0;
	linkage_size = MIN_LINK_PSECT_SIZE;	/* Minimum size of linkage Psect, assuming no references from generated code */
	return;
}

/*
 *	comp_linkages
 *
 *	Description: Define the symbols and relocation entries we will need to create
 *		     the linkage section when this module is linked in.
 */
void comp_linkages(void)
{
	mstr			name;
	struct linkage_entry	*linkagep;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	for (linkagep = TREF(linkage_first); NULL != linkagep; linkagep = linkagep->next)
	{
		name.len = linkagep->symbol->name_len - 1;	/* Don't count '\0' terminator */
		name.addr = (char *)&linkagep->symbol->name[0];
		emit_link_reference(linkagep->symbol->linkage_offset, &name);
	}

	return;
}

void emit_literals(void)
{
	mstr			name;
	uint4			offset, padsize;
	mliteral		*p;
	struct linkage_entry	*linkagep;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* emit the literal text pool which includes the source file path and routine name */
	offset = (uint4)(stringpool.free - stringpool.base);
	emit_immed((char *)stringpool.base, offset);
	padsize = (uint4)(PADLEN(offset, NATIVE_WSIZE)); /* comp_lits() aligns literal area on NATIVE_WSIZE boundary */
	if (padsize)
	{
		emit_immed(PADCHARS, padsize);
		offset += padsize;
	}
	emit_immed((char *)source_file_name, source_name_len);
	offset += source_name_len;
	padsize = (uint4)(PADLEN(offset, NATIVE_WSIZE)); /* comp_lits() aligns routine_name on NATIVE_WSIZE boundary */
	if (padsize)
	{
		emit_immed(PADCHARS, padsize);
		offset += padsize;
	}
	emit_immed(routine_name.addr, routine_name.len);
	offset += routine_name.len;
	padsize = (uint4)(PADLEN(offset, NATIVE_WSIZE)); /* comp_lits() aligns extern symbols on NATIVE_WSIZE boundary */
	if (padsize)
	{
		emit_immed(PADCHARS, padsize);
		offset += padsize;
	}
	/* Emit the names associated with the linkage table */
	for (linkagep = TREF(linkage_first); NULL != linkagep; linkagep = linkagep->next)
	{
		emit_immed((char *)linkagep->symbol->name, linkagep->symbol->name_len - 1);
		offset += linkagep->symbol->name_len - 1;
		padsize = (uint4)(PADLEN(offset, NATIVE_WSIZE)); /* comp_lits() aligns extern symbols on NATIVE_WSIZE boundary */
		if (padsize)
		{
			emit_immed(PADCHARS, padsize);
			offset += padsize;
		}
	}
	assert(offset == lits_text_size);
	/* Emit the linkage name mstr table (never relocated - always offset/length pair). Same index as linkage table */
	offset = 0;
	for (linkagep = TREF(linkage_first); (NULL != linkagep)
			&& ((MAXPOSINT4 - (2 * sizeof(mstr))) > offset); linkagep = linkagep->next)
	{
		name.char_len = 0;				/* No need for this length */
		name.len = linkagep->symbol->name_len - 1;	/* Don't count '\0' terminator */
		name.addr = (char *)linkagep->lit_offset;
		emit_immed((char *)&name, sizeof(mstr));
		offset += sizeof(mstr);
	}
	assert((ROUND_UP2((sm_long_t)(offset), NATIVE_WSIZE)) >= (sm_long_t)(offset));
	assert(MAXPOSINT4 >= offset);
	padsize = (uint4)(PADLEN(offset, NATIVE_WSIZE));
	if (padsize && (offset < (offset + padsize)))
	{
		emit_immed(PADCHARS, padsize);
		offset += padsize;
		PRO_ONLY(UNUSED(offset));
	}
	assert((MAXPOSINT4 - (3 * sizeof(mstr))) > offset); /* Should never happen */
	assert(0 == (uint4)(PADLEN(offset, NATIVE_WSIZE)));
	/* Emit the literal mval list (also aligned by comp_lits() on NATIVE_WSIZE boundary */
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
		if (!(MV_UTF_LEN & p->v.mvtype))
			p->v.str.char_len = 0;
		assert(MAX_STRLEN >= p->v.str.char_len);
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
int4 find_linkage(mstr* name)
{
	struct linkage_entry	*newlnk;
	struct sym_table	*sym;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	sym = define_symbol(GTM_LITERALS, name);
	if (-1 == sym->linkage_offset)
	{
		/* Add new linkage psect entry at end of list.  */
		sym ->linkage_offset = linkage_size;

		newlnk = (struct linkage_entry *)mcalloc(sizeof(struct linkage_entry));
		newlnk->symbol = sym;
		newlnk->next = NULL;
		if (NULL == TREF(linkage_first))
			TREF(linkage_first) = newlnk;
		if (NULL != TREF(linkage_last))
			(TREF(linkage_last))->next = newlnk;
		TREF(linkage_last) = newlnk;

		linkage_size += SIZEOF(lnk_tabent);
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
