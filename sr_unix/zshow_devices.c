/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gtm_socket.h"
#include "gtm_inet.h"

#include "gtm_stdio.h"

#include "mlkdef.h"
#include "zshow.h"
#include "io.h"
#include "iottdef.h"
#include "trmdef.h"
#include "iormdef.h"
#include "gt_timer.h"
#include "iosocketdef.h"
#include "zshow_params.h"
#include "nametabtyp.h"
#include "mvalconv.h"
#ifdef __MVS__
#include "gtm_zos_io.h"
#include <_Ccsid.h>
#endif

#define ZS_ONE_OUT(V,TEXT) ((V)->str.len = 1, (V)->str.addr = (TEXT), zshow_output(output,&(V)->str))
#define ZS_STR_OUT(V,TEXT) ((V)->str.len = SIZEOF((TEXT)) - 1, (V)->str.addr = (TEXT), zshow_output(output,&(V)->str))
#define ZS_VAR_STR_OUT(V,TEXT) ((V)->str.len = STRLEN((TEXT)), (V)->str.addr = (TEXT), zshow_output(output,&(V)->str))
#define ZS_PARM_SP(V,TEXT) ((V)->str.len = dev_param_names[dev_param_index[zshow_param_index[(TEXT)].letter] + \
			zshow_param_index[(TEXT)].offset ].len, \
			(V)->str.addr = (char *)dev_param_names[dev_param_index[zshow_param_index[(TEXT)].letter] + \
			zshow_param_index[(TEXT)].offset ].name, zshow_output(output,&(V)->str), ZS_ONE_OUT((V),space_text))
#define ZS_PARM_EQU(V,TEXT) ((V)->str.len = dev_param_names[dev_param_index[zshow_param_index[(TEXT)].letter] + \
			zshow_param_index[(TEXT)].offset ].len, \
			(V)->str.addr = (char *)dev_param_names[dev_param_index[zshow_param_index[(TEXT)].letter] + \
			zshow_param_index[(TEXT)].offset ].name, zshow_output(output,&(V)->str), ZS_ONE_OUT((V),equal_text))

static readonly char	space_text[] = {' '};

GBLREF boolean_t	ctrlc_on, gtm_utf8_mode;
GBLREF io_log_name	*io_root_log_name;
GBLREF io_pair		io_std_device;
GBLREF int		process_exiting;

LITREF mstr		chset_names[];
LITREF nametabent	dev_param_names[];
LITREF uint4		dev_param_index[];
LITREF zshow_index	zshow_param_index[];

