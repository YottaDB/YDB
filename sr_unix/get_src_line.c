/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/types.h>
#include <errno.h>
#include "gtm_string.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stat.h"
#include "gtm_stdio.h"

#include <rtnhdr.h>
#include "zroutines.h"
#include "compiler.h"
#include "srcline.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "op.h"
#include "zbreak.h"
#include "hashtab_mname.h"
#ifdef GTM_TRIGGER
# include "gdsroot.h"
# include "gtm_facility.h"
# include "fileinfo.h"
# include "gdsbt.h"
# include "gdsfhead.h"
# include "gv_trigger.h"
# include "cdb_sc.h"
# include "t_retry.h"
# include "trigger_source_read_andor_verify.h"
#endif
#include "stack_frame.h"
#include "rtn_src_chksum.h"
#include "cmd_qlf.h"

#define RT_TBL_SZ 20

STATICFNDCL boolean_t fill_src_tbl(routine_source **src_tbl_result, rhdtyp *rtn_vector);
STATICFNDCL boolean_t fill_src_tbl_via_litpool(routine_source **src_tbl_result, rhdtyp *rtn_vector);
STATICFNDCL boolean_t fill_src_tbl_via_mfile(routine_source **src_tbl_result, rhdtyp *rtn_vector);

GBLREF uint4		dollar_tlevel;
GBLREF unsigned int	t_tries;
GBLREF stack_frame	*frame_pointer;

LITDEF char		litconst_space = ' ';
LITDEF mval		literal_null;

error_def(ERR_TXTSRCFMT);
error_def(ERR_SYSCALL);

