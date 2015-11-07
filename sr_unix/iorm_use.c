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

#include "gtm_string.h"

#include <errno.h>
#include "gtm_unistd.h"
#include "gtm_stat.h"
#include "gtm_stdio.h"
#include "gtm_iconv.h"

#include "copy.h"
#include "io.h"
#include "iormdef.h"
#include "io_params.h"
#include "iosp.h"
#include "eintr_wrappers.h"
#include "stringpool.h"
#ifdef UNICODE_SUPPORTED
#include "gtm_conv.h"
#include "gtm_utf8.h"
#endif

/*  Only want to do fstat() once on this file, not on evry use.
 */
#define FSTAT_CHECK 													\
        if (!fstat_done)												\
        {														\
		FSTAT_FILE(rm_ptr->fildes, &statbuf, fstat_res);							\
		if (-1 == fstat_res)											\
		{													\
			save_errno = errno;										\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("fstat"),            \
			CALLFROM, save_errno);							                        \
		}													\
		mode = mode1 = statbuf.st_mode;										\
		fstat_done = TRUE;											\
	}

#define	IS_PADCHAR_VALID(chset, padchar)	(IS_ASCII(padchar))

typedef struct
{
	unsigned short mem;
	unsigned short grp;
} uic_struct;

LITREF	unsigned char	io_params_size[];
GBLREF	boolean_t	gtm_utf8_mode;

#ifdef UNICODE_SUPPORTED
GBLREF	UConverter	*chset_desc[];
LITREF	mstr		chset_names[];
#endif
error_def(ERR_DEVPARMNEG);
error_def(ERR_RMWIDTHPOS);
error_def(ERR_RMWIDTHTOOBIG);
error_def(ERR_RECSIZENOTEVEN);
error_def(ERR_SYSCALL);
error_def(ERR_WIDTHTOOSMALL);
error_def(ERR_PADCHARINVALID);
error_def(ERR_IOERROR);

