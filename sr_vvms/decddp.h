/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef DECDDP_H_INCLUDED
#define DECDDP_H_INCLUDED

#define INIT_DESCRIP(X,A) ((X).dsc$a_pointer = (A), (X).dsc$b_dtype = DSC$K_DTYPE_T, (X).dsc$b_class = DSC$K_CLASS_S)
#define DESCRIP_LENGTH(X,A) ((X).dsc$w_length = (char *)(A) - (X).dsc$a_pointer)

condition_code	decddp_init(void);
condition_code	decddp_start_ast(void);
void		decddp_ast_handler(void);
boolean_t	dcp_get_input_buffer(struct in_buffer_struct *input_buffer, size_t inbuflen);
void		decddp_shdr(unsigned char trancode, short jobno, unsigned short remote_circuit, short return_jobno,
				unsigned char msgno, unsigned char *etheraddr);
void		decddp_set_etheraddr(unsigned char *adr);
unsigned char	*decddp_s5bit(unsigned char *cp);
unsigned char	*decddp_s5asc(unsigned short fivebit);
unsigned char	*decddp_s7bit(unsigned char *cp);
unsigned char	*decddp_s8bit(unsigned char *cp);
void		decddp_putbyte(unsigned char ch);
condition_code	decddp_send(void);
condition_code	dcp_send_message(unsigned char *buffer, int length, struct iosb_struct *iosbp);
void		decddp_sinit(unsigned char *response_code);
void		decddp_log_error(condition_code status, char *message_text, unsigned short *source_circuit,
				 unsigned short *source_job);
void		decddp_s8bit_counted(char *cp, int len);
char 		*ddp_db_op(struct in_buffer_struct *bp, condition_code (*f)(), unsigned char *addr, int len);
char 		*ddp_lock_op(struct in_buffer_struct *bptr, condition_code (*f)(), int unlock_code);
condition_code	dcp_get_circuit(mval *logical);
condition_code	dcp_get_volsets(void);
condition_code	dcp_get_groups(void);
condition_code	dcp_get_maxrecsize(void);
unsigned char	*decddp_xmtbuf(void);

#endif /* DECDDP_H_INCLUDED */
