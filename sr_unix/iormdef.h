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

#ifndef IORMDEF_H
#define IORMDEF_H

#include "gtmcrypt.h"		/* For gtmcrypt_key_t below. */

#define DEF_RM_WIDTH		32767
#define DEF_RM_RECORDSIZE	32767
#define DEF_RM_LENGTH		66
#define CHUNK_SIZE		BUFSIZ

#define	ONE_COMMA_UNAVAILABLE	"1,Resource temporarily unavailable"
#define ONE_COMMA_DEV_DET_EOF	"1,Device detected EOF"
#define	ONE_COMMA_DEV_DET_EOF_DOLLARDEVICE	"1,Device detected EOF ..... Now just exceed this line to more than DD_BUFLEN (80)."
#define	ONE_COMMA_CRYPTBADWRTPOS	"1,Encrypted WRITE disallowed from a position different than where the last WRITE completed"

#define	DEF_RM_PADCHAR		' '	/* SPACE */

/*	#define DEBUG_PIPE	*/

#ifdef DEBUG_PIPE
#define PIPE_DEBUG(X) X
#define DEBUGPIPEFLUSH FFLUSH(stdout)
int pid;
#else
# define PIPE_DEBUG(X)
#define DEBUGPIPEFLUSH
#endif

#ifdef UNICODE_SUPPORTED
#define SET_WIDTH_BYTES				\
	width_bytes = 1;			\
	width_chset = iod->ichset;		\
	switch (iod->ichset)			\
	{					\
	case CHSET_UTF16:			\
	case CHSET_UTF16BE:			\
	case CHSET_UTF16LE:			\
		width_bytes = 2;		\
		width_chset = iod->ichset;	\
	default:				\
		break;				\
	}					\
	switch (iod->ochset)			\
	{					\
	case CHSET_UTF16:			\
	case CHSET_UTF16BE:			\
	case CHSET_UTF16LE:			\
		width_bytes = 2;		\
		width_chset = iod->ochset;	\
	default:				\
		break;				\
	}
#else
#define SET_WIDTH_BYTES	width_bytes = 1;
#endif

error_def(ERR_CLOSEFAIL);
error_def(ERR_CRYPTBADWRTPOS);

#define MEMSET_IF_DEFINED(BUFFER, CHAR, SIZE)				\
{									\
	if (NULL != BUFFER)						\
		memset(BUFFER, CHAR, SIZE);				\
}

#define	IORM_FCLOSE(D_RM, FILDES, FILSTR)								\
{													\
	GBLREF int	process_exiting;								\
													\
	int		fclose_res, rc, save_fd;							\
													\
	if (NULL != D_RM->FILSTR)									\
	{	/* Since FCLOSE also closes the fd, reset FILDES (no need to close it separately). */	\
		LINUX_ONLY(assert(D_RM->FILDES == D_RM->FILSTR->_fileno);)				\
		if (!process_exiting)									\
		{	/* Only do the actual FCLOSE() if the process is not exiting because a) the OS	\
			 * takes care of opened file descriptors anyway; and b) we might have received	\
			 * a deadly signal, such as SIGTERM, while in getc(), which can ultimately lead	\
			 * to a hang on FCLOSE().							\
			 */										\
			FCLOSE(D_RM->FILSTR, fclose_res);						\
			if (0 != fclose_res)								\
			{										\
				save_fd = D_RM->FILDES;							\
				rc = errno;								\
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CLOSEFAIL, 1, save_fd, rc);\
			}										\
		}											\
		D_RM->FILSTR = NULL;									\
		D_RM->FILDES = FD_INVALID;								\
	} else if (FD_INVALID != D_RM->FILDES)								\
	{												\
		save_fd = D_RM->FILDES;									\
		CLOSEFILE_RESET(D_RM->FILDES, rc);	/* resets "D_RM->FILDES" to FD_INVALID */	\
		if (0 != rc)										\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CLOSEFAIL, 1, save_fd, rc);	\
	}												\
}

#define READ_ENCRYPTED_DATA(DEVICE, NAME, INBUF, INBUF_LEN, OUTBUF)					\
{													\
	LITREF gtm_string_t	null_iv;								\
													\
	int			rv;									\
													\
	if (INBUF_LEN > 0)										\
	{												\
		GTMCRYPT_DECRYPT_CONT_IV(NULL, (DEVICE)->input_cipher_handle,				\
				INBUF, INBUF_LEN, OUTBUF, rv);						\
		if (0 != rv)										\
			GTMCRYPT_REPORT_ERROR(rv, rts_error, (NAME)->len, (NAME)->dollar_io);		\
	}												\
}

#define WRITE_ENCRYPTED_DATA(DEVICE, NAME, INBUF, INBUF_LEN, OUTBUF)					\
{													\
	LITREF gtm_string_t	null_iv;								\
													\
	int			rv;									\
													\
	if (INBUF_LEN > 0)										\
	{												\
		GTMCRYPT_ENCRYPT_CONT_IV(NULL, (DEVICE)->output_cipher_handle,				\
				INBUF, INBUF_LEN, OUTBUF, rv);						\
		if (0 != rv)										\
			GTMCRYPT_REPORT_ERROR(rv, rts_error, (NAME)->len, (NAME)->dollar_io);		\
	}												\
}

