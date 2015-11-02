/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef UTIL_included
#define UTIL_included

boolean_t util_is_log_open(void);

#ifdef VMS
#include <descrip.h>

/* defines for util_out_send_oper in util_output.c */

#define SNDOPR_TRIES 3
#define SNDOPR_DELAY 10
#define GTMOPCOMMISSED1 "%GTM-I-OPCOMMISSED "
#define GTMOPCOMMISSED2 " errors and "
#define GTMOPCOMMISSED3 " MBFULLs sending prior operator messages"

/* While the maximum for OPER_LOG_SIZE is 1906 (found from experimentation), we
   set it's maximum to 1904 being the largest 4 byte aligned size we can use. This
   4 byte aligned size is necessary to make the header length calculation done as
   SIZEOF(oper) - SIZEOF(oper.text) work correctly. If a size is used that is not
   4 byte aligned, this calculation will incorrectly contain the compiler pad chars
   causing garbage at the end of operator log lines. SE 9/2001
*/
#define	OPER_LOG_SIZE 1904

typedef struct
{
	unsigned int	req_code : 8;
	unsigned int	target   : 24;
	uint4		mess_code;
	char		text[OPER_LOG_SIZE];
} oper_msg_struct;

void util_in_open(struct dsc$descriptor_s *file_prompt);
void util_out_open(struct dsc$descriptor_s *file_prompt);
void util_log_open(char *filename, uint4 len);
void util_out_write(unsigned char *addr, unsigned int len);
#else /* UNIX */
#include "gtm_stdio.h"		/* for FILE * */
void util_in_open(void *);
char *util_input(char *buffer, int buffersize, FILE *fp, boolean_t remove_leading_spaces);
void util_out_print_gtmio(caddr_t message, int flush, ...);
#ifdef DEBUG
void		util_out_syslog_dump(void);

#define		UTIL_OUT_SYSLOG_INTERVAL 50
#endif
#endif

/* This value determines how many levels of nesting are allowed for protection of util_outbuff. */
#define UTIL_OUTBUFF_STACK_SIZE	3

/* Save the current va_list and repoint util_outbuff_ptr to the next chunk of util_outbuff,
 * so that if an interrupt handler invokes util_out_print, the buffer under construction is
 * left intact. In theory, we should not need more than two buffer chunks, but in pro we
 * will allow to utilize a third one just to provide more protection against overwrites of
 * under-construction messages.
 */
#define SAVE_UTIL_OUT_BUFFER(UTIL_OUT_SAVE_PTR, VA_LIST_SAVE_PTR, COPY_SAVED)		\
{											\
	if (TREF(util_outbuff_ptr) < (TADR(util_outbuff) + 				\
	    OUT_BUFF_SIZE * (UTIL_OUTBUFF_STACK_SIZE - 1)))				\
	{										\
		(TREF(util_outbuff_ptr)) += OUT_BUFF_SIZE;				\
		VAR_COPY(VA_LIST_SAVE_PTR, TREF(last_va_list_ptr));			\
		UTIL_OUT_SAVE_PTR = TREF(util_outptr);					\
		TREF(util_outptr) = NULL;						\
		COPY_SAVED = TRUE;							\
	} else										\
		assert(FALSE);								\
}

/* Repoint util_outbuff_ptr to the previous chunk of util_outbuff, so that the construction
 * of the buffer that was saved there could be finished safely, and also restore the
 * corresponding va_list.
 */
#define RESTORE_UTIL_OUT_BUFFER(UTIL_OUT_SAVE_PTR, VA_LIST_SAVE_PTR, COPY_SAVED)	\
{											\
	if (COPY_SAVED)									\
	{										\
		assert(TREF(util_outbuff_ptr) > TADR(util_outbuff));			\
		(TREF(util_outbuff_ptr)) -= OUT_BUFF_SIZE;				\
		TREF(util_outptr) = UTIL_OUT_SAVE_PTR;					\
		VAR_COPY(TREF(last_va_list_ptr), VA_LIST_SAVE_PTR);			\
		COPY_SAVED = FALSE;							\
	}										\
}

#define OUT_BUFF_SIZE	2048
#define	NOFLUSH		0
#define FLUSH		1
#define RESET		2
#define OPER		4
#define SPRINT		5
#define HEX8		8
#define HEX16		16

void util_exit_handler(void);
void util_out_close(void);
void util_out_send_oper(char *addr, unsigned int len);
void util_out_print(caddr_t message, int flush, ...);

#include "cmidef.h"	/* for clb_struct */
void util_cm_print(clb_struct *lnk, int code, char *message, int flush, ...);

#endif /* UTIL_included */
