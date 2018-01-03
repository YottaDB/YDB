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
#include "gtm_stdlib.h"		/* for EXIT() */

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "stp_parms.h"
#include "iosp.h"
#include "cli.h"
#include "util.h"
#include "gtm_caseconv.h"
#include "mupip_exit.h"
#include "mupip_cvtgbl.h"
#include "file_input.h"
#include "load.h"
#include "mu_outofband_setup.h"
#include <gtm_fcntl.h>
#include <errno.h>
#include "muextr.h"
#include <regex.h>
#include "op.h"
#include "min_max.h"

GBLREF	int		gv_fillfactor;
GBLREF	bool		mupip_error_occurred;
GBLREF	boolean_t	is_replicator;
GBLREF	boolean_t	skip_dbtriggers;
GBLREF	mstr		sys_input;
GBLDEF	int		onerror;

error_def(ERR_LDBINFMT);
error_def(ERR_LOADBGSZ);
error_def(ERR_LOADBGSZ2);
error_def(ERR_LOADINVCHSET);
error_def(ERR_LOADEDBG);
error_def(ERR_LOADEDSZ);
error_def(ERR_LOADEDSZ2);
error_def(ERR_MAXSTRLEN);
error_def(ERR_MUNOFINISH);
error_def(ERR_MUPCLIERR);

#define CHAR_TO_READ_LINE1_BIN	STR_LIT_LEN("d0GDS BINARY")  /* read first 12 characters to check file is binary [d\0GDS BINARY] */
#define	MAX_ONERROR_VALUE_LEN	STR_LIT_LEN("INTERACTIVE") /* PROCEED, STOP, INTERACTIVE are the choices with INTERACTIVE as max */
#define	MAX_FORMAT_VALUE_LEN	STR_LIT_LEN("BINARY") /* ZWR, BINARY, GO, GOQ are the choices with BINARY being the longest */

