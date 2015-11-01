/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef CLIIF_included
#define CLIIF_included

boolean_t cli_get_hex(char *entry, int4 *dst);
boolean_t cli_get_int(char *entry, int *dst);
boolean_t cli_get_num(char *entry, int4 *dst);
boolean_t cli_get_str(char *entry, char *dst, unsigned short *max_len);
bool cli_get_str_ele_upper(char *inbuff, char *dst, unsigned short *dst_len);
boolean_t cli_get_time(char *entry, uint4 *dst);
bool cli_get_value(char *entry, char val_buf[]);
boolean_t cli_negated(char *entry);
int4 cli_t_f_n(char *entry);
int cli_get_string_token(int *eof);
int cli_gettoken(int *eof);
int cli_is_assign(char *p);
int cli_is_dcm(char *p);
int cli_is_hex(char *p);
int cli_is_qualif(char *p);
int cli_look_next_string_token(int *eof);
int cli_look_next_token(int *eof);
int cli_present(char *entry);		/***type int added***/
void cli_str_setup(int length, char *addr);
void cli_strupper(char *sp);
#ifdef __osf__
	/* N.B. argv is passed in from main (in gtm.c) almost straight from the operating system.  */
#pragma pointer_size (save)
#pragma pointer_size (long)
#endif
void    cli_lex_setup (int argc, char *argv[]);
#ifdef __osf__
#pragma pointer_size (restore)
#endif

#endif /* CLIIF_included */
