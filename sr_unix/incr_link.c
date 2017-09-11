/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/types.h>
#include <sys/shm.h>

#include <errno.h>
#include "gtm_string.h"
#include "gtm_unistd.h"
#include "gtm_stdio.h"
#include "gtm_stat.h"

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
#include "send_msg.h"
#include "cacheflush.h"
#include "rtnobj.h"
#include "zbreak.h"
#include "interlock.h"
#include "util.h"
#include "arlinkdbg.h"

/* Define linkage types */
typedef enum
{
	LINK_SHRLIB = 1,	/* 0001 Link routine from a shared library */
	LINK_SHROBJ,		/* 0002 Link routine from a shared object */
	LINK_PPRIVOBJ		/* 0003 Link a process-private object */
} linktype;

#define RELOCATE(field, type, base) field = (type)((unsigned char *)(field) + (UINTPTR_T)(base))
#define RELREAD 50		/* number of relocation entries to buffer */

/* This macro will check if the file is an old non-shared-binary variant of GT.M code and if
 * so just return IL_RECOMPILE to signal a recompile. The assumption is that if we fall out of this
 * macro that there is truly a problem and other measures should be taken (e.g. call zl_error()).
 * At some point this code can be disabled with the NO_NONUSB_RECOMPILE varible defined. Rather
 * than keep old versions of control blocks around that will confuse the issue, we know that the
 * routine header of these versions started 10 32bit words into the object. Read in the eight
 * bytes from that location and check against the JSB_MARKER we still use today.
 */
#ifndef NO_NONUSB_RECOMPILE
#  define CHECK_NONUSB_RECOMPILE_RETURN								\
{												\
	if (-1 != (status = (ssize_t)lseek(*file_desc, COFFHDRLEN, SEEK_SET)))			\
	{											\
		DOREADRC_OBJFILE(*file_desc, marker, SIZEOF(JSB_MARKER) - 1, status);		\
	} else											\
		status = errno;									\
	if ((0 == status) && (0 == MEMCMP_LIT(marker, JSB_MARKER)))				\
	{											\
		ZOS_FREE_TEXT_SECTION;								\
		zl_error_hskpng(linktyp, file_desc, RECENT_ZHIST);				\
		return IL_RECOMPILE;	/* Signal recompile */					\
	}											\
}
#else
#  define CHECK_NONUSB_RECOMPILE_RETURN 	/* No old recompile check is being generated */
#endif

/* Define debugging macro for low-level relinking issues */
/* #define DEBUG_LOW_RELINK*/
#ifdef DEBUG_LOW_RELINK
#  define DBGLRL(x) DBGFPF(x)
#  define DBGLRL_ONLY(x) x
#else
#  define DBGLRL(x)
#  define DBGLRL_ONLY(x)
#endif

/* At some point these statics (like all the others) need to move into gtm_threadgbl since these values should
 * *not* be shared amongst the threads of the future.
 */
static unsigned char	*sect_ro_rel, *sect_rw_rel, *sect_rw_nonrel;
static rhdtyp		*hdr;

GBLREF mident_fixed	zlink_mname;
GBLREF mach_inst	jsb_action[JSB_ACTION_N_INS];
GBLREF uint4		gtmDebugLevel;
GBLREF boolean_t	gtm_utf8_mode;
#ifdef DEBUG_ARLINK
GBLREF mval		dollar_zsource;
#endif

#ifdef __MVS__
GBLDEF unsigned char	*text_section;
GBLDEF boolean_t	extended_symbols_present;
/* If ZOS is ever revived, verify the following two fields used properly. For example, total_length is declared here and
 * passed to extract_text where it is instead called text_counter_ptr. The name "total_length" is also overloaded with
 * a local of the same name in $sr_os390/obj_filesp.c.
 */
GBLDEF int		total_length;
GBLDEF int		text_counter;
#endif

LITREF char		gtm_release_name[];
LITREF int4		gtm_release_name_len;

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
error_def(ERR_INVOBJFILE);
error_def(ERR_LOADRUNNING);
error_def(ERR_RLNKRECLATCH);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);
error_def(ERR_ZLINKBYPASS);
error_def(ERR_ZLINKFILE);

STATICFNDCL void	res_free(res_list *root);
STATICFNDCL boolean_t	addr_fix(int file, unsigned char *shdr, linktype linktyp, urx_rtnref *urx_lcl);
STATICFNDCL void	zl_error(void *recent_zhist, linktype linktyp, int4 *file, int4 err, int4 len, char *addr,
				 int4 len2, char *addr2);
STATICFNDCL void	zl_error_hskpng(linktype linktyp, int4 *file, void *recent_zhist);

