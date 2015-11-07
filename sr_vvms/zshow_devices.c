/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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

#include <iodef.h>
#include <rms.h>
#include <trmdef.h>
#include <ttdef.h>
#include <tt2def.h>

#include "io.h"
#include "iottdef.h"
#include "iombdef.h"
#include "iomtdef.h"
#include "iormdef.h"
#include "iotcpdef.h"
#include "gt_timer.h"
#include "iosocketdef.h"
#include "nametabtyp.h"
#include "mlkdef.h"
#include "zshow.h"
#include "zshow_params.h"
#include "mvalconv.h"

typedef struct
{
	unsigned short mem;
	unsigned short grp;
} uic_struct;

LITREF nametabent dev_param_names[];
LITREF unsigned char dev_param_index[];
LITREF zshow_index zshow_param_index[];

static readonly char space_text[] = {' '};

#define ZS_ONE_OUT(V, TEXT) ((V)->str.len = 1, (V)->str.addr = (TEXT), zshow_output(output, &(V)->str))
#define ZS_STR_OUT(V, TEXT) ((V)->str.len = SIZEOF((TEXT)) - 1, (V)->str.addr = (TEXT), zshow_output(output, &(V)->str))
#define ZS_PARM_SP(V, TEXT) ((V)->str.len = dev_param_names[dev_param_index[zshow_param_index[(TEXT)].letter] + \
			zshow_param_index[(TEXT)].offset ].len, \
			(V)->str.addr = dev_param_names[dev_param_index[zshow_param_index[(TEXT)].letter] + \
			zshow_param_index[(TEXT)].offset ].name, zshow_output(output, &(V)->str), ZS_ONE_OUT((V), space_text))
#define ZS_PARM_EQU(V, TEXT) ((V)->str.len = dev_param_names[dev_param_index[zshow_param_index[(TEXT)].letter] + \
			zshow_param_index[(TEXT)].offset ].len, \
			(V)->str.addr = dev_param_names[dev_param_index[zshow_param_index[(TEXT)].letter] + \
			zshow_param_index[(TEXT)].offset ].name, zshow_output(output, &(V)->str), ZS_ONE_OUT((V), equal_text))

GBLREF bool ctrlc_on;
GBLREF io_log_name *io_root_log_name;
GBLREF io_pair *io_std_device;