/* Set prin_out_dev_failure if a write failed on the principal device. If it is a recurrence, issue the
 * NOPRINCIO error.
 */
#define ISSUE_NOPRINCIO_IF_NEEDED_RM(VAR, CMP_SIGN, IOD)						\
{													\
	GBLREF io_pair		io_std_device;								\
	GBLREF bool		prin_out_dev_failure;							\
													\
	int			write_status;								\
													\
	if (0 CMP_SIGN VAR)										\
	{	/* Set prin_out_dev_failure to FALSE if it was set TRUE earlier but is working now. */	\
		if (IOD == io_std_device.out)								\
			prin_out_dev_failure = FALSE;							\
	} else												\
	{												\
		write_status = (-1 == VAR) ? errno : VAR;						\
		if (IOD == io_std_device.out)								\
		{											\
			if (!prin_out_dev_failure)							\
				prin_out_dev_failure = TRUE;						\
			else										\
			{										\
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOPRINCIO);			\
				stop_image_no_core();							\
			}										\
		}											\
		if (EAGAIN == write_status)								\
			SET_DOLLARDEVICE_ERRSTR(IOD, ONE_COMMA_UNAVAILABLE);		\
		else											\
			SET_DOLLARDEVICE_ONECOMMA_STRERROR(IOD, write_status);		\
		IOD->dollar.za = 9;									\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) write_status);					\
	}												\
}

/* Operations for this device type */
#define RM_NOOP		0
#define RM_WRITE	1
#define RM_READ		2

enum pipe_which		/* which module saved the interrupted info */
{
	pipewhich_invalid,
	pipewhich_readfl,
};

/* ***************************************************** */
/* *********** structure for RMS driver **************** */
/* ***************************************************** */

/* The Dev_param_pair structure may exist for any device parameter whose definition is a character string, */
/* but it is being added here primarily for the pipe device type. */
/* For instance, a pipe will contain a "command" device parameter like command="/usr/bin/cat" */
/* The name field will be initialized to "COMMAND=" and the definition field will be "/usr/bin/cat" */
/* These will be output when the type is fifo.  Other device types may be modified as desired */

typedef struct dev_param_pair{
	char *name;
	char *definition;
} Dev_param_pair;

typedef	struct dev_pairs {
	int num_pairs;
	Dev_param_pair pairs[3];
} Dev_param_pairs;

typedef struct pipe_interrupt_type
{
	ABS_TIME		end_time;
	enum pipe_which		who_saved;
	int			max_bufflen;
	int			bytes_read;
	int			bytes2read;
	int			char_count;
	int			bytes_count;
	int			add_bytes;
	boolean_t               end_time_valid;
	struct d_rm_struct	*newpipe;
} pipe_interrupt;