/* incr_link - read and process a mumps object module.  Link said module to currently executing image */
#ifdef AUTORELINK_SUPPORTED
boolean_t incr_link(int *file_desc, zro_ent *zro_entry, zro_hist *recent_zhist, uint4 fname_len, char *fname)
#else
boolean_t incr_link(int *file_desc, zro_ent *zro_entry, uint4 fname_len, char *fname)
#endif
{
	rhdtyp			*old_rhead;
	rtn_tabent		*tabent_ptr;
	int			sect_ro_rel_size, sect_rw_rel_size, name_buf_len, alloc_len, order, zerofd;
	uint4			lcl_compiler_qlf;
	boolean_t		dynlits;
	ssize_t	 		status, sect_rw_nonrel_size, sect_ro_rel_offset;
	size_t			offset_correction, rtnname_off;
	lab_tabent		*lbt_ent, *lbt_bot, *lbt_top, *olbt_ent, *olbt_bot, *olbt_top;
	mident_fixed		module_name;
	pre_v5_mident		*pre_v5_routine_name;
	urx_rtnref		urx_lcl_anchor;
	unsigned char		*shdr, *rel_base, *newaddr;
	mstr			rtnname;
	mval			*curlit, *littop;
	lab_tabent		*curlbe, *lbetop;
	var_tabent		*curvar, *vartop;
	char			name_buf[PATH_MAX + 1], marker[SIZEOF(JSB_MARKER) - 1], *rw_rel_start;
	char			rtnname_buf[MAX_MIDENT_LEN];
	linktype		linktyp;
	ZOS_ONLY(ESD		symbol;)
#	ifdef _AIX
	FILHDR          	hddr;
	unsigned short  	magic;
#	endif
#	ifdef AUTORELINK_SUPPORTED
	relinkrec_t		*rec;
	zro_validation_entry	*zhent;
	open_relinkctl_sgm	*linkctl;
#	ifdef DEBUG
	relinkrec_t		*relinkrec;
	int			rtnlen;
	char			save_char, *rtnname2;
#	endif /* DEBUG */
#	ifdef ZLINK_BYPASS
	va_list			save_last_va_list_ptr;
	boolean_t		util_copy_saved;
	char			*save_util_outptr;
	int4			save_error_condition;
#	endif
#	endif /* AUTORELINK_SUPPORTED */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (NULL == file_desc)
	{	/* Make sure file_desc can *always* be de-referenced */
		zerofd = 0;
		file_desc = &zerofd;
	}
#	ifdef __MVS__
	total_length = 0;
	extended_symbols_present = FALSE;
	text_counter = 0;
	memset(&symbol, 0 , SIZEOF(ESD));
	assert(NULL == text_section);
	ZOS_FREE_TEXT_SECTION;
#	endif
	urx_lcl_anchor.len = 0;
	urx_lcl_anchor.addr = NULL;
	urx_lcl_anchor.lab = 0;
	urx_lcl_anchor.next = NULL;
	assert(NULL == hdr);
	assert(NULL == sect_ro_rel);
	assert(NULL == sect_rw_rel);
	assert(NULL == sect_rw_nonrel);
	hdr = NULL;
	shdr = NULL;
	sect_ro_rel = sect_rw_rel = sect_rw_nonrel = NULL;
	if (*file_desc)
	{	/* This is a disk resident object we share if autorelink is enabled in that directory, or instead we
		 * read/link into process private storage if autorelink is not enabled.
		 */
		assert(FD_INVALID != *file_desc);
		if ((NULL == zro_entry) || (NULL == zro_entry->relinkctl_sgmaddr))
		{
			linktyp = LINK_PPRIVOBJ;
			/* No history if process private link */
			ARLINK_ONLY(assert(NULL == recent_zhist));
		} else
		{
			linktyp =  LINK_SHROBJ;
#			if defined(AUTORELINK_SUPPORTED) && defined(DEBUG)
			/* Need a history if sharing the object */
			assert(NULL != recent_zhist);
			/* Later call to rtnobj_shm_malloc will use recent_zhist->end - 1 as the zro_validation_entry
			 * to get at the linkctl and relinkrec so assert that it is indeed linkctl corresponding to zro_entry.
			 */
			assert((recent_zhist->end - 1)->relinkctl_bkptr
			       == (open_relinkctl_sgm *)zro_entry->relinkctl_sgmaddr);
			relinkrec = (recent_zhist->end - 1)->relinkrec;
			rtnlen = STRLEN(relinkrec->rtnname_fixed.c);
			assert(fname_len >= (STR_LIT_LEN(DOTOBJ) + rtnlen));
			rtnname2 = fname + fname_len - 1;
			while (rtnname2 >= fname)
			{
				if ('/' == *rtnname2)
				{
					rtnname2++;
					break;
				}
				rtnname2--;
			}
			save_char = *rtnname2;
			if ('_' == save_char)
				*rtnname2 = '%'; /* Temporary adjustment for below assert to not fail */
			assert(!memcmp(relinkrec->rtnname_fixed.c, rtnname2, rtnlen));
			if ('_' == save_char)
				*rtnname2 = save_char;	/* restore */
#			endif	/* AUTORELINK_SUPPORTED && DEBUG */
		}
		NON_USHBIN_ONLY(assert(LINK_PPRIVOBJ == linktyp));	/* No autorelink in non-ushbin platform */
	} else
	{
		/* With no file descriptor, this can only be linkage from a shared library and shared libraries have no history */
		ARLINK_ONLY(assert(NULL == recent_zhist));
		assert(NULL != zro_entry);
		linktyp = LINK_SHRLIB;
	}
#	ifdef DEBUG_ARLINK
	switch(linktyp)
	{
		case LINK_SHROBJ:
		case LINK_PPRIVOBJ:
			if (NULL != zro_entry)
			{
				DBGFPF((stderr, "incr_link: Requested to (re)link routine %.*s from %.*s\n", fname_len, fname,
					zro_entry->str.len, zro_entry->str.addr));
			} else
			{
				DBGFPF((stderr, "incr_link: Requested to (re)link routine %.*s from %.*s\n", fname_len, fname,
					dollar_zsource.str.len, dollar_zsource.str.addr));
			}
			break;
		default:						/* case LINK_SHRLIB: */
			assert(LINK_SHRLIB == linktyp);
			DBGFPF((stderr, "incr_link: Requested (re)link routine %.*s from shared library %.*s\n", fname_len, fname,
				zro_entry->str.len, zro_entry->str.addr));
			break;
	}
#	endif
	/* Get the routine header where we can make use of it */
	hdr = (rhdtyp *)malloc(SIZEOF(rhdtyp));
	switch(linktyp)
	{
#		ifdef USHBIN_SUPPORTED
		case LINK_SHRLIB:
			/* Copy routine header to process private storage so we can make changes to it. Note that on
			 * some platforms, the address returned by dlsym() is not the actual shared code address, but
			 * normally an address to the linkage table, eg. TOC (AIX), PLT (HP-UX). Computing the actual
			 * shared code address is platform dependent and is handled by the macro (see incr_link_sp.h).
			 */
			shdr = (unsigned char *)GET_RTNHDR_ADDR(zro_entry->shrsym);
			memcpy(hdr, shdr, SIZEOF(rhdtyp));
			hdr->shlib_handle = zro_entry->shrlib;
			break;
#		endif
		case LINK_PPRIVOBJ:
		case LINK_SHROBJ:
#			ifdef _AIX
			/* Seek past native object headers to get GT.M object's routine header
			 * To check if it is not an xcoff64 bit .o.
			 */
			DOREADRC(*file_desc, &hddr, SIZEOF(FILHDR), status);
			if (0 == status)
			{
				magic = hddr.f_magic;
				if (magic != U64_TOCMAGIC)
				{
					ZOS_FREE_TEXT_SECTION;
					zl_error_hskpng(linktyp, file_desc, RECENT_ZHIST);
					return IL_RECOMPILE;
				}
			} else
				zl_error(RECENT_ZHIST, linktyp, file_desc, ERR_INVOBJFILE, fname_len, fname, 0, NULL);
#			endif
#			ifdef __MVS__
			/* In the GOFF .o on zOS, if the symbol name(name of the module) exceeds ESD_NAME_MAX_LENGTH (8),
			 * then 2 extra extended records are emitted, which causes the start of text section to vary
			 */
			DOREADRC(*file_desc, &symbol, SIZEOF(symbol), status);	/* This is HDR record */
			if (0 == status)
			{
				DOREADRC(*file_desc, &symbol, SIZEOF(symbol), status);	/* First symbol (ESD record) */
				if (0 == status)
				{
					if (0x01 == symbol.ptv[1])	/* Means the extended records are there */
						extended_symbols_present = TRUE;
					else
					{
						assert(0x0 == symbol.ptv[1]);
						extended_symbols_present = FALSE;
					}
				}
			}
			if (0 != status)
				zl_error(RECENT_ZHIST, linktyp, file_desc, ERR_INVOBJFILE, fname_len, fname, 0, NULL);
#			endif
			if (-1 != (status = (ssize_t)lseek(*file_desc, NATIVE_HDR_LEN, SEEK_SET)))
			{
				ZOS_ONLY(extract_text(*file_desc, &total_length));
				DOREADRC_OBJFILE(*file_desc, hdr, SIZEOF(rhdtyp), status);
			} else
				status = errno;
			if (0 != status)
			{
				CHECK_NONUSB_RECOMPILE_RETURN;
				zl_error(RECENT_ZHIST, linktyp, file_desc, ERR_INVOBJFILE, fname_len, fname, 0, NULL);
			}
			break;
		default:
			assertpro(FALSE /* Invalid linkage type for this platform*/);
	}
	if ((0 != memcmp(hdr->jsb, (char *)jsb_action, SIZEOF(jsb_action)))
	    || (0 != memcmp(&hdr->jsb[SIZEOF(jsb_action)], JSB_MARKER,
			    MIN(STR_LIT_LEN(JSB_MARKER), SIZEOF(hdr->jsb) - SIZEOF(jsb_action)))))
	{
		if (LINK_SHRLIB != linktyp)	/* Shared library cannot recompile so this is always an error */
		{
			CHECK_NONUSB_RECOMPILE_RETURN;
		}
		zl_error(RECENT_ZHIST, linktyp, file_desc, ERR_INVOBJFILE, fname_len, fname, 0, NULL);
	}
	/* Binary version check. If no match, shlib gets error, otherwise signal recompile */
	if (MAGIC_COOKIE != hdr->objlabel)
	{
		if (LINK_SHRLIB == linktyp)
		{
			if (MAGIC_COOKIE_V5 > hdr->objlabel)
			{ 	/* The library was built using a version prior to V50FT01. The routine_name field of the
				 * pre-V5 routine header was an 8-byte char array, so read the routine name in the old format
				 */
				int len;
				pre_v5_routine_name = (pre_v5_mident *)((char*)hdr + PRE_V5_RTNHDR_RTNOFF);
				for (len = 0; len < SIZEOF(pre_v5_mident) && pre_v5_routine_name->c[len]; len++)
					;
				zl_error(RECENT_ZHIST, linktyp, NULL, ERR_DLLVERSION, len, &(pre_v5_routine_name->c[0]),
					 zro_entry->str.len, zro_entry->str.addr);
			}
#			if defined(__osf__) || defined(__hppa)
			else if (MAGIC_COOKIE_V52 > hdr->objlabel)
			{	/* Note: routine_name field has not been relocated yet, so compute its absolute
				 * address in the shared library and use it
				 */
				v50v51_mstr	*mstr5051;	/* declare here so don't have to conditionally add above */
				mstr5051 = (v50v51_mstr *)((char *)hdr + V50V51_RTNHDR_RTNMSTR_OFFSET);
				zl_error(RECENT_ZHIST, linktyp, NULL, ERR_DLLVERSION, mstr5051->len,
					 ((char *)shdr + *(int4 *)((char *)hdr + V50V51_FTNHDR_LITBASE_OFFSET)
					  + (int4)mstr5051->addr), zro_entry->str.len, zro_entry->str.addr);
			}
#			endif
			else	/* V52 or later but not current version */
			{	/* Note: routine_name field has not been relocated yet, so compute its absolute
				 * address in the shared library and use it
				 */
				zl_error(RECENT_ZHIST, linktyp, NULL, ERR_DLLVERSION, hdr->routine_name.len,
					(char *)shdr + (UINTPTR_T)hdr->literal_text_adr + (UINTPTR_T)hdr->routine_name.addr,
					 zro_entry->str.len, zro_entry->str.addr);
			}
		}
		ZOS_FREE_TEXT_SECTION;
		zl_error_hskpng(linktyp, file_desc, RECENT_ZHIST);
		return IL_RECOMPILE;
	}
	if (((hdr->compiler_qlf & CQ_UTF8) && !gtm_utf8_mode) || (!(hdr->compiler_qlf & CQ_UTF8) && gtm_utf8_mode))
	{ 	/* Object file compiled with a different $ZCHSET is being used */
		lcl_compiler_qlf = hdr->compiler_qlf;		/* Will be cleaned up soon so save a copy */
		if (LINK_SHRLIB == linktyp)	/* Shared library cannot recompile so this is always an error */
		{	/* Note: routine_name field has not been relocated yet, so compute its absolute address
			 * in the shared library and use it
			 */
			if ((lcl_compiler_qlf & CQ_UTF8) && !gtm_utf8_mode)
			{
				zl_error(NULL, linktyp, NULL, ERR_DLLCHSETUTF8, (int)hdr->routine_name.len,
					(char *)shdr + (UINTPTR_T)hdr->literal_text_adr + (UINTPTR_T)hdr->routine_name.addr,
					 (int)zro_entry->str.len, zro_entry->str.addr);
			} else
			{
				zl_error(NULL, linktyp, NULL, ERR_DLLCHSETM, (int)hdr->routine_name.len,
					(char *)shdr + (UINTPTR_T)hdr->literal_text_adr + (UINTPTR_T)hdr->routine_name.addr,
					 (int)zro_entry->str.len, zro_entry->str.addr);
			}
		}
		zl_error_hskpng(linktyp, file_desc, RECENT_ZHIST);
		if ((lcl_compiler_qlf & CQ_UTF8) && !gtm_utf8_mode)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_INVOBJFILE, 2, fname_len, fname, ERR_TEXT, 2,
				LEN_AND_LIT("Object compiled with CHSET=UTF-8 which is different from $ZCHSET"));
		else
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_INVOBJFILE, 2, fname_len, fname, ERR_TEXT, 2,
				LEN_AND_LIT("Object compiled with CHSET=M which is different from $ZCHSET"));
	}
	/* We need to check if this is a relink of an already linked routine. To do that, we need to be able to fetch
	 * the routine name from the object file in question in order to look the routine up.
	 */
	switch(linktyp)
	{
		ARLINK_ONLY(case LINK_SHROBJ:);
		case LINK_PPRIVOBJ:
			rtnname = ((rhdtyp *)hdr)->routine_name;
			rtnname_off = (size_t)hdr->literal_text_adr + (size_t)rtnname.addr;	/* Offset into object of rtnname */
			ZOS_ONLY(assertpro(FALSE /* Read file pointer being reset - recode for ZOS */));
			/* Read the routine name from the object file */
			if (-1 != (status = (ssize_t)lseek(*file_desc, rtnname_off + NATIVE_HDR_LEN, SEEK_SET)))
			{
				DOREADRC_OBJFILE(*file_desc, rtnname_buf, rtnname.len, status);
			} else
				status = errno;
			if (0 != status)
				zl_error(RECENT_ZHIST, linktyp, file_desc, ERR_INVOBJFILE, fname_len, fname, 0, NULL);
			rtnname.addr = rtnname_buf;
			/* Reset read file-pointer for object file back to what it was */
			status = (ssize_t)lseek(*file_desc, NATIVE_HDR_LEN + SIZEOF(rhdtyp), SEEK_SET);
			if (-1 == status)
				zl_error(RECENT_ZHIST, linktyp, file_desc, ERR_INVOBJFILE, fname_len, fname, 0, NULL);
			break;
		case LINK_SHRLIB:
			rtnname = ((rhdtyp *)shdr)->routine_name;
			/* Routine name not yet relocated so effectively do so */
			rtnname.addr = (char *)shdr + (size_t)((rhdtyp *)shdr)->literal_text_adr + (size_t)rtnname.addr;
			break;
		default:
			assert(FALSE);
 	}
	/* Now check if we know about this routine and if so, if the (new) object hash is the same as the one we currently
	 * have linked. Note both new and old flavors of the routine must either both not be autorelink-enabled or must
	 * both be autorelink-enabled and if autorelink-enabled, must be loaded from the same directory and thus into the same
	 * set of shared memory structures to be able to bypass the link. This might could be eased in the future for certain
	 * special cases.
	 */
	if (find_rtn_tabent(&tabent_ptr, &rtnname))
	{
		old_rhead = (rhdtyp *)tabent_ptr->rt_adr;
		assert(NULL != old_rhead);
#		ifdef AUTORELINK_SUPPORTED
		zhent = (LINK_SHROBJ == linktyp) ? recent_zhist->end - 1 : NULL;
		if ((old_rhead->objhash == hdr->objhash)
		    && (((NULL == zhent) && (NULL == old_rhead->relinkctl_bkptr))			/* Both non-relink */
			|| (NULL != zhent) && (zhent->relinkctl_bkptr == old_rhead->relinkctl_bkptr)))  /* Both same relink */
#		else
		if (old_rhead->objhash == ((rhdtyp *)hdr)->objhash)
#		endif
		{	/* This is the same routine - abort re-link but if autorelink-enabled, use the new history. The reason
			 * we do this is we should behave at this point as-if we *had* done the relink. That and the fact that
			 * we don't want this routine to attempt to reload itself on every call dictates that we get rid of the
			 * old history we had (with old cycle values) and instead use the new history with the latest cycle
			 * numbers - as if this routine *had been* relinked.
			 *
			 * Note even a pseudo-relink should cancel all the breakpoints in the module. Note we don't address
			 * issues with whether this would have been a recursive relink or not since when we don't actually do
			 * the relink, there's no way to remove breakpoints when the last use of this routine leaves the stack
			 * so eliminate the breakpoints now since newly linked routines are anyway not expected to have
			 * breakpoints.
			 */
			zr_remove_zbrks(old_rhead, NOBREAKMSG);
			/* Info level message that link was bypassed. Since this could pollute the error buffer, save and
			 * restore it across the info message we put out (it is either displayed or it isn't - no need to cache it.
			 */
			DBGARLNK((stderr, "incr_link: Bypassing (re)zlink for routine %.*s (old rhead 0x"lvaddr") - same objhash\n",
				  old_rhead->routine_name.len, old_rhead->routine_name.addr, old_rhead));
#			ifdef ZLINK_BYPASS /* #ifdef'd out for now due to issues with ERRWETRAP */
			/* If this code is enabled please remove the ZLINKBYPASS message from merrors.msg's undocumented
			 * section and document the message.
			 */
			SAVE_UTIL_OUT_BUFFER(save_util_outptr, save_last_va_list_ptr, util_copy_saved);
			save_error_condition = error_condition;
			error_condition = 0;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZLINKBYPASS, 2, fname_len, fname); /* Info msg returns */
			error_condition = save_error_condition;
			RESTORE_UTIL_OUT_BUFFER(save_util_outptr, save_last_va_list_ptr, util_copy_saved);
#			endif
			/* Note do this cleanup (release/reset) after the info message so these values still around if the error
			 * causes an issue in debugging.
			 */
			free(hdr);		/* Don't need this (new) copy of routine header */
			hdr = NULL;
#			ifdef AUTORELINK_SUPPORTED
			if (NULL != old_rhead->zhist)
				free(old_rhead->zhist);		/* Free up previously auto-relinked structures */
			/* Save new history if supplied or NULL */
			old_rhead->zhist = recent_zhist;
#			endif
			return IL_DONE;	/* bypass link since we have already done the link before */
		}
	}