void mupip_cvtgbl(void)
{
	char		fn[MAX_FN_LEN + 1], *line1_ptr, *line3_ptr;
	gtm_int64_t	begin_i8, end_i8;
	int		dos, i, file_format, line1_len, line3_len, utf8;
	uint4	        begin, cli_status, end, max_rec_size;
	unsigned char	buff[MAX_ONERROR_VALUE_LEN];
	unsigned short	fn_len, len;

	DCL_THREADGBL_ACCESS;
	SETUP_THREADGBL_ACCESS;
	assert(MAX_ONERROR_VALUE_LEN > MAX_FORMAT_VALUE_LEN);	/* so the buff[] definition above is good for FORMAT and ONERROR */
	/* If an online rollback occurs when we are loading up the database with new globals and takes us back to a prior logical
	 * state, then we should not continue with the load. The reason being that the application might rely on certain globals to
	 * be present before loading others and that property could be violated if online rollback takes the database back to a
	 * completely different logical state. Set the variable issue_DBROLLEDBACK_anyways that forces the restart logic to issue
	 * an rts_error the first time it detects an online rollback (that takes the database to a prior logical state).
	 */
	TREF(issue_DBROLLEDBACK_anyways) = TRUE;
	is_replicator = TRUE;
	TREF(ok_to_see_statsdb_regs) = TRUE;
	skip_dbtriggers = TRUE;
	fn_len = SIZEOF(fn);
	if (cli_present("STDIN"))
	{
		/* Check if both file name and -STDIN specified. */
		if (cli_get_str("FILE", fn, &fn_len))
		{
			util_out_print("STDIN and FILE (!AD) cannot be specified at the same time", TRUE, fn_len, fn);
			mupip_exit(ERR_MUPCLIERR);
		}
		/* User wants to load from standard input */
		assert(SIZEOF(fn) > sys_input.len);
		memcpy(fn, sys_input.addr, sys_input.len);
		fn_len = sys_input.len;
		assert(-1 != fcntl(fileno(stdin), F_GETFD));
	} else if (!cli_get_str("FILE", fn, &fn_len))  /* User wants to read from a file. */
		mupip_exit(ERR_MUPCLIERR); /* Neither -STDIN nor file name specified. */
	file_input_init(fn, fn_len, IOP_EOL);
	if (mupip_error_occurred)
		EXIT(-1);
	mu_outofband_setup();
	if ((cli_status = cli_present("BEGIN")) == CLI_PRESENT)
	{
	        if (!cli_get_int64("BEGIN", &begin_i8))
			mupip_exit(ERR_MUPCLIERR);
		if (1 > begin_i8)
			mupip_exit(ERR_LOADBGSZ);
		else if (MAXUINT4 < begin_i8)
			mupip_exit(ERR_LOADBGSZ2);
		begin = (uint4) begin_i8;
	} else
	{
		begin = 1;
		begin_i8 = 1;
	}
	if ((cli_status = cli_present("END")) == CLI_PRESENT)
	{
	        if (!cli_get_int64("END", &end_i8))
			mupip_exit(ERR_MUPCLIERR);
		if (1 > end_i8)
			mupip_exit(ERR_LOADEDSZ);
		else if (MAXUINT4 < end_i8)
			mupip_exit(ERR_LOADEDSZ2);
		if (end_i8 < begin_i8)
			mupip_exit(ERR_LOADEDBG);
		end = (uint4) end_i8;
	} else
		end = MAXUINT4;
	if ((cli_status = cli_present("FILL_FACTOR")) == CLI_PRESENT)
	{
		assert(SIZEOF(gv_fillfactor) == SIZEOF(int4));
	        if (!cli_get_int("FILL_FACTOR", (int4 *)&gv_fillfactor))
			gv_fillfactor = MAX_FILLFACTOR;
		if (gv_fillfactor < MIN_FILLFACTOR)
			gv_fillfactor = MIN_FILLFACTOR;
		else if (gv_fillfactor > MAX_FILLFACTOR)
			gv_fillfactor = MAX_FILLFACTOR;
	} else
		gv_fillfactor = MAX_FILLFACTOR;
	if (cli_present("ONERROR") == CLI_PRESENT)
	{
		len = SIZEOF(buff);
		if (!cli_get_str("ONERROR", (char *)buff, &len))
		{
			assert(FALSE);
			onerror = ONERROR_PROCEED;
		} else
		{
			lower_to_upper(buff, buff, len);
			if (!STRNCMP_LIT_LEN(buff, "STOP", len))
				onerror = ONERROR_STOP;
			else if (!STRNCMP_LIT_LEN(buff, "PROCEED", len))
				onerror = ONERROR_PROCEED;
			else if (!STRNCMP_LIT_LEN(buff, "INTERACTIVE", len))
			{
				if (isatty(0)) /*if stdin is a terminal*/
					onerror = ONERROR_INTERACTIVE;
				else
					onerror = ONERROR_STOP;
			} else
			{
				util_out_print("Illegal ONERROR parameter for load",TRUE);
				mupip_exit(ERR_MUPCLIERR);
			}
		}
	} else
		onerror = ONERROR_PROCEED; /* Default: Proceed on error */
	file_format = get_load_format(&line1_ptr, &line3_ptr, &line1_len, &line3_len, &max_rec_size, &utf8, &dos); /* from header */
	if (MU_FMT_GOQ == file_format)
		mupip_exit(ERR_LDBINFMT);
	if (BADZCHSET == utf8)
		mupip_exit(ERR_MUNOFINISH);
	if (cli_present("FORMAT") == CLI_PRESENT)
	{	/* If the command speficies a format see if it matches the label */
		len = SIZEOF(buff);
		if (!cli_get_str("FORMAT", (char *)buff, &len))
			go_load(begin, end, (unsigned char *)line1_ptr, line3_ptr, line3_len, max_rec_size, file_format, utf8, dos);
		else
		{
		        lower_to_upper(buff, buff, len);
			if (!STRNCMP_LIT_LEN(buff, "ZWR", len))
			{	/* If the label did not determine a format let them specify ZWR and they can sort out the result */
				if ((MU_FMT_ZWR == file_format) || (MU_FMT_UNRECOG == file_format))
					go_load(begin, end, (unsigned char *)line1_ptr, line3_ptr, line3_len, max_rec_size,
						MU_FMT_ZWR, utf8, dos);
				else
					mupip_exit(ERR_LDBINFMT);
			} else if (!STRNCMP_LIT_LEN(buff, "BINARY", len))
			{
				if (MU_FMT_BINARY == file_format)
					bin_load(begin, end, line1_ptr, line1_len);
				else
					mupip_exit(ERR_LDBINFMT);
			} else if (!STRNCMP_LIT_LEN(buff, "GO", len))
			{	/* If the label did not determine a format let them specify GO and they can sort out the result */
				if ((MU_FMT_GO == file_format) || (MU_FMT_UNRECOG == file_format))
					go_load(begin, end, (unsigned char *)line1_ptr, line3_ptr, line3_len, max_rec_size,
						MU_FMT_GO, utf8, dos);
				else
					mupip_exit(ERR_LDBINFMT);
			} else if (!STRNCMP_LIT_LEN(buff, "GOQ", len))
			{	/* get_load_format doesn't recognize GOQ labels' */
				if (MU_FMT_UNRECOG == file_format)
					goq_load();
				else
					mupip_exit(ERR_LDBINFMT);
			} else
			{
					util_out_print("Illegal file format for load",TRUE);
					mupip_exit(ERR_MUPCLIERR);
			}
		}
	} else
	{
		if (MU_FMT_BINARY == file_format)
			bin_load(begin, end, line1_ptr, line1_len);
		else if ((MU_FMT_ZWR == file_format) || (MU_FMT_GO == file_format))
			go_load(begin, end, (unsigned char *)line1_ptr, line3_ptr, line3_len, max_rec_size, file_format, utf8, dos);
		else
		{
			assert(MU_FMT_UNRECOG == file_format);
			mupip_exit(ERR_LDBINFMT);
		}
	}
	mupip_exit(mupip_error_occurred ? ERR_MUNOFINISH : SS_NORMAL);
}

