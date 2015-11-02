/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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

#define RT_TBL_SZ 20

GBLREF uint4		dollar_tlevel;
GBLREF unsigned int	t_tries;

LITDEF char		litconst_space = ' ';

error_def(ERR_TXTSRCFMT);
error_def(ERR_SYSCALL);

int get_src_line(mval *routine, mval *label, int offset, mstr **srcret, boolean_t verifytrig)
{
	int		srcrecs, *lt_ptr, size, line_indx, srcfilnamlen;
	uint4		checksum;
	boolean_t	found, added, eof_seen, srcstat;
	mstr		src;
	rhdtyp		*rtn_vector;
	zro_ent		*srcdir;
	mstr		*base, *current, *top;
	char		buff[MAX_SRCLINE], *cptr, *srcfile_name;
	char		srcnamebuf[SIZEOF(mident_fixed) + STR_LIT_LEN(DOTM)];
	ht_ent_mname	*tabent;
	var_tabent	rtnent;
	routine_source	*src_tbl;
	int		rc, fclose_res;
	char		*fgets_rc;
	FILE		*fp;
	struct stat	srcfile_stat;
	off_t		srcsize;
	unsigned char	*srcptr, *srcptr_max, *srcstart;
#	ifdef GTM_TRIGGER
	boolean_t	is_trigger;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	srcstat = 0;
	*srcret = NULL;
	if (NULL == (TREF(rt_name_tbl)).base)
		init_hashtab_mname(TADR(rt_name_tbl), RT_TBL_SZ, HASHTAB_COMPACT, HASHTAB_SPARE_TABLE);
	assert (routine->mvtype & MV_STR);

	/* The source to be loaded can be of two types:
	 *
	 *   1. Normal routine source to be looked up on disk
	 *   2. Trigger source located in a database.
	 *
	 * Determine which source we need.
	 */
	GTMTRIG_ONLY(IS_TRIGGER_RTN(&routine->str, is_trigger));
	/* Need source on a trigger. Get trigger source loaded and/or verified which may involve
	 * creating a TP fence and dealing with TP restarts.
	 */
#	ifdef GTM_TRIGGER
	if (is_trigger)
	{	/* ZPRINT wants a consistent view of triggers across multiple calls so it bypasses verification
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
	}
	if (!rtn_vector->src_full_name.len)
		return SRCNOTAVAIL;
	rtnent.var_name = rtn_vector->routine_name;
	COMPUTE_HASH_MNAME(&rtnent);
	added = add_hashtab_mname(TADR(rt_name_tbl), &rtnent, NULL, &tabent);
	src_tbl = (routine_source *)tabent->value;
	if (added || (NULL == tabent->value))
	{
		checksum = 0;
#		ifdef GTM_TRIGGER
		if (is_trigger)
		{
			srcstart = (unsigned char *)((gv_trigger_t *)rtn_vector->trigr_handle)->xecute_str.str.addr;
			srcsize = ((gv_trigger_t *)rtn_vector->trigr_handle)->xecute_str.str.len;
			assert(0 < srcsize);
			assert(NULL != srcstart);
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
				if (0 != (size = (srcptr - (unsigned char *)current->addr)))	/* note assignment */
				{	/* Do checksum computation plus set length */
					RTN_SRC_CHKSUM((char *)srcstart, size, checksum);
					current->len = size;
				} else
				{	/* Null record -  point to a single space for the record */
					current->addr = (char *)&litconst_space;
					current->len = 1;
				}
				srcptr++;	/* Skip line end char */
				srcstart = srcptr;
			}
			if (checksum != rtn_vector->checksum)
			{	/* Should never happen with a trigger unless it ran into some restartable concurrency
				 * issues. Assert we can restart and do it.
				 */
				if (0 < dollar_tlevel)
				{
					assert(CDB_STAGNATE > t_tries);
					t_retry(cdb_sc_triggermod);
				} else
					GTMASSERT;
			}
		} else
#		endif
		{
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
					if (NULL == fp)
					{
						rts_error(VARLSTCNT(1) errno);
						assert(FALSE);
					}
					found = TRUE;
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
						rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("stat"), CALLFROM, rc);
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
							rts_error(VARLSTCNT(3) ERR_TXTSRCFMT, 0, errno);
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
						/* Strip trailing '\n' if any (if at least one byte was read in) */
						if (size && ('\n' == buff[size - 1]))
							size--;
					}
				} else	/* eof seen; nothing more to read in file */
					size = 0;
				if (size)
				{	/* Calculate checksum to verify with loaded routine */
					RTN_SRC_CHKSUM(buff, size, checksum);
					assert(NULL != srcptr);
					assert((srcptr + size) <= srcptr_max);
					current->len = size;
					current->addr = (char *)srcptr;
					memcpy(srcptr, buff, size);
					srcptr += size;
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
				if (!eof_seen || (checksum != rtn_vector->checksum))
					srcstat |= CHECKSUMFAIL;
				FCLOSE(fp, fclose_res);
				assert(!fclose_res);
			}
		}
		src_tbl->srcstat = srcstat;
		tabent->value = (char *)src_tbl;
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