void	iorm_use(io_desc *iod, mval *pp)
{
	boolean_t	seen_wrap, fstat_done;
	unsigned char	c;
	short		mode, mode1;
	int4		length, width, width_bytes, recordsize, padchar;
	int		fstat_res, save_errno;
	d_rm_struct	*rm_ptr;
	struct stat	statbuf;
	int		p_offset;
	mstr		chset_mstr;
	boolean_t	ichset_specified, ochset_specified, chset_allowed;
	gtm_chset_t	width_chset, temp_chset;

	p_offset = 0;
	rm_ptr = (d_rm_struct *)iod->dev_sp;
	seen_wrap = fstat_done = FALSE;
	ichset_specified = ochset_specified = chset_allowed = FALSE;
	if (ZOS_ONLY(TRUE ||) gtm_utf8_mode)
		chset_allowed = TRUE;
	while (*(pp->str.addr + p_offset) != iop_eol)
	{
		assert((params) *(pp->str.addr + p_offset) < (params)n_iops);
		switch (c = *(pp->str.addr + p_offset++))
		{
		case iop_exception:
			iod->error_handler.len = *(pp->str.addr + p_offset);
			iod->error_handler.addr = (char *)(pp->str.addr + p_offset + 1);
			s2pool(&iod->error_handler);
			break;
		case iop_fixed:
			if (iod->state != dev_open)
				rm_ptr->fixed = TRUE;
			break;
		case iop_nofixed:
			if (iod->state != dev_open)
			{
				rm_ptr->fixed = FALSE;
				if (CHSET_BINARY == iod->ichset)
				{
					ichset_specified = FALSE;
					iod->ichset = CHSET_M;
				}
				if (CHSET_BINARY == iod->ochset)
				{
					ochset_specified = FALSE;
					iod->ochset = CHSET_M;
				}
			}
			break;
		case iop_length:
			GET_LONG(length, (pp->str.addr + p_offset));
			if (length < 0)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DEVPARMNEG);
			iod->length = length;
			break;
		case iop_w_protection:
			FSTAT_CHECK;
			mode &= ~(0x07);
			mode |= *(pp->str.addr + p_offset);
			break;
		case iop_g_protection:
			FSTAT_CHECK;
			mode &= ~(0x07 << 3);
			mode |= *(pp->str.addr + p_offset) << 3;
			break;
		case iop_s_protection:
		case iop_o_protection:
			FSTAT_CHECK;
			mode &= ~(0x07 << 6);
			mode |= *(pp->str.addr + p_offset) << 6;
			break;
		case iop_pad:
			if (iod->state != dev_open)
			{
				GET_LONG(padchar, (pp->str.addr + p_offset));
				if (!IS_PADCHAR_VALID(iod->ochset, padchar))
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(2) ERR_PADCHARINVALID, 0);
				rm_ptr->padchar = padchar;
			}
			break;
		case iop_readonly:
			rm_ptr->noread = TRUE;
			break;
		case iop_noreadonly:
			rm_ptr->noread = FALSE;
			break;
		case iop_writeonly:
			rm_ptr->write_only = TRUE;
			break;
		case iop_nowriteonly:
			rm_ptr->write_only = FALSE;
			break;
		case iop_recordsize:
			if (dev_open != iod->state || (!IS_UTF_CHSET(iod->ichset) && !IS_UTF_CHSET(iod->ochset)) ||
				(!rm_ptr->done_1st_read && !rm_ptr->done_1st_write))
			{	/* only if not open, not UTF, or no reads or writes yet */
				GET_LONG(recordsize, (pp->str.addr + p_offset));
				if (recordsize <= 0)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_RMWIDTHPOS);
				else if (MAX_STRLEN < recordsize)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_RMWIDTHTOOBIG);
				rm_ptr->recordsize = recordsize;
				rm_ptr->def_recsize = FALSE;
			}
			break;
		case iop_rewind:
			if (iod->state == dev_open && !rm_ptr->fifo && !rm_ptr->pipe && 1 != rm_ptr->fildes)
			{
				iorm_flush(iod);
				if (lseek(rm_ptr->fildes, (off_t)0, SEEK_SET) == -1)
				{
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_IOERROR, 7, RTS_ERROR_LITERAL("lseek"),
							RTS_ERROR_LITERAL("REWIND"), CALLFROM, errno);
				}
				if (fseek(rm_ptr->filstr, (long)0, SEEK_SET) == -1)	/* Rewind the input stream */
				{
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_IOERROR, 7, RTS_ERROR_LITERAL("fseek"),
							RTS_ERROR_LITERAL("REWIND"), CALLFROM, errno);
				}
				iod->dollar.zeof = FALSE;
				iod->dollar.y = 0;
				iod->dollar.x = 0;
				rm_ptr->lastop = RM_NOOP;
				rm_ptr->done_1st_read = rm_ptr->done_1st_write = rm_ptr->crlast = FALSE;
				rm_ptr->out_bytes = rm_ptr->bom_buf_cnt = rm_ptr->bom_buf_off = 0;
				rm_ptr->inbuf_pos = rm_ptr->inbuf_off = rm_ptr->inbuf;
				rm_ptr->file_pos = 0;
				/* Reset temporary buffer so that the next read starts afresh */
				if (IS_UTF_CHSET(iod->ichset))
				{
					DEBUG_ONLY(memset(rm_ptr->utf_tmp_buffer, 0, CHUNK_SIZE));
					rm_ptr->utf_start_pos = 0;
					rm_ptr->utf_tot_bytes_in_buffer = 0;
				}
			}
			break;
		case iop_stream:
			if (iod->state != dev_open)
			{
				rm_ptr->stream = TRUE;
				if (CHSET_BINARY == iod->ichset)
				{
					ichset_specified = FALSE;
					iod->ichset = CHSET_M;
				}
				if (CHSET_BINARY == iod->ochset)
				{
					ochset_specified = FALSE;
					iod->ochset = CHSET_M;
				}
			}
			break;
		case iop_truncate:
			if (!rm_ptr->fifo && !rm_ptr->pipe && 1 != rm_ptr->fildes)
			{
				int ftruncate_res;
				if (fseek(rm_ptr->filstr, (long)rm_ptr->file_pos, SEEK_SET) == -1)
				{
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_IOERROR, 7, RTS_ERROR_LITERAL("fseek"),
							RTS_ERROR_LITERAL("TRUNCATE"), CALLFROM, errno);
				}
				if (lseek(rm_ptr->fildes, (off_t)rm_ptr->file_pos, SEEK_SET) == -1)
				{
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_IOERROR, 7, RTS_ERROR_LITERAL("lseek"),
							RTS_ERROR_LITERAL("TRUNCATE"), CALLFROM, errno);
				}
				FTRUNCATE(rm_ptr->fildes, (off_t)rm_ptr->file_pos, ftruncate_res);
				if (0 != ftruncate_res)
				{
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_IOERROR, 7, RTS_ERROR_LITERAL("ftruncate"),
							RTS_ERROR_LITERAL("TRUNCATE"), CALLFROM, errno);
				}
				iod->dollar.zeof = TRUE;
				/* Reset temporary buffer so that the next read can start afresh */
				if (IS_UTF_CHSET(iod->ichset))
				{
					rm_ptr->inbuf_top = rm_ptr->inbuf_off = rm_ptr->inbuf_pos = rm_ptr->inbuf;
					rm_ptr->out_bytes = rm_ptr->bom_buf_cnt = rm_ptr->bom_buf_off = 0;
					DEBUG_ONLY(memset(rm_ptr->utf_tmp_buffer, 0, CHUNK_SIZE));
					rm_ptr->utf_start_pos = 0;
					rm_ptr->utf_tot_bytes_in_buffer = 0;
					if (0 == rm_ptr->file_pos)
					{
						/* If the truncate size is zero reset done_1st_read and done_1st_write
						 * to FALSE.
						 */
						rm_ptr->done_1st_read = rm_ptr->done_1st_write = FALSE;
					}
				}
			}
			break;
		case iop_uic:
			{
				unsigned char	*ch, ct, *end;
				int		chown_res;
				uic_struct	uic;

				ch = (unsigned char *)pp->str.addr + p_offset;
				ct = *ch++;
				end = ch + ct;
				uic.grp = uic.mem = 0;
				while ((*ch != ',') && (ch < end))
					uic.mem = (10 * uic.mem) + (*ch++ - '0');
				if (*ch == ',')
				{
					while (++ch < end)
						uic.grp = (10 * uic.grp) + (*ch - '0');
				}
				CHG_OWNER(iod->trans_name->dollar_io, uic.mem, uic.grp, chown_res);
				if (-1 == chown_res)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) errno);
				break;
			}
		case iop_width:
			assert(iod->state == dev_open);
			GET_LONG(width, (pp->str.addr + p_offset));
			if (0 > width || (0 == width && !IS_UTF_CHSET(iod->ochset)))
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_RMWIDTHPOS);
			else if (MAX_STRLEN < width)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_RMWIDTHTOOBIG);
			/* Do not allow a WIDTH of 1 if either ICHSET or OCHSET is UTF-* */
			if ((1 == width) && gtm_utf8_mode && ((IS_UTF_CHSET(iod->ochset)) || (IS_UTF_CHSET(iod->ichset))))
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_WIDTHTOOSMALL);
			if (IS_UTF_CHSET(iod->ochset) && rm_ptr->fixed)
				iorm_flush(iod);	/* need to flush current record first */
			rm_ptr->def_width = FALSE;
			if (0 == width)
			{
				iod->width = rm_ptr->recordsize; /* Effectively eliminates consideration of width */
				if (!seen_wrap)
					iod->wrap = FALSE;
			} else
			{
				iod->width = width;
				iod->wrap = TRUE;
			}
			break;
		case iop_wrap:
			if (dev_open != iod->state)
			{
				if (CHSET_BINARY == iod->ichset || CHSET_BINARY == iod->ochset)
					break;		/* ignore wrap if BINARY specified */
			} else
			{	/* already open so check what conversion is in use  */
#ifdef __MVS__
				if (CHSET_BINARY == iod->process_chset)
					break;
#endif
			}
			iod->wrap = TRUE;
			seen_wrap = TRUE;	/* Don't allow WIDTH=0 to override WRAP */
			break;
		case iop_nowrap:
			iod->wrap = FALSE;
			break;
		case iop_ipchset:
			{
#ifdef KEEP_zOS_EBCDIC
				if ( (iconv_t)0 != iod->input_conv_cd )
				{
					ICONV_CLOSE_CD(iod->input_conv_cd);
				}
				SET_CODE_SET(iod->in_code_set, (char *)(pp->str.addr + p_offset + 1));
				if (DEFAULT_CODE_SET != iod->in_code_set)
					ICONV_OPEN_CD(iod->input_conv_cd, (char *)(pp->str.addr + p_offset + 1),
												INSIDE_CH_SET);
#endif
				if (chset_allowed && (dev_open != iod->state))
				{
					chset_mstr.addr = (char *)(pp->str.addr + p_offset + 1);
					chset_mstr.len = *(pp->str.addr + p_offset);
					SET_ENCODING(temp_chset, &chset_mstr);
					if (!gtm_utf8_mode && IS_UTF_CHSET(temp_chset))
						break;	/* ignore UTF chsets if not utf8_mode */
					iod->ichset = temp_chset;
					ichset_specified = TRUE;
				}
			}
                        break;
                case iop_opchset:
			{
#ifdef KEEP_zOS_EBCDIC
				if ( (iconv_t) 0 != iod->output_conv_cd )
				{
					ICONV_CLOSE_CD(iod->output_conv_cd);
				}
				SET_CODE_SET(iod->out_code_set, (char *)(pp->str.addr + p_offset + 1));
				if (DEFAULT_CODE_SET != iod->out_code_set)
					ICONV_OPEN_CD(iod->output_conv_cd, INSIDE_CH_SET,
							(char *)(pp->str.addr + p_offset + 1));
#endif
				if (chset_allowed && (dev_open != iod->state))
				{
					chset_mstr.addr = (char *)(pp->str.addr + p_offset + 1);
					chset_mstr.len = *(pp->str.addr + p_offset);
					SET_ENCODING(temp_chset, &chset_mstr);
					if (!gtm_utf8_mode && IS_UTF_CHSET(temp_chset))
						break;	/* ignore UTF chsets if not utf8_mode */
					iod->ochset = temp_chset;
					ochset_specified = TRUE;
					if (gtm_utf8_mode && !IS_PADCHAR_VALID(iod->ochset, rm_ptr->padchar))
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(2) ERR_PADCHARINVALID, 0);
				}
			}
                        break;
                case iop_chset:
			{
				if (chset_allowed && (dev_open != iod->state))
				{
					chset_mstr.addr = (char *)(pp->str.addr + p_offset + 1);
					chset_mstr.len = *(pp->str.addr + p_offset);
					SET_ENCODING(temp_chset, &chset_mstr);
					if (!gtm_utf8_mode && IS_UTF_CHSET(temp_chset))
						break;	/* ignore UTF chsets if not utf8_mode */
					iod->ochset = temp_chset;
					if (gtm_utf8_mode && !IS_PADCHAR_VALID(iod->ochset, rm_ptr->padchar))
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(2) ERR_PADCHARINVALID, 0);
					iod->ichset = iod->ochset;
					ochset_specified = ichset_specified = TRUE;
				}
			}
                        break;
		case iop_m:
			if (chset_allowed && (dev_open != iod->state))
			{
				iod->ichset = iod->ochset = CHSET_M;
				ichset_specified = ochset_specified = TRUE;
			}
			break;
		case iop_utf8:
		case iop_utf16:
		case iop_utf16be:
		case iop_utf16le:
			if (gtm_utf8_mode && dev_open != iod->state)
			{
				iod->ichset = iod->ochset =
					(iop_utf8    == c) ? CHSET_UTF8 :
					(iop_utf16   == c) ? CHSET_UTF16 :
					(iop_utf16be == c) ? CHSET_UTF16BE : CHSET_UTF16LE;
				ichset_specified = ochset_specified = TRUE;
			}
			break;
		case iop_follow:
			if (!rm_ptr->fifo && !rm_ptr->pipe)
				rm_ptr->follow = TRUE;
			break;
		case iop_nofollow:
			if (!rm_ptr->fifo && !rm_ptr->pipe)
				rm_ptr->follow = FALSE;
			break;
		default:
			break;
		}
		p_offset += ((IOP_VAR_SIZE == io_params_size[c]) ?
			(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[c]);
	}
	if (dev_open != iod->state)
	{
		if (!ichset_specified)
		{
#ifdef __MVS__
			iod->is_ichset_default = TRUE;
#endif
			iod->ichset = (gtm_utf8_mode) ? CHSET_UTF8 : CHSET_M;
		}
		if (!ochset_specified)
		{
#ifdef __MVS__
			iod->is_ochset_default = TRUE;
#endif
			iod->ochset = (gtm_utf8_mode) ? CHSET_UTF8 : CHSET_M;
		}
		if ((CHSET_M != iod->ichset) && (CHSET_UTF16 != iod->ichset) && (CHSET_MAX_IDX > iod->ichset))
			get_chset_desc(&chset_names[iod->ichset]);
		if ((CHSET_M != iod->ochset) && (CHSET_UTF16 != iod->ochset) && (CHSET_MAX_IDX > iod->ochset))
			get_chset_desc(&chset_names[iod->ochset]);
		/* If ICHSET or OCHSET is of type UTF-16, check that RECORDSIZE is even */
		if (gtm_utf8_mode && (IS_UTF16_CHSET(iod->ichset) || IS_UTF16_CHSET(iod->ochset)))
		{
			if (rm_ptr->def_recsize)
			{	/* DEF_RM_RECORDSIZE is currently an odd number (32K-1). Round it down
				 * to be a multiple of 4 bytes since a UTF-16 char can be 2 or 4 bytes */
				assert(DEF_RM_RECORDSIZE == 32767);
				rm_ptr->recordsize = ROUND_DOWN2(rm_ptr->recordsize, 4);
			} else if (0 != rm_ptr->recordsize % 2)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_RECSIZENOTEVEN, 1, rm_ptr->recordsize);
		}
	}
	if (fstat_done && mode != mode1)
	{	/* if the mode has been changed by the qualifiers, reset it */
		if (-1 == CHMOD(iod->trans_name->dollar_io, mode))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) errno);
	}
	return;
}