#	ifdef AUTORELINK_SUPPORTED
	/* For shared object, time to pull it into shared memory */
	if (LINK_SHROBJ == linktyp)
	{
		/* Currently, rtnobj_hdr_t->objLen is a 4-byte field so we cannot support object file sizes > 4Gb for now */
		assert(4 == SIZEOF(hdr->object_len));	/* i.e. object file size is guaranteed to be < 4Gb */
		assert(4 == SIZEOF(((rtnobj_hdr_t *)NULL)->objLen));	/* i.e. object file size is guaranteed to be < 4Gb */
		/* Allocate buffer for routine object in shared memory. If one already exists, use it */
		if ((size_t)MAXUINT4 <= ((size_t)hdr->object_len + OFFSETOF(rtnobj_hdr_t, userStorage)))
		{	/* Currently, rtnobj_hdr_t->objLen is a 4-byte field so we cannot support object file
			 * sizes > 4Gb for now.
			 */
			ZOS_FREE_TEXT_SECTION;
			zl_error_hskpng(linktyp, file_desc, RECENT_ZHIST);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ZLINKFILE, 2, fname_len, fname, ERR_TEXT, 2,
				LEN_AND_LIT("Object file size > 4Gb cannot be auto-relinked"));
		}
		shdr = rtnobj_shm_malloc(recent_zhist, *file_desc, hdr->object_len, hdr->objhash);
		if (NULL == shdr)
		{	/* Most likely lseek or read of object file into shared memory failed. Possible for example
			 * if the object file is truncated. Issue error.
			 */
			zl_error(RECENT_ZHIST, linktyp, file_desc, ERR_INVOBJFILE, fname_len, fname, 0, NULL);
		}
		/* We need this link - continue setup */
		assert(!memcmp(hdr, shdr, SIZEOF(rhdtyp)));	/* Already read rtnhdr from disk, assert that is same as in shm */
		(TREF(arlink_loaded))++;			/* Count autorelink routines loaded */
	}
