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

#include "gtm_unistd.h"
#include "gtm_stat.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_limits.h"
#include "io.h"
#include "iormdef.h"
#include "io_params.h"
#include "eintr_wrappers.h"
#include "have_crit.h"
#ifdef __MVS__
#include "gtm_zos_io.h"
#include "gtm_zos_chset.h"
#endif

GBLREF io_pair		io_curr_device;
GBLREF	boolean_t	gtm_utf8_mode;
ZOS_ONLY(GBLREF boolean_t	gtm_tag_utf8_as_ascii;)
#ifdef __MVS__
error_def(ERR_BADTAG);
#endif
error_def(ERR_DEVOPENFAIL);
error_def(ERR_TEXT);



LITREF	mstr		chset_names[];
LITREF unsigned char	io_params_size[];

/* WARNING, this routine is called from ioff_open as well as from the dispatch table. */
/* WARNING, this routine is called from iopi_open as well as from the dispatch table. */

short	iorm_open(io_log_name *dev_name, mval *pp, int fd, mval *mspace, int4 timeout)
{
	io_desc		*iod;
 	d_rm_struct	*d_rm;
	off_t		size;
	unsigned char	ch;
	int		fstat_res, width_bytes;
	int4		recsize_before;
	struct stat	statbuf;
	int 		p_offset;
	boolean_t	utf_active, def_recsize_before, text_tag, newversion = FALSE;
	gtm_chset_t	width_chset, dummy_chset;
	long		pipe_buff_size;
#ifdef __MVS__
	int		file_tag, obtained_tag, realfiletag;
	char		*errmsg;
#endif

	iod = dev_name->iod;
	size = 0;
	p_offset = 0;
	assert((params) *(pp->str.addr + p_offset) < (unsigned char)n_iops);
	assert(NULL != iod);
	assert(0 <= iod->state && n_io_dev_states > iod->state);
	assert(rm == iod->type);
	if (dev_never_opened == iod->state)
	{
		iod->dev_sp = (void *)malloc(SIZEOF(d_rm_struct));
		memset(iod->dev_sp, 0, SIZEOF(d_rm_struct));
		d_rm = (d_rm_struct *)iod->dev_sp;
		iod->state = dev_closed;
		d_rm->stream = FALSE;
		iod->width = DEF_RM_WIDTH;
		iod->length = DEF_RM_LENGTH;
		d_rm->recordsize = DEF_RM_RECORDSIZE;
		d_rm->def_width = d_rm->def_recsize = TRUE;
		d_rm->fixed = FALSE;
		d_rm->noread = FALSE;
		d_rm->fifo = FALSE;
		d_rm->pipe = FALSE;
		d_rm->padchar = DEF_RM_PADCHAR;
		d_rm->inbuf = NULL;
		d_rm->outbuf = NULL;
		d_rm->follow = FALSE;
	} else
		d_rm = (d_rm_struct *)iod->dev_sp;
	if (dev_closed == iod->state)
	{
		d_rm->lastop = RM_NOOP;
		d_rm->out_bytes = 0;
		d_rm->crlast = FALSE;
		d_rm->done_1st_read = FALSE;
		d_rm->done_1st_write = FALSE;
		d_rm->follow = FALSE;
		assert(0 <= fd);
		d_rm->fildes = fd;
		FSTAT_FILE(fd, &statbuf, fstat_res);
		if (-1 == fstat_res)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_DEVOPENFAIL, 2, dev_name->len,
				      dev_name->dollar_io, ERR_TEXT, 2,	LEN_AND_LIT("Error in fstat"), errno);
		for (p_offset = 0; iop_eol != *(pp->str.addr + p_offset); )
		{
			if (iop_append == (ch = *(pp->str.addr + p_offset++)))
			{
				if (!d_rm->fifo && !d_rm->pipe && (off_t)-1 == (size = lseek(fd, (off_t)0, SEEK_END)))
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_DEVOPENFAIL, 2, dev_name->len,
						      dev_name->dollar_io,
						      ERR_TEXT, 2, LEN_AND_LIT("Error setting file pointer to end of file"), errno);
				if (0 < statbuf.st_size)
				{	/* Only disable BOM writing if there is something in the file already (not empty) */
					d_rm->done_1st_read = FALSE;
					d_rm->done_1st_write = FALSE;
				}
				break;
			} else if (iop_newversion == ch)
				newversion = TRUE;
			p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ? (unsigned char)*(pp->str.addr + p_offset) + 1 :
					io_params_size[ch]);
		}
		if (!d_rm->fifo && !d_rm->pipe && fd != 0)
		{
			if ((off_t)-1 == (size = lseek(fd, (off_t)0, SEEK_CUR)))
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_DEVOPENFAIL, 2, dev_name->len, dev_name->dollar_io,
				  ERR_TEXT, 2, LEN_AND_LIT("Error setting file pointer to the current position"), errno);
			if (size == statbuf.st_size)
				iod->dollar.zeof = TRUE;
		} else
		{
			pipe_buff_size = fpathconf(fd, _PC_PIPE_BUF);
			d_rm->pipe_buff_size = (-1 == pipe_buff_size) ? _POSIX_PIPE_BUF : pipe_buff_size;
		}
		if (1 == fd)
			d_rm->filstr = NULL;
		else
		{
			FDOPEN(d_rm->filstr, fd, "r");
			if (NULL == d_rm->filstr)
			{
				FDOPEN(d_rm->filstr, fd, "w");
				if (NULL == d_rm->filstr)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_DEVOPENFAIL, 2, dev_name->len,
						      dev_name->dollar_io, ERR_TEXT, 2,
						      LEN_AND_LIT("Error in stream open"), errno);
			}
		}
	}
	recsize_before = d_rm->recordsize;
	def_recsize_before = d_rm->def_recsize;
	iorm_use(iod, pp);
	/* Now that recordsize and CHSET parms have been handled (if any), set WIDTH if necessary */
	if (recsize_before != d_rm->recordsize || def_recsize_before != d_rm->def_recsize)
	{	/* record size was specified even if the same value */
		SET_WIDTH_BYTES
		assert(width_bytes <= d_rm->recordsize);	/* or else RECSIZENOTEVEN error would have been issued */
		iod->width = d_rm->recordsize;/* we are guaranteed this width will hold at least one character due to above check */
	}
	/* Now that recordsize and CHSET parms have been handled (if any), allocate the record buffer if needed (utf8 fixed) */
	if (dev_closed == iod->state)
	{
#ifdef __MVS__
		/* need to get file tag info before set policy which can change what is returned */
		if (-1 == gtm_zos_check_tag(fd, &file_tag, &text_tag))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_DEVOPENFAIL, 2, dev_name->len, dev_name->dollar_io, ERR_TEXT,
				      2, LEN_AND_LIT("Error in check_tag fstat"), errno);
		SET_CHSET_FROM_TAG(file_tag, iod->file_chset);
		iod->text_flag = text_tag;
		if (!d_rm->pipe && 2 < fd)
		{	/* not for stdin, stdout, stderr or pipes */
			if (iod->newly_created || newversion)
			{	/* tag the file.  The macros also modify text_tag and file_tag. */
				if (d_rm->fifo && (iod->is_ochset_default || d_rm->noread))
				{	/* If FIFO, set tag per ichset if no ochset or READONLY */
					SET_TAG_FROM_CHSET(iod->ichset, iod->file_chset, TRUE);
				} else
				{
					SET_TAG_FROM_CHSET(iod->ochset, iod->file_chset, TRUE);
				}
				iod->file_tag = (unsigned int)file_tag;
				iod->text_flag = text_tag;
				if (-1 == gtm_zos_set_tag(fd, file_tag, text_tag, TAG_FORCE, &realfiletag))
				{
					errmsg = STRERROR(errno);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(10) ERR_BADTAG, 4, dev_name->len,
						      dev_name->dollar_io, realfiletag, file_tag, ERR_TEXT, 2,
						      RTS_ERROR_STRING(errmsg));
				}
				if (gtm_utf8_mode && gtm_tag_utf8_as_ascii && (CHSET_UTF8 == iod->ochset))
					iod->process_chset = iod->ochset;
				else
					iod->process_chset = iod->file_chset;
			} else
			{
				iod->file_tag = (unsigned int)file_tag;	/* save real tag for file */
				if (iod->is_ochset_default || d_rm->noread)
				{	/* set tag per ichset if no ochset or READONLY */
					SET_TAG_FROM_CHSET(iod->ichset, dummy_chset, FALSE);
				} else
				{
					SET_TAG_FROM_CHSET(iod->ochset, dummy_chset, FALSE);
				}
				if (-1 == (obtained_tag = gtm_zos_tag_to_policy(fd, file_tag, &realfiletag)))
				{
					errmsg = STRERROR(errno);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(10) ERR_BADTAG, 4, dev_name->len, dev_name->dollar_io,
						      realfiletag, file_tag, ERR_TEXT, 2, RTS_ERROR_STRING(errmsg));
				}
				SET_CHSET_FROM_TAG(obtained_tag, iod->process_chset);
			}
		} else
			iod->process_chset = iod->file_chset;	/* stdin, stdout, stderr */
		if (!gtm_utf8_mode || !IS_UTF_CHSET(iod->ochset))
			iod->ochset = CHSET_M;
		if (!gtm_utf8_mode || !IS_UTF_CHSET(iod->ichset))
			iod->ichset = CHSET_M;
		if (CHSET_BINARY == iod->process_chset)
		{
			d_rm->fixed = TRUE;
			iod->wrap = TRUE;
		}