typedef struct
{
	boolean_t	fixed;		/* Fixed format file */
	boolean_t	read_only;	/* READONLY specified */
	boolean_t	write_only;	/* WRITEONLY specified */
	boolean_t	stream;
	boolean_t	fifo;
	boolean_t	is_pipe;	/* True if pipe device */
	boolean_t	independent; 	/* True if pipe process to live after pipe is closed */
	boolean_t	parse;		/* True if pipe command is to be parsed */
	boolean_t	done_1st_read;	/* If UTF16, we need to know if this is the first read or not to check for BOM */
	boolean_t	done_1st_write;	/* If UTF16, we need to know if this is the first write or not to check for BOM */
	boolean_t	crlast;		/* If not M, the last character seen was a CR so if LF is next, ignore it */
	boolean_t	def_width;	/* WIDTH has not been changed */
	boolean_t	def_recsize;	/* RECORDSIZE has not been changed */
	boolean_t	bom_read_one_done;	/* If pipe/fifo and UTF8, read one byte to check for bom if not set */
	boolean_t	follow;	/* True if disk read with follow - similar to tail -f on a file */
	boolean_t	no_destroy;	/* true if disk and NO_DESTROY on CLOSE and filedes >= 2*/
	boolean_t	bom_checked;	/* If disk and UTF8 the bom has been read and bom_num_bytes has been set */
	pipe_interrupt	pipe_save_state;	/* Saved state of interrupted IO */
	boolean_t	mupintr;	/* We were mupip interrupted */
	unsigned int	lastop;		/* Last operation done on file */
	int		fildes;		/* File descriptor */
	int		read_fildes;	/* Read file descriptor if it's a pipe and stdout is returned */
	FILE		*read_filstr;	/* Read FILE pointer if it's a pipe and stdout is returned */
	struct	io_desc_struct	*stderr_child;	/* pointer to io descriptor for pipe stderr device */
	struct	io_desc_struct	*stderr_parent;	/* pointer to io descriptor for pipe which opened stderr device */
	pid_t		pipe_pid;		/* child process id for reaping on close */
	Dev_param_pairs dev_param_pairs;
	int		bufsize;	/* Size of inbuf */
	int		outbufsize;	/* Size of outbuf */
	int4		recordsize;	/* Size of record in bytes */
	int4		padchar;	/* Character to use for padding */
	int4		fol_bytes_read;	/* Number of bytes read from current record in utf fix mode with follow */
	int4		last_was_timeout;	/* last read in utf fix mode with follow was a timeout */
	int4		orig_bytes_already_read;	/* bytes_already_read from a previous read
							   in utf fix mode with follow */
	int		out_bytes;	/* Number of bytes output for this fixed record */
	uint4		bom_buf_cnt;	/* Count of bytes in BOM buffer */
	uint4		bom_buf_off;	/* Next available byte in BOM buffer */
	uint4		bom_num_bytes;	/* number of bom bytes read */
	unsigned char	bom_buf[4];	/* Buffer area for BOM assembly */
	unsigned char	*inbuf;		/* Input buffer area */
	unsigned char	*inbuf_pos;	/* Put next char in inbuf here */
	unsigned char	*inbuf_off;	/* Next char to take from inbuf */
	unsigned char	*inbuf_top;	/* Last char (+1) in inbuf */
	unsigned char	*outbuf;	/* Output buffer area */
	FILE		*filstr;
	off_t		file_pos;
	long		pipe_buff_size;
	char		*tmp_buffer;			/* Buffer to store CHUNK_SIZE bytes */
	int		tot_bytes_in_buffer;		/* Number of bytes read from device, it refers tmp_buffer buffer */
	int		start_pos;			/* Current position in tmp_buffer */
	boolean_t	write_occurred;			/* Flag indicating whether a write has occurred on this device. */
	boolean_t	read_occurred;			/* Flag indicating whether a read has occurred on this device. */
	boolean_t	input_encrypted;		/* Whether this device's input stream is encrypted or not. */
	boolean_t	output_encrypted;		/* Whether this device's output stream is encrypted or not. */
	mstr		input_iv;			/* Input Initialization Vector for this device's encryption. */
	mstr		output_iv;			/* Output Initialization Vector for this device's encryption. */
	mstr		input_key;			/* Name that maps to an input encryption key on disk. */
	mstr		output_key;			/* Name that maps to an output encryption key on disk. */
	gtmcrypt_key_t	input_cipher_handle;		/* Encryption cipher handle for this device. */
	gtmcrypt_key_t	output_cipher_handle;		/* Decryption cipher handle for this device. */
	gtm_chset_t	ichset_utf16_variant;		/* Used to determine UTF16 variant when CHSET is changed b/w UTF16 & M. */
	gtm_chset_t	ochset_utf16_variant;		/* Used to determine UTF16 variant when CHSET is changed b/w UTF16 & M. */
	uint4		fsblock_buffer_size;		/* I/O buffer size; 1 == default size; 0 == no buffering */
	char		*fsblock_buffer;		/* I/O buffer for, erm, buffered I/O */
	boolean_t	crlastbuff;			/* Whether CR was last char of the buffer */
} d_rm_struct;	/*  rms		*/

#ifdef KEEP_zOS_EBCDIC
#define NATIVE_NL	0x15		/* EBCDIC */
#else
#define NATIVE_NL	'\n'		/* ASCII */
#endif

int gtm_utf_bomcheck(io_desc *iod, gtm_chset_t *chset, unsigned char *buffer, int len);
void iorm_cond_wteol(io_desc *iod);
int iorm_get_bom(io_desc *io_ptr, int *blocked_in, boolean_t ispipe, int flags, int4 *tot_bytes_read,
		 TID timer_id, int4 *msec_timeout, boolean_t colon_zero);
int iorm_get_bom_fol(io_desc *io_ptr, int4 *tot_bytes_read, int4 *msec_timeout, boolean_t timed,
		     boolean_t *bom_timeout, ABS_TIME end_time);
int iorm_get_fol(io_desc *io_ptr, int4 *tot_bytes_read, int4 *msec_timeout, boolean_t timed, boolean_t zint_restart,
		 boolean_t *follow_timeout, ABS_TIME end_time);
int iorm_get(io_desc *io_ptr, int *blocked_in, boolean_t ispipe, int flags, int4 *tot_bytes_read,
	     TID timer_id, int4 *msec_timeout, boolean_t colon_zero, boolean_t zint_restart);
int iorm_write_utf_ascii(io_desc *iod, char *string, int len);
void iorm_write_utf(mstr *v);
void iorm_readfl_badchar(mval *vmvalptr, int datalen, int delimlen, unsigned char *delimptr, unsigned char *strend);
int open_get_bom(io_desc *io_ptr, int bom_size);
int open_get_bom2(io_desc *io_ptr, int max_bom_size );

#endif
