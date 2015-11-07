/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include "gtm_string.h"
#include "gtm_unistd.h"
#include "gtm_stdio.h"

#include <rtnhdr.h>
#include "compiler.h"
#include "urx.h"
#include "objlabel.h"
#include "gtmio.h"
#include "zroutines.h"
#include "incr_link.h"
#include "cachectl.h"
#include "obj_file.h"
#include "stringpool.h"
#include "gtm_limits.h"
#include "min_max.h"
#include "gtmdbglvl.h"
#include "cmd_qlf.h"	/* needed for CQ_UTF8 */
#include "gtm_text_alloc.h"

#define RELOCATE(field, type, base) field = (type)((unsigned char *)(field) + (UINTPTR_T)(base))
#define RELREAD 50			/* number of relocation entries to buffer */

/* This macro will check if the file is an old non-shared-binary variant of GT.M code and if
 * so just return false to signal a recompile. The assumption is that if we fall out of this
 * macro that there is truly a problem and other measures should be taken (e.g. call zlerror()).
 * At some point this code can be disabled with the NO_NONUSB_RECOMPILE varible defined. Rather
 * than keep old versions of control blocks around that will confuse the issue, we know that the
 * routine header of these versions started 10 32bit words into the object. Read in the eight
 * bytes from that location and check against the JSB_MARKER we still use today.
 */
#ifndef NO_NONUSB_RECOMPILE
#  define CHECK_NONUSB_RECOMPILE								\
{												\
	if (-1 != (status = (ssize_t)lseek(file_desc, COFFHDRLEN, SEEK_SET)))			\
	{											\
		DOREADRC_OBJFILE(file_desc, marker, SIZEOF(JSB_MARKER) - 1, status);		\
	} else											\
		status = errno;									\
	if ((0 == status) && (0 == MEMCMP_LIT(marker, JSB_MARKER)))				\
	{											\
		free(hdr);									\
		return FALSE;	/* Signal recompile */						\
	}											\
}
#else
#  define CHECK_NONUSB_RECOMPILE 	/* No old recompile check is being generated */
#endif

/* INCR_LINK - read and process a mumps object module.  Link said module to currently executing image */

LITREF char		gtm_release_name[];
LITREF int4		gtm_release_name_len;

static unsigned char	*sect_ro_rel, *sect_rw_rel, *sect_rw_nonrel;
static boolean_t	shlib;
static rhdtyp		*hdr;

GBLREF mident_fixed	zlink_mname;
GBLREF mach_inst	jsb_action[JSB_ACTION_N_INS];
GBLREF uint4		gtmDebugLevel;
GBLREF boolean_t	gtm_utf8_mode;

ZOS_ONLY(
GBLDEF unsigned char	*text_section;
GBLDEF boolean_t	extended_symbols_present;
GBLDEF int		total_length;
GBLDEF int		text_counter = 0;
)

typedef struct	res_list_struct
{
	struct res_list_struct	*next,
				*list;
	unsigned int		addr,
				symnum;
} res_list;

error_def(ERR_DLLCHSETM);
error_def(ERR_DLLCHSETUTF8);
error_def(ERR_DLLVERSION);
error_def(ERR_INVOBJ);
error_def(ERR_LOADRUNNING);
error_def(ERR_TEXT);

void		res_free(res_list *root);
boolean_t	addr_fix(int file, unsigned char *shdr, urx_rtnref *urx_lcl);
void		zl_error(int4 file, zro_ent *zroe, int4 err, int4 len, char *addr, int4 len2, char *addr2);
void		zl_error_hskpng(int4 file);
int		cacheflush (void *addr, long nbytes, int cache_select);

