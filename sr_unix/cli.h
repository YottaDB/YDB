/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef CLI_H
#define CLI_H

/*
 * -----------------------------------------------------------
 * Parser include file
 * -----------------------------------------------------------
 */
#define	MAX_PARMS	5	/* Maximum parameters on commands line */

#define MAX_CMD_LEN	25	/* Max Command name string length */
#define MAX_OPT_LEN	25	/* Max Option name string length */
#define MAX_CLI_ERR_STR	256	/* Max error string length */
#define MAX_LINE	32767+256	/* Max line len , maximum record size plus some overhead */

#define PARM_OVHD 	32	/* Parameter overhead value */


#define VAL_N_A		0	/* value type not applicable */
#define VAL_STR		1	/* String value type */
#define VAL_NUM		2	/* Number */
#define VAL_TIME	3	/* Time (can never be used on verb) */
#define VAL_LIST	3	/* Value can be a list
				 	(only used on verb, applies to last parameter) */

#define VAL_DCM		0	/* Number is Decimal */
#define VAL_HEX		1	/* Number is Hex */

#define VAL_DISALLOWED	0	/* Value Disallowed */
#define VAL_NOT_REQ	1	/* Value not Required, but allowed */
#define VAL_REQ		2	/* Value Required */

#define PARM_NOT_REQ	0	/* Parameter optional */
#define PARM_REQ	1	/* Parameter required */

#define NON_NEG		0	/* Non Negatable */
#define NEG		1	/* Negatable */

#define CLI_ABSENT	0
#define CLI_PRESENT	1
#define CLI_NEGATED	2
#define CLI_DEFAULT	3	/* default present: The present field is only one
				 * bit, therefore, 3 is euqiv. to 1, i.e. CLI_PRESENT
				 * (since there is not CLI_DEFAULT on VMS,
				 * cli_present() should not return CLI_DEFAULT).
				 */
#define DEFA_PRESENT	(char *) 1L /* Should be same as CLI_PRESENT - default present */

#define CLI_GET_STR_ALL	cli_get_str

/*
 * ------------------------------------------------------
 * Here the CLI_PARM structure is used
 * to give default values to a qualifier
 * wherever qualifiers dont require values.
 * Where qualifiers require values, the
 * CLI_PARM structure is used to prompt for the values.
 * ------------------------------------------------------
 */

typedef struct cmd_parm_struct {
	char		name[MAX_OPT_LEN];
	char		prompt[MAX_OPT_LEN];
	boolean_t	parm_required;	/* Is this parameter required or optional? */
} CLI_PARM;

typedef struct cmd_parm_tag {
	char		name[MAX_OPT_LEN];	/* name string */
	void		(*func)(void);		/* Ptr to worker function */
	struct cmd_parm_tag
			*parms;			/* Qualifiers */
	struct cmd_parm_struct
			*parm_values;		/* Parameters */
	struct cmd_parm_tag
			*qual_vals;			/* Extra Qualifiers */
	boolean_t	(*disallow_func)(void);		/* Ptr to disallow function */
	char		*dfault_str;
	unsigned 	required : 2;		/* Value required flag. Values :
							0 - disallowed,
							1 - optional
							2 - required */
	unsigned 	max_parms : 3;		/* Max. # of parameters allowed */
	unsigned 	negatable : 1;		/* Negatable flag  */
	unsigned 	val_type : 2;		/* Value Type
							VAL_N_A - type not applicable
							VAL_STR - String value type
							VAL_NUM - Number
							VAL_TIME - Time */
	unsigned 	hex_num : 1;		/* Number is hex */
	unsigned 	present : 2;		/* Arg. is present on command line */
	unsigned 	negated : 1;		/* Arg. negated on command line */
	char		*pval_str;		/* Value string */
} CLI_ENTRY;

typedef struct
{
	int     argc;

#ifdef __osf__
#pragma pointer_size (save)
#pragma pointer_size (long)
#endif

	char    **argv;

#ifdef __osf__
#pragma pointer_size (restore)
#endif

	char    *tp;            /* token pointer */
	int	buflen;		/* length of in_str */
	char    in_str[1];	/* input string buffer. The real length is computed and added to this block */
} IN_PARMS;

/* include platform independent prototypes */

#include "cliif.h"
#include "gtm_stdio.h"

void cli_strlwr(char *sp);
int cli_is_id(char *p);
void skip_white_space(void);
int cli_has_space(char *p);
char *cli_fgets(char *buffer, int buffersize, FILE *fp, boolean_t cli_lex_str);

#endif
