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

#include "gtm_string.h"

#include <errno.h>
#include "gtm_unistd.h"
#include "gtm_stat.h"
#include "gtm_stdio.h"
#include "gtm_iconv.h"
#include "gtm_limits.h"

#include "copy.h"
#include "io.h"
#include "iormdef.h"
#include "io_params.h"
#include "iosp.h"
#include "eintr_wrappers.h"
#include "stringpool.h"
#include "gtm_stdlib.h"
#include "min_max.h"
#include "arit.h"
#ifdef UNICODE_SUPPORTED
#include "gtm_conv.h"
#include "gtm_utf8.h"
#endif
#include "gtmcrypt.h"
#include "error.h"
#include "send_msg.h"
#include "get_fs_block_size.h"
#include "op.h"
#include "indir_enum.h"

/* Only want to do fstat() once on this file, not on every use. */
#define FSTAT_CHECK(GETMODE)										\
	if (!(GETMODE) || !(fstat_done))								\
	{												\
		FSTAT_FILE(rm_ptr->fildes, &statbuf, fstat_res);					\
		if (-1 == fstat_res)									\
		{											\
			save_errno = errno;								\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,			\
				RTS_ERROR_LITERAL("fstat"),						\
			CALLFROM, save_errno);								\
		}											\
		fstat_done = TRUE;									\
	}												\
	if (GETMODE && !get_mode_done)									\
	{												\
		mode = mode1 = statbuf.st_mode;								\
		get_mode_done = TRUE;									\
	}

#define	IS_PADCHAR_VALID(chset, padchar)	(IS_ASCII(padchar))
/* limit seek string to a signed long long */
#define LIMIT_SEEK_STR				NUM_DEC_DG_2L + 2
/* define size of early_terminate message */

#define INVALID_SEEK_HEAD "SEEK value "

#define INVALID_SEEK_TAIL " invalid."

/* define max size of SDSEEKERR error string */
#define MAX_ERROR_SIZE STR_LIT_LEN(INVALID_SEEK_HEAD) + MAXDEVPARLEN + STR_LIT_LEN(INVALID_SEEK_TAIL)

/* output SDSEEKERR error */
#define OUTPUT_SDSEEKERR(SEEK_STR)									\
{													\
	SNPRINTF(error_str, MAX_ERROR_SIZE,"%s%s%s", INVALID_SEEK_HEAD, SEEK_STR, INVALID_SEEK_TAIL);	\
	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SDSEEKERR, 2, LEN_AND_STR(error_str));		\
}

