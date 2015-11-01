/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 * -----------------------------------------------------
 * Lexical analyzer routines for command line interpreter
 * -----------------------------------------------------
 */

#include "mdef.h"

#include "gtm_ctype.h"
#include <errno.h>
#include "gtm_stdio.h"
#include "gtm_string.h"

#include "cli.h"
#include "eintr_wrappers.h"

GBLDEF char cli_token_buf[MAX_LINE+1];	/* Token buffer */
GBLREF int	cmd_cnt;
GBLREF char	**cmd_arg;

static IN_PARMS cli_lex_in;

GBLDEF IN_PARMS *cli_lex_in_ptr;

static int tok_string_extract(void)
{
	int token_len;
	bool	have_quote, first_quote;
	char *in_sp, *out_sp;

	in_sp = cli_lex_in.tp;
	out_sp = cli_token_buf;
	token_len = 0;
	have_quote = FALSE;
	for( ; ;)
	{	while(*in_sp && !ISSPACE((int) *in_sp) && *in_sp != '-')
		{	if (*in_sp == '"')
			{	if (!have_quote)
				{	have_quote = TRUE;
					in_sp++;
				}else
				{	if (*++in_sp == '"')
					{	*out_sp++ = *in_sp++;	/* double quote, one goes in string, still have quote */
						token_len++;
					}else
					{	have_quote = FALSE;
					}
				}
			}else
			{
				*out_sp++ = *in_sp++;
				token_len++;
			}
		}


		if (*in_sp == '\0')
		   break;

		if (have_quote)
		{
			*out_sp++ = *in_sp++;
			token_len++;
			continue;
		}
		break;
	}
	*out_sp = '\0';
	cli_lex_in.tp = in_sp;

	return(token_len);
}



/*
 * -------------------------
 * Inintialize lexer
 * -------------------------
 */
#ifdef __osf__
	/* N.B. argv is passed in from main (in gtm.c) almost straight from the operating system.  */
#pragma pointer_size (save)
#pragma pointer_size (long)
#endif
void	cli_lex_setup ( int	argc, char	*argv[])
#ifdef __osf__
#pragma pointer_size (restore)
#endif
{
#ifdef __MVS__
	__argvtoascii_a(argc,argv);
#endif
	cmd_cnt = argc;
	cmd_arg = (char **)&argv[0];
	cli_lex_in.argc = argc;
	cli_lex_in.argv = &argv[0];
	*cli_lex_in.in_str = '\0';
	cli_lex_in.tp = 0;
	cli_lex_in_ptr = &cli_lex_in;
}

void cli_str_setup(int length, char *addr)
{
	cli_lex_in.argc = 0;
	cli_lex_in.tp = cli_lex_in.in_str;
	memcpy(cli_lex_in.in_str, addr, length > MAX_LINE ? MAX_LINE : length);
}

/*
 * ----------------------------
 * Convert string to lower case
 * ----------------------------
 */
void cli_strlwr(char *sp)
{
	int c;

	while (c = *sp)
		*sp++ = TOLOWER(c);
}

/*
 * ----------------------------
 * Convert string to upper case
 * ----------------------------
 */
void cli_strupper(char *sp)
{
	int c;

	while (c = *sp)
		*sp++ = TOUPPER(c);
}

/*
 * -------------------------------------------------------
 * Check if string is an identifier
 * (consists of at least one alpha character followed by
 * alphanumerics or underline characters in any order)
 *
 * Return:
 *	TRUE	- identifier
 *	FALSE	- otherwise
 * -------------------------------------------------------
 */
int cli_is_id(char *p)
{
	if (!ISALPHA(*p))
		return (FALSE);

	while (*p && (ISALPHA(*p) || ISDIGIT(*p) || *p == '_' ))
		p++;

	if (*p) return (FALSE);
	else return (TRUE);
}


/*
 * -------------------------------------------------------
 * Check if string is a decimal number
 *
 * Return:
 *	TRUE	- identifier
 *	FALSE	- otherwise
 * -------------------------------------------------------
 */