#	endif
	/* Read in and/or relocate the pointers to the various sections. To understand the size calculations
	 * being done note that the contents of the various xxx_adr pointers in the routine header are
	 * initially the offsets from the start of the object. This is so we can address the various sections
	 * via offset now while linking and via address later during runtime.
	 *
	 * Read-only releasable section
	 */
	dynlits = DYNAMIC_LITERALS_ENABLED(hdr);
	rw_rel_start = RW_REL_START_ADR(hdr);	/* Marks end of R/O-release section and start of R/W-release section */
	/* Assert that relinkctl_bkptr is NULL for LINK_SHRLIB and LINK_PPRIVOBJ. This is assumed by the link-bypass
	 * code above (which returns IL_DONE) which uses "hdr->relinkctl_bkptr" without regard to linktyp of that routine.
	 */
	ARLINK_ONLY(assert((LINK_SHROBJ == linktyp) || (NULL == hdr->relinkctl_bkptr));)
	switch(linktyp)
	{
		case LINK_SHROBJ:
			hdr->shared_object = TRUE;		/* Indicate this is linked as a shared object */
#			ifdef AUTORELINK_SUPPORTED
			zhent = recent_zhist->end - 1;
			hdr->relinkctl_bkptr = zhent->relinkctl_bkptr;
#			endif
			/* Note - fall through */
		case LINK_SHRLIB:
			rel_base = shdr;
			break;
		case LINK_PPRIVOBJ:
			sect_ro_rel_size = (unsigned int)((INTPTR_T)rw_rel_start - (INTPTR_T)hdr->ptext_adr);
			sect_ro_rel = GTM_TEXT_ALLOC(sect_ro_rel_size);
			/* R/O-release section should be aligned well at this point but make a debug level check to verify */
			assert((INTPTR_T)sect_ro_rel == ((INTPTR_T)sect_ro_rel & ~(LINKAGE_PSECT_BOUNDARY - 1)));
			DOREADRC_OBJFILE(*file_desc, sect_ro_rel, sect_ro_rel_size, status);
			if (0 != status)
				zl_error(NULL, linktyp, file_desc, ERR_INVOBJFILE, fname_len, fname, 0, NULL);
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
			break;
		default:
			assert(FALSE /* Invalid link type */);
	}
	RELOCATE(hdr->ptext_adr, unsigned char *, rel_base);
	RELOCATE(hdr->ptext_end_adr, unsigned char *, rel_base);
	/* Initialize hdr->shared_ptext_adr appropriately for the link type */
	switch(linktyp)
	{
		case LINK_SHROBJ:
		case LINK_SHRLIB:
			hdr->shared_ptext_adr = hdr->ptext_adr;
			break;
		case LINK_PPRIVOBJ:
			hdr->shared_ptext_adr = NULL;
			break;
		default:
			assert(FALSE /* Invalid link type */);
	}
	RELOCATE(hdr->lnrtab_adr, lnr_tabent *, rel_base);
	RELOCATE(hdr->literal_text_adr, unsigned char *, rel_base);
	RELOCATE(hdr->linkage_names, mstr *, rel_base);
	if (dynlits)
		RELOCATE(hdr->literal_adr, mval *, rel_base);
	/* Read-write releasable section */
	sect_rw_rel_size = (int)((INTPTR_T)hdr->labtab_adr - (INTPTR_T)rw_rel_start);
	sect_rw_rel = malloc(sect_rw_rel_size);
	switch(linktyp)
	{
		case LINK_SHROBJ:
		case LINK_SHRLIB:
			memcpy(sect_rw_rel, shdr + (INTPTR_T)rw_rel_start, sect_rw_rel_size);
			break;
		case LINK_PPRIVOBJ:
			DOREADRC_OBJFILE(*file_desc, sect_rw_rel, sect_rw_rel_size, status);
			if (0 != status)
				zl_error(NULL, linktyp, file_desc, ERR_INVOBJFILE, fname_len, fname, 0, NULL);
			break;
		default:
			assert(FALSE /* Invalid link type */);
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
	 * beginning of the literal text pool.
	 */
	hdr->src_full_name.addr += (INTPTR_T)hdr->literal_text_adr;
	hdr->routine_name.addr += (INTPTR_T)hdr->literal_text_adr;
	if (LINK_SHROBJ == linktyp)
	{	/* For values in shared objects (but not shared libraries) put the name and
		 * path in malloc'd space so in a core file we can have access to the names.
		 */
		newaddr = malloc(hdr->src_full_name.len + hdr->routine_name.len);
		memcpy(newaddr, hdr->src_full_name.addr, hdr->src_full_name.len);
		memcpy(newaddr + hdr->src_full_name.len, hdr->routine_name.addr, hdr->routine_name.len);
		hdr->src_full_name.addr = (char *)newaddr;
		hdr->routine_name.addr = (char *)(newaddr + hdr->src_full_name.len);
	}
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
	switch(linktyp)
	{
		case LINK_SHROBJ:
		case LINK_SHRLIB:
			memcpy(sect_rw_nonrel, shdr + (INTPTR_T)hdr->labtab_adr, sect_rw_nonrel_size);
			break;
		case LINK_PPRIVOBJ:
			DOREADRC_OBJFILE(*file_desc, sect_rw_nonrel, sect_rw_nonrel_size, status);
			if (0 != status)
				zl_error(NULL, linktyp, file_desc, ERR_INVOBJFILE, fname_len, fname, 0, NULL);
			break;
		default:
			assert(FALSE /* Invalid link type */);
	}
	hdr->labtab_adr = (lab_tabent *)sect_rw_nonrel;
	/* Relocations for read-write non-releasable section. Perform relocation on label table entries. */
	for (curlbe = hdr->labtab_adr, lbetop = curlbe + hdr->labtab_len; curlbe < lbetop; ++curlbe)
	{
		RELOCATE(curlbe->lab_name.addr, char *, hdr->literal_text_adr);
		RELOCATE(curlbe->lnr_adr, lnr_tabent *, hdr->lnrtab_adr);
	}
	/* Remaining initialization */
	ARLINK_ONLY(assert(((NULL == recent_zhist) && (LINK_SHROBJ != linktyp))
			   || ((NULL != recent_zhist) && (LINK_SHROBJ == linktyp))));
	ARLINK_ONLY(hdr->zhist = recent_zhist);
	hdr->current_rhead_adr = hdr;
	assert(hdr->routine_name.len < SIZEOF(zlink_mname.c));
	memcpy(&zlink_mname.c[0], hdr->routine_name.addr, hdr->routine_name.len);
	zlink_mname.c[hdr->routine_name.len] = 0;
	/* Do address fix up with relocation and symbol entries from the object. Note that shdr will
	 * never be dereferenced except under a test of the linktyp flag to indicate we are processing
	 * a shared library or a shared object.
	 */
	if (!addr_fix(*file_desc, shdr, linktyp, &urx_lcl_anchor))
	{
		urx_free(&urx_lcl_anchor);
		/* Decrement "refcnt" bump done by "rtnobj_shm_malloc" above */
		ARLINK_ONLY(if (LINK_SHROBJ == linktyp) rtnobj_shm_free(hdr, LATCH_GRABBED_FALSE));
		zl_error(RECENT_ZHIST, linktyp, file_desc, ERR_INVOBJFILE, fname_len, fname, 0, NULL);
	}
	/* Register new routine in routine name vector displacing old one and performing any necessary cleanup */
	DBGARLNK((stderr, "incr_link: Registering new version of routine %.*s (rtnhdr 0x"lvaddr")\n",
		  hdr->routine_name.len, hdr->routine_name.addr, hdr));
	if (!zlput_rname(hdr))
	{
		urx_free(&urx_lcl_anchor);
		/* Copy routine name to local variable because zl_error frees it. */
		memcpy(&module_name.c[0], hdr->routine_name.addr, hdr->routine_name.len);
		/* Decrement "refcnt" bump done by "rtnobj_shm_malloc" above */
		ARLINK_ONLY(if (LINK_SHROBJ == linktyp) rtnobj_shm_free(hdr, LATCH_GRABBED_FALSE));
		zl_error(RECENT_ZHIST, linktyp, file_desc, ERR_LOADRUNNING, (int)hdr->routine_name.len, &module_name.c[0],
			 0, NULL);
	}
	/* Now that new routine is registered, no longer need to do rtnobj_shm_free.
	 * That will be done by relinkctl_rundown as part of scanning the rtn_names[] array.
	 */
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
				if (0 >= order)
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
			DBGLRL((stderr, "incr_link: Routine %.*s (rtnhdr 0x"lvaddr") label %.*s (labtab_ent 0x"lvaddr
				") set to "lvaddr"\n",
				hdr->routine_name.len, hdr->routine_name.addr, old_rhead,
				olbt_ent->lab_name.len, olbt_ent->lab_name.addr, olbt_ent, olbt_ent->lnr_adr));
		}
		old_rhead->src_full_name = hdr->src_full_name;
		old_rhead->routine_name = hdr->routine_name;
		old_rhead->vartab_len = hdr->vartab_len;
		old_rhead->vartab_adr = hdr->vartab_adr;
		old_rhead->shared_ptext_adr = hdr->shared_ptext_adr;
		old_rhead->ptext_adr = hdr->ptext_adr;
		old_rhead->ptext_end_adr = hdr->ptext_end_adr;
		old_rhead->lnrtab_adr = hdr->lnrtab_adr;
		old_rhead->lnrtab_len = hdr->lnrtab_len;
		old_rhead->current_rhead_adr = hdr;
		old_rhead->temp_mvals = hdr->temp_mvals;
		old_rhead->temp_size = hdr->temp_size;
		old_rhead->linkage_adr = hdr->linkage_adr;
		old_rhead->linkage_len = hdr->linkage_len;
		old_rhead->linkage_names = hdr->linkage_names;
		old_rhead->literal_adr = hdr->literal_adr;
		old_rhead->literal_text_adr = hdr->literal_text_adr;
		old_rhead->literal_len = hdr->literal_len;
		ARLINK_ONLY(old_rhead->zhist = hdr->zhist);
		old_rhead = (rhdtyp *)old_rhead->old_rhead_adr;
	}
	/* Add local unresolves to global chain freeing elements that already existed in the global chain */
	urx_add(&urx_lcl_anchor);
	/* Resolve all unresolved entries in the global chain that reference this routine */
	urx_resolve(hdr, (lab_tabent *)lbt_bot, (lab_tabent *)lbt_top);
	if (LINK_PPRIVOBJ == linktyp)
		cacheflush(hdr->ptext_adr, (hdr->ptext_end_adr - hdr->ptext_adr), BCACHE);
	DBGARLNK((stderr, "incr_link: (re)link for %.*s complete\n", hdr->routine_name.len, hdr->routine_name.addr));
	/* zOS cleanups */
	ZOS_FREE_TEXT_SECTION;
	/* Don't leave global pointers around to active blocks */
	hdr = NULL;
	shdr = NULL;
	sect_ro_rel = sect_rw_rel = sect_rw_nonrel = NULL;
	return IL_DONE;
}