bool incr_link (int file_desc, zro_ent *zro_entry)
{
	rhdtyp		*old_rhead;
	int		sect_ro_rel_size, sect_rw_rel_size;
	ssize_t	 	status, sect_rw_nonrel_size;
	lab_tabent	*lbt_ent, *lbt_bot, *lbt_top, *olbt_ent, *olbt_bot, *olbt_top;
	mident_fixed	module_name;
	pre_v5_mident	*pre_v5_routine_name;
	urx_rtnref	urx_lcl_anchor;
	int		order;
	boolean_t	dynlits;
	size_t		offset_correction;
	unsigned char	*shdr, *rel_base;
	mval		*curlit, *littop;
	lab_tabent	*curlbe, *lbetop;
	var_tabent	*curvar, *vartop;
	char		name_buf[PATH_MAX+1];
	int		name_buf_len, alloc_len;
	char		marker[SIZEOF(JSB_MARKER) - 1];
	char		*rw_rel_start;

	AIX_ONLY(
		FILHDR	  	hddr;
		unsigned short  magic;
	)
	ZOS_ONLY(
		ESD symbol;
		total_length = 0;
		extended_symbols_present = FALSE;
		text_counter = 0;
		memset(&symbol, 0 , SIZEOF(ESD));
		assert(NULL == text_section);
		ZOS_FREE_TEXT_SECTION;
	)
	urx_lcl_anchor.len = 0;
	urx_lcl_anchor.addr = 0;
	urx_lcl_anchor.lab = 0;
	urx_lcl_anchor.next = 0;
	shlib = FALSE;
	hdr = NULL;
	shdr = NULL;
	sect_ro_rel = sect_rw_rel = sect_rw_nonrel = NULL;
	if (file_desc)
	{	/* This is a disk resident object we will be reading in */
		shlib = FALSE;
		assert(NULL == zro_entry);
	} else
	{
		shlib = TRUE;
		assert(zro_entry);
	}
	/* Get the routine header where we can make use of it */
	hdr = (rhdtyp *)malloc(SIZEOF(rhdtyp));
	if (shlib)
	{	/* Make writable copy of header as header of record.
		 * On some platforms, the address returned by dlsym() is not the actual shared code address, but normally
		 * an address to the linkage table, eg. TOC (AIX), PLT (HP-UX). Computing the actual shared code address
		 * is platform dependent and is handled by the macro (see incr_link_sp.h)
		 */
		shdr = (unsigned char *)GET_RTNHDR_ADDR(zro_entry->shrsym);
		memcpy(hdr, shdr, SIZEOF(rhdtyp));
		hdr->shlib_handle = zro_entry->shrlib;
	} else
	{	/* Seek past native object headers to get GT.M object's routine header */
		/* To check if it is not an xcoff64 bit .o */
		AIX_ONLY(
			DOREADRC(file_desc, &hddr, SIZEOF(FILHDR), status);
			if (0 == status)
			{
				magic = hddr.f_magic;
				if (-1 == (status = (ssize_t)lseek(file_desc, -(SIZEOF(FILHDR)), SEEK_SET)))
					status = errno;
				if (magic != U64_TOCMAGIC)
					return FALSE;
			} else
				zl_error(file_desc, zro_entry, ERR_INVOBJ, 0, 0, 0, 0);
		)
		/* In the GOFF .o on zOS, if the symbol name(name of the module) exceeds ESD_NAME_MAX_LENGTH (8), *
		 * then 2 extra extended records are emitted, which causes the start of text section to vary
		 */
#ifdef __MVS__
			DOREADRC(file_desc, &symbol, SIZEOF(symbol), status);	/* This is HDR record */
			if (0 == status)
			{
				DOREADRC(file_desc, &symbol, SIZEOF(symbol), status)	/* First symbol (ESD record) */
				if (0 == status)
				{
					if (0x01 == symbol.ptv[1])	/* which means the extended records are there */
						extended_symbols_present = TRUE;
					else
					{
						assert(0x0 == symbol.ptv[1]);
						extended_symbols_present = FALSE;
					}
				}
			}
			if (0 != status)
				zl_error(file_desc, zro_entry, ERR_INVOBJ, 0, 0, 0, 0);
#endif
		if (-1 != (status = (ssize_t)lseek(file_desc, NATIVE_HDR_LEN, SEEK_SET)))
		{
			ZOS_ONLY(extract_text(file_desc, &total_length);)
			DOREADRC_OBJFILE(file_desc, hdr, SIZEOF(rhdtyp), status);
		} else
			status = errno;
		if (0 != status)
		{
			CHECK_NONUSB_RECOMPILE;
			zl_error(file_desc, zro_entry, ERR_INVOBJ, 0, 0, 0, 0);
		}
	}
	if ((0 != memcmp(hdr->jsb, (char *)jsb_action, SIZEOF(jsb_action)))
		|| (0 != memcmp(&hdr->jsb[SIZEOF(jsb_action)], JSB_MARKER,
		MIN(STR_LIT_LEN(JSB_MARKER), SIZEOF(hdr->jsb) - SIZEOF(jsb_action)))))
	{
		if (!shlib)	/* Shared library cannot recompile so this is always an error */
		{
			CHECK_NONUSB_RECOMPILE;
		}
		zl_error(file_desc, zro_entry, ERR_INVOBJ, 0, 0, 0, 0);
	}
	/* Binary version check. If no match, shlib gets error, otherwise signal recompile */
	if (MAGIC_COOKIE != hdr->objlabel)
	{
		if (shlib)
		{
			if (MAGIC_COOKIE_V5 > hdr->objlabel)
			{ 	/* The library was built using a version prior to V50FT01. The routine_name field of the
				 * pre-V5 routine header was an 8-byte char array, so read the routine name in the old format
				 */
				int len;
				pre_v5_routine_name = (pre_v5_mident *)((char*)hdr + PRE_V5_RTNHDR_RTNOFF);
				for (len = 0; len < SIZEOF(pre_v5_mident) && pre_v5_routine_name->c[len]; len++)
					;
				zl_error(0, zro_entry, ERR_DLLVERSION, len, &(pre_v5_routine_name->c[0]),
				 	zro_entry->str.len, zro_entry->str.addr);
			}
#if defined(__osf__) || defined(__hppa)
			else if (MAGIC_COOKIE_V52 > hdr->objlabel)
			{	/* Note: routine_name field has not been relocated yet, so compute its absolute
				 * address in the shared library and use it
				 */
				v50v51_mstr	*mstr5051;	/* declare here so don't have to conditionally add above */
				mstr5051 = (v50v51_mstr *)((char *)hdr + V50V51_RTNHDR_RTNMSTR_OFFSET);
				zl_error(0, zro_entry, ERR_DLLVERSION, mstr5051->len,
					 ((char *)shdr + *(int4 *)((char *)hdr + V50V51_FTNHDR_LITBASE_OFFSET)
					  + (int4)mstr5051->addr),
					 zro_entry->str.len, zro_entry->str.addr);
			}
#endif
			else	/* V52 or later but not current version */
			{	/* Note: routine_name field has not been relocated yet, so compute its absolute
				 * address in the shared library and use it
				 */
				zl_error(0, zro_entry, ERR_DLLVERSION, hdr->routine_name.len, (char *)shdr +
					(UINTPTR_T)hdr->literal_text_adr + (UINTPTR_T)hdr->routine_name.addr,
				 	zro_entry->str.len, zro_entry->str.addr);
			}
		}
		ZOS_FREE_TEXT_SECTION;
		return FALSE;
	}
	if (((hdr->compiler_qlf & CQ_UTF8) && !gtm_utf8_mode) || (!(hdr->compiler_qlf & CQ_UTF8) && gtm_utf8_mode))
	{ 	/* object file compiled with a different $ZCHSET is being used */
		if (shlib)	/* Shared library cannot recompile so this is always an error */
		{	/* Note: routine_name field has not been relocated yet, so compute its absolute address
			 * in the shared library and use it
			 */
			if ((hdr->compiler_qlf & CQ_UTF8) && !gtm_utf8_mode)
			{
				zl_error(0, zro_entry, ERR_DLLCHSETUTF8, (int)hdr->routine_name.len, (char *)shdr +
					(UINTPTR_T)hdr->literal_text_adr + (UINTPTR_T)hdr->routine_name.addr,
					 (int)zro_entry->str.len, zro_entry->str.addr);
			} else
			{
				zl_error(0, zro_entry, ERR_DLLCHSETM, (int)hdr->routine_name.len, (char *)shdr +
					 (UINTPTR_T)hdr->literal_text_adr + (UINTPTR_T)hdr->routine_name.addr,
					 (int)zro_entry->str.len, zro_entry->str.addr);
			}
		}
		zl_error_hskpng(file_desc);
		if ((hdr->compiler_qlf & CQ_UTF8) && !gtm_utf8_mode)
			rts_error(VARLSTCNT(6) ERR_INVOBJ, 0,
				ERR_TEXT, 2, LEN_AND_LIT("Object compiled with CHSET=UTF-8 which is different from $ZCHSET"));
		else
			rts_error(VARLSTCNT(6) ERR_INVOBJ, 0,
				ERR_TEXT, 2, LEN_AND_LIT("Object compiled with CHSET=M which is different from $ZCHSET"));
	}
	/* Read in and/or relocate the pointers to the various sections. To understand the size calculations
	 * being done note that the contents of the various xxx_adr pointers in the routine header are
	 * initially the offsets from the start of the object. This is so we can address the various sections
	 * via offset now while linking and via address later during runtime.
	 *
	 * Read-only releasable section
	 */
	dynlits = DYNAMIC_LITERALS_ENABLED(hdr);
	rw_rel_start = RW_REL_START_ADR(hdr);	/* Marks end of R/O-release section and start of R/W-release section */
	if (shlib)
		rel_base = shdr;
	else
	{
		sect_ro_rel_size = (unsigned int)((INTPTR_T)rw_rel_start - (INTPTR_T)hdr->ptext_adr);
		sect_ro_rel = GTM_TEXT_ALLOC(sect_ro_rel_size);
		/* It should be aligned well at this point but make a debug level check to verify */
		assert((INTPTR_T)sect_ro_rel == ((INTPTR_T)sect_ro_rel & ~(LINKAGE_PSECT_BOUNDARY - 1)));
		DOREADRC_OBJFILE(file_desc, sect_ro_rel, sect_ro_rel_size, status);
		if (0 != status)
			zl_error(file_desc, zro_entry, ERR_INVOBJ, 0, 0, 0, 0);
		/* The offset correction is the amount that needs to be applied to a given storage area that
		 * is no longer contiguous with the routine header. In this case, the code and other sections
		 * are no longer contiguous with the routine header but the initial offsets in the routine
		 * header make the assumption that they are. Therefore these sections have a base address equal
		 * to the length of the routine header. The offset correction is what will adjust the base
		 * address so that this offset is removed and the pointer can now truly point to the section
		 * it needs to point to.
		 *
		 * An example may make this more clear. We have two blocks of storage: block A and block B. Now
		 * block A has 2 fields that will ultimately point into various places in block B. These pointers
		 * are initialized to be the offset from the start of block A to the position in block B. Now we
		 * have two cases. In the first case block A and block B are contiguous. Therefore in order to
		 * relocate the addresses in block A, all you have to do is add the base address of block A to
		 * those addresses and they then properly address the areas in block B. Case 2 is that block A
		 * and block B are not contiguous. In this case, to properly adjust the addresses in block A, we
		 * need to do two things. Obviously we need the address for block B. But the offsets currently in
		 * the addresses in block A assume that block A is the origin, not block B so the length of block A
		 * must be subtracted from the offsets to provide the true offset into block B. Then we can add the
		 * address of the block B to this address and have now have the addesses in block A properly address
		 * the areas in block B. In this case, block A is the routine header, block B is the read-only
		 * releasable section. Case one is when the input is from a shared library, case 2 when from a file.
		 */
		offset_correction = (size_t)hdr->ptext_adr;
		rel_base = sect_ro_rel - offset_correction;
	}
	RELOCATE(hdr->ptext_adr, unsigned char *, rel_base);
	RELOCATE(hdr->ptext_end_adr, unsigned char *, rel_base);
	RELOCATE(hdr->lnrtab_adr, lnr_tabent *, rel_base);
	RELOCATE(hdr->literal_text_adr, unsigned char *, rel_base);
	if (dynlits)
		RELOCATE(hdr->literal_adr, mval *, rel_base);
	/* Read-write releasable section */
	sect_rw_rel_size = (int)((INTPTR_T)hdr->labtab_adr - (INTPTR_T)rw_rel_start);
	sect_rw_rel = malloc(sect_rw_rel_size);
	if (shlib)
		memcpy(sect_rw_rel, shdr + (INTPTR_T)rw_rel_start, sect_rw_rel_size);
	else
	{
		DOREADRC_OBJFILE(file_desc, sect_rw_rel, sect_rw_rel_size, status);
		if (0 != status)
			zl_error(file_desc, zro_entry, ERR_INVOBJ, 0, 0, 0, 0);
	}
	offset_correction = (size_t)rw_rel_start;
	rel_base = sect_rw_rel - offset_correction;
	if (!dynlits)
		RELOCATE(hdr->literal_adr, mval *, rel_base);
	RELOCATE(hdr->vartab_adr, var_tabent *, rel_base);
	/* Also read-write releasable is the linkage section which had no initial value and was thus
	 * not resident in the object. The values in this section will be setup later by addr_fix()
	 * and/or auto-zlink. Note we always allocate at least one element here just so we don't get
	 * the potentially unaligned "null string" address provided by gtm_malloc() when a zero
	 * length is requested.
	 */
	alloc_len = hdr->linkage_len * SIZEOF(lnk_tabent);
	hdr->linkage_adr = (lnk_tabent *)malloc((0 != alloc_len) ? alloc_len : SIZEOF(lnk_tabent));
	assert(PADLEN(hdr->linkage_adr, SIZEOF(lnk_tabent) == 0));
	assert(((UINTPTR_T)hdr->linkage_adr % SIZEOF(lnk_tabent)) == 0);
	memset((char *)hdr->linkage_adr, 0, (hdr->linkage_len * SIZEOF(lnk_tabent)));
	/* Relocations for read-write releasable section. Perform relocation on literal mval table and
	 * variable table entries since they both point to the offsets from the beginning of the
	 * literal text pool. The relocations for the linkage section is done in addr_fix()
	 */
	if (!dynlits)
	{
		for (curlit = hdr->literal_adr, littop = curlit + hdr->literal_len; curlit < littop; ++curlit)
			if (curlit->str.len)
				RELOCATE(curlit->str.addr, char *, hdr->literal_text_adr);
	}
	for (curvar = hdr->vartab_adr, vartop = curvar + hdr->vartab_len; curvar < vartop; ++curvar)
	{
		assert(0 < curvar->var_name.len);
		RELOCATE(curvar->var_name.addr, char *, hdr->literal_text_adr);
	}
	/* Fixup header's source path and routine names as they both point to the offsets from the
	 * beginning of the literal text pool
	 */
	hdr->src_full_name.addr += (INTPTR_T)hdr->literal_text_adr;
	hdr->routine_name.addr += (INTPTR_T)hdr->literal_text_adr;
	if (GDL_PrintEntryPoints & gtmDebugLevel)
	{	/* Prepare name and address for announcement.. */
		name_buf_len = (PATH_MAX > hdr->src_full_name.len) ? hdr->src_full_name.len : PATH_MAX;
		memcpy(name_buf, hdr->src_full_name.addr, name_buf_len);
		name_buf[name_buf_len] = '\0';
		PRINTF("incr_link: %s loaded at 0x%08lx\n", name_buf, (long unsigned int) hdr->ptext_adr);
	}
	/* Read-write non-releasable section */
	sect_rw_nonrel_size = hdr->labtab_len * SIZEOF(lab_tabent);
	sect_rw_nonrel = malloc(sect_rw_nonrel_size);
	if (shlib)
		memcpy(sect_rw_nonrel, shdr + (INTPTR_T)hdr->labtab_adr, sect_rw_nonrel_size);
	else
	{
		DOREADRC_OBJFILE(file_desc, sect_rw_nonrel, sect_rw_nonrel_size, status);
		if (0 != status)
			zl_error(file_desc, zro_entry, ERR_INVOBJ, 0, 0, 0, 0);
	}
	hdr->labtab_adr = (lab_tabent *)sect_rw_nonrel;
	/* Relocations for read-write non-releasable section. Perform relocation on label table entries. */
	for (curlbe = hdr->labtab_adr, lbetop = curlbe + hdr->labtab_len; curlbe < lbetop; ++curlbe)
	{
		RELOCATE(curlbe->lab_name.addr, char *, hdr->literal_text_adr);
		RELOCATE(curlbe->lnr_adr, lnr_tabent *, hdr->lnrtab_adr);
	}
	/* Remaining initialization */
	hdr->current_rhead_adr = hdr;
	assert(hdr->routine_name.len < SIZEOF(zlink_mname.c));
	memcpy(&zlink_mname.c[0], hdr->routine_name.addr, hdr->routine_name.len);
	zlink_mname.c[hdr->routine_name.len] = 0;
	/* Do address fix up with relocation and symbol entries from the object. Note that shdr will
	 * never be dereferenced except under a test of the shlib static flag to indicate we are processing
	 * a shared library.
	 */
	if (!addr_fix(file_desc, shdr, &urx_lcl_anchor))
	{
		urx_free(&urx_lcl_anchor);
		zl_error(file_desc, zro_entry, ERR_INVOBJ, 0, 0, 0, 0);
	}
	/* Register new routine in routine name vector displacing old one and performing any necessary cleanup */
	if (!zlput_rname(hdr))
	{
		urx_free(&urx_lcl_anchor);
		/* Copy routine name to local variable because zl_error frees it. */
		memcpy(&module_name.c[0], hdr->routine_name.addr, hdr->routine_name.len);
		zl_error(file_desc, zro_entry, ERR_LOADRUNNING, (int)hdr->routine_name.len, &module_name.c[0], 0, 0);
	}
	/* Fix up of routine headers for old versions of routine so they point to the newest version */
	old_rhead = hdr->old_rhead_adr;
	lbt_bot = hdr->labtab_adr;
	lbt_top = lbt_bot + hdr->labtab_len;
	while (old_rhead)
	{
		lbt_ent = lbt_bot;
		olbt_bot = old_rhead->labtab_adr;
		olbt_top = olbt_bot + old_rhead->labtab_len;
		for (olbt_ent = olbt_bot;  olbt_ent < olbt_top;  olbt_ent++)
		{	/* Match new label entries with old label entries */
			for (; lbt_ent < lbt_top; lbt_ent++)
			{
				MIDENT_CMP(&olbt_ent->lab_name, &lbt_ent->lab_name, order);
				if (order <= 0)
					break;
			}
			if ((lbt_ent < lbt_top) && !order)
			{	/* Have a label name match. Update line pointer for this entry */
				olbt_ent->lnr_adr = lbt_ent->lnr_adr;
				olbt_ent->has_parms = lbt_ent->has_parms;
			} else
			{	/* This old label entry has no match. Mark as undefined */
				olbt_ent->lnr_adr = NULL;
				olbt_ent->has_parms = 0;
			}
		}
		old_rhead->src_full_name = hdr->src_full_name;
		old_rhead->routine_name = hdr->routine_name;
		old_rhead->vartab_len = hdr->vartab_len;
		old_rhead->vartab_adr = hdr->vartab_adr;
		old_rhead->ptext_adr = hdr->ptext_adr;
		old_rhead->ptext_end_adr = hdr->ptext_end_adr;
		old_rhead->lnrtab_adr = hdr->lnrtab_adr;
		old_rhead->lnrtab_len = hdr->lnrtab_len;
		old_rhead->current_rhead_adr = hdr;
		old_rhead->temp_mvals = hdr->temp_mvals;
		old_rhead->temp_size = hdr->temp_size;
		old_rhead->linkage_adr = hdr->linkage_adr;
		old_rhead->literal_adr = hdr->literal_adr;
		old_rhead->literal_text_adr = hdr->literal_text_adr;
		old_rhead->literal_len = hdr->literal_len;
		old_rhead = (rhdtyp *)old_rhead->old_rhead_adr;
	}
	/* Add local unresolves to global chain freeing elements that already existed in the global chain */
	urx_add(&urx_lcl_anchor);
	/* Resolve all unresolved entries in the global chain that reference this routine */
	urx_resolve(hdr, (lab_tabent *)lbt_bot, (lab_tabent *)lbt_top);
	if (!shlib)
		cacheflush(hdr->ptext_adr, (hdr->ptext_end_adr - hdr->ptext_adr), BCACHE);
	ZOS_FREE_TEXT_SECTION;
	return TRUE;
}