void zshow_devices(zshow_out *output)
{
	io_log_name	*l;		/* logical name pointer		*/
	io_log_name	*savel;		/* logical name pointer		*/
	mval		v;
	mval		m;
	io_desc 	*tiod;
	d_rm_struct	*rm_ptr;
	d_tt_struct	*tt_ptr;
	d_socket_struct *dsocketptr;
	socket_struct	*socketptr;
	io_termmask	*mask_out;
	int4		i, j, ii, jj;
	boolean_t	first;
	unsigned char	delim_buff_sm[MAX_DELIM_LEN];
	unsigned short	delim_len_sm;
	char		delim_mstr_buff[(MAX_DELIM_LEN * MAX_ZWR_EXP_RATIO) + 11];
	mstr		delim;
	int		delim_len, tmpport;
	boolean_t	same_encr_settings;
	static readonly char space8_text[] = "        ";
	static readonly char filchar_text[] = "CHARACTERS";
	static readonly char filesc_text[] = "ESCAPES";
	static readonly char terminal_text[] = "TERMINAL ";
	static readonly char magtape_text[] =  "MAGTAPE ";
	static readonly char rmsfile_text[] =  "RMS ";
	static readonly char fifo_text[] =  "FIFO ";
	static readonly char pipe_text[] =  "PIPE ";
	static readonly char mailbox_text[] =  "MAILBOX ";
	static readonly char dollarc_text[] = "$C(";
	static readonly char equal_text[] = {'='};
	static readonly char comma_text[] = {','};
	static readonly char quote_text[] = {'"'};
	static readonly char lparen_text[] = {'('};
	static readonly char rparen_text[] = {')'};
	static readonly char lb_text[] = {'['};
	static readonly char rb_text[] = {']'};
	static readonly char devop[] = "OPEN ";
	static readonly char devcl[] = "CLOSED ";
	static readonly char interrupt_text[] = "ZINTERRUPT ";
	static readonly char stdout_text[] =  "0-out";
	static readonly char input_key[] = "IKEY=";
	static readonly char input_iv[] = "IIV=";
	static readonly char output_key[] = "OKEY=";
	static readonly char output_iv[] = "OIV=";
	static readonly char key[] = "KEY=";
	static readonly char iv[] = "IV=";

	/* gtmsocket specific */
	static readonly char at_text[] = {'@'};
	static readonly char delimiter_text[] = "DELIMITER ";
	static readonly char nodelimiter_text[] = "NODELIMITER ";
	static readonly char local_text[] = "LOCAL=";
	static readonly char remote_text[] = "REMOTE=";
	static readonly char total_text[] = "TOTAL=";
	static readonly char current_text[] = "CURRENT=";
	static readonly char passive_text[] = "PASSIVE ";
	static readonly char active_text[] = "ACTIVE ";
	static readonly char socket_text[] = "SOCKET";
	static readonly char descriptor_text[] = "DESC=";
	static readonly char trap_text[] = "TRAP ";
	static readonly char notrap_text[] = "NOTRAP ";
	static readonly char zdelay_text[] = "ZDELAY ";
	static readonly char znodelay_text[] = "ZNODELAY ";
	static readonly char zbfsize_text[] = "ZBFSIZE=";
	static readonly char zibfsize_text[] = "ZIBFSIZE=";
	static readonly char port_text[] = "PORT=";
	static readonly char ichset_text[] = "ICHSET=";
	static readonly char ochset_text[] = "OCHSET=";
	static readonly char tls_text[] = "TLS ";
#ifdef __MVS__
	static readonly char filetag_text[] = "FILETAG=";
	static readonly char untagged_text[] = "UNTAGGED";
	static readonly char ebcdic_text[] = "EBCDIC";
	static readonly char binary_text[] = "BINARY";
	static readonly char processchset_text[] = "CHSET=";
	static readonly char text_text[] = " TEXT";
	char   csname[_CSNAME_LEN_MAX + 1], *csptr;
#endif
	static readonly char zsh_socket_state[][10] =
		{	"CONNECTED"
			,"LISTENING"
			,"BOUND"
			,"CREATED"
		};
	static readonly char morereadtime_text[] = "MOREREADTIME=";
	char	*charptr;
#	ifdef DEBUG
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	endif
	v.mvtype = MV_STR;
	savel = NULL;
	for (l = io_root_log_name;  ((l != NULL) || (NULL != savel));  l = l->next)
	{
		/* If savel is set then process the output side of $principal */
		if (NULL != savel)
			l = savel;
		if ((NULL != l) && (NULL != l->iod) && (n_io_dev_types == l->iod->type))
		{
			assert(FALSE);
			continue;       /* skip it on pro */
		}
		if (NULL == l->iod)
		{
			assert(process_exiting); /* YDB-F-MEMORY occurred during device setup & we are creating zshow dump file */
			assert(TREF(jobexam_counter));
			continue;
		}
		if (l->iod->trans_name == l)
		{
			/* If it is an rm type we don't want to output the device if it is the stderr device for a pipe device */
			if ((rm_ptr = (d_rm_struct*)l->iod->dev_sp) && (rm == l->iod->type)
					&& rm_ptr->is_pipe && rm_ptr->stderr_parent)
				continue;
			if (l->iod->pair.in != l->iod->pair.out)
			{
				/* process the output side of $principal if savel is set */
				if (NULL != savel)
				{
					tiod = l->iod->pair.out;
					savel = NULL;
					rm_ptr = (d_rm_struct*)tiod->dev_sp;
					if ('&' == tiod->trans_name->dollar_io[0])
					{	/* replace & with 0-out */
						ZS_STR_OUT(&v, stdout_text);
					} else
					{	/* prepend with 0-out and a space */
						ZS_STR_OUT(&v, stdout_text);
						ZS_ONE_OUT(&v, space_text);
						v.str.addr = &tiod->trans_name->dollar_io[0];
						v.str.len = tiod->trans_name->len;
						zshow_output(output,&v.str);
					}
				} else
				{	/* plan to process the output side of $principal if it is std out */
					if (l->iod->pair.out == io_std_device.out)
						savel = l;
					tiod = l->iod;
					v.str.addr = &l->dollar_io[0];
					v.str.len = l->len;
					zshow_output(output,&v.str);
				}
			} else
			{
				tiod = l->iod;
				v.str.addr = &l->dollar_io[0];
				v.str.len = l->len;
				zshow_output(output,&v.str);
			}
			ZS_ONE_OUT(&v, space_text);
			if (tiod->state == dev_open)
			{
				ZS_STR_OUT(&v, devop);
				switch(tiod->type)
				{
				case tt:
					ZS_STR_OUT(&v, terminal_text);
					tt_ptr = (d_tt_struct*)tiod->dev_sp;
					if (!ctrlc_on && io_std_device.out == tiod) /* and standard input */
					{	ZS_PARM_SP(&v, zshow_nocene);
					}
					if (tt_ptr->enbld_outofbands.mask)
					{	ZS_PARM_EQU(&v, zshow_ctra);
						ZS_STR_OUT(&v,dollarc_text);
						first = TRUE;
						for ( i = 1, j = 0; j < 32  ; j++,i = i * 2)
						{	if (i & tt_ptr->enbld_outofbands.mask)
							{	if (!first)
								{	ZS_ONE_OUT(&v, comma_text);
								}else
								{	first = FALSE;
								}
								MV_FORCE_MVAL(&m,j);
								mval_write(output,&m,FALSE);
							}
						}
						ZS_ONE_OUT(&v, rparen_text);
						ZS_ONE_OUT(&v, space_text);
					}
					if ((int4)(tt_ptr->term_ctrl) & TRM_NOECHO)
					{
						ZS_PARM_SP(&v, zshow_noecho);
					}
					if (tt_ptr->term_ctrl & TRM_PASTHRU)
					{
						ZS_PARM_SP(&v, zshow_past);
					} else
					{
						ZS_PARM_SP(&v, zshow_nopast);
					}
					if (!(tt_ptr->term_ctrl & TRM_ESCAPE))
					{
						ZS_PARM_SP(&v, zshow_noesca);
					}
					if (tt_ptr->term_ctrl & TRM_READSYNC)
					{
						ZS_PARM_SP(&v, zshow_reads);
					} else
					{
						ZS_PARM_SP(&v, zshow_noreads);
					}
					if (tt_ptr->term_ctrl & TRM_NOTYPEAHD)
					{
						ZS_PARM_SP(&v, zshow_notype);
					} else
					{
						ZS_PARM_SP(&v, zshow_type);
					}
					if (!tiod->wrap)
					{
						ZS_PARM_SP(&v, zshow_nowrap);
					}
					mask_out = &tt_ptr->mask_term;
					if (!tt_ptr->default_mask_term)
					{
						ZS_PARM_EQU(&v, zshow_term);
						ZS_STR_OUT(&v,dollarc_text);
						first = TRUE;
						for ( i = 0; i < 8 ;i++)
						{
							for ( j = 0; j < 32; j++)
								if (mask_out->mask[i] & (1 << j))
								{
									if (!first)
									{
										ZS_ONE_OUT(&v, comma_text);
									} else
										first = FALSE;
									MV_FORCE_MVAL(&m,i * 32 + j);
									mval_write(output,&m,FALSE);
								}
						}
						ZS_ONE_OUT(&v, rparen_text);
						ZS_ONE_OUT(&v, space_text);
					}
					ZS_PARM_EQU(&v, zshow_width);
					MV_FORCE_MVAL(&m,(int)tiod->width);
					mval_write(output, &m, FALSE);
					ZS_ONE_OUT(&v, space_text);
					ZS_PARM_EQU(&v, zshow_leng);
					MV_FORCE_MVAL(&m,(int)tiod->pair.out->length);
					mval_write(output, &m, FALSE);
					ZS_ONE_OUT(&v, space_text);
					if (tiod->write_filter)
					{
						bool twoparms = FALSE;

						ZS_PARM_EQU(&v, zshow_fil);
						if (tiod->write_filter & CHAR_FILTER)
						{
							if (tiod->write_filter & ESC1)
							{
								twoparms = TRUE;
								ZS_ONE_OUT(&v,lparen_text);
							}
							ZS_STR_OUT(&v,filchar_text);
							if (twoparms)
							{
								ZS_ONE_OUT(&v, comma_text);
								ZS_ONE_OUT(&v, space_text);
							}
						}
						if (tiod->write_filter & ESC1)
							ZS_STR_OUT(&v,filesc_text);
						if (twoparms)
							ZS_ONE_OUT(&v,rparen_text);
						ZS_ONE_OUT(&v, space_text);
					}
					if (TT_EDITING & tt_ptr->ext_cap)
						ZS_PARM_SP(&v, zshow_edit);
					if (TT_NOINSERT & tt_ptr->ext_cap)
						ZS_PARM_SP(&v, zshow_noinse);
					if (TT_EMPTERM & tt_ptr->ext_cap)
						ZS_PARM_SP(&v, zshow_empterm);
					if (tt_ptr->canonical)
						ZS_STR_OUT(&v, "CANONICAL ");
					switch(tiod->ichset)
					{
					case CHSET_M:
						if (gtm_utf8_mode)
						{
							ZS_STR_OUT(&v, ichset_text);
							zshow_output(output, &chset_names[tiod->ichset]);
							ZS_ONE_OUT(&v, space_text);
						}
						break;
					case CHSET_UTF8:
						assert(gtm_utf8_mode);
						break;
					default:
						assertpro(tiod->ichset != tiod->ichset);
					}
					switch(tiod->ochset)
					{
					case CHSET_M:
						if (gtm_utf8_mode)
						{
							ZS_STR_OUT(&v, ochset_text);
							zshow_output(output, &chset_names[tiod->ochset]);
							ZS_ONE_OUT(&v, space_text);
						}
						break;
					case CHSET_UTF8:
						assert(gtm_utf8_mode);
						break;
					default:
						assertpro(tiod->ochset != tiod->ochset);
					}
					if (tt_ptr->mupintr)
						ZS_STR_OUT(&v, interrupt_text);
					break;
				case rm:
					/* we go to rm_ptr above for the rm type */

					if (rm_ptr->fifo)
						ZS_STR_OUT(&v,fifo_text);
					else if (!rm_ptr->is_pipe)
					{
						ZS_STR_OUT(&v,rmsfile_text);
						if (rm_ptr->follow)
						{
							ZS_PARM_SP(&v, zshow_follow);
						}
					}
					else
					{
						ZS_STR_OUT(&v,pipe_text);
						if (rm_ptr->dev_param_pairs.num_pairs)
						{
							int ignore_stderr = FALSE;
							/* if one of the dev_param_pairs[i]->name is the
							   STDERR then we don't want to output it if the
							   device is closed.  We'll check them all even though
							   it is currently the last one - just to be safe. */
							if (rm_ptr->stderr_child && (dev_open !=
										     rm_ptr->stderr_child->state))
								ignore_stderr = TRUE;

							for ( i = 0; i < rm_ptr->dev_param_pairs.num_pairs; i++ )
							{
								if (TRUE == ignore_stderr &&
								    0 == STRCMP(rm_ptr->dev_param_pairs.pairs[i].name,
										"STDERR="))
									continue;
								ZS_VAR_STR_OUT(
									&v,rm_ptr->dev_param_pairs.pairs[i].name);
								ZS_VAR_STR_OUT(
									&v,rm_ptr->dev_param_pairs.pairs[i].definition);
								ZS_ONE_OUT(&v, space_text);
							}
						}
						if (rm_ptr->independent)
						{
							ZS_PARM_SP(&v, zshow_independent);
						}
						if (rm_ptr->parse)
						{
							ZS_PARM_SP(&v, zshow_parse);
						}
					}
					if (rm_ptr->fixed)
					{
						ZS_PARM_SP(&v, zshow_fixed);
					} else if (rm_ptr->stream)
					{
						ZS_PARM_SP(&v, zshow_stream);
					}
					if (rm_ptr->read_only)
					{
						ZS_PARM_SP(&v, zshow_read);
					}
					if (gtm_utf8_mode && (IS_UTF_CHSET(tiod->ichset) || IS_UTF_CHSET(tiod->ochset)))
					{
						if (!rm_ptr->def_recsize)
						{
							ZS_PARM_EQU(&v, zshow_rec);
							MV_FORCE_MVAL(&m, (int)rm_ptr->recordsize);
							mval_write(output, &m, FALSE);
							ZS_ONE_OUT(&v, space_text);
						}
						if (!rm_ptr->def_width)
						{
							ZS_PARM_EQU(&v, zshow_width);
							MV_FORCE_MVAL(&m, (int)tiod->width);
							mval_write(output, &m, FALSE);
							ZS_ONE_OUT(&v, space_text);
						}
					}
					else if (tiod->width != DEF_RM_WIDTH)
					{
						ZS_PARM_EQU(&v, zshow_rec);
						MV_FORCE_MVAL(&m,(int)tiod->width);
						mval_write(output, &m, FALSE);
						ZS_ONE_OUT(&v, space_text);
					}
					if (!tiod->wrap)
					{
						ZS_PARM_SP(&v, zshow_nowrap);
					}
					switch(tiod->ichset)
					{
					case CHSET_M:
						if (gtm_utf8_mode)
						{
							ZS_STR_OUT(&v, ichset_text);
							zshow_output(output, &chset_names[tiod->ichset]);
							ZS_ONE_OUT(&v, space_text);
						}
						break;
					case CHSET_UTF8:
						assert(gtm_utf8_mode);
						break;
					case CHSET_UTF16:
					case CHSET_UTF16BE:
					case CHSET_UTF16LE:
						assert(gtm_utf8_mode);
						ZS_STR_OUT(&v, ichset_text);
						zshow_output(output, &chset_names[tiod->ichset]);
						ZS_ONE_OUT(&v, space_text);
						break;
					default:
						assertpro(tiod->ichset != tiod->ichset);
					}
					switch(tiod->ochset)
					{
					case CHSET_M:
						if (gtm_utf8_mode)
						{
							ZS_STR_OUT(&v, ochset_text);
							zshow_output(output, &chset_names[tiod->ochset]);
							ZS_ONE_OUT(&v, space_text);
						}
						break;
					case CHSET_UTF8:
						assert(gtm_utf8_mode);
						break;
					case CHSET_UTF16:
					case CHSET_UTF16BE:
					case CHSET_UTF16LE:
						assert(gtm_utf8_mode);
						ZS_STR_OUT(&v, ochset_text);
						zshow_output(output, &chset_names[tiod->ochset]);
						ZS_ONE_OUT(&v, space_text);
						break;
					default:
						assertpro(tiod->ochset != tiod->ochset);
					}
#ifdef __MVS__
					if (TAG_ASCII != tiod->file_tag)
					{
						ZS_STR_OUT(&v, filetag_text);
						switch ((unsigned int)tiod->file_tag)
						{
						case TAG_UNTAGGED:
							ZS_STR_OUT(&v, untagged_text);
							break;
						case TAG_EBCDIC:
							ZS_STR_OUT(&v, ebcdic_text);
							break;
						case TAG_BINARY:
							ZS_STR_OUT(&v, binary_text);
							break;
						default:
							if (-1 == __toCSName((__ccsid_t)tiod->file_tag, csname))
							{ /* no name so output number */
								csptr = (char *)i2asc((uchar_ptr_t)csname,
										      (unsigned int)tiod->file_tag);
								*csptr = '\0';	/* terminate */
							}
							ZS_VAR_STR_OUT(&v, csname);
						}
						if (tiod->text_flag)
							ZS_STR_OUT(&v, text_text);
						ZS_ONE_OUT(&v, space_text);
					}
					if (tiod->file_chset != tiod->process_chset &&
					    (!(0 == tiod->file_chset && CHSET_ASCII == tiod->process_chset) &&
					     !(CHSET_ASCII == tiod->file_chset && CHSET_M == tiod->process_chset)))
					{	/* suppress default cases */
						ZS_STR_OUT(&v, processchset_text);
						zshow_output(output, &chset_names[tiod->process_chset]);
						ZS_ONE_OUT(&v, space_text);
					}
#endif
					same_encr_settings = (rm_ptr->input_encrypted && rm_ptr->output_encrypted
						&& MSTR_EQ(&rm_ptr->input_key, &rm_ptr->output_key)
						&& MSTR_EQ(&rm_ptr->input_iv, &rm_ptr->output_iv));
					if (rm_ptr->input_encrypted)
					{
						if (same_encr_settings)
							ZS_STR_OUT(&v, key);
						else
							ZS_STR_OUT(&v, input_key);
						if (NULL != rm_ptr->input_key.addr)
						{
							v.str.addr = rm_ptr->input_key.addr;
							v.str.len = rm_ptr->input_key.len;
							zshow_output(output, &v.str);
						}
						ZS_ONE_OUT(&v, space_text);
						if (same_encr_settings)
							ZS_STR_OUT(&v, iv);
						else
							ZS_STR_OUT(&v, input_iv);
						if (NULL != rm_ptr->input_iv.addr)
						{
							v.str.addr = rm_ptr->input_iv.addr;
							v.str.len = rm_ptr->input_iv.len;
							zshow_output(output, &v.str);
						}
						ZS_ONE_OUT(&v, space_text);
					}
					if (!same_encr_settings && rm_ptr->output_encrypted)
					{
						ZS_STR_OUT(&v, output_key);
						if (NULL != rm_ptr->output_key.addr)
						{
							v.str.addr = rm_ptr->output_key.addr;
							v.str.len = rm_ptr->output_key.len;
							zshow_output(output, &v.str);
						}
						ZS_ONE_OUT(&v, space_text);
						ZS_STR_OUT(&v, output_iv);
						if (NULL != rm_ptr->output_iv.addr)
						{
							v.str.addr = rm_ptr->output_iv.addr;
							v.str.len = rm_ptr->output_iv.len;
							zshow_output(output, &v.str);
						}
						ZS_ONE_OUT(&v, space_text);
					}
					break;
				case gtmsocket:
					delim.addr = delim_mstr_buff;
					delim_len = 0;
					ZS_STR_OUT(&v, socket_text);
					dsocketptr = (d_socket_struct *)tiod->dev_sp;
					ZS_ONE_OUT(&v, space_text);
					if (!tiod->wrap)
					{
						ZS_PARM_SP(&v, zshow_nowrap);
					}
					ZS_STR_OUT(&v, total_text);
					MV_FORCE_MVAL(&m, (int)dsocketptr->n_socket);
					mval_write(output, &m, FALSE);
					ZS_ONE_OUT(&v, space_text);
					ZS_STR_OUT(&v, current_text);
					MV_FORCE_MVAL(&m, (int)dsocketptr->current_socket);
					mval_write(output, &m, FALSE);
					ZS_ONE_OUT(&v, space_text);
					if (dsocketptr->mupintr)
						ZS_STR_OUT(&v, interrupt_text);
					output->flush = TRUE;
					zshow_output(output, 0);
					for(ii = 0; ii < dsocketptr->n_socket; ii++)
					{
						/* output each socket */
						socketptr = dsocketptr->socket[ii];
						ZS_STR_OUT(&v, space8_text);
						/* socket handle */
						ZS_STR_OUT(&v, socket_text);
						ZS_ONE_OUT(&v, lb_text);
						MV_FORCE_MVAL(&m, ii);
						mval_write(output, &m, FALSE);
						ZS_ONE_OUT(&v, rb_text);
						ZS_ONE_OUT(&v, equal_text);
						v.str.addr = socketptr->handle;
						v.str.len = socketptr->handle_len;
						zshow_output(output, &v.str);
						ZS_ONE_OUT(&v, space_text);
						/* socket descriptor */
						ZS_STR_OUT(&v, descriptor_text);
						MV_FORCE_MVAL(&m, socketptr->sd);
						mval_write(output, &m, FALSE);
						ZS_ONE_OUT(&v, space_text);
						/* socket state */
						ZS_STR_OUT(&v, zsh_socket_state[socketptr->state]);
						ZS_ONE_OUT(&v, space_text);
						/* socket IO mode */		switch(tiod->ichset)
						{
						case CHSET_M:
							if (gtm_utf8_mode)
							{
								ZS_STR_OUT(&v, ichset_text);
								zshow_output(output, &chset_names[tiod->ichset]);
								ZS_ONE_OUT(&v, space_text);
							}
							break;
						case CHSET_UTF8:
							assert(gtm_utf8_mode);
							break;
						case CHSET_UTF16:
						case CHSET_UTF16BE:
						case CHSET_UTF16LE:
							assert(gtm_utf8_mode);
							ZS_STR_OUT(&v, ichset_text);
							zshow_output(output, &chset_names[tiod->ichset]);
							ZS_ONE_OUT(&v, space_text);
							break;
						default:
							assertpro(tiod->ichset != tiod->ichset);
						}
						switch(tiod->ochset)
						{
						case CHSET_M:
							if (gtm_utf8_mode)
							{
								ZS_STR_OUT(&v, ochset_text);
								zshow_output(output, &chset_names[tiod->ochset]);
								ZS_ONE_OUT(&v, space_text);
							}
							break;
						case CHSET_UTF8:
							assert(gtm_utf8_mode);
							break;
						case CHSET_UTF16:
						case CHSET_UTF16BE:
						case CHSET_UTF16LE:
							assert(gtm_utf8_mode);
							ZS_STR_OUT(&v, ochset_text);
							zshow_output(output, &chset_names[tiod->ochset]);
							ZS_ONE_OUT(&v, space_text);
							break;
						default:
							assertpro(tiod->ochset != tiod->ochset);
						}
						/* socket type */
						if (socketptr->passive)
						{
							ZS_STR_OUT(&v, passive_text);
						} else
						{
							ZS_STR_OUT(&v, active_text);
						}
						ZS_ONE_OUT(&v, space_text);
						/* error trapping */
						if (socketptr->ioerror)
						{
							ZS_STR_OUT(&v, trap_text);
						} else
						{
							ZS_STR_OUT(&v, notrap_text);
						}
						ZS_ONE_OUT(&v, space_text);
						/* address + port */
						if ((socket_local != socketptr->protocol) && socketptr->passive)
						{
							ZS_STR_OUT(&v, port_text);
							tmpport = (int)socketptr->local.port;
							MV_FORCE_MVAL(&m, tmpport);
							mval_write(output, &m, FALSE);
						} else
						{
							ZS_STR_OUT(&v, remote_text);
							if (NULL != socketptr->remote.saddr_ip)
							{
								v.str.addr = socketptr->remote.saddr_ip;
								v.str.len = STRLEN(socketptr->remote.saddr_ip);
							} else
							{
								v.str.addr = "";
								v.str.len = 0;
							}
							zshow_output(output, &v.str);
							if (socket_local != socketptr->protocol)
							{
								ZS_ONE_OUT(&v, at_text);
								tmpport = (int)socketptr->remote.port;
								MV_FORCE_MVAL(&m, tmpport);
								mval_write(output, &m, FALSE);
							}
							ZS_ONE_OUT(&v, space_text);
							if (socket_local == socketptr->protocol)
							{
								ZS_STR_OUT(&v, local_text);
								if (NULL != socketptr->local.sa)
								{
									charptr = ((struct sockaddr_un *)
										   (socketptr->local.sa))->sun_path;
									ZS_VAR_STR_OUT(&v, charptr);
								} else if (NULL != socketptr->remote.sa)
								{ /* CONNECT shows remote path as LOCAL */
									charptr = ((struct sockaddr_un *)
										   (socketptr->remote.sa))->sun_path;
									ZS_VAR_STR_OUT(&v, charptr);
								}
							} else if (NULL != socketptr->local.saddr_ip)
							{
								ZS_STR_OUT(&v, local_text);
								v.str.addr = socketptr->local.saddr_ip;
								v.str.len = STRLEN(socketptr->local.saddr_ip);
								zshow_output(output, &v.str);
								ZS_ONE_OUT(&v, at_text);
								tmpport = (int)socketptr->local.port;
								MV_FORCE_MVAL(&m, tmpport);
								mval_write(output, &m, FALSE);
							}
						}
						ZS_ONE_OUT(&v, space_text);
						output->flush = TRUE;
						zshow_output(output, 0);
						ZS_STR_OUT(&v, space8_text);
						ZS_STR_OUT(&v, space8_text);
						/* zdelay */
						if (socketptr->nodelay)
						{
							ZS_STR_OUT(&v, znodelay_text);
						} else
						{
							ZS_STR_OUT(&v, zdelay_text);
						}
						ZS_ONE_OUT(&v, space_text);
						/* zbfsize */
						ZS_STR_OUT(&v, zbfsize_text);
						MV_FORCE_MVAL(&m, (int4)(socketptr->buffer_size));
						mval_write(output, &m, FALSE);
						ZS_ONE_OUT(&v, space_text);
						/* izbfsize */
						ZS_STR_OUT(&v, zibfsize_text);
						MV_FORCE_MVAL(&m, socketptr->bufsiz);
						mval_write(output, &m, FALSE);
						ZS_ONE_OUT(&v, space_text);
						if (socketptr->tlsenabled && (NULL != socketptr->tlssocket))
						{
							ZS_STR_OUT(&v, tls_text);
						}
						/* delimiters */
						if (socketptr->n_delimiter > 0)
						{
							output->flush = TRUE;
							zshow_output(output, 0);
							ZS_STR_OUT(&v, space8_text);
							ZS_STR_OUT(&v, space8_text);
							ZS_STR_OUT(&v, delimiter_text);
							for (jj = 0; jj < socketptr->n_delimiter; jj++)
							{
								delim_len_sm = socketptr->delimiter[jj].len;
								memcpy(delim_buff_sm,
								       socketptr->delimiter[jj].addr, delim_len_sm);
								format2zwr(delim_buff_sm, delim_len_sm,
									   (uchar_ptr_t)delim.addr, &delim_len);
								delim.len = (unsigned short)delim_len;
								assert(SIZEOF(delim_mstr_buff) >= delim_len);
								zshow_output(output, &delim);
								ZS_ONE_OUT(&v, space_text);
							}
						} else
						{
							ZS_STR_OUT(&v, nodelimiter_text);
						}
						/* readmoretime */
						if (DEFAULT_MOREREAD_TIMEOUT != socketptr->moreread_timeout)
						{
							ZS_STR_OUT(&v, morereadtime_text);
							MV_FORCE_MVAL(&m, (int)socketptr->moreread_timeout);
							mval_write(output, &m, FALSE);
						}
						output->flush = TRUE;
						zshow_output(output, 0);
					}
				default:
					v.str.len = 0;
					break;
				}
				if (tiod->error_handler.len)
				{
					ZS_PARM_EQU(&v, zshow_exce);
					ZS_ONE_OUT(&v, quote_text);
					v.str = tiod->error_handler;
					zshow_output(output, &v.str);
					output->flush = TRUE;
					ZS_ONE_OUT(&v, quote_text);
				} else
				{	output->flush = TRUE;
					zshow_output(output, 0);
				}
			} else
			{	output->flush = TRUE;
				ZS_STR_OUT(&v, devcl);
			}
		}
	}
}
