/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef TRIGGER_PARSE_PROTOS_INCLUDED
#define TRIGGER_PARSE_PROTOS_INCLUDED

STATICFNDCL char *scan_to_end_quote(char *ptr, int len, int max_len);
STATICFNDCL boolean_t process_dollar_char(char **src_ptr, int *src_len, boolean_t have_star, char **d_ptr);
STATICFNDCL boolean_t check_delim(char *delim_str, unsigned short *delim_len);
STATICFNDCL boolean_t check_options(char *option_str, unsigned short option_len, boolean_t *isolation, boolean_t *noisolation,
				    boolean_t *consistency, boolean_t *noconsistency);
STATICFNDCL boolean_t check_subscripts(char *subscr_str, unsigned short *subscr_len, char **next_str);
STATICFNDCL boolean_t check_pieces(char *piece_str, unsigned short *piece_len);
STATICFNDCL boolean_t check_xecute(char *xecute_str, unsigned short *xecute_len);

boolean_t check_name(char *name_str, unsigned short name_len);
boolean_t trigger_parse(char *input, short input_len, char *trigvn, char **values, unsigned short *value_len);
#endif /* TRIGGER_PARSE_PROTOS_INCLUDED */
