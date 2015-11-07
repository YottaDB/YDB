/****************************************************************
 *								*
 *	Copyright 2002, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"

#include <ssdef.h>
#include <lnmdef.h>
#include <descrip.h>
#include <fab.h>
#include <rms.h>
#include <iodef.h>
#include <errno.h>

#include "ddphdr.h"
#include "ddpcom.h"
#include "five_bit.h"
#include "is_five_bit.h"
#include "eintr_wrappers.h"

GBLREF mstr	my_circuit_name;
static int 	vug_parse(char *line, unsigned short *vol, unsigned short *uci, mstr *gld);

static int vug_parse(char *line, unsigned short *vol, unsigned short *uci, mstr *gld)
{
	char	*begin, esa[MAX_FN_LEN];
	struct FAB fab;
	struct NAM nam;

	error_def(ERR_DDPCONFGOOD);
	error_def(ERR_DDPCONFIGNORE);
	error_def(ERR_DDPCONFINCOMPL);
	error_def(ERR_DDPCONFBADVOL);
	error_def(ERR_DDPCONFBADUCI);
	error_def(ERR_DDPCONFBADGLD);

	for (; '\0' != *line && ISSPACE(*line); line++) /* skip over leading spaces */
		;
	if ('\0' == *line || VUG_CONFIG_COMMENT_CHAR == *line) /* ignore empty or comment line */
		return ERR_DDPCONFIGNORE;
	for (begin = line++; '\0' != *line && !ISSPACE(*line); line++) /* look for beginning of volume */
		;
	if ('\0' == *line) /* end of line when volume expected */
		return ERR_DDPCONFINCOMPL;
	if (DDP_VOLUME_NAME_LEN != line - begin || !is_five_bit(begin)) /* valid volume spec? */
		return ERR_DDPCONFBADVOL;
	*vol = five_bit(begin);
	for (line++; '\0' != *line && ISSPACE(*line); line++) /* skip over spaces; look for uci */
		;
	if ('\0' == *line) /* end of line when uci expected */
		return ERR_DDPCONFINCOMPL;
	for (begin = line++; '\0' != *line && !ISSPACE(*line); line++) /* find end of uci spec */
		;
	if ('\0' == *line) /* line ends with uci, gld not in the configuration */
		return ERR_DDPCONFINCOMPL;
	if (DDP_UCI_NAME_LEN != line - begin || !is_five_bit(begin)) /* valid uci spec? */
		return ERR_DDPCONFBADUCI;
	*uci = five_bit(begin);
	for (line++; '\0' != *line && ISSPACE(*line); line++) /* skip over spaces, find beginning of gld */
		;
	if ('\0' == *line) /* end of line when gld expected */
		return ERR_DDPCONFINCOMPL;
	for (begin = line++; '\0' != *line && !ISSPACE(*line); line++) /* find end of gld */
		;
	gld->addr = begin;
	gld->len = line - begin;
	/* valid file specified for gld? */
	fab = cc$rms_fab;
	nam = cc$rms_nam;
	nam.nam$b_nop = NAM$M_SYNCHK;
	fab.fab$l_nam = &nam;
	fab.fab$l_fop = FAB$M_NAM;
	fab.fab$l_fna = gld->addr;
	fab.fab$b_fns = gld->len;
	nam.nam$l_esa = esa;
	nam.nam$b_ess = SIZEOF(esa);
	if (RMS$_NORMAL != sys$parse(&fab, 0, 0))
		return ERR_DDPCONFBADGLD;
	return ERR_DDPCONFGOOD;
}

condition_code dcp_get_volsets(void)
{
	char		vug_conf_buffer[MAX_TRANS_NAME_LEN], vug_logical_buffer[MAX_TRANS_NAME_LEN];
	mstr 		vug_conf, vug_logical;
	condition_code	status;
	int		volset_count, line_no, parse_status;
	unsigned short	vol, uci;
	char		*line_p;
	char		line[BUFSIZ];
	FILE		*vol_fp;
	mstr		gld;
	char		err_str[1024];

	error_def(ERR_DDPVOLSETCONFIG);
	error_def(ERR_DDPCONFGOOD);
	error_def(ERR_DDPCONFIGNORE);

	vug_logical.addr = vug_logical_buffer;
	memcpy(vug_logical.addr, DDP_VOLSET_CONF_LOGICAL_PREFIX, STR_LIT_LEN(DDP_VOLSET_CONF_LOGICAL_PREFIX));
	memcpy(&vug_logical.addr[STR_LIT_LEN(DDP_VOLSET_CONF_LOGICAL_PREFIX)], my_circuit_name.addr, my_circuit_name.len);
	vug_logical.len = STR_LIT_LEN(DDP_VOLSET_CONF_LOGICAL_PREFIX) + my_circuit_name.len;
	if (SS$_NORMAL != (status = trans_log_name(&vug_logical, &vug_conf, vug_conf_buffer)))
	{
		decddp_log_error(status, "Volume Set Configuration File logical translation failed", 0, 0);
		return status;
	}
	vug_conf.addr[vug_conf.len] = '\0';
	if (NULL == (vol_fp = Fopen(vug_conf.addr, "r")))
	{
		SNPRINTF(err_str, SIZEOF(err_str), "Volume Set Configuration File %s open error : %s", vug_conf.addr,
				strerror(errno));
		decddp_log_error(ERR_DDPVOLSETCONFIG, err_str, 0, 0);
		return ERR_DDPVOLSETCONFIG;
	}
	/* Read lines from the configuration file and set up volset_table */
	for (line_no = 1, volset_count = 0; DDP_MAX_VOLSETS > volset_count; line_no++)
	{
		FGETS_FILE(line, BUFSIZ, vol_fp, line_p);
		if (NULL != line_p)
		{
			if (ERR_DDPCONFGOOD == (parse_status = vug_parse(line_p, &vol, &uci, &gld)))
			{
				if (FALSE != enter_vug(vol, uci, &gld))
					volset_count++;
			} else if (ERR_DDPCONFIGNORE != parse_status)
			{
				SNPRINTF(err_str, SIZEOF(err_str),
						"Incomplete or invalid configuration entry at line %d of file %s", line_no,
						vug_conf.addr);
				decddp_log_error(parse_status, err_str, 0, 0);
			}
		} else if (feof(vol_fp))
			break;
		else /* ferror(vol_fp) */
		{
			SNPRINTF(err_str, SIZEOF(err_str), "Volume Set Configuration File %s read error : %s", vug_conf.addr,
					strerror(errno));
			decddp_log_error(ERR_DDPVOLSETCONFIG, err_str, 0, 0);
			fclose(vol_fp);
			return ERR_DDPVOLSETCONFIG;
		}
	}
	fclose(vol_fp);
	if (0 == volset_count)
	{
		SNPRINTF(err_str, SIZEOF(err_str), "Volume Set Configuration File %s does not contain any valid VOL UCI GLD triple",
				vug_conf.addr);
		decddp_log_error(ERR_DDPVOLSETCONFIG, err_str, 0, 0);
		return ERR_DDPVOLSETCONFIG;
	}
	return SS$_NORMAL;
}
