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
#include "gtmio.h"
#ifdef __MVS__
#include "gtm_zos_io.h"
#include "gtm_zos_chset.h"
#endif
#ifdef UNICODE_SUPPORTED
#include "gtm_conv.h"
#include "gtm_utf8.h"
#endif
#include "gtmcrypt.h"
#include "error.h"

GBLREF io_pair		io_curr_device;
GBLREF	boolean_t	gtm_utf8_mode;
ZOS_ONLY(GBLREF boolean_t	gtm_tag_utf8_as_ascii;)
#ifdef __MVS__
error_def(ERR_BADTAG);
#endif
error_def(ERR_CRYPTNOAPPEND);
error_def(ERR_DEVOPENFAIL);
error_def(ERR_TEXT);
error_def(ERR_IOERROR);

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
	int		bom_size_toread;
	boolean_t	utf_active, def_recsize_before, text_tag, newversion;
	boolean_t	closed_nodestroy;
	boolean_t	append;
	gtm_chset_t	width_chset, dummy_chset;
	off_t		new_position;
	long		pipe_buff_size;
#	ifdef __MVS__
	int		file_tag, obtained_tag, realfiletag;
	char		*errmsg;
#	endif
	boolean_t	ch_set;

	newversion = closed_nodestroy = append = FALSE;
	iod = dev_name->iod;
	ESTABLISH_RET_GTMIO_CH(&iod->pair, -1, ch_set);
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
		d_rm->read_only = FALSE;
		d_rm->fifo = FALSE;
		d_rm->is_pipe = FALSE;
		d_rm->padchar = DEF_RM_PADCHAR;
		d_rm->inbuf = NULL;
		d_rm->outbuf = NULL;
		d_rm->follow = FALSE;
		d_rm->no_destroy = FALSE;
		d_rm->read_fildes = FD_INVALID;
		d_rm->input_iv.addr = NULL;
		d_rm->input_iv.len = 0;
		d_rm->output_iv.addr = NULL;
		d_rm->output_iv.len = 0;
		d_rm->input_key.addr = NULL;
		d_rm->input_key.len = 0;
		d_rm->output_key.addr = NULL;
		d_rm->output_key.len = 0;
		d_rm->input_cipher_handle = GTMCRYPT_INVALID_KEY_HANDLE;
		d_rm->output_cipher_handle = GTMCRYPT_INVALID_KEY_HANDLE;
		d_rm->input_encrypted = FALSE;
		d_rm->output_encrypted = FALSE;
		d_rm->read_occurred = FALSE;
		d_rm->write_occurred = FALSE;
		d_rm->fsblock_buffer_size = 0;
		d_rm->fsblock_buffer = NULL;
		d_rm->ichset_utf16_variant = d_rm->ochset_utf16_variant = 0;
	} else
	{
		d_rm = (d_rm_struct *)iod->dev_sp;
		/* remember if device closed by nodestroy in case the no_destroy flag was cleared in io_open_try()
		   due to a deviceparameter other than SEEK on reopen */
		if ((dev_closed == iod->state) && !d_rm->no_destroy && !d_rm->fifo && !d_rm->is_pipe && (2 < fd))
			closed_nodestroy = TRUE;
	}
	if (dev_closed == iod->state)
	{
		if (!d_rm->no_destroy)
		{
			d_rm->lastop = RM_NOOP;
			d_rm->out_bytes = 0;
			d_rm->crlast = FALSE;
			d_rm->done_1st_read = FALSE;
			d_rm->done_1st_write = FALSE;
			d_rm->follow = FALSE;
		}
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
				if (!d_rm->fifo && !d_rm->is_pipe && (off_t)-1 == (size = lseek(fd, 0, SEEK_END)))
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_DEVOPENFAIL, 2, dev_name->len,
						      dev_name->dollar_io,
						      ERR_TEXT, 2, LEN_AND_LIT("Error setting file pointer to end of file"), errno);
				if (0 < statbuf.st_size)
				{	/* Only disable BOM writing if there is something in the file already (not empty) */
					d_rm->done_1st_read = FALSE;
					d_rm->done_1st_write = FALSE;
					/* the file is not empty so set file_pos to the size of the file */
					d_rm->file_pos = size;
				}
				append = TRUE; /* remember so we don't clear file_pos later */
				break;
			} else if (iop_newversion == ch)
				newversion = TRUE;
			p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ? (unsigned char)*(pp->str.addr + p_offset) + 1 :
					io_params_size[ch]);
		}
		if (!d_rm->fifo && !d_rm->is_pipe && (2 < fd))
		{
			if (d_rm->no_destroy)
			{
				/* if last operation was a write then file position set in iorm_close() */
				if (RM_WRITE == d_rm->lastop)
				{
					new_position = d_rm->file_pos;
				} else
				{
					if (d_rm->fixed)
						new_position = d_rm->file_pos;
					else
					{	/* Buffered reads in non-fixed M & UTF modes. */
						new_position = d_rm->file_pos +
							d_rm->tot_bytes_in_buffer - d_rm->start_pos;
					}
				}

				/* lseek to file position for nodestroy */
				if ((off_t)-1 == (size =lseek (fd, new_position, SEEK_SET)))
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_DEVOPENFAIL, 2,
						      dev_name->len, dev_name->dollar_io, ERR_TEXT, 2,
						      LEN_AND_LIT("Error setting file pointer to the current position"), errno);
			} else
			{
				if ((off_t)-1 == (size = lseek(fd, 0, SEEK_CUR)))
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_DEVOPENFAIL, 2, dev_name->len,
						      dev_name->dollar_io, ERR_TEXT, 2,
						      LEN_AND_LIT("Error setting file pointer to the current position"), errno);
				/* clear some status if the close was nodestroy but not saving state.  The file pointer
				 will be at the beginning or at the end if append is TRUE */
				if (closed_nodestroy)
				{
					iod->dollar.y = 0;
					iod->dollar.x = 0;
					d_rm->lastop = RM_NOOP;
					d_rm->done_1st_read = d_rm->done_1st_write = d_rm->crlast = FALSE;
					d_rm->out_bytes = d_rm->bom_buf_cnt = d_rm->bom_buf_off = 0;
					d_rm->inbuf_top = d_rm->inbuf_off = d_rm->inbuf_pos = d_rm->inbuf;
					if (!append)
						d_rm->file_pos = 0;
					/* Reset temporary buffer so that the next read starts afresh */
					if (!d_rm->fixed || IS_UTF_CHSET(iod->ichset))
					{
						DEBUG_ONLY(MEMSET_IF_DEFINED(d_rm->tmp_buffer, 0, CHUNK_SIZE));
						d_rm->start_pos = 0;
						d_rm->tot_bytes_in_buffer = 0;
					}
				}
			}
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
			FDOPEN(d_rm->filstr, fd, "r+");		/* Try open R/W */
			if (NULL == d_rm->filstr)
				FDOPEN(d_rm->filstr, fd, "r");	/* Try open RO */
			if (NULL == d_rm->filstr)
				FDOPEN(d_rm->filstr, fd, "w");	/* Try open WO */
			if (NULL == d_rm->filstr)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_DEVOPENFAIL, 2, dev_name->len,
					      dev_name->dollar_io, ERR_TEXT, 2, LEN_AND_LIT("Error in stream open"), errno);
		}
	}
	/* setting of recordsize, WIDTH, etc, based on changed (if any) CHSET is taken care in iorm_use(). */
	if (closed_nodestroy)
		d_rm->no_destroy = TRUE;
	iorm_use(iod, pp);

	/* Now that recordsize and CHSET parms have been handled (if any), allocate the record buffer if needed (utf8 fixed) */
	if (dev_closed == iod->state)
	{
#		ifdef __MVS__
		/* need to get file tag info before set policy which can change what is returned */
		if (-1 == gtm_zos_check_tag(fd, &file_tag, &text_tag))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_DEVOPENFAIL, 2, dev_name->len, dev_name->dollar_io, ERR_TEXT,
				      2, LEN_AND_LIT("Error in check_tag fstat"), errno);
		SET_CHSET_FROM_TAG(file_tag, iod->file_chset);
		iod->text_flag = text_tag;
		if (!d_rm->is_pipe && 2 < fd)
		{	/* not for stdin, stdout, stderr or pipes */
			if (iod->newly_created || newversion)
			{	/* tag the file.  The macros also modify text_tag and file_tag. */
				if (d_rm->fifo && (iod->is_ochset_default || d_rm->read_only))
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
				if (iod->is_ochset_default || d_rm->read_only)
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
		/* Unless NEWVERSION is specified or the file is empty, or the file was CLOSEd with NODESTROY at EOF, we do not
		 * allow APPEND with encryption.
		 */
		if (d_rm->output_encrypted && append && (!newversion) && (0 != statbuf.st_size) &&
				(!d_rm->no_destroy || !iod->dollar.zeof))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CRYPTNOAPPEND, 2, dev_name->len, dev_name->dollar_io);
	}

	if (!d_rm->bom_checked && !d_rm->fifo && !d_rm->is_pipe && (2 < fd) && IS_UTF_CHSET(iod->ochset))
	{
		/* if file opened with WRITEONLY */
		if (d_rm->write_only)
		{
			/* get the file size again in case it was truncated */
			FSTAT_FILE(fd, &statbuf, fstat_res);
			if (-1 == fstat_res)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_DEVOPENFAIL, 2, dev_name->len,
					      dev_name->dollar_io, ERR_TEXT, 2,	LEN_AND_LIT("Error in fstat"), errno);

			/* and is not empty generate and error */
			if (0 < statbuf.st_size)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_DEVOPENFAIL, 2, dev_name->len, dev_name->dollar_io,
					      ERR_TEXT, 2, LEN_AND_LIT("Cannot read BOM from non-empty WRITEONLY file"));

			/* it is empty so set bom_checked as it will be 0 for all
			   but UTF-16 which will be set in iorm_write.c on the first write */
			else
				d_rm->bom_checked = TRUE;
		} else
		{ /* if not a reopen of file, and is not empty and not writeonly, then go to the
		     beginning of the file if BOM bytes available and read the potential BOM */
			if (!d_rm->no_destroy && (0 < statbuf.st_size) && (!d_rm->input_encrypted))
			{
				assert(UTF16BE_BOM_LEN < UTF8_BOM_LEN);
				if ((CHSET_UTF8 == iod->ichset) && (statbuf.st_size >= UTF8_BOM_LEN))
					bom_size_toread = UTF8_BOM_LEN;
				else if (IS_UTF16_CHSET(iod->ichset) && (statbuf.st_size >= UTF16BE_BOM_LEN))
					bom_size_toread = UTF16BE_BOM_LEN;
				else
					bom_size_toread = 0;
				if (0 < bom_size_toread)
				{
					if ((off_t)-1 == lseek(fd, 0, SEEK_SET))
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_DEVOPENFAIL, 2, dev_name->len,
							      dev_name->dollar_io, ERR_TEXT, 2,
							      LEN_AND_LIT("Error setting file pointer to beginning of file"),
							      errno);
					d_rm->bom_num_bytes = open_get_bom(iod, bom_size_toread);
					/* move back to previous file position */
					if ((off_t)-1 == lseek(fd, d_rm->file_pos, SEEK_SET))
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_DEVOPENFAIL, 2, dev_name->len,
							      dev_name->dollar_io, ERR_TEXT, 2,
							      LEN_AND_LIT("Error setting file pointer to previous file position"),
							      errno);

				}
				d_rm->bom_checked = TRUE;
			}
		}
	}


	if (d_rm->no_destroy)
		d_rm->no_destroy = FALSE;
	iod->state = dev_open;
	REVERT_GTMIO_CH(&iod->pair, ch_set);
	return TRUE;
}