int cli_is_dcm(char *p)
{
	while (*p && ISDIGIT(*p))
		p++;

	if (*p) return (FALSE);
	else return (TRUE);
}


/*
 * -------------------------------------------------------
 * Check if string is a Hex number
 *
 * Return:
 *	TRUE	- identifier
 *	FALSE	- otherwise
 * -------------------------------------------------------
 */
int cli_is_hex(char *p)
{
	if (('+' == *p) || ('-' == *p))
		p++;

	if (('0' == *p) && ('X' == toupper(*(p + 1))))
        {
                p = p + 2;
        }

	while (*p && ISXDIGIT(*p))
		p++;

	return ((*p) ? FALSE : TRUE);
}


/*
 * -------------------------------------------------------
 * Check if token is a qualifier
 *
 * Return:
 *	TRUE	- qualifier
 *	FALSE	- otherwise
 * -------------------------------------------------------
 */
int cli_is_qualif(char *p)
{
	return (*p == '-');
}


/*
 * -------------------------------------------------------
 * Check if token is an assignmet symbol
 *
 * Return:
 *	TRUE	- assignment
 *	FALSE	- otherwise
 * -------------------------------------------------------
 */
int cli_is_assign(char *p)
{
	return (*p == '=');
}

/* ----------------------------------------------
 * Routine to skip white space while reading.
 * Called when a parameter has to be read.
 * The tok_string_extract () doesnt remove
 * starting spaces while reading a string.
 * To make use of that while reading a parameter
 * this has to be called first.
 * ----------------------------------------------
 */

void	skip_white_space(void)
{
	char	*in_sp;

	in_sp = cli_lex_in.tp;

	while(ISSPACE((int)*in_sp))
		in_sp++;

	cli_lex_in.tp = in_sp;
}


/*
 * --------------------------------------------
 * Extract one token from a string.
 * Token is anything between the separator characters
 * or separator character itself, if it is '-' or '='.
 *
 * Return:
 *	token Length
 * --------------------------------------------
 */
static int	tok_extract (void)
{
	int	token_len;
	char	*in_sp, *out_sp;

	in_sp = cli_lex_in.tp;

	out_sp = cli_token_buf;
	token_len = 0;

		/* Skip leading blanks */
	while(ISSPACE((int) *in_sp))
		in_sp++;

	if (*in_sp == '-' || *in_sp == '=')
	{
		*out_sp++ = *in_sp++;
		token_len = 1;
	}
	else
	{
		while(*in_sp && !ISSPACE((int) *in_sp)
		  && *in_sp != '-'
		  && *in_sp != '=')
		{
			*out_sp++ = *in_sp++;
			token_len++;
		}
	}
	*out_sp = '\0';
	cli_lex_in.tp = in_sp;

	return(token_len);
}


/*
 * -------------------------------------------------------
 * Get token
 *
 * Return:
 *	Token Length
 *
 * Side effects:
 *	set eof to <> 0 for EOF condition.
 * -------------------------------------------------------
 */

int	cli_gettoken (int *eof)
{
	int	arg_no, token_len;
	char	*from, *to;


	/* Reading from program argument list */
	if (cli_lex_in.argc > 1 && cli_lex_in.tp == 0)
	{
		cli_lex_in.tp = cli_lex_in.in_str;
		arg_no = 1;
			/* convert arguments into array */
		while(arg_no < cli_lex_in.argc)
		{
			if (arg_no > 1)
				strcat(cli_lex_in.in_str, " ");
			if (strlen(cli_lex_in.in_str)
			  + strlen(cli_lex_in.argv[arg_no]) > MAX_LINE)
				break;
			if (cli_has_space(cli_lex_in.argv[arg_no]))
			{
				from = cli_lex_in.argv[arg_no++];
				to = cli_lex_in.in_str + strlen(cli_lex_in.in_str);
				*to++ = '\"';
				while(*from != '\0')
				{
					if ('\"' == *from)
						*to++ = *from;
					*to++ = *from++;
				}
				*to++ = '\"';
				*to = '\0';
			}
			else
			{
				strcat(cli_lex_in.in_str, cli_lex_in.argv[arg_no++]);
			}
		}
	        cli_lex_in_ptr = &cli_lex_in;
	}


	if (cli_lex_in.tp == 0 || strlen(cli_lex_in.tp) < 1)
	{

		FGETS_FILE(cli_lex_in.in_str, MAX_LINE, stdin, cli_lex_in.tp);
    		if (NULL != cli_lex_in.tp)
    		{
    			cli_lex_in.in_str[strlen(cli_lex_in.in_str)-1] = '\0';
    			cli_lex_in_ptr = &cli_lex_in;
      			*eof = 0;
           	}
	    	else
	    	{
	      		*eof = EOF;
	      		return (0);
            	}

	}

	token_len = tok_extract();
	*eof = (cli_lex_in.argc > 1 && token_len == 0);
	return token_len;
}