/* Check if the IV for the specified direction has changed. */
#define IV_CHANGED(DIRECTION, NEW_IV, RM_PTR)								\
	((NEW_IV.len != RM_PTR->DIRECTION ## _iv.len) 							\
		|| (0 != memcmp(NEW_IV.addr, RM_PTR->DIRECTION ## _iv.addr, NEW_IV.len)))

/* Check if the key for the specified direction has changed. */
#define KEY_CHANGED(DIRECTION, NEW_KEY, RM_PTR)								\
	((NEW_KEY.len != RM_PTR->DIRECTION ## _key.len)							\
		|| (0 != memcmp(NEW_KEY.addr, RM_PTR->DIRECTION ## _key.addr, NEW_KEY.len)))

#define GET_LEN_AND_ADDR(ADDR, LEN)									\
	LEN = (int)(*(pp->str.addr + p_offset));							\
	ADDR = (char *)(pp->str.addr + p_offset + 1);

/* Obtain the key and IV from the KEY field, which is expected to be in the format <key><space><iv>, where iv is optional. */
#define GET_KEY_AND_IV(DIRECTION)									\
{													\
	int i, len, encr_key_delim_pos;									\
													\
	GET_ADDR_AND_LEN(DIRECTION ## _key.addr, DIRECTION ## _key.len);				\
	for (i = 0, encr_key_delim_pos = len = DIRECTION ## _key.len; i < DIRECTION ## _key.len; i++)	\
	{												\
		if (ENCR_KEY_DELIM_CHAR == *((char *)DIRECTION ## _key.addr + i))			\
		{											\
			encr_key_delim_pos = i + 1;							\
			DIRECTION ## _key.len = i;							\
			break;										\
		}											\
	}												\
	DIRECTION ## _iv.addr = (char *)DIRECTION ## _key.addr + encr_key_delim_pos;			\
	DIRECTION ## _iv.len = len - encr_key_delim_pos;						\
	if (GTMCRYPT_MAX_KEYNAME_LEN <= DIRECTION ## _key.len)						\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CRYPTKEYTOOBIG, 2,				\
			DIRECTION ## _key.len, GTMCRYPT_MAX_KEYNAME_LEN - 1);				\
	else if (0 < DIRECTION ## _key.len)								\
		DIRECTION ## _key_not_empty = TRUE;							\
	else												\
		DIRECTION ## _key_not_empty = FALSE;							\
	DIRECTION ## _key_entry_present = TRUE;								\
}

#define INIT_CIPHER_CONTEXT(OPERATION, KEY, IV, CIPHER_HANDLE, DEV_NAME)				\
{													\
	int rv;												\
													\
	GTMCRYPT_INIT_CIPHER_CONTEXT((KEY).len, (KEY).addr, (IV).len, (IV).addr,			\
		CIPHER_HANDLE, OPERATION, rv);								\
	if (0 != rv)											\
		GTMCRYPT_REPORT_ERROR(rv, rts_error, (DEV_NAME)->len, (DEV_NAME)->dollar_io);		\
}

#define ENCR_KEY_DELIM_CHAR	' '


typedef struct
{
	unsigned short mem;
	unsigned short grp;
} uic_struct;

LITREF	unsigned char		io_params_size[];
GBLREF	boolean_t		gtm_utf8_mode;
GBLREF	io_pair			io_std_device;		/* standard device	*/


#ifdef UNICODE_SUPPORTED
GBLREF	UConverter		*chset_desc[];
LITREF	mstr			chset_names[];
#endif

enum
{
	SEEK_PLUS = 1,	/* seek from current position plus offset */
	SEEK_MINUS,	/* seek from current position minus offset */
	SEEK_ABS	/* seek absolute position from start of file */
};

error_def(ERR_CRYPTKEYRELEASEFAILED);
error_def(ERR_CRYPTKEYTOOBIG);
error_def(ERR_CRYPTNOKEYSPEC);
error_def(ERR_CRYPTNOOVERRIDE);
error_def(ERR_CRYPTNOSEEK);
error_def(ERR_CRYPTNOTRUNC);
error_def(ERR_DEVPARMNEG);
error_def(ERR_RMWIDTHPOS);
error_def(ERR_RMWIDTHTOOBIG);
error_def(ERR_RECSIZENOTEVEN);
error_def(ERR_SYSCALL);
error_def(ERR_WIDTHTOOSMALL);
error_def(ERR_PADCHARINVALID);
error_def(ERR_IOERROR);
error_def(ERR_SDSEEKERR);

void	iorm_use(io_desc *iod, mval *pp)
{
	boolean_t	seen_wrap, fstat_done, get_mode_done, outdevparam;
	boolean_t	input_key_not_empty, output_key_not_empty, input_key_entry_present, output_key_entry_present;
	boolean_t	init_input_encryption, init_output_encryption, reset_input_encryption, reset_output_encryption;
	boolean_t	seek_specified, ichset_specified, ochset_specified, chset_allowed;
	unsigned char	c;
	short		mode, mode1;
	int4		length, width, width_bytes, recordsize, recsize_before, padchar;
	int		fstat_res, save_errno, rv;
	d_rm_struct	*rm_ptr;
	struct stat	statbuf;
	int		p_offset;
	mstr		chset_mstr;
	gtm_chset_t	width_chset, temp_chset;
	int		seek_len;
	char		seek_str[LIMIT_SEEK_STR];
	char		*seek_ptr;
	int		seek_type;
	off_t		seek_value;
	off_t		new_position;
	off_t		current_offset;
	char		*endptr;
	off_t		cur_position;
	int		bom_size_toread;
	io_log_name	*dev_name;
	mstr		input_iv, output_iv, input_key, output_key;
	char		error_str[MAX_ERROR_SIZE];
	boolean_t	ch_set, def_recsize_before;
	int		disk_block_multiple;
	size_t		fwrite_buffer_size;
	unsigned char	*ch, ct, *end;
	int		chown_res;
	uic_struct	uic;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	p_offset = 0;
	ESTABLISH_GTMIO_CH(&iod->pair, ch_set);
	rm_ptr = (d_rm_struct *)iod->dev_sp;
	input_key_not_empty = output_key_not_empty = FALSE;
	input_key_entry_present = output_key_entry_present = FALSE;
	reset_input_encryption = reset_output_encryption = FALSE;
	init_input_encryption = init_output_encryption = FALSE;
	seen_wrap = fstat_done = get_mode_done = outdevparam = FALSE;
	seek_specified = ichset_specified = ochset_specified = chset_allowed = FALSE;
	dev_name = iod->trans_name;
	recsize_before = rm_ptr->recordsize;
	def_recsize_before = rm_ptr->def_recsize;
	if (ZOS_ONLY(TRUE ||) gtm_utf8_mode)
		chset_allowed = TRUE;
	while (*(pp->str.addr + p_offset) != iop_eol)
	{
		assert((params) *(pp->str.addr + p_offset) < (params)n_iops);
		/* need to make sure outdevparam is FALSE for every iteration in case multiple instances of OUTSEEK, etc */
		if (TRUE == outdevparam)
			outdevparam = FALSE;
		switch (c = *(pp->str.addr + p_offset++))
		{
		case iop_exception:
			DEF_EXCEPTION(pp, p_offset, iod);
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
			FSTAT_CHECK(TRUE);
			mode &= ~(0x07);
			mode |= *(pp->str.addr + p_offset);
			break;
		case iop_g_protection:
			FSTAT_CHECK(TRUE);
			mode &= ~(0x07 << 3);
			mode |= *(pp->str.addr + p_offset) << 3;
			break;
		case iop_s_protection:
		case iop_o_protection:
			FSTAT_CHECK(TRUE);
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
			rm_ptr->read_only = TRUE;
			break;
		case iop_noreadonly:
			rm_ptr->read_only = FALSE;
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
				/* for sequential device in fixed M mode, recordsize defines initial width */
				if (dev_open != iod->state && rm_ptr->fixed && (!rm_ptr->fifo && !rm_ptr->is_pipe) &&
					(!IS_UTF_CHSET(iod->ichset) && !IS_UTF_CHSET(iod->ochset)))
					iod->width = recordsize;
			}
			break;
		case iop_outrewind:
			/* for a non-split device, OUTREWIND, INREWIND, and REWIND are equivalent.  For a split device
			 * like $PRINCIPAL INREWIND and REWIND operate on the input side and OUTREWIND operates on the
			 * output side.  "op_use" calls iorm_use() for both sides for $PRINCIPAL.  Since the input
			 * side of $PRINCIPAL is equal to io_std_device.in we break if OUTREWIND is the device parameter.
			 * A similar check is done on the output side if INREWIND or REWIND is the device parameter.
			 */
			if ((io_std_device.in == iod))
				break;
			outdevparam = TRUE;
		case iop_inrewind:
		case iop_rewind:
			if ((FALSE == outdevparam) && (io_std_device.out == iod))
				break;
			if (iod->state == dev_open && !rm_ptr->fifo && !rm_ptr->is_pipe)
			{
				if ((off_t)-1 == lseek(rm_ptr->fildes, 0, SEEK_SET))
				{
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_IOERROR, 7, RTS_ERROR_LITERAL("lseek"),
						RTS_ERROR_LITERAL("REWIND"), CALLFROM, errno);
				}
				/* need to do FSTAT_FILE to get file size if not already done */
				FSTAT_CHECK(FALSE);
				/* if file size is not zero then not at end of file */
				if (0 != statbuf.st_size)
					iod->dollar.zeof = FALSE;
				else
					iod->dollar.zeof = TRUE;
				iod->dollar.y = 0;
				iod->dollar.x = 0;
				rm_ptr->lastop = RM_NOOP;
				rm_ptr->done_1st_read = rm_ptr->done_1st_write = rm_ptr->crlast = FALSE;
				rm_ptr->out_bytes = rm_ptr->bom_buf_cnt = rm_ptr->bom_buf_off = 0;
				rm_ptr->inbuf_top = rm_ptr->inbuf_off = rm_ptr->inbuf_pos = rm_ptr->inbuf;
				rm_ptr->file_pos = 0;
				/* Reset temporary buffer so that the next read starts afresh */
				if (!rm_ptr->fixed || IS_UTF_CHSET(iod->ichset))
				{
					DEBUG_ONLY(MEMSET_IF_DEFINED(rm_ptr->tmp_buffer, 0, CHUNK_SIZE));
					rm_ptr->start_pos = 0;
					rm_ptr->tot_bytes_in_buffer = 0;
				}
				/* If encrypted, release and acquire the cipher handle once again. This way, we reset the encryption
				 * state to read the encrypted data. Any writes henceforth will issue error (NOTTOEOFOUTPUT) unless
				 * a truncate is also done following the rewind or reads take us to the end of the file.
				 */
				rm_ptr->read_occurred = FALSE;
				if (rm_ptr->input_encrypted)
					reset_input_encryption = TRUE;
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
			/* truncate if not a fifo and not a pipe and (it is a non-split device or this is the
			 * output side of a split device)
			 */
			if (!rm_ptr->fifo && !rm_ptr->is_pipe && ((iod->pair.in == iod->pair.out) || (io_std_device.out == iod)))
			{
				int ftruncate_res;
				int fc_res;
				FILE *newstr;
				int newfd;
				/* don't truncate unless it is a regular file */
				FSTAT_CHECK(TRUE);
				if (!(S_IFREG & mode))
					break;
				/* If encrypted, we cannot allow a truncate if the file pointer is not positioned at the beginning
				 * or the end of the file, unless there have been no writes.
				 */
				if ((0 != rm_ptr->file_pos) && (!iod->dollar.zeof)
					&& rm_ptr->output_encrypted && rm_ptr->write_occurred)
				{
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CRYPTNOTRUNC, 2,
						dev_name->len, dev_name->dollar_io);
				}
				/* If already open, truncate, close, and reopen the device. */
				if (dev_open == iod->state)
				{
					/* if last operation was a write then set file_pos to position after write */
					if (RM_WRITE == rm_ptr->lastop)
					{	/* need to do an lseek to get current location in file */
						cur_position = lseek(rm_ptr->fildes, 0, SEEK_CUR);
						if ((off_t)-1 == cur_position)
						{
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_IOERROR, 7,
								RTS_ERROR_LITERAL("lseek"),
								RTS_ERROR_LITERAL("TRUNCATE"), CALLFROM, errno);
						} else
							rm_ptr->file_pos = cur_position;
					}
					FTRUNCATE(rm_ptr->fildes, rm_ptr->file_pos, ftruncate_res);
					if (0 != ftruncate_res)
					{
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_IOERROR, 7,
							RTS_ERROR_LITERAL("ftruncate"),
							RTS_ERROR_LITERAL("TRUNCATE"), CALLFROM, errno);
					}
					/* the following is only necessary for a non-split device */
					if (iod->pair.in == iod->pair.out)
					{
						do
						{
							newfd = dup(rm_ptr->fildes);
						} while (-1 == newfd && EINTR == errno);
						if (-1 == newfd)
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
								RTS_ERROR_LITERAL("dup"), CALLFROM, save_errno);
						if (TRUE == rm_ptr->read_only)
						{
							FDOPEN(newstr,newfd,"r");
						} else if (TRUE == rm_ptr->write_only)
						{
							FDOPEN(newstr,newfd,"w");
						} else
							FDOPEN(newstr,newfd,"r+");
						CLOSE(rm_ptr->fildes,fc_res);
						FCLOSE(rm_ptr->filstr,fc_res);
						rm_ptr->filstr = newstr;
						rm_ptr->fildes = newfd;
					}
				}
				/* the following it only necessary for a non-split device */
				if (iod->pair.in == iod->pair.out)
				{
					if ((off_t)-1 == fseeko(rm_ptr->filstr, rm_ptr->file_pos, SEEK_SET))
					{
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_IOERROR, 7,
							RTS_ERROR_LITERAL("fseeko"),
							RTS_ERROR_LITERAL("TRUNCATE"), CALLFROM, errno);
					}
				}
				if ((off_t)-1 == lseek(rm_ptr->fildes, rm_ptr->file_pos, SEEK_SET))
				{
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_IOERROR, 7, RTS_ERROR_LITERAL("lseek"),
						RTS_ERROR_LITERAL("TRUNCATE"), CALLFROM, errno);
				}
				/* if not open then do the truncate here */
				if (dev_open != iod->state)
				{
					FTRUNCATE(rm_ptr->fildes, rm_ptr->file_pos, ftruncate_res);
					if (0 != ftruncate_res)
					{
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_IOERROR, 7,
							RTS_ERROR_LITERAL("ftruncate"),
							RTS_ERROR_LITERAL("TRUNCATE"), CALLFROM, errno);
					}
				}
				/* If we have truncated to the beginning of the file, we are starting afresh, so even a different
				 * KEY or IV may be specified (hence ...was_encrypted is being set to FALSE).
				 */
				if (0 == rm_ptr->file_pos)
				{
					rm_ptr->read_occurred = FALSE;
					rm_ptr->write_occurred = FALSE;
					if (rm_ptr->input_encrypted)
						reset_input_encryption = TRUE;
					if (rm_ptr->output_encrypted)
						reset_output_encryption = TRUE;
				}
				iod->dollar.zeof = TRUE;
				/* Reset temporary buffer so that the next read can start afresh */
				if (!rm_ptr->fixed || IS_UTF_CHSET(iod->ichset))
				{
					rm_ptr->inbuf_top = rm_ptr->inbuf_off = rm_ptr->inbuf_pos = rm_ptr->inbuf;
					if (RM_WRITE != rm_ptr->lastop && rm_ptr->fixed)
						rm_ptr->out_bytes = 0;
					rm_ptr->bom_buf_cnt = rm_ptr->bom_buf_off = 0;
					DEBUG_ONLY(MEMSET_IF_DEFINED(rm_ptr->tmp_buffer, 0, CHUNK_SIZE));
					rm_ptr->start_pos = 0;
					rm_ptr->tot_bytes_in_buffer = 0;
					if (RM_WRITE == rm_ptr->lastop && rm_ptr->fixed && iod->dollar.x == iod->width)
						iod->dollar.x = 0;
					if (0 == rm_ptr->file_pos)
					{
						/* If the truncate size is zero reset done_1st_read and done_1st_write
						 * to FALSE.
						 */
						rm_ptr->done_1st_read = rm_ptr->done_1st_write = FALSE;
						rm_ptr->bom_num_bytes = 0;
						rm_ptr->bom_checked = 0;
					}
				}
			}
			break;
		case iop_uic:
		{
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
				iorm_cond_wteol(iod);	/* Need to insert a newline if $X is non-zero. */
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
#				ifdef __MVS__
				if (CHSET_BINARY == iod->process_chset)
					break;
#				endif
			}
			iod->wrap = TRUE;
			seen_wrap = TRUE;	/* Don't allow WIDTH=0 to override WRAP */
			break;
		case iop_nowrap:
			iod->wrap = FALSE;
			break;
		case iop_ipchset:
		{
#			ifdef KEEP_zOS_EBCDIC
			if ((iconv_t)0 != iod->input_conv_cd)
			{
				ICONV_CLOSE_CD(iod->input_conv_cd);
			}
			SET_CODE_SET(iod->in_code_set, (char *)(pp->str.addr + p_offset + 1));
			if (DEFAULT_CODE_SET != iod->in_code_set)
				ICONV_OPEN_CD(iod->input_conv_cd, (char *)(pp->str.addr + p_offset + 1),
					INSIDE_CH_SET);
#			endif
			if (chset_allowed)
			{
				GET_ADDR_AND_LEN(chset_mstr.addr, chset_mstr.len);
				SET_ENCODING(temp_chset, &chset_mstr);
				if (!gtm_utf8_mode && IS_UTF_CHSET(temp_chset))
					break;	/* ignore UTF chsets if not utf8_mode */
				CHECK_UTF16_VARIANT_AND_SET_CHSET(rm_ptr->ichset_utf16_variant, iod->ichset, temp_chset);
				ichset_specified = TRUE;
			}
		}
		break;
		case iop_opchset:
		{
#			ifdef KEEP_zOS_EBCDIC
			if ((iconv_t) 0 != iod->output_conv_cd)
			{
				ICONV_CLOSE_CD(iod->output_conv_cd);
			}
			SET_CODE_SET(iod->out_code_set, (char *)(pp->str.addr + p_offset + 1));
			if (DEFAULT_CODE_SET != iod->out_code_set)
				ICONV_OPEN_CD(iod->output_conv_cd, INSIDE_CH_SET, (char *)(pp->str.addr + p_offset + 1));
#			endif
			if (chset_allowed)
			{
				GET_ADDR_AND_LEN(chset_mstr.addr, chset_mstr.len);
				SET_ENCODING(temp_chset, &chset_mstr);
				if (!gtm_utf8_mode && IS_UTF_CHSET(temp_chset))
					break;	/* ignore UTF chsets if not utf8_mode */
				CHECK_UTF16_VARIANT_AND_SET_CHSET(rm_ptr->ochset_utf16_variant, iod->ochset, temp_chset);
				ochset_specified = TRUE;
				if (gtm_utf8_mode && !IS_PADCHAR_VALID(iod->ochset, rm_ptr->padchar))
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(2) ERR_PADCHARINVALID, 0);
			}
		}
		break;
		case iop_chset:
		{
			if (chset_allowed)
			{
				GET_ADDR_AND_LEN(chset_mstr.addr, chset_mstr.len);
				SET_ENCODING(temp_chset, &chset_mstr);
				if (!gtm_utf8_mode && IS_UTF_CHSET(temp_chset))
					break;	/* ignore UTF chsets if not utf8_mode */
				CHECK_UTF16_VARIANT_AND_SET_CHSET(rm_ptr->ichset_utf16_variant, iod->ichset, temp_chset);
				CHECK_UTF16_VARIANT_AND_SET_CHSET(rm_ptr->ochset_utf16_variant, iod->ochset, temp_chset);
				if (gtm_utf8_mode && !IS_PADCHAR_VALID(iod->ochset, rm_ptr->padchar))
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(2) ERR_PADCHARINVALID, 0);
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
			if (!rm_ptr->fifo && !rm_ptr->is_pipe)
				rm_ptr->follow = TRUE;
			break;
		case iop_nofollow:
			if (!rm_ptr->fifo && !rm_ptr->is_pipe)
				rm_ptr->follow = FALSE;
			break;
		case iop_outseek:
			/* for a non-split device, OUTSEEK, INSEEK, and SEEK are equivalent.  For a split device
			 * like $PRINCIPAL INSEEK and SEEK operate on the input side and OUTSEEK operates on the
			 * output side.  "op_use" calls iorm_use() for both sides for $PRINCIPAL.  Since the input
			 * side of $PRINCIPAL is equal to io_std_device.in we break if OUTSEEK is the device parameter.
			 * A similar check is done on the output side if INSEEK or SEEK is the device parameter.
			 */
			if ((io_std_device.in == iod))
				break;
			outdevparam = TRUE;
		case iop_inseek:
		case iop_seek:
			if ((FALSE == outdevparam) && (io_std_device.out == iod))
				break;
			seek_specified = TRUE;
			if (!rm_ptr->fifo && !rm_ptr->is_pipe)
			{
				/* need to do FSTAT_FILE to get file size if not already done */
				FSTAT_CHECK(FALSE);
				/* if file size is not zero then process seek request */
				if (0 != statbuf.st_size)
				{
					/* if last operation was a write then set file_pos to position after write */
					if (RM_WRITE == rm_ptr->lastop)
					{
						/* need to do an lseek to get current location in file */
						cur_position = lseek(rm_ptr->fildes, 0, SEEK_CUR);
						if ((off_t)-1 == cur_position)
						{
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_IOERROR, 7,
								RTS_ERROR_LITERAL("lseek"),
								RTS_ERROR_LITERAL("SEEK"), CALLFROM, errno);
						} else
							rm_ptr->file_pos = cur_position;
					}
					seek_len = MIN(*(pp->str.addr + p_offset), (LIMIT_SEEK_STR - 1));
					/* if seek_len not greater than zero then it's an error so quit now */
					if (0 >= seek_len)
						OUTPUT_SDSEEKERR("\"\"");
					/* If BOM not already checked and not a fifo or pipe, not a reopen of file, is UTF mode,
					 * file is not empty, and not writeonly, and encryption is not in effect, then go to the
					 * beginning of the file and read the potential BOM.
					 */
					if ((!rm_ptr->bom_checked) && (!rm_ptr->no_destroy) && (!rm_ptr->fifo) && (!rm_ptr->is_pipe)
						&& (!rm_ptr->input_encrypted) && (2 < rm_ptr->fildes) && (0 < statbuf.st_size)
						&& IS_UTF_CHSET(iod->ichset) && !rm_ptr->write_only)
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
							if ((off_t)-1 == lseek(rm_ptr->fildes, 0, SEEK_SET))
								rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_IOERROR, 7,
									RTS_ERROR_LITERAL("lseek"),
									RTS_ERROR_LITERAL("SEEK"), CALLFROM, errno);
							rm_ptr->bom_num_bytes = open_get_bom(iod, bom_size_toread);
							/* move back to previous file position */
							if ((off_t)-1 == lseek(rm_ptr->fildes, rm_ptr->file_pos, SEEK_SET))
								rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_IOERROR, 7,
									RTS_ERROR_LITERAL("lseek"),
									RTS_ERROR_LITERAL("SEEK"), CALLFROM, errno);
						}
						rm_ptr->bom_checked = TRUE;
					}
					memcpy(seek_str, (char *)(pp->str.addr + p_offset + 1), seek_len);
					seek_str[seek_len] = '\0';
					seek_ptr = seek_str;
					if ('+' == *seek_str)
					{
						seek_type = SEEK_PLUS;
						seek_ptr++; /* skip sign */
					} else if ('-' == *seek_str)
					{
						seek_type = SEEK_MINUS;
						seek_ptr++; /* skip sign */
					} else
					{
						seek_type = SEEK_ABS;
					}
					/* require a digit as the next character to make sure 2 signs not entered */
					if (!ISDIGIT_ASCII(*seek_ptr))
						OUTPUT_SDSEEKERR(seek_str);
					errno = 0; /* not reset if previous failure */
					seek_value = STRTOLL(seek_ptr, &endptr, 10);
					if (ERANGE == errno)
						OUTPUT_SDSEEKERR(seek_str);
					if ('\0' != *endptr)
						OUTPUT_SDSEEKERR(seek_str);
					if (SEEK_MINUS == seek_type)
						seek_value = -seek_value;
					/* if fixed then convert record offset to byte offset in file */
					if (rm_ptr->fixed)
					{
						if (IS_UTF_CHSET(iod->ichset))
						{
							/* utf reads recordsize so if we only processed part of the record
							 * adjust it back to the beginning of the record.
							 */
							if (rm_ptr->inbuf_top - rm_ptr->inbuf_off)
							{
								current_offset = rm_ptr->file_pos - rm_ptr->recordsize;
							} else
								current_offset = rm_ptr->file_pos;
							if (!rm_ptr->bom_num_bytes)
								seek_value *= rm_ptr->recordsize; /* no bom to deal with */
							else
							{
								/* account for bom if absolute seek or relative positive seek
								 * and we are at the beginning of the file
								 */
								if ((seek_type == SEEK_ABS)
										|| ((0 == current_offset) && (0 < seek_value)))
									seek_value = seek_value * rm_ptr->recordsize
										+ rm_ptr->bom_num_bytes;
								else
								{
									if (0 == current_offset)
									{	/* zero or less seek from beginning of the file */
										seek_value = -1; /* new_position will be 0 below */
									} else
										seek_value *= rm_ptr->recordsize;
								}
							}
						} else
						{
							current_offset = (rm_ptr->file_pos / iod->width) * iod->width;
							/* Need to adjust current_offset so we land on a record boundary.
							 * If the current_offset is in the middle of the record then adjust
							 * it so that a relative +0 lands at the beginning of the record
							 */
							seek_value *= iod->width;
						}
					} else
						current_offset = rm_ptr->file_pos;
					if (seek_type == SEEK_ABS)
						new_position = seek_value;
					else
						new_position = current_offset + seek_value;
					/* limit the range of the new calculated position from zero to the file size */
					if (0 > new_position)
						new_position = 0;
					else if (statbuf.st_size < new_position)
						new_position = statbuf.st_size;
					if (-1 == lseek(rm_ptr->fildes, new_position, SEEK_SET))
					{
						save_errno = errno;
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_IOERROR, 7,
							RTS_ERROR_LITERAL("lseek"),
							RTS_ERROR_LITERAL("SEEK"), CALLFROM, save_errno);
					}
					if (statbuf.st_size == new_position) /* at end of file */
						iod->dollar.zeof = TRUE;
					else
					{
						iod->dollar.zeof = FALSE;
						if (0 == new_position) /* at beginning of file */
							rm_ptr->done_1st_read = rm_ptr->done_1st_write = rm_ptr->crlast = FALSE;
					}
					iod->dollar.y = 0;
					iod->dollar.x = 0;
					rm_ptr->lastop = RM_NOOP;
					rm_ptr->file_pos = new_position;
					/* Reset temporary buffer so that the next read starts afresh */
					if (!rm_ptr->fixed || IS_UTF_CHSET(iod->ichset))
					{
						/* if not at beginning of file then don't read BOM */
						rm_ptr->out_bytes = rm_ptr->bom_buf_cnt = rm_ptr->bom_buf_off = 0;
						rm_ptr->inbuf_top = rm_ptr->inbuf_off = rm_ptr->inbuf_pos = rm_ptr->inbuf;
						DEBUG_ONLY(MEMSET_IF_DEFINED(rm_ptr->tmp_buffer, 0, CHUNK_SIZE));
						rm_ptr->start_pos = 0;
						rm_ptr->tot_bytes_in_buffer = 0;
					}
				}
			}
			break;
		case iop_key:			/* CAUTION: fall-through */
		case iop_input_key:
			GET_KEY_AND_IV(input);
			if (iop_key != c)	/* CAUTION: potential fall-through */
				break;
		case iop_output_key:
			GET_KEY_AND_IV(output);
			break;
		case iop_buffered:
			disk_block_multiple = (int)*((unsigned char *)(pp->str.addr + p_offset + 1));
			/* Not enabled for stdout (initially) */
			if (1 == rm_ptr->fildes)
				break;
			assert(rm_ptr->filstr);
			/* No change, do nothing */
			if (rm_ptr->fsblock_buffer_size == disk_block_multiple)
				break;
			/* Simple buffering request - handles both enable and disable */
			if (2 > disk_block_multiple)
			{
				rm_ptr->fsblock_buffer_size = disk_block_multiple;
				if (NULL != rm_ptr->fsblock_buffer)
					free(rm_ptr->fsblock_buffer);
				break;
			}
			/* Request to buffer with buffer size different from before; clear out existing buffer */
			if (NULL != rm_ptr->fsblock_buffer)
				free(rm_ptr->fsblock_buffer);
			/* Grab the FS block size */
			fwrite_buffer_size = (size_t)get_fs_block_size(rm_ptr->fildes);
			fwrite_buffer_size = (size_t)(fwrite_buffer_size << disk_block_multiple);
			rm_ptr->fsblock_buffer = (char *)malloc(fwrite_buffer_size);
			if (setvbuf(rm_ptr->filstr, rm_ptr->fsblock_buffer, _IOFBF, fwrite_buffer_size))
			{	/* Non-fatal error. Continue to use buffered IO */
				free(rm_ptr->fsblock_buffer);
				rm_ptr->fsblock_buffer_size = 1;
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("setvbuff"), CALLFROM, errno);
				break;
			}
			rm_ptr->fsblock_buffer_size = fwrite_buffer_size;
			break;
		default:
			break;
		}
		p_offset += ((IOP_VAR_SIZE == io_params_size[c])
			? (unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[c]);
	}
	if (dev_open != iod->state || ichset_specified || ochset_specified)
	{
		if (dev_open != iod->state && !ichset_specified && !rm_ptr->no_destroy)
		{
#			ifdef __MVS__
			iod->is_ichset_default = TRUE;
#			endif
			iod->ichset = (gtm_utf8_mode) ? CHSET_UTF8 : CHSET_M;
		}
		if (dev_open != iod->state && !ochset_specified && !rm_ptr->no_destroy)
		{
#			ifdef __MVS__
			iod->is_ochset_default = TRUE;
#			endif
			iod->ochset = (gtm_utf8_mode) ? CHSET_UTF8 : CHSET_M;
		}
		if ((CHSET_M != iod->ichset) && (CHSET_UTF16 != iod->ichset) && (CHSET_MAX_IDX > iod->ichset))
			get_chset_desc(&chset_names[iod->ichset]);
		if ((CHSET_M != iod->ochset) && (CHSET_UTF16 != iod->ochset) && (CHSET_MAX_IDX > iod->ochset))
			get_chset_desc(&chset_names[iod->ochset]);
		/* If ICHSET or OCHSET is of type UTF-16, check that RECORDSIZE is even. */
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
	/* Now that recordsize and CHSET parms have been handled (if any), set WIDTH if necessary */
	if (rm_ptr->def_width && (recsize_before != rm_ptr->recordsize || def_recsize_before != rm_ptr->def_recsize))
	{	/* record size was specified even if the same value */
		SET_WIDTH_BYTES;
		assert(width_bytes <= rm_ptr->recordsize);	/* or else RECSIZENOTEVEN error would have been issued */
		iod->width = rm_ptr->recordsize;		/* this width will hold at least one character due to above check */
	}
	if (dev_closed == iod->state)
	{
		if (IS_UTF_CHSET(iod->ichset))
		{
			assert(gtm_utf8_mode);
			/* We shouldn't have an existing buffer at this point if not closed nodestroy */
			if (!rm_ptr->no_destroy)
				assert(NULL == rm_ptr->inbuf);
		}
		if (IS_UTF16_CHSET(iod->ochset))
		{
			/* We shouldn't have an existing buffer at this point if not closed nodestroy*/
			if (!rm_ptr->no_destroy)
				assert(NULL == rm_ptr->outbuf);
		}
	}
	/* Now adjust the buffer based on new CHSET & recordsize, If the file was already opened */
	if (dev_open == iod->state)
	{
		if ((!rm_ptr->fixed || IS_UTF_CHSET(iod->ichset)) && (recsize_before != rm_ptr->recordsize))
		{
			if (NULL != rm_ptr->inbuf)
			{
				free(rm_ptr->inbuf);
				rm_ptr->inbuf = NULL;
			}
		}
		if (IS_UTF16_CHSET(iod->ochset) && (recsize_before != rm_ptr->recordsize))
		{
			if (NULL != rm_ptr->outbuf)
			{
				free(rm_ptr->outbuf);
				rm_ptr->outbuf=NULL;
			}
		}
	}
	/* Allocate the buffers in case it is UTF mode and not already allocated. */
	if ((NULL == rm_ptr->inbuf) && IS_UTF_CHSET(iod->ichset))
	{
		rm_ptr->bufsize = rm_ptr->fixed ? (rm_ptr->recordsize + 4) : 20;
		rm_ptr->inbuf = malloc(rm_ptr->bufsize);
		rm_ptr->inbuf_pos = rm_ptr->inbuf_top = rm_ptr->inbuf_off = rm_ptr->inbuf;
	}
	if ((NULL == rm_ptr->tmp_buffer) && (IS_UTF_CHSET(iod->ichset) || !rm_ptr->fixed))
		rm_ptr->tmp_buffer = malloc(CHUNK_SIZE);
	if ((NULL == rm_ptr->outbuf) && IS_UTF16_CHSET(iod->ochset))
	{
		rm_ptr->outbufsize = rm_ptr->recordsize + 4;
		rm_ptr->outbuf = malloc(rm_ptr->outbufsize);
		rm_ptr->out_bytes = 0;
	}
	assert(((!rm_ptr->input_encrypted) && (!rm_ptr->output_encrypted)) || (dev_open == iod->state) || rm_ptr->no_destroy);
	assert((!rm_ptr->input_encrypted) || (0 != rm_ptr->input_key.len));
	assert((!rm_ptr->output_encrypted) || (0 != rm_ptr->output_key.len));
	if (rm_ptr->input_encrypted)
	{	/* Device's input already encrypted. */
		if (input_key_entry_present)
		{	/* Input KEY deviceparameter is present. */
			if (input_key_not_empty)
			{	/* Input KEY is meaningful. */
				if (KEY_CHANGED(input, input_key, rm_ptr) || IV_CHANGED(input, input_iv, rm_ptr))
				{	/* Requested a new IV or KEY; only allow if no reads have happened. */
					if (rm_ptr->read_occurred)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CRYPTNOOVERRIDE);
					else
						reset_input_encryption = init_input_encryption = TRUE;
				}
			} else
			{	/* Encryption requested to be turned off; only allow if there have not been any reads. Also,
				 * a non-empty IV cannot be specified at this time.
				 */
				if (rm_ptr->read_occurred)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CRYPTNOOVERRIDE);
				else if (0 != input_iv.len)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CRYPTNOKEYSPEC);
				else
				{
					rm_ptr->input_encrypted = FALSE;
					reset_input_encryption = init_input_encryption = FALSE;
					if (GTMCRYPT_INVALID_KEY_HANDLE != rm_ptr->input_cipher_handle)
					{
						GTMCRYPT_REMOVE_CIPHER_CONTEXT(rm_ptr->input_cipher_handle, rv);
						if (0 != rv)
							GTMCRYPT_REPORT_ERROR(rv, rts_error, dev_name->len, dev_name->dollar_io);
					}
				}
			}
		}
	} else
	{	/* Device's input is not yet encrypted. */
		if (input_key_entry_present)
		{	/* Only initialize encryption if the input key is not empty. */
			if (input_key_not_empty)
			{	/* Initialize encryption unless some unencrypted reads have occurred. */
				if (rm_ptr->read_occurred)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CRYPTNOOVERRIDE);
				else
					init_input_encryption = TRUE;
			} else if (0 != input_iv.len)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CRYPTNOKEYSPEC);
		}
	}
	if (rm_ptr->output_encrypted)
	{	/* Device's output already encrypted. */
		if (output_key_entry_present)
		{	/* Input KEY deviceparameter is present. */
			if (output_key_not_empty)
			{	/* Output KEY is meaningful. */
				if (KEY_CHANGED(output, output_key, rm_ptr) || IV_CHANGED(output, output_iv, rm_ptr))
				{	/* Requested a new IV or KEY; only allow if no writes have happened. */
					if (rm_ptr->write_occurred)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CRYPTNOOVERRIDE);
					else
						reset_output_encryption = init_output_encryption = TRUE;
				}
			} else
			{	/* Encryption requested to be turned off; only allow if there have not been any writes. Also,
				 * a non-empty IV cannot be specified at this time.
				 */
				if (rm_ptr->write_occurred)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CRYPTNOOVERRIDE);
				else if (0 != output_iv.len)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CRYPTNOKEYSPEC);
				else
				{
					rm_ptr->output_encrypted = FALSE;
					reset_output_encryption = init_output_encryption = FALSE;
					if (GTMCRYPT_INVALID_KEY_HANDLE != rm_ptr->output_cipher_handle)
					{
						GTMCRYPT_REMOVE_CIPHER_CONTEXT(rm_ptr->output_cipher_handle, rv);
						if (0 != rv)
							GTMCRYPT_REPORT_ERROR(rv, rts_error, dev_name->len, dev_name->dollar_io);
					}
				}
			}
		}
	} else
	{	/* Device's output is not yet encrypted. */
		if (output_key_entry_present)
		{	/* Only initialize encryption if the output key is not empty. */
			if (output_key_not_empty)
			{	/* Initialize encryption unless some unencrypted writes have occurred. */
				if (rm_ptr->write_occurred)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CRYPTNOOVERRIDE);
				else
					init_output_encryption = TRUE;
			} else if (0 != output_iv.len)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CRYPTNOKEYSPEC);
		}
	}
	assert((!reset_input_encryption) || rm_ptr->input_encrypted);
	assert((!reset_output_encryption) || rm_ptr->output_encrypted);
	if ((rm_ptr->input_encrypted || init_input_encryption || reset_input_encryption
			|| rm_ptr->output_encrypted || init_output_encryption || reset_output_encryption) && seek_specified)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CRYPTNOSEEK, 2, dev_name->len, dev_name->dollar_io);
	if (init_input_encryption || init_output_encryption)
	{	/* First time the device is getting encryption turned on. Initialize encryption and setup the keys. */
		INIT_PROC_ENCRYPTION(NULL, rv);
		if (0 != rv)
			GTMCRYPT_REPORT_ERROR(rv, rts_error, dev_name->len, dev_name->dollar_io);
	}
	if (reset_input_encryption && (GTMCRYPT_INVALID_KEY_HANDLE != rm_ptr->input_cipher_handle))
	{
		GTMCRYPT_REMOVE_CIPHER_CONTEXT(rm_ptr->input_cipher_handle, rv);
		if (0 != rv)
			GTMCRYPT_REPORT_ERROR(rv, rts_error, dev_name->len, dev_name->dollar_io);
	}
	if (init_input_encryption)
	{	/* Get the key handle corresponding to the keyname provided. */
		INIT_CIPHER_CONTEXT(GTMCRYPT_OP_DECRYPT, input_key, input_iv, rm_ptr->input_cipher_handle, dev_name);
		rm_ptr->input_encrypted = TRUE;
		rm_ptr->input_key.addr = input_key.addr;
		rm_ptr->input_key.len = input_key.len;
		rm_ptr->input_iv.addr = input_iv.addr;
		rm_ptr->input_iv.len = input_iv.len;
		s2pool(&rm_ptr->input_key);
		s2pool(&rm_ptr->input_iv);
	} else if (reset_input_encryption)
	{
		INIT_CIPHER_CONTEXT(GTMCRYPT_OP_DECRYPT, rm_ptr->input_key, rm_ptr->input_iv,
			rm_ptr->input_cipher_handle, dev_name);
	}
	if (reset_output_encryption && (GTMCRYPT_INVALID_KEY_HANDLE != rm_ptr->output_cipher_handle))
	{
		GTMCRYPT_REMOVE_CIPHER_CONTEXT(rm_ptr->output_cipher_handle, rv);
		if (0 != rv)
			GTMCRYPT_REPORT_ERROR(rv, rts_error, dev_name->len, dev_name->dollar_io);
	}
	if (init_output_encryption)
	{	/* Get the key handle corresponding to the keyname provided. */
		INIT_CIPHER_CONTEXT(GTMCRYPT_OP_ENCRYPT, output_key, output_iv, rm_ptr->output_cipher_handle, dev_name);
		rm_ptr->output_encrypted = TRUE;
		rm_ptr->output_key.addr = output_key.addr;
		rm_ptr->output_key.len = output_key.len;
		rm_ptr->output_iv.addr = output_iv.addr;
		rm_ptr->output_iv.len = output_iv.len;
		s2pool(&rm_ptr->output_key);
		s2pool(&rm_ptr->output_iv);
	} else if (reset_output_encryption)
	{
		INIT_CIPHER_CONTEXT(GTMCRYPT_OP_ENCRYPT, rm_ptr->output_key, rm_ptr->output_iv,
			rm_ptr->output_cipher_handle, dev_name);
	}
	if (init_input_encryption || reset_input_encryption || init_output_encryption || reset_output_encryption)
	{	/* Setup an encryption private buffer of size equal to record size. */
		REALLOC_CRYPTBUF_IF_NEEDED(rm_ptr->recordsize);
	}
	if (get_mode_done && (mode != mode1))
	{	/* if the mode has been changed by the qualifiers, reset it */
		if (-1 == CHMOD(iod->trans_name->dollar_io, mode))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) errno);
	}
	REVERT_GTMIO_CH(&iod->pair, ch_set);
	return;
}