int get_src_line(mval *routine, mval *label, int offset, mstr **srcret, boolean_t verifytrig)
{
	int			srcrecs, *lt_ptr, size, line_indx, srcfilnamlen;
	boolean_t		found, added, eof_seen, srcstat;
	mstr			src;
	rhdtyp			*rtn_vector;
	zro_ent			*srcdir;
	mstr			*base, *current, *top;
	char			buff[MAX_SRCLINE], *cptr, *srcfile_name;
	char			srcnamebuf[SIZEOF(mident_fixed) + STR_LIT_LEN(DOTM)];
	routine_source		*src_tbl;
	int			rc, fclose_res;
	char			*fgets_rc;
	FILE			*fp;
	struct stat		srcfile_stat;
	off_t			srcsize;
	unsigned char		*srcptr, *srcptr_max, *srcstart, *eol_srcstart, *prev_srcptr;
	gtm_rtn_src_chksum_ctx	checksum_ctx;
#	ifdef GTM_TRIGGER
	boolean_t	is_trigger;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	srcstat = 0;
	*srcret = NULL;
	assert (routine->mvtype & MV_STR);

	/* The source to be loaded can be of two types:
	 *
	 *   1. Normal routine source to be looked up on disk
	 *   2. Trigger source located in a database.
	 *
	 * Determine which source we need.
	 */
	GTMTRIG_ONLY(IS_TRIGGER_RTN(&routine->str, is_trigger));
	if (WANT_CURRENT_RTN(routine))
	{	/* we want $TEXT for the routine currently executing. */
		rtn_vector = CURRENT_RHEAD_ADR(frame_pointer->rvector);
	} else
#	ifdef GTM_TRIGGER
	if (is_trigger)
	{	/* Need source on a trigger. Get trigger source loaded and/or verified which may involve
		 * creating a TP fence and dealing with TP restarts.
		 * ZPRINT wants a consistent view of triggers across multiple calls so it bypasses verification
		 * after the first call. In this case, the trigger had better be found since the first call found it.
		 */
		if (verifytrig)
		{
			rc = trigger_source_read_andor_verify(&routine->str, TRIGGER_SRC_LOAD);
			if (0 != rc)
				return SRCNOTAVAIL;
		}
		rtn_vector = find_rtn_hdr(&routine->str);	/* Trigger routine should be loaded now */
		if (NULL == rtn_vector)
		{
			assert(verifytrig);			/* Noverify trig should always be found */
			return SRCNOTAVAIL;			/* Could happen if trigger name got modified or uniqueified */
		}
	} else
#	endif
	{
		if (NULL == (rtn_vector = find_rtn_hdr(&routine->str)))		/* Note assignment */
		{
			op_zlink(routine, NULL);
			rtn_vector = find_rtn_hdr(&routine->str);
			if (NULL == rtn_vector)
				return OBJMODMISS;
		}
		USHBIN_ONLY(rtn_vector = op_rhd_ext(routine, (mval *)&literal_null, rtn_vector, NULL));
	}
	if (!rtn_vector->src_full_name.len)
		return SRCNOTAVAIL;
	src_tbl = rtn_vector->source_code;
	if (NULL == src_tbl)
	{
#		ifdef GTM_TRIGGER
		if (is_trigger)
		{
			srcstart = (unsigned char *)((gv_trigger_t *)rtn_vector->trigr_handle)->xecute_str.str.addr;
			srcsize = ((gv_trigger_t *)rtn_vector->trigr_handle)->xecute_str.str.len;
			assert(0 < srcsize);
			assert(NULL != srcstart);
			/* Calculate source checksum */
			if (NULL == memchr(srcstart, '\n', srcsize))
			{	/* In this case, gtm_trigger_complink() would have written an extra newline character to the
				 * source file. Since we want to take the checksum of the same data, we append a newline character
				 * here as well.
				 */
				eol_srcstart = (unsigned char *)malloc(srcsize + 1);
				memcpy(eol_srcstart, srcstart, srcsize);
				eol_srcstart[srcsize] = '\n';
				rtn_src_chksum_buffer(&checksum_ctx, eol_srcstart, srcsize + 1);
				free(eol_srcstart);
			} else
				rtn_src_chksum_buffer(&checksum_ctx, srcstart, srcsize);
			srcrecs = (int)rtn_vector->lnrtab_len;
			/* Allocate the array to hold the mstr array pointing to the src lines. */
			src_tbl = (routine_source *)malloc(SIZEOF(routine_source) + ((srcrecs - 1) * SIZEOF(mstr)));
			src_tbl->srcbuff = srcstart;
			src_tbl->srcrecs = srcrecs;
			/* Remove the source buffer from the trigger descriptor so nobody frees it inappropriately */
			((gv_trigger_t *)rtn_vector->trigr_handle)->xecute_str.str.len = 0;
			((gv_trigger_t *)rtn_vector->trigr_handle)->xecute_str.str.addr = NULL;
			/* Parse code setting the mstrs for each line encountered */
			srcptr = srcstart;
			srcptr_max = srcptr + srcsize;
			for (current = (src_tbl->srclines + 1), top = current + (srcrecs - 1); current < top; current++)
			{
				assert(srcptr < srcptr_max);
				current->addr = (char *)srcstart;
				/* Find end of this record */
				for (; (srcptr < srcptr_max) && ('\n' != *srcptr); srcptr++)
					;
				size = (srcptr - (unsigned char *)current->addr);
				if (0 != size)
				{	/* Set line length */
					current->len = size;
				} else
				{	/* Null record -  point to a single space for the record */
					current->addr = (char *)&litconst_space;
					current->len = 1;
				}
				srcptr++;	/* Skip line end char */
				srcstart = srcptr;
			}
			if (!rtn_src_chksum_match(get_ctx_checksum(&checksum_ctx), get_rtnhdr_checksum(rtn_vector)))
			{	/* Should never happen with a trigger unless it ran into some restartable concurrency
				 * issues. Assert we can restart and do it.
				 */
				assertpro(0 < dollar_tlevel);
				assert(CDB_STAGNATE > t_tries);
				t_retry(cdb_sc_triggermod);
			}
		} else
#		endif
		{
			srcstat = fill_src_tbl(&src_tbl, rtn_vector);
		}
		src_tbl->srcstat = srcstat;
		rtn_vector->source_code = src_tbl;
	} else
		srcstat |= src_tbl->srcstat;
	lt_ptr = (int *)find_line_addr(rtn_vector, &label->str, 0, NULL);
	if (!lt_ptr)
		srcstat |= LABELNOTFOUND;
	else if (!(srcstat & (SRCNOTFND | SRCNOTAVAIL)))
	{
		line_indx = (int)(lt_ptr - (int *)LNRTAB_ADR(rtn_vector));
		line_indx += offset;
		if (line_indx == 0)
			srcstat |= ZEROLINE;
		else if (line_indx < 0)
			srcstat |= NEGATIVELINE;
		else if (line_indx >= rtn_vector->lnrtab_len)
			srcstat |= AFTERLASTLINE;
		else	/* successfully located line */
		{
			*srcret = &src_tbl->srclines[line_indx];
			/* DBGFPF((stderr, "get_src_line: returning string %.*s\n", (*srcret)->len, (*srcret)->addr)); */
		}
	}
	return srcstat;
}

void free_src_tbl(rhdtyp *rtn_vector)
{
	routine_source		*src_tbl;

	src_tbl = rtn_vector->source_code;
	if (NULL != src_tbl)
	{	/* Release the source. Entries and source are malloc'd in two blocks on UNIX */
		if (NULL != src_tbl->srcbuff)
			free(src_tbl->srcbuff);
		free(src_tbl);
		rtn_vector->source_code = NULL;
	}
}