/* Routine to do address relocations/fixups for the M routine being linked as well as resolve those addresses that
 * can be resolved and create a chain of the external routine/label references that remain unresolved.
 *
 * Parameters:
 *   file       - file descriptor of open file.
 *   shdr	- address of shared header for shared object and shared library linked objects.
 *   linktyp    - type of linkage performed (enum linktype).
 *   urx_lcl	- address of urx_lcl structure to put our references in temporarily.
 */
STATICFNDEF boolean_t addr_fix(int file, unsigned char *shdr, linktype linktyp, urx_rtnref *urx_lcl)
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
	while (0 < numrel)
	{
		switch(linktyp)
		{
			case LINK_SHROBJ:
			case LINK_SHRLIB:
				/* All relocation entries already available */
				rel_read = numrel;
				rel_ptr = (struct relocation_info *)((char *)shdr + hdr->rel_table_off);
				break;
			case LINK_PPRIVOBJ:
				/* Buffer the relocation entries */
				rel_read = (numrel < RELREAD ? numrel : RELREAD);
				DOREADRC_OBJFILE(file, &rel[0], rel_read * SIZEOF(struct relocation_info), status);
				if (0 != status)
				{
					res_free(res_root);
					return FALSE;
				}
				rel_ptr = &rel[0];
				break;
			default:
				assert(FALSE /* Invalid link type */);
		}
		numrel -= rel_read;
		for (; rel_read; --rel_read, ++rel_ptr)
		{
			new_res = (res_list *)malloc(SIZEOF(*new_res));
			new_res->symnum = rel_ptr->r_symbolnum;
			new_res->addr = rel_ptr->r_address;
			new_res->next = new_res->list = NULL;
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
	switch(linktyp)
	{
		case LINK_SHROBJ:
		case LINK_SHRLIB:
			memcpy(&string_size, shdr + hdr->sym_table_off, SIZEOF(string_size));
			symbols = shdr + hdr->sym_table_off + SIZEOF(string_size);
			string_size -= SIZEOF(string_size);
			break;
		case LINK_PPRIVOBJ:
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
			break;
		default:
			assert(FALSE /* Invalid link type */);
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
					if (LINK_PPRIVOBJ == linktyp)
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
				if (LINK_PPRIVOBJ == linktyp)
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
					if (LINK_PPRIVOBJ == linktyp)
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
				res_addr = (unsigned char *)rtn;	/* Resolve to routine header */
			else
			{	/* Look our target label up in the routines label table */
				label = rtn->labtab_adr;
				labtop = label + rtn->labtab_len;
				for (; label < labtop && ((sym_size != label->lab_name.len) ||
							  memcmp(&labid.c[0], label->lab_name.addr, sym_size)); label++)
					;
				if (label < labtop)
					res_addr = (unsigned char *)&label->lnr_adr; /* Resolve to label entry address */
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
		{	/* Add unresolved addr entry to existing or new routine and/or label node. */
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
	if (LINK_PPRIVOBJ == linktyp)
		free(symbols);
	return TRUE;
}

/* Release the resolution chain .. Called as part of an error since normal processing will
 * have already released all elements on this chain.
 *
 * Parameter:
 *   root - anchor for system resolution chain
 */
STATICFNDEF void res_free(res_list *root)
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

/* Routine to perform cleanup and signal errors found in zlinking a mumps object module.
 *
 * Parameters:
 *   linktyp    - type of linkage performed (enum linktype).
 *   file       - pointer to file descriptor of open file.
 *   err        - error code of error to raise.
 *   len/addr   - length/address of 1st substitution string.
 *   len2/addr2 - length/address of 2nd substitution string.
 */
STATICFNDEF void zl_error(void *recent_zhist, linktype linktyp, int4 *file, int4 err, int4 len, char *addr, int4 len2, char *addr2)
{
	ZOS_FREE_TEXT_SECTION;
	zl_error_hskpng(linktyp, file, recent_zhist);
	/* 5 or 7 arguments */
	if (0 == len2)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) err, 2, len, addr);
	else
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) err, 4, len, addr, len2, addr2);
}