boolean_t addr_fix (int file, unsigned char *shdr, urx_rtnref *urx_lcl)
{
	res_list		*res_root, *new_res, *res_temp, *res_temp1;
	unsigned char		*symbols, *sym_temp, *sym_temp1, *symtop, *res_addr;
	struct relocation_info	rel[RELREAD], *rel_ptr;
	int			numrel, rel_read, string_size, sym_size, i;
	ssize_t			status;
	mident_fixed		rtnid, labid;
	mstr			rtn_str;
	rhdtyp			*rtn;
	lab_tabent		*label, *labtop;
	boolean_t		labsym;
	urx_rtnref		*urx_rp;
	urx_addr		*urx_tmpaddr;

	res_root = NULL;
	numrel = (int)((hdr->sym_table_off - hdr->rel_table_off) / SIZEOF(struct relocation_info));
	if ((numrel * SIZEOF(struct relocation_info)) != (hdr->sym_table_off - hdr->rel_table_off))
		return FALSE;	/* Size was not even multiple of relocation entries */
	while (numrel > 0)
	{
		if (shlib)
		{	/* All relocation entries already available */
			rel_read = numrel;
			rel_ptr = (struct relocation_info *)((char *)shdr + hdr->rel_table_off);
		} else
		{	/* Buffer the relocation entries */
			rel_read = (numrel < RELREAD ? numrel : RELREAD);
			DOREADRC_OBJFILE(file, &rel[0], rel_read * SIZEOF(struct relocation_info), status);
			if (0 != status)
			{
				res_free(res_root);
				return FALSE;
			}
			rel_ptr = &rel[0];
		}
		numrel -= rel_read;
		for (; rel_read; --rel_read, ++rel_ptr)
		{
			new_res = (res_list *)malloc(SIZEOF(*new_res));
			new_res->symnum = rel_ptr->r_symbolnum;
			new_res->addr = rel_ptr->r_address;
			new_res->next = new_res->list = 0;
			/* Insert the relocation entry in symbol number order on the unresolved chain */
			if (!res_root)
				res_root = new_res;
			else
			{
				res_temp1 = NULL;
				for (res_temp = res_root; res_temp && res_temp->symnum < new_res->symnum; res_temp = res_temp->next)
					res_temp1 = res_temp;
				if (!res_temp)
					res_temp1->next = new_res;
				else
				{	/* More than one reference to this symbol. Chain multiple refs in list */
					if (res_temp->symnum == new_res->symnum)
					{
						new_res->list = res_temp->list;
						res_temp->list = new_res;
					} else
					{
						if (res_temp1)
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
				}
			}
		}
	}
	if (!res_root)
		return TRUE;	/* No unresolved symbols .. we have been successful */
	/* Read in the symbol table text area. First word is length of following section */
	if (shlib)
	{
		memcpy(&string_size, shdr + hdr->sym_table_off, SIZEOF(string_size));
		symbols = shdr + hdr->sym_table_off + SIZEOF(string_size);
		string_size -= SIZEOF(string_size);
	} else
	{
		DOREADRC_OBJFILE(file, &string_size, SIZEOF(string_size), status);
		if (0 != status)
		{
			res_free(res_root);
			return FALSE;
		}
		string_size -= SIZEOF(string_size);
		symbols = malloc(string_size);
		DOREADRC_OBJFILE(file, symbols, string_size, status);
		if (0 != status)
		{
			free(symbols);
			res_free(res_root);
			return FALSE;
		}
	}
	/* Match up unresolved entries with the null terminated symbol name entries from the
	 * symbol text pool we just read in.
	 */
	sym_temp = sym_temp1 = symbols;
	symtop = symbols + string_size;
	for (i = 0;  res_root;  i++)
	{
		for (; i < res_root->symnum; i++)
		{	/* Forward space symbols until our symnum index (i) matches the symbol
			 * we are processing in res_root.
			 */
			for (; *sym_temp; sym_temp++)
			{	/* Find end of *this* symbol we are bypassing */
				if (sym_temp >= symtop)
				{
					if (!shlib)
						free(symbols);
					res_free(res_root);
					return FALSE;
				}
			}
			sym_temp++;
			sym_temp1 = sym_temp;
		}
		assert(i == res_root->symnum);
		/* Find end of routine name that we care about */
		for (; *sym_temp1 != '.' && *sym_temp1; sym_temp1++)
		{
			if (sym_temp1 >= symtop)
			{
				if (!shlib)
					free(symbols);
				res_free(res_root);
				return FALSE;
			}
		}
		sym_size = (int)(sym_temp1 - sym_temp);
		assert(sym_size <= MAX_MIDENT_LEN);
		memcpy(&rtnid.c[0], sym_temp, sym_size);
		rtnid.c[sym_size] = 0;
		if ('_' == rtnid.c[0])
			rtnid.c[0] = '%';
		assert((mid_len(&zlink_mname) != sym_size) || (0 != memcmp(&zlink_mname.c[0], &rtnid.c[0], sym_size)));
		rtn_str.addr = &rtnid.c[0];
		rtn_str.len = sym_size;
		rtn = find_rtn_hdr(&rtn_str);	/* Routine already resolved? */
		sym_size = 0;
		labsym = FALSE;
		/* If symbol is for a label, find the end of the label name */
		if ('.' == *sym_temp1)
		{
			sym_temp1++;
			sym_temp = sym_temp1;
			for (; *sym_temp1; sym_temp1++)
			{
				if (sym_temp1 >= symtop)
				{
					if (!shlib)
						free(symbols);
					res_free(res_root);
					return FALSE;
				}
			}
			sym_size = (int)(sym_temp1 - sym_temp);
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
			if (!labsym)
				res_addr = (unsigned char *)rtn;	/* resolve to routine header */
			else
			{	/* Look our target label up in the routines label table */
				label = rtn->labtab_adr;
				labtop = label + rtn->labtab_len;
				for (; label < labtop && ((sym_size != label->lab_name.len) ||
				    memcmp(&labid.c[0], label->lab_name.addr, sym_size)); label++)
					;
				if (label < labtop)
					res_addr = (unsigned char *)&label->lnr_adr; /* resolve to label entry address */
				else
					res_addr = NULL;	/* Label not found .. potential future problem. For now
								 * just leave it unresolved */
			}
			if (res_addr)
			{	/* We can fully resolve this symbol now */
				res_temp = res_root->next;
				while(res_root)
				{	/* Resolve all entries for this known symbol */
					((lnk_tabent * )((char *)hdr->linkage_adr + res_root->addr))->ext_ref =
						(char_ptr_t)res_addr;
					res_temp1 = res_root->list;
					free(res_root);
					res_root = res_temp1;
				}
				res_root = res_temp;
				continue;
			}
		}
		/* This symbol is unknown. Put on the (local) unresolved extern chain -- either for labels or routines */
		urx_rp = urx_putrtn(rtn_str.addr, (int)rtn_str.len, urx_lcl);  /* Find/create unresolved node for routine */
		res_temp = res_root->next;
		while(res_root)
		{	/* add unresolved addr entry to existing or new routine and/or label node. */
			if (labsym)
				urx_putlab(&labid.c[0], sym_size, urx_rp, (char *)hdr->linkage_adr + res_root->addr);
			else
			{
				urx_tmpaddr = (urx_addr *)malloc(SIZEOF(urx_addr));
				urx_tmpaddr->next = urx_rp->addr;
				urx_tmpaddr->addr = (INTPTR_T *)((char *)hdr->linkage_adr + res_root->addr);
				urx_rp->addr = urx_tmpaddr;
			}
			res_temp1 = res_root->list;
			free(res_root);
			res_root = res_temp1;
		}
		res_root = res_temp;
	}
	if (!shlib)
		free(symbols);
	return TRUE;
}

/* Release the resolution chain .. Called as part of an error since normal processing will
 * have already released all elements on this chain.
 */
void res_free (res_list *root)
{
	res_list *temp;

	while (root)
	{
		while (root->list)
		{
			temp = root->list->list;
			free(root->list);
			root->list = temp;
		}
		temp = root->next;
		free(root);
		root = temp;
	}
}

/* ZL_ERROR - perform cleanup and signal errors found in zlinking a mumps object module */
void zl_error (int4 file, zro_ent *zroe, int4 err, int4 len, char *addr, int4 len2, char *addr2)
{
	ZOS_FREE_TEXT_SECTION;
	zl_error_hskpng(file);
	/* 0, 2, or 4 arguments */
	if (0 == len)
		rts_error(VARLSTCNT(1) err);
	else
		if (0 == len2)
			rts_error(VARLSTCNT(4) err, 2, len, addr);
		else
			rts_error(VARLSTCNT(6) err, 4, len, addr, len2, addr2);
}

/* ZL_ERROR-housekeeping */
void zl_error_hskpng(int4 file)
{
	int rc;

	if (!shlib)
	{	/* Only non shared library links have these areas to free */
		if (hdr)
			free(hdr);
		if (sect_ro_rel)
			GTM_TEXT_FREE(sect_ro_rel);
		if (sect_rw_rel)
			free(sect_rw_rel);
		if (sect_rw_nonrel)
			free(sect_rw_nonrel);
		CLOSEFILE_RESET(file, rc);	/* resets "file" to FD_INVALID */
	}
}