/*
 * --------------------------------------------
 * Copy next token to the token buffer.
 * Do not advance the token pointer.
 *
 * Return:
 *	Token Length
 *
 * Side effects:
 *	set eof to <> 0 for EOF condition.
 * -------------------------------------------------------
 */
int cli_look_next_token(int *eof)
{
	int tok_len;
	char *old_tp;

	if (((char *) NULL == cli_lex_in.tp) || (!strlen(cli_lex_in.tp)))
		return(0);

	old_tp = cli_lex_in.tp;
	tok_len = cli_gettoken(eof);
	cli_lex_in.tp = old_tp;

	return(tok_len);
}

int cli_look_next_string_token(int *eof)
{
	int tok_len;
	char *old_tp;

	if (!strlen(cli_lex_in.tp))
		return(0);

	old_tp = cli_lex_in.tp;
	tok_len = cli_get_string_token(eof);
	cli_lex_in.tp = old_tp;

	return(tok_len);
}

int cli_get_string_token(int *eof)
{
	int arg_no, token_len;
	char *from, *to;

		/* Reading from program argument list */
	if (cli_lex_in.argc > 1 && cli_lex_in.tp == 0)
	{
		cli_lex_in.tp = cli_lex_in.in_str;
		arg_no = 1;
			/* convert arguments into array */
		while(arg_no < cli_lex_in.argc)
		{
			if (arg_no > 1)
				strcat(cli_lex_in.in_str, " ");
			if (strlen(cli_lex_in.in_str) + strlen(cli_lex_in.argv[arg_no]) > MAX_LINE)
				break;
			if (cli_has_space(cli_lex_in.argv[arg_no]))
			{
				from = cli_lex_in.argv[arg_no++];
				to = cli_lex_in.in_str + strlen(cli_lex_in.in_str) - 1;
				*to++ = '\"';
				while(*from != '\0')
				{
					if ('\"' == *from)
						*to++ = *from;
					*to++ = *from++;
				}
				*to++ = '\"';
				*to = '\0';
			}
			else
			{
				strcat(cli_lex_in.in_str, cli_lex_in.argv[arg_no++]);
			}
		}
	        cli_lex_in_ptr = &cli_lex_in;
	}

	if (cli_lex_in.tp == 0 || strlen(cli_lex_in.tp) < 1)
	{

	    FGETS_FILE(cli_lex_in.in_str, MAX_LINE, stdin, cli_lex_in.tp);
	    if (NULL != cli_lex_in.tp)
	    {
	      cli_lex_in.in_str[strlen(cli_lex_in.in_str)-1] = '\0';
	      cli_lex_in_ptr = &cli_lex_in;
	      *eof = 0;
            }
	    else
	    {
	      *eof = EOF;
	      return (0);
            }

        }

	token_len = tok_string_extract();
	*eof = (cli_lex_in.argc > 1 && token_len == 0);
	return token_len;
}


/*
 * -------------------------------------------------------
 * Check if string has space in it
 *
 * Return:
 *      TRUE    - identifier
 *      FALSE   - otherwise
 * -------------------------------------------------------
 */
int cli_has_space(char *p)
{
	while (*p && *p != ' ')
		p++;

	return ((*p) ? (TRUE) : (FALSE));
}