/* Routine to perform basic housekeeping for zl_error() but allowed to be called separately by other errors.
 *
 * Parameters:
 *   linktyp    - type of linkage performed (enum linktype).
 *   file       - pointer to file descriptor of open file.
 */
STATICFNDEF void zl_error_hskpng(linktype linktyp, int4 *file, void *recent_zhist)
{
	int rc;

	if ((NULL != file) && (0 < *file))
	{	/* We have a file descriptor for both process private and shared object type links */
		assert ((LINK_PPRIVOBJ == linktyp) || (LINK_SHROBJ == linktyp));
		CLOSEFILE_RESET(*file, rc);	/* resets "*file" to FD_INVALID, ignore any failure at this point as it is not the
						 * the primary error.
						 */
	}
	if (NULL != hdr)
	{
		free(hdr);
		hdr = NULL;
	}
	if (NULL != sect_rw_rel)
	{
		free(sect_rw_rel);
		sect_rw_rel = NULL;
	}
	if (NULL != sect_rw_nonrel)
	{
		free(sect_rw_nonrel);
		sect_rw_nonrel = NULL;
	}
	if ((LINK_PPRIVOBJ == linktyp) && (NULL != sect_ro_rel))
	{	/* Only private process links have this area to free */
		GTM_TEXT_FREE(sect_ro_rel);
		sect_ro_rel = NULL;
	}
	RELEASE_RECENT_ZHIST;
}