#endif
		if (IS_UTF_CHSET(iod->ichset))
		{
			assert(NULL == d_rm->inbuf);			/* We shouldn't have an existing buffer at this point */
			assert(gtm_utf8_mode);
			d_rm->bufsize = d_rm->fixed ? (d_rm->recordsize + 4) : 20;		/* +4 for one extra utf8 char */
			d_rm->inbuf = malloc(d_rm->bufsize);
			d_rm->inbuf_pos = d_rm->inbuf_top = d_rm->inbuf_off = d_rm->inbuf;	/* Force read first time thru */
		}
		if (IS_UTF16_CHSET(iod->ochset))
		{
			assert(NULL == d_rm->outbuf);			/* We shouldn't have an existing buffer at this point */
			d_rm->outbufsize = d_rm->recordsize + 4;
			d_rm->outbuf = malloc(d_rm->outbufsize);
			d_rm->out_bytes = 0;
		}
	} else
	{	/* check for changed recordsize */
		if (IS_UTF_CHSET(iod->ichset) && (recsize_before != d_rm->recordsize))
		{
			if (NULL != d_rm->inbuf)
				free(d_rm->inbuf);
			d_rm->bufsize = d_rm->fixed ? (d_rm->recordsize + 4) : 20;		/* +4 for one extra utf8 char */
			d_rm->inbuf = malloc(d_rm->bufsize);
			d_rm->inbuf_pos = d_rm->inbuf_top = d_rm->inbuf_off = d_rm->inbuf;	/* Force read first time thru */
		}
		if (IS_UTF16_CHSET(iod->ochset) && (recsize_before != d_rm->recordsize))
		{
			if (NULL != d_rm->outbuf)
				free(d_rm->outbuf);
			d_rm->outbufsize = d_rm->recordsize + 4;
			d_rm->outbuf = malloc(d_rm->outbufsize);
			d_rm->out_bytes = 0;
		}
	}
	iod->state = dev_open;
	return TRUE;
}