STATICFNDEF boolean_t fill_src_tbl(routine_source **src_tbl_result, rhdtyp *rtn_vector)
{
	if (rtn_vector->compiler_qlf & CQ_EMBED_SOURCE)
		return fill_src_tbl_via_litpool(src_tbl_result, rtn_vector);
	else
		return fill_src_tbl_via_mfile(src_tbl_result, rtn_vector);
}

STATICFNDEF boolean_t fill_src_tbl_via_litpool(routine_source **src_tbl_result, rhdtyp *rtn_vector)
{
	int			srcrecs, size;
	mstr			*current, *top;
	routine_source		*src_tbl;
	off_t			srcsize;
	unsigned char		*srcptr, *srcptr_max, *srcstart, *prev_srcptr;

	srcrecs = (int)rtn_vector->lnrtab_len;
	/* Each line of source (srcrecs of them) resides at end of lit pool (srcptr) */
	USHBIN_ONLY(srcptr = rtn_vector->literal_text_adr + rtn_vector->routine_source_offset);
	USHBIN_ONLY(srcsize = rtn_vector->literal_text_len - rtn_vector->routine_source_offset);
	NON_USHBIN_ONLY(srcptr = (unsigned char *)rtn_vector->routine_source_offset);
	NON_USHBIN_ONLY(srcsize = (int)rtn_vector->routine_source_length);
	srcptr_max = srcptr + srcsize;
	src_tbl = (routine_source *)malloc(SIZEOF(routine_source) + ((srcrecs - 1) * SIZEOF(mstr)));
	src_tbl->srcbuff = NULL;
	src_tbl->srcrecs = srcrecs;
	for (current = src_tbl->srclines + 1, top = src_tbl->srclines + srcrecs ; current < top ; current++)
	{
		prev_srcptr = srcptr++;
		while ((srcptr < srcptr_max) && (*(srcptr - 1) != '\n')) /* find end of current line */
			srcptr++;
		size = (int4)(srcptr - prev_srcptr);
		if (*(srcptr - 1) == '\n')
			size--; /* Strip trailing '\n' */
		if (size)
		{
			current->len = size;
			current->addr = (char *)prev_srcptr;
		} else
		{
			current->len = 1;
			current->addr = (char *)&litconst_space;
		}
	}
	/* NOTE: no need to verify source checksum. no chance of mismatch */
	*src_tbl_result = src_tbl;
	return 0;
}