/* check for BOM in a disk file under the covers during open, seek, or the first read after a write if not already checked */
int	open_get_bom(io_desc *io_ptr, int bom_size)
{
	int		status = 0;
	d_rm_struct	*rm_ptr;
	gtm_chset_t	chset;
	int 		num_bom_bytes;
	boolean_t	ch_set;

	ESTABLISH_RET_GTMIO_CH(&io_ptr->pair, -1, ch_set);
	rm_ptr = (d_rm_struct *)(io_ptr->dev_sp);
	assert(UTF16BE_BOM_LEN == UTF16LE_BOM_LEN);
	assert(!rm_ptr->input_encrypted);

	/* If ichset is UTF-16 save chset to see if it changes */
	if (UTF16BE_BOM_LEN == bom_size)
		chset = io_ptr->ichset;
	DOREADRL(rm_ptr->fildes, &rm_ptr->bom_buf[0], bom_size, status);
	if (0 > status)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_IOERROR, 7,
			      RTS_ERROR_LITERAL("read"),
			      RTS_ERROR_LITERAL("READING BOM"), CALLFROM, errno);
	} else if (0 == status)
	{
		num_bom_bytes = 0;
	} else
	{
		num_bom_bytes = gtm_utf_bomcheck(io_ptr, &io_ptr->ichset, &rm_ptr->bom_buf[0], bom_size);
		if ((UTF16BE_BOM_LEN == bom_size) && chset != io_ptr->ichset)
		{
			rm_ptr->ichset_utf16_variant = chset = io_ptr->ichset;
			get_chset_desc(&chset_names[chset]);
		}
	}
	REVERT_GTMIO_CH(&io_ptr->pair, ch_set);
	return (num_bom_bytes);
}