void zshow_devices(zshow_out *output)
{
	static readonly char filchar_text[] = "CHARACTERS";
	static readonly char filesc_text[] = "ESCAPES";
	static readonly char terminal_text[] = "TERMINAL ";
	static readonly char magtape_text[] =  "MAGTAPE ";
	static readonly char rmsfile_text[] =  "RMS ";
	static readonly char mailbox_text[] =  "MAILBOX ";
	static readonly char dollarc_text[] = "$C(";
	static readonly char equal_text[] = {'='};
	static readonly char comma_text[] = {','};
	static readonly char quote_text[] = {'"'};
	static readonly char lparen_text[] = {'('};
	static readonly char rparen_text[] = {')'};
	static readonly char devop[] = "OPEN ";
	static readonly char devcl[] = "CLOSED ";
	static readonly char largerecord[] = "BIGRECORD ";
	static readonly char rfm_tab[][7] = {"UDF   ", "FIXED ", "VAR   ", "VFC   ", "STM   ", "STMLF ", "STMCR "};
	bool		first;
	int4		i, j, ii, jj;
	io_log_name	*l;		/* logical name pointer		*/
	mval		v;
	mval		m;
	struct FAB	*f;
	struct RAB	*r;
	d_mb_struct	*mb_ptr;
	d_mt_struct	*mt_ptr;
	d_rm_struct	*rm_ptr;
	d_tt_struct	*tt_ptr;
	d_socket_struct	*dsocketptr;
	socket_struct	*socketptr;
	io_termmask	*mask_out;
	unsigned char	delim_buff_sm[MAX_DELIM_LEN];
	unsigned short	delim_len_sm;
	char		delim_mstr_buff[(MAX_DELIM_LEN * MAX_ZWR_EXP_RATIO) + 11];
	mstr		delim;
	int		delim_len;
	static readonly char space8_text[] = "        "; 	/* starting from here, for gtmsocket only */
	static readonly char lb_text[] = {'['};
	static readonly char rb_text[] = {']'};
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
	static readonly char zsh_socket_state[][10] =
					{	"CONNECTED"
						,"LISTENING"
						,"BOUND"
						,"CREATED"
					};
	static readonly char morereadtime_text[] = "MOREREADTIME=";

	v.mvtype = MV_STR;
	for (l = io_root_log_name;  l != 0;  l = l->next)
	{
		if (l->dollar_io[0] != IO_ESC && l->iod->trans_name == l)
		{
			v.str.addr = &l->dollar_io[0];
			v.str.len = l->len;
			zshow_output(output, &v.str);
			ZS_ONE_OUT(&v, space_text);
			if (l->iod->state == dev_open)
			{
				ZS_STR_OUT(&v, devop);
				switch(l->iod->type)
				{
				case tt:
					ZS_STR_OUT(&v, terminal_text);
					tt_ptr = (d_tt_struct *)l->iod->dev_sp;
					if (!ctrlc_on && io_std_device->out == l->iod) /* and standard input */
						ZS_PARM_SP(&v, zshow_nocene);
					if ((int4)(tt_ptr->item_list[0].addr) & TRM$M_TM_CVTLOW)
						ZS_PARM_SP(&v, zshow_conv);
					if (tt_ptr->enbld_outofbands.mask)
					{
						ZS_PARM_EQU(&v, zshow_ctra);
						ZS_STR_OUT(&v, dollarc_text);
						first = TRUE;
						for (i = 1, j = 0;  j < 32;  j++, i = i * 2)
						{
							if (i & tt_ptr->enbld_outofbands.mask)
							{
								if (!first)
									ZS_ONE_OUT(&v, comma_text);
								else
									first = FALSE;
								MV_FORCE_MVAL(&m, j);
								mval_write(output, &m, FALSE);
							}
						}
						ZS_ONE_OUT(&v, rparen_text);
						ZS_ONE_OUT(&v, space_text);
					}
					if ((int4)(tt_ptr->term_char) & TT$M_NOECHO)
						ZS_PARM_SP(&v, zshow_noecho);
					if ((int4)(tt_ptr->ext_cap) & TT2$M_EDITING)
						ZS_PARM_SP(&v, zshow_edit);
					else
						ZS_PARM_SP(&v, zshow_noedit);
					if (!((int4)(tt_ptr->term_char) & TT$M_ESCAPE))
						ZS_PARM_SP(&v, zshow_noesca);
					if (tt_ptr->in_buf_sz != TTDEF_BUF_SZ)
					{
						ZS_PARM_EQU(&v, zshow_field);
						MV_FORCE_MVAL(&m, tt_ptr->in_buf_sz);
						mval_write(output, &m, FALSE);
						ZS_ONE_OUT(&v, space_text);
					}
					if (tt_ptr->term_char & TT$M_HOSTSYNC)
						ZS_PARM_SP(&v, zshow_host);
					else
						ZS_PARM_SP(&v, zshow_nohost);
					if (tt_ptr->ext_cap & TT2$M_INSERT)
						ZS_PARM_SP(&v, zshow_inse);
					else
						ZS_PARM_SP(&v, zshow_noinse);
					if (tt_ptr->ext_cap & TT2$M_PASTHRU)
						ZS_PARM_SP(&v, zshow_past);
					else
						ZS_PARM_SP(&v, zshow_nopast);
					if (tt_ptr->term_char & TT$M_READSYNC)
						ZS_PARM_SP(&v, zshow_reads);
					else
						ZS_PARM_SP(&v, zshow_noreads);
					if (tt_ptr->term_char & TT$M_TTSYNC)
						ZS_PARM_SP(&v, zshow_ttsy);
					else
						ZS_PARM_SP(&v, zshow_nottsy);
					if (tt_ptr->term_char & TT$M_NOTYPEAHD)
						ZS_PARM_SP(&v, zshow_notype);
					else
						ZS_PARM_SP(&v, zshow_type);
					if (!l->iod->wrap)
						ZS_PARM_SP(&v, zshow_nowrap);
					mask_out = tt_ptr->item_list[2].addr;
					if (mask_out->mask[0] != TERM_MSK)
					{
						ZS_PARM_EQU(&v, zshow_term);
						ZS_STR_OUT(&v, dollarc_text);
						first = TRUE;
						for (i = 0;  i < 8;  i++)
						{
							for (j = 0;  j < 32;  j++)
								if (mask_out->mask[i] & (1 << j))
								{
									if (!first)
										ZS_ONE_OUT(&v, comma_text);
									else
										first = FALSE;
									MV_FORCE_MVAL(&m, i * 32 + j);
									mval_write(output, &m, FALSE);
								}
						}
						ZS_ONE_OUT(&v, rparen_text);
						ZS_ONE_OUT(&v, space_text);
					}
					ZS_PARM_EQU(&v, zshow_width);
					MV_FORCE_MVAL(&m, l->iod->width);
					mval_write(output, &m, FALSE);
					ZS_ONE_OUT(&v, space_text);
					ZS_PARM_EQU(&v, zshow_leng);
					MV_FORCE_MVAL(&m, l->iod->pair.out->length);
					mval_write(output, &m, FALSE);
					ZS_ONE_OUT(&v, space_text);
					if (l->iod->write_filter)
					{
						bool twoparms = FALSE;

						ZS_PARM_EQU(&v, zshow_fil);
						if (l->iod->write_filter & CHAR_FILTER)
						{
							if (l->iod->write_filter & ESC1)
							{
								twoparms = TRUE;
								ZS_ONE_OUT(&v, lparen_text);
							}
							ZS_STR_OUT(&v, filchar_text);
							if (twoparms)
							{
								ZS_ONE_OUT(&v, comma_text);
								ZS_ONE_OUT(&v, space_text);
							}
						}
						if (l->iod->write_filter & ESC1)
							ZS_STR_OUT(&v, filesc_text);
						if (twoparms)
							ZS_ONE_OUT(&v, rparen_text);
						ZS_ONE_OUT(&v, space_text);
					}
					break;
				case mt:
					ZS_STR_OUT(&v, magtape_text);
					mt_ptr = (d_tt_struct *)l->iod->dev_sp;
					if (mt_ptr->block_sz != MTDEF_BUF_SZ)
					{
						ZS_PARM_EQU(&v, zshow_bloc);
						MV_FORCE_MVAL(&m, mt_ptr->block_sz);
						mval_write(output, &m, FALSE);
						ZS_ONE_OUT(&v, space_text);
					}
					if (mt_ptr->record_sz != MTDEF_REC_SZ)
					{
						ZS_PARM_EQU(&v, zshow_rec);
						MV_FORCE_MVAL(&m, mt_ptr->record_sz);
						mval_write(output, &m, FALSE);
						ZS_ONE_OUT(&v, space_text);
					}
					if (mt_ptr->read_only)
						ZS_PARM_SP(&v, zshow_read);
					if (mt_ptr->ebcdic)
						ZS_PARM_SP(&v, zshow_ebcd);
					if (mt_ptr->fixed)
						ZS_PARM_SP(&v, zshow_fixed);
					if (mt_ptr->read_mask & IO$M_DATACHECK)
						ZS_PARM_SP(&v, zshow_rchk);
					if (mt_ptr->write_mask & IO$M_DATACHECK)
						ZS_PARM_SP(&v, zshow_wchk);
					if (!l->iod->wrap)
						ZS_PARM_SP(&v, zshow_nowrap);
					break;
				case rm:
					ZS_STR_OUT(&v, rmsfile_text);
					rm_ptr = (d_rm_struct *)l->iod->dev_sp;
					f = &rm_ptr->f;
					r = &rm_ptr->r;
					if (r->rab$l_rop & RAB$M_CVT)
						ZS_PARM_SP(&v, zshow_conv);
					if (f->fab$l_alq != 0)
					{
						ZS_PARM_EQU(&v, zshow_allo);
						MV_FORCE_MVAL(&m, f->fab$l_alq);
						mval_write(output, &m, FALSE);
						ZS_ONE_OUT(&v, space_text);
					}
					if (f->fab$l_fop & FAB$M_DLT)
						ZS_PARM_SP(&v, zshow_dele);
					if (f->fab$w_deq != 0)
					{
						ZS_PARM_EQU(&v, zshow_exte);
						MV_FORCE_MVAL(&m, f->fab$w_deq);
						mval_write(output, &m, FALSE);
						ZS_ONE_OUT(&v, space_text);
					}
					assert(FAB$C_MAXRFM == (SIZEOF(rfm_tab)/SIZEOF(rfm_tab[0]) - 1));
					if (FAB$C_MAXRFM >= rm_ptr->b_rfm && FAB$C_RFM_DFLT != rm_ptr->b_rfm)
						ZS_STR_OUT(&v, rfm_tab[rm_ptr->b_rfm]);
					if (rm_ptr->largerecord)
						ZS_STR_OUT(&v, largerecord);
					if (f->fab$b_fac == FAB$M_GET)
						ZS_PARM_SP(&v, zshow_read);
					if (rm_ptr->l_mrs != DEF_RM_WIDTH)
					{
						ZS_PARM_EQU(&v, zshow_rec);
						MV_FORCE_MVAL(&m, rm_ptr->l_mrs);
						mval_write(output, &m, FALSE);
						ZS_ONE_OUT(&v, space_text);
					}
/* smw should we add width aka usz */
					if (f->fab$b_shr & FAB$M_SHRGET)
						ZS_PARM_SP(&v, zshow_shar);
	/*
					if (f->fab$l_fop & FAB$M_SPL)
						ZS_PARM_SP(&v, zshow_spo);
					if (f->fab$l_fop & FAB$M_SCF)
						ZS_PARM_SP(&v, zshow_sub);
	*/
					if (!l->iod->wrap)
						ZS_PARM_SP(&v, zshow_nowrap);
					if (f->fab$l_xab)
					{
/* smw need to handle other XABs and uic as octal */
						struct XABPRO	*xabpro;
						uic_struct uic;

						ZS_PARM_EQU(&v, zshow_uic);
						ZS_ONE_OUT(&v, quote_text);
						xabpro = f->fab$l_xab;
						memcpy(&uic, &xabpro->xab$l_uic, SIZEOF(int4));
						MV_FORCE_MVAL(&m, uic.grp);
						mval_write(output, &m, FALSE);
						ZS_ONE_OUT(&v, comma_text);
						MV_FORCE_MVAL(&m, uic.mem);
						mval_write(output, &m, FALSE);
						ZS_ONE_OUT(&v, quote_text);
						ZS_ONE_OUT(&v, space_text);
					}
					break;
				case mb:
					ZS_STR_OUT(&v, mailbox_text);
					mb_ptr = (d_mb_struct *)l->iod->dev_sp;
					if (!(mb_ptr->write_mask & IO$M_NOW))
						ZS_PARM_SP(&v, zshow_wait);
					if (mb_ptr->prmflg)
						ZS_PARM_SP(&v, zshow_prmmbx);
					if (mb_ptr->maxmsg != DEF_MB_MAXMSG)
					{
						ZS_PARM_EQU(&v, zshow_bloc);
						MV_FORCE_MVAL(&m, mb_ptr->maxmsg);
						mval_write(output, &m, FALSE);
						ZS_ONE_OUT(&v, space_text);
					}
					if (mb_ptr->promsk & IO_RD_ONLY)
						ZS_PARM_SP(&v, zshow_read);
					if (mb_ptr->del_on_close)
						ZS_PARM_SP(&v, zshow_dele);
					if (mb_ptr->promsk & IO_SEQ_WRT)
						ZS_PARM_SP(&v, zshow_write);
					if (l->iod->width != DEF_MB_MAXMSG)
					{
						ZS_PARM_EQU(&v, zshow_width);
						MV_FORCE_MVAL(&m, l->iod->width);
						mval_write(output, &m, FALSE);
						ZS_ONE_OUT(&v, space_text);
					}
					if (!l->iod->wrap)
						ZS_PARM_SP(&v, zshow_nowrap);
					if (l->iod->length != DEF_MB_LENGTH)
					{
						ZS_PARM_EQU(&v, zshow_leng);
						MV_FORCE_MVAL(&m, l->iod->length);
						mval_write(output, &m, FALSE);
						ZS_ONE_OUT(&v, space_text);
					}
					break;
				case gtmsocket:
					delim.addr = delim_mstr_buff;
					delim_len = 0;
					ZS_STR_OUT(&v, socket_text);
					dsocketptr = (d_socket_struct *)l->iod->dev_sp;
					ZS_ONE_OUT(&v, space_text);
					ZS_STR_OUT(&v, total_text);
					MV_FORCE_MVAL(&m, (int)dsocketptr->n_socket);
					mval_write(output, &m, FALSE);
					ZS_ONE_OUT(&v, space_text);
					ZS_STR_OUT(&v, current_text);
					MV_FORCE_MVAL(&m, (int)dsocketptr->current_socket);
					mval_write(output, &m, FALSE);
					output->flush = TRUE;
					zshow_output(output, 0);
					for(ii = 0; ii < dsocketptr->n_socket; ii++)
					{
						/* output each socket */
						socketptr = dsocketptr->socket[ii];
						ZS_STR_OUT(&v, space8_text);
		/* socket handle */		ZS_STR_OUT(&v, socket_text);
						ZS_ONE_OUT(&v, lb_text);
						MV_FORCE_MVAL(&m, ii);
						mval_write(output, &m, FALSE);
						ZS_ONE_OUT(&v, rb_text);
						ZS_ONE_OUT(&v, equal_text);
						v.str.addr = socketptr->handle;
						v.str.len = socketptr->handle_len;
						zshow_output(output, &v.str);
						ZS_ONE_OUT(&v, space_text);
		/* socket descriptor */		ZS_STR_OUT(&v, descriptor_text);
						MV_FORCE_MVAL(&m, socketptr->sd);
						mval_write(output, &m, FALSE);
						ZS_ONE_OUT(&v, space_text);
		/* socket state */		ZS_STR_OUT(&v, zsh_socket_state[socketptr->state]);
						ZS_ONE_OUT(&v, space_text);
		/* socket type */		if (socketptr->passive)
						{
							ZS_STR_OUT(&v, passive_text);
						} else
						{
							ZS_STR_OUT(&v, active_text);
						}
						ZS_ONE_OUT(&v, space_text);
		/* error trapping */		if (socketptr->ioerror)
						{
							ZS_STR_OUT(&v, trap_text);
						} else
						{
							ZS_STR_OUT(&v, notrap_text);
						}
						ZS_ONE_OUT(&v, space_text);
		/* address + port */		if (socketptr->passive)
						{
							ZS_STR_OUT(&v, port_text);
							MV_FORCE_MVAL(&m, (int)socketptr->local.port);
							mval_write(output, &m, FALSE);
						} else
						{
							ZS_STR_OUT(&v, remote_text);
							if (NULL != socketptr->remote.saddr_ip)
							{
								v.str.addr = socketptr->remote.saddr_ip;
								v.str.len = strlen(socketptr->remote.saddr_ip);
							} else
							{
								v.str.addr = "";
								v.str.len = 0;
							}
							zshow_output(output, &v.str);
							ZS_ONE_OUT(&v, at_text);
							MV_FORCE_MVAL(&m, (int)socketptr->remote.port);
							mval_write(output, &m, FALSE);
							ZS_ONE_OUT(&v, space_text);
							/* to be added later ...
								ZS_STR_OUT(&v, local_text);
								v.str.addr = socketptr->local.saddr_ip;
								v.str.len = strlen(socketptr->local.saddr_ip);
								zshow_output(output, &v.str);
								ZS_ONE_OUT(&v, at_text);
								MV_FORCE_MVAL(&m, (int)socketptr->local.port);
								mval_write(output, &m, FALSE);
							*/
						}
						ZS_ONE_OUT(&v, space_text);
						output->flush = TRUE;
						zshow_output(output, 0);
						ZS_STR_OUT(&v, space8_text);
						ZS_STR_OUT(&v, space8_text);
		/* zdelay */			if (socketptr->nodelay)
						{
							ZS_STR_OUT(&v, znodelay_text);
						} else
						{
							ZS_STR_OUT(&v, zdelay_text);
						}
						ZS_ONE_OUT(&v, space_text);
		/* zbfsize */			ZS_STR_OUT(&v, zbfsize_text);
						MV_FORCE_MVAL(&m, socketptr->buffer_size);
						mval_write(output, &m, FALSE);
						ZS_ONE_OUT(&v, space_text);
		/* izbfsize */			ZS_STR_OUT(&v, zibfsize_text);
						MV_FORCE_MVAL(&m, socketptr->bufsiz);
						mval_write(output, &m, FALSE);
						ZS_ONE_OUT(&v, space_text);
		/* delimiters */		if (socketptr->n_delimiter > 0)
						{
							output->flush = TRUE;
							zshow_output(output, 0);
							ZS_STR_OUT(&v, space8_text);
							ZS_STR_OUT(&v, space8_text);
							ZS_STR_OUT(&v, delimiter_text);
							for (jj = 0; jj < socketptr->n_delimiter; jj++)
							{
								delim_len_sm = socketptr->delimiter[jj].len;
								memcpy(delim_buff_sm, socketptr->delimiter[jj].addr, delim_len_sm);
								format2zwr(delim_buff_sm, delim_len_sm, (uchar_ptr_t)delim.addr,
									&delim_len);
								delim.len = (unsigned short) delim_len;
								assert(SIZEOF(delim_mstr_buff) >= delim_len);
								zshow_output(output, &delim);
								ZS_ONE_OUT(&v, space_text);
							}
						} else
						{
							ZS_STR_OUT(&v, nodelimiter_text);
						}
		/* readmoretime */		if (DEFAULT_MOREREAD_TIMEOUT != socketptr->moreread_timeout)
						{
							ZS_STR_OUT(&v, morereadtime_text);
							MV_FORCE_MVAL(&m, socketptr->moreread_timeout);
							mval_write(output, &m, FALSE);
						}
						output->flush = TRUE;
						zshow_output(output, 0);
					}
				default:
					v.str.len = 0;
					break;
				}
				if (l->iod->error_handler.len)
				{
					ZS_PARM_EQU(&v, zshow_exce);
					ZS_ONE_OUT(&v, quote_text);
					v.str = l->iod->error_handler;
					zshow_output(output, &v.str);
					output->flush = TRUE;
					ZS_ONE_OUT(&v, quote_text);
				} else
				{
					output->flush = TRUE;
					zshow_output(output, 0);
				}
			}
			else
			{
				output->flush = TRUE;
				ZS_STR_OUT(&v, devcl);
			}
		}
	}
}