STATICFNDEF boolean_t fill_src_tbl_via_mfile(routine_source **src_tbl_result, rhdtyp *rtn_vector)
{
	int			srcrecs, *lt_ptr, size, line_indx, srcfilnamlen;
	boolean_t		found, added, eof_seen, srcstat;
	mstr			src;
	zro_ent			*srcdir;
	mstr			*base, *current, *top;
	char			buff[MAX_SRCLINE], *cptr, *srcfile_name;
	char			srcnamebuf[SIZEOF(mident_fixed) + STR_LIT_LEN(DOTM)];
	routine_source		*src_tbl;
	int			rc, fclose_res;
	char			*fgets_rc;
	FILE			*fp;
	struct stat		srcfile_stat;
	off_t			srcsize;
	unsigned char		*srcptr, *srcptr_max, *srcstart, *eol_srcstart, *prev_srcptr;
	gtm_rtn_src_chksum_ctx	checksum_ctx;

	srcstat = 0;
	srcfile_name = malloc(rtn_vector->src_full_name.len + 1);
	memcpy(srcfile_name, rtn_vector->src_full_name.addr, rtn_vector->src_full_name.len);
	*(srcfile_name + rtn_vector->src_full_name.len) = 0;	/* ensure string is null terminated */
	/* At this point, it is not clear if Fopen will handle zos tagging correctly in all cases.
	 * especially when tagged with other than ISO8859-1 or IBM-1047. When we resurrect the zOS
	 * platform, we need to test this out.
	 */
	fp = Fopen(srcfile_name, "r");
	if (NULL == fp)
	{
		free(srcfile_name);
		srcfile_name = NULL;
		srcfilnamlen = (int)rtn_vector->routine_name.len;
		memcpy(srcnamebuf, rtn_vector->routine_name.addr, srcfilnamlen);
		if (srcnamebuf[0] == '%')	/* percents are translated to _ on filenames */
			srcnamebuf[0] = '_';
		MEMCPY_LIT(&srcnamebuf[srcfilnamlen], DOTM);
		src.addr = srcnamebuf;
		src.len = INTCAST(srcfilnamlen + STR_LIT_LEN(DOTM));
		zro_search (0, 0, &src, &srcdir, TRUE);
		if (srcdir)
		{
			srcfile_name = malloc(src.len + srcdir->str.len + 2);
			memcpy(srcfile_name, srcdir->str.addr, srcdir->str.len);
			cptr = srcfile_name + srcdir->str.len;
			*cptr++ = '/';
			memcpy(cptr, src.addr, src.len);
			cptr += src.len;
			*cptr++ = 0;
			fp = Fopen(srcfile_name, "r");
			found = (NULL != fp) ? TRUE : FALSE;
		} else
			found = FALSE;
	} else
		found = TRUE;

	if (!found)
	{
		srcstat |= SRCNOTFND;
		srcrecs = 0;
		srcsize = 0;
	} else
	{
		srcrecs = (int)rtn_vector->lnrtab_len;
		/* Find out how big the file is so we can allocate the memory in one shot */
		if ((NULL != fp) && !(srcstat & (SRCNOTFND | SRCNOTAVAIL)))
		{
			rc = stat(srcfile_name, &srcfile_stat);
			if (0 != rc)
			{
				free(srcfile_name);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL,
						5, LEN_AND_LIT("stat"), CALLFROM, rc);
			}
			srcsize = srcfile_stat.st_size;
		} else
			srcsize = 0;
	}
	if (NULL != srcfile_name)
		free(srcfile_name);
	assert((found && srcrecs >= 1) || (srcrecs == 0));

	/* Allocate source mstr structure. Since structure has one mstr in it, allocate one less.
	 * Note, the size we get from lnrtab_len has an extra [0] origin entry in the total. This
	 * entry is not used in the source array for direct referencing ease.
	 */
	src_tbl = (routine_source *)malloc(SIZEOF(routine_source) + ((srcrecs - 1) * SIZEOF(mstr)));
	src_tbl->srcbuff = (0 < srcsize) ? malloc(srcsize) : NULL;
	base = src_tbl->srclines;
	srcptr = src_tbl->srcbuff;
	DEBUG_ONLY(srcptr_max = srcptr + srcsize);
	src_tbl->srcrecs = srcrecs;
	eof_seen = FALSE;
	for (current = base + 1, top = base + srcrecs ; current < top ; current++)
	{
		assert(found && (NULL != fp));
		if (!eof_seen)
		{
			FGETS(buff, MAX_SRCLINE, fp, fgets_rc);
			if (NULL == fgets_rc)
			{
				if (ferror(fp))
				{
					FCLOSE(fp, fclose_res);
					assert(!fclose_res);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_TXTSRCFMT, 0, errno);
					assert(FALSE);
				} else
				{
					eof_seen = TRUE;
					assert(feof(fp));
					size = 0;
				}
			} else
			{
				size = (int)STRLEN(buff);
				prev_srcptr = srcptr;
				srcptr += size;
				if (srcptr > (src_tbl->srcbuff + srcsize))
				{	/* source file has been concurrently overwritten (and extended) */
					srcstat |= CHECKSUMFAIL;
					eof_seen = TRUE;
					size = 0;
				} else
				{
					memcpy(prev_srcptr, buff, size);
					/* Strip trailing '\n' if any (if at least one byte was read in) */
					if (size && ('\n' == buff[size - 1]))
						size--;
				}
			}
		} else	/* eof seen; nothing more to read in file */
			size = 0;
		if (size)
		{
			assert((prev_srcptr + size) <= srcptr_max);
			current->len = size;
			current->addr = (char *)prev_srcptr;
		} else
		{
			current->addr = (char *)&litconst_space;
			current->len = 1;
		}
	}
	if (found)
	{
		*base = *(base + 1);
		/* Ensure we have reached the end of the source file. If not, we need to issue a CHECKSUMFAIL
		 * error. Most often the !eof_seen part of the check is not needed since the checksums will not
		 * match. But if it so happens that the checksums do match, then this extra check helps us
		 * correctly identify a TXTSRCMAT error.
		 */
		if (!eof_seen)
		{
			FGETS(buff, MAX_SRCLINE, fp, fgets_rc);
			if ((NULL == fgets_rc) && !ferror(fp))
			{
				eof_seen = TRUE;
				assert(feof(fp));
			}
		}
		rtn_src_chksum_buffer(&checksum_ctx, src_tbl->srcbuff, srcsize);
		if (!eof_seen
			|| !rtn_src_chksum_match(get_ctx_checksum(&checksum_ctx), get_rtnhdr_checksum(rtn_vector)))
			srcstat |= CHECKSUMFAIL;
		FCLOSE(fp, fclose_res);
		assert(!fclose_res);
	}
	*src_tbl_result = src_tbl;
	return srcstat;
}