/* Make an attempt to discover the input file format based on its content principally the label */

int get_load_format(char **line1_ptr, char **line3_ptr, int *line1_len, int *line3_len, uint4 *max_rec_size, int *utf8_extract,
		int *dos)
{
	char	*c, *c1, *ctop, *line1, *line2, *line3, *ptr;
	int	len, line2_len, ret;
	mval	v;
	uint4	max_io_size;

	max_io_size = MAX_IO_BLOCK_SIZE - 1;				/* label gets less room */
	*max_rec_size = MAX_STRLEN + ZWR_EXP_RATIO(MAX_KEY_SZ);		/* go for max to avoid interaction with the regex stuff */
	line1 = *line1_ptr = malloc(*max_rec_size);			/* no corresponding free; released at MUPIP termination */
	line3 = *line3_ptr = malloc(*max_rec_size);			/*  ditto */
	*line1_len = file_input_read_xchar(line1, CHAR_TO_READ_LINE1_BIN);
	*dos = *line3_len = *utf8_extract = 0;
	ret = MU_FMT_UNRECOG;		/* actually means as yet undetermined; used to decide if still trying to find a format */
	if (0 < *line1_len)
	{
		if (0 == STRNCMP_LIT(line1 + 6, "BINARY")) /* If file is binary do not look further */
			return MU_FMT_BINARY;
		for (line2_len = 0, c = line1, ctop = c + *line1_len; c < ctop; c++)
		{	/* that 1st read is fixed length, so look for a terminator */
			if ('\n' == *c)
			{	/* found a terminator */
				line2 = c + 1;
				line2_len = *line1_len - (line2 - line1);
				*line1_len -= (line2_len + 1);
				break;
			}
		}
		if (c == ctop)
		{	/* did not find a terminator - read some more of 1st line */
			ptr = c;
			if (0 <= (len = go_get(&ptr, 0, max_io_size)))		/* WARNING assignment */
				*line1_len += len;
			else
			{	/* chances of this are small but we are careful not to overflow buffers */
				mupip_error_occurred = TRUE;
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXSTRLEN);
			}
			line2_len = 0;
			line2 = line1 + *line1_len;
		} else if (line2_len)
		{	/* If line1 length is actually < 12 chars, the buffer has characters from line2 as well */
			for (c = line2, ctop = c + line2_len; c < ctop; c++)
			{	/* look for a line 2 terminator */
				if ('\n' == *c)
				{	/* found a terminator */
					*line3_len = line2_len - (c - line2 + 1);
					line2_len = c - line2;
					break;
				}
			}
		}
		c1 = line1 + *line1_len;
		*c1-- = 0;				/* null terminate the line to keep util_out_print happy */
		if (*dos = ('\r' == *c1))		/* WARNING assignment */
		{	/* [cariage] return before the <LF> / new line - we'll need to keep stripping them off */
			*line1_len -= 1;
			*c1 = 0;			/* null terminate earlier to keep util_out_print happy */
		}
		util_out_print("!AD", TRUE, *line1_len, line1);
		if ((0 == line2_len) || (c == ctop))
		{	/* need to get at least some more of 2nd line */
			ptr = line2 + line2_len;
			if (0 < (len = go_get(&ptr, 0, max_io_size)))		/* WARNING assignment */
				line2_len += len;
			else
			{	/* chances of this are small but we are careful not to overflow buffers */
				ret = MU_FMT_GOQ;	/* abusing this value to mean not working, as we can't discover GOQ */
				line2_len = 0;
				mupip_error_occurred = TRUE;
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXSTRLEN);
			}
		}
		if (0 < line2_len)
		{	/* we have 2 label lines to work with */
			line2_len -= *dos;
			c1 = line2 + line2_len;
			*c1 = 0;	/* null terminate the line to keep regex in bounds */
			util_out_print("!AD", TRUE, line2_len, line2);
			if (gtm_regex_perf("ZWR", line2))
				ret = MU_FMT_ZWR;		/* settle for any ZWR in the second line of the label */
			if ((MU_FMT_UNRECOG == ret) &&
				gtm_regex_perf("(GT.M )?[0-9]{2}[-]([A-Z]{3})[-][0-9]{4}[ ]{1,2}[0-9]{2}[:][0-9]{2}[:][0-9]{2}",
					line2))
				ret = MU_FMT_GO;	/* GT.M DD-MON-YEAR  24:60:SS used by MUPIP EXTRACT & %GO */
			if ((MU_FMT_UNRECOG == ret) && gtm_regex_perf("GLO", line2))
				ret = MU_FMT_GO;	/* settle for any GLO in the second line of the label */
			for (c = line2 + line2_len + 1, ctop = c + *line3_len, c1 = line3; c < ctop; c++)
			{	/* if the first 2 lines were really short, move to other buffer looking for a line 3 terminator */
				if ('\n' == *c)
				{	/* found a terminator */
					*line3_len = c1 - line3;
					break;
				} else
					*c1 = *c;
			}
			if (c == ctop)
			{	/* get all or some of line 3 - the first non-label line */
				ptr = line3 + *line3_len;
				if (0 < (len = go_get(&ptr, 0, *max_rec_size)))
				{
					*line3_len += (len - *dos);
					c1 = line3 + *line3_len;
					*c1 = 0;		/* null terminate the line to keep regex in bounds */
				} else
				{	/* chances of this are small but we are careful not to overflow buffers */
					ret = MU_FMT_GOQ;
					mupip_error_occurred = TRUE;
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXSTRLEN);
				}
			} else
			{
				*line3_len = 0;
				ret = MU_FMT_GOQ;	/* abusing this value to mean not working, as we can't discover GOQ */
			}
			if ((MU_FMT_UNRECOG == ret) && gtm_regex_perf("\\^[%A-Za-z][0-9A-Za-z]*(\\(.*\\))?$", line3))
				ret = MU_FMT_GO;	/* gvn only */
			if ((MU_FMT_UNRECOG == ret)
				&& gtm_regex_perf("\\^[%A-Za-z][0-9A-Za-z]*(\\(.*\\))?=(\".*\"|-?([0-9]+|[0-9]*\\.[0-9]+))$",
					line3))
				ret = MU_FMT_ZWR;	 /* gvn=val */
			if (MU_FMT_UNRECOG != ret)
			{
				*utf8_extract = gtm_regex_perf("UTF-8", line1);
				if ((*utf8_extract && !gtm_utf8_mode) || (!*utf8_extract && gtm_utf8_mode))
				{	/* extract CHSET doesn't match current $ZCHSET */
					if (*utf8_extract)
						gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_LOADINVCHSET,
							2, LEN_AND_LIT("UTF-8"));
					else
						gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_LOADINVCHSET, 2, LEN_AND_LIT("M"));
					*utf8_extract = BADZCHSET;
				}
			}
		} else
			return MU_FMT_GOQ;
	} else
		return MU_FMT_GOQ;
	*max_rec_size = (MU_FMT_GO == ret) ? MAX_STRLEN : *max_rec_size;		/* for GO, keys are separate */
	return MU_FMT_GOQ == ret ? MU_FMT_UNRECOG : ret;				/* turn the GOQs back into unrecognized */
}

/* given a regular expression definition and a string run the glibc interface NOT RECOMMENDED for general use */
boolean_t	gtm_regex_perf(const char *rexpr, char *str_buff)
{	/* This routine interacts VICIOUSLY with gtm_malloc and gtm_free and thus should only be used if they are not */
	boolean_t	ret;
	regex_t		regex;

	regcomp(&regex, rexpr, REG_EXTENDED | REG_ICASE);
	ret = !regexec(&regex, str_buff, 0, NULL, 0);
	regfree(&regex);
	return ret;
}
