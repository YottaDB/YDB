/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef CLIIF_included
#define CLIIF_included

boolean_t	cli_get_hex(char *entry, uint4 *dst);
boolean_t	cli_get_int(char *entry, int4 *dst);
boolean_t	cli_get_hex64(char *entry, gtm_uint64_t *dst);
boolean_t	cli_get_uint64(char *entry, gtm_uint64_t *dst);
boolean_t	cli_get_int64(char *entry, gtm_int64_t *dst);
boolean_t	cli_get_num(char *entry, int4 *dst);
boolean_t	cli_get_num64(char *entry, gtm_int64_t *dst);
boolean_t	cli_get_str(char *entry, char *dst, unsigned short *max_len);
bool		cli_get_str_ele(char *inbuff, char *dst, unsigned short *dst_len, boolean_t upper_case);
boolean_t	cli_get_time(char *entry, uint4 *dst);
bool		cli_get_value(char *entry, char val_buf[]);
boolean_t	cli_negated(char *entry);
boolean_t	cli_str_to_hex(char *str, uint4 *dst);
boolean_t	cli_str_to_hex64(char *str, gtm_uint64_t *dst);
boolean_t	cli_str_to_uint64(char *str, gtm_uint64_t *dst);
boolean_t	cli_str_to_int(char *str, int4 *dst);
boolean_t	cli_str_to_int64(char *str, gtm_int64_t *dst);
boolean_t	cli_str_to_num(char *str, int4 *dst);
boolean_t	cli_str_to_num64(char *str, gtm_int64_t *dst);
int4		cli_t_f_n(char *entry);
int4		cli_n_a_e(char *entry);
int		cli_get_string_token(int *eof);
int		cli_gettoken(int *eof);
int		cli_is_assign(char *p);
int		cli_is_dcm(char *p);
int		cli_is_hex(char *p);
int		cli_is_qualif(char *p);
int		cli_look_next_string_token(int *eof);
int		cli_look_next_token(int *eof);
int		cli_present(char *entry);		/***type int added***/
void		cli_str_setup(int length, char *addr);
void		cli_strupper(char *sp);
int		cli_parse_two_numbers(char *qual_name, const char delimiter, uint4 *first_num, uint4 *second_num);
#ifdef __osf__
	/* N.B. argv is passed in from main (in gtm.c) almost straight from the operating system.  */
#pragma pointer_size (save)
#pragma pointer_size (long)
#endif
void		cli_lex_setup(int argc, char *argv[]);
#ifdef __osf__
#pragma pointer_size (restore)
#endif

#define CLI_2NUM_FIRST_SPECIFIED	0x2
#define CLI_2NUM_SECOND_SPECIFIED	0x1

#endif /* CLIIF_included */
