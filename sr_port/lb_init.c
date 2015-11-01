/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "compiler.h"
#include "toktyp.h"
#include "comp_esc.h"
#include "advancewindow.h"
#include "lb_init.h"

GBLREF	unsigned char		*source_buffer;
GBLREF	short int		last_source_column;
GBLREF	char			*lexical_ptr;
GBLREF	struct ce_sentinel_desc	*ce_def_list;

LITREF char	ctypetab[NUM_CHARS];

void lb_init(void)
{
	int			num_subs, y;
	short int		sav_last_src_col, source_col;
	int4			source_len, skip_count;
	unsigned char		*cp, *cp1;
	bool			possible_sentinel;
	struct ce_sentinel_desc	*shp;

	error_def(ERR_CETOOMANY);

#ifdef DEBUG
	unsigned char		original_source[MAX_SRCLINE];
#endif

#define MAX_SUBSTITUTIONS	1024

	if (ce_def_list != NULL)
	{
		for (source_len = 0;  source_buffer[source_len] != '\0';  source_len++) ;
#ifdef DEBUG
		memcpy (original_source, source_buffer, source_len+2);	/* include terminating null characters */
#endif
		source_col = 1;
		num_subs = 0;
		cp = source_buffer;
		possible_sentinel = TRUE;
		while (possible_sentinel && source_buffer[source_col-1])
		{
			possible_sentinel = FALSE;
			cp = source_buffer + source_col - 1;
			if (*cp > DEL)
				break;

			if (*cp == '\"')
			{
				for (cp1 = cp + 1; ; )
				{
					if (*cp1 < SP)
						break;

					if (*cp1++ == '\"')
					{
						if (*cp1 == '\"')
							cp1++;			/* escaped quotation mark inside string */
						else
							break;			/* end of string */
					}
				}
				source_col += cp1 - cp;
				cp = cp1;
				if (*cp == '\0')
					break;
			} else if (*cp == '?')
			{
				for (cp1 = cp + 1; ; )
				{
					if (*cp1 >= NUM_ASCII_CHARS)
						break;
					y = ctypetab[*cp1];
					if (y == TK_UPPER || y == TK_LOWER || y == TK_DIGIT || y == TK_PERIOD || y == TK_ATSIGN)
						cp1++;
					else if (*cp1 == '\"')	/* quoted string in pattern */
					{
						for (cp1++; ; )
						{
							if (*cp1 < SP)
								break;

							if (*cp1 == '\"')
							{
								cp1++;
								if (*cp1 == '\"')	/* escaped quotation mark in string */
									cp1++;
								else			/* character following string */
									break;
							} else
								cp1++;
						}
					} else
						break;
				}
				source_col += cp1 - cp;
				cp = cp1;
				if (*cp == '\0')
					break;
			}

			for (shp = ce_def_list;  shp != NULL;  shp = shp->next)
			{
				if (memcmp(cp, shp->escape_sentinel, shp->escape_length) == 0)
				{
					ce_substitute (shp, source_col, &skip_count);
					num_subs++;
					if (num_subs <= MAX_SUBSTITUTIONS)
					{
						if (skip_count > 0)		/* a substitution occurred */
							possible_sentinel = TRUE;
					} else
					{
						sav_last_src_col = last_source_column;
						last_source_column = source_col;
						stx_error (ERR_CETOOMANY);
						last_source_column = sav_last_src_col;
					}
					break;
				}
			}

			if (!possible_sentinel)	/* in this column */
			{
				source_col++;
				possible_sentinel = TRUE;	/* next column may have sentinel */
			}
		}
	}
	lexical_ptr = (char *) &source_buffer[0];
	advancewindow();
	advancewindow();
	return;
}
