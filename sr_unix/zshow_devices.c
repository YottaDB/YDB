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

#include "mdef.h"

#include "gtm_string.h"

#include <sys/socket.h>
#include <netinet/in.h>

#include "gtm_stdio.h"

#include "hashdef.h"
#include "lv_val.h"
#include "mlkdef.h"
#include "zshow.h"
#include "io.h"
#include "iottdef.h"
#include "trmdef.h"
#include "iombdef.h"
#include "iormdef.h"
#include "iotcpdef.h"
#include "gt_timer.h"
#include "iosocketdef.h"
#include "zshow_params.h"
#include "nametabtyp.h"
#include "mvalconv.h"

LITREF nametabent dev_param_names[];
LITREF unsigned char dev_param_index[];
LITREF zshow_index zshow_param_index[];

static readonly char space_text[] = {' '};

#define ZS_ONE_OUT(V,TEXT) ((V)->str.len = 1, (V)->str.addr = (TEXT), zshow_output(output,&(V)->str))
#define ZS_STR_OUT(V,TEXT) ((V)->str.len = sizeof((TEXT)) - 1, (V)->str.addr = (TEXT), zshow_output(output,&(V)->str))
#define ZS_PARM_SP(V,TEXT) ((V)->str.len = dev_param_names[dev_param_index[zshow_param_index[(TEXT)].letter] + \
			zshow_param_index[(TEXT)].offset ].len, \
			(V)->str.addr = (char *)dev_param_names[dev_param_index[zshow_param_index[(TEXT)].letter] + \
			zshow_param_index[(TEXT)].offset ].name, zshow_output(output,&(V)->str), ZS_ONE_OUT((V),space_text))
#define ZS_PARM_EQU(V,TEXT) ((V)->str.len = dev_param_names[dev_param_index[zshow_param_index[(TEXT)].letter] + \
			zshow_param_index[(TEXT)].offset ].len, \
			(V)->str.addr = (char *)dev_param_names[dev_param_index[zshow_param_index[(TEXT)].letter] + \
			zshow_param_index[(TEXT)].offset ].name, zshow_output(output,&(V)->str), ZS_ONE_OUT((V),equal_text))

GBLREF bool ctrlc_on;
GBLREF io_log_name *io_root_log_name;
GBLREF io_pair *io_std_device;

void zshow_devices(zshow_out *output)
{
	io_log_name	*l;		/* logical name pointer		*/
	io_log_name	*prev = 0;	/* previous logical name	*/
	mval		v;
	mval		m;
	d_rm_struct	*rm_ptr;
	d_mb_struct	*mb_ptr;
	d_tt_struct	*tt_ptr;
	d_socket_struct *dsocketptr;
	socket_struct	*socketptr;
	io_termmask	*mask_out;
	int4		i, j, ii, jj;
	bool		first;
	sm_uc_ptr_t	delim_buff_sm;
	unsigned short  delim_len_sm;
	mstr		delim;
	int		delim_len;
	static readonly char space8_text[] = "        ";
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
	static readonly char lb_text[] = {'['};
	static readonly char rb_text[] = {']'};
	static readonly char devop[] = "OPEN ";
	static readonly char devcl[] = "CLOSED ";

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
	static readonly char zsh_socket_state[][10] =
					{       "CONNECTED"
					        ,"LISTENING"
					        ,"BOUND"
					        ,"CREATED"
					};

	v.mvtype = MV_STR;
	for (l = io_root_log_name;  l != 0;  prev = l, l = l->next)
	{
		if (l->iod->trans_name == l)
		{
			v.str.addr = &l->dollar_io[0];
			v.str.len = l->len;
			zshow_output(output,&v.str);
			ZS_ONE_OUT(&v,space_text);
			if (l->iod->state == dev_open)
			{
				ZS_STR_OUT(&v, devop);
				switch(l->iod->type)
				{
				case tt:
					ZS_STR_OUT(&v, terminal_text);
					tt_ptr = (d_tt_struct*)l->iod->dev_sp;
					if (!ctrlc_on && io_std_device->out == l->iod) /* and standard input */
					{	ZS_PARM_SP(&v,zshow_nocene);
					}
					if (tt_ptr->enbld_outofbands.mask)
					{	ZS_PARM_EQU(&v,zshow_ctra);
						ZS_STR_OUT(&v,dollarc_text);
						first = TRUE;
						for ( i = 1, j = 0; j < 32  ; j++,i = i * 2)
						{	if (i & tt_ptr->enbld_outofbands.mask)
							{	if (!first)
								{	ZS_ONE_OUT(&v,comma_text);
								}else
								{	first = FALSE;
								}
								MV_FORCE_MVAL(&m,j) ;
								mval_write(output,&m,FALSE);
							}
						}
						ZS_ONE_OUT(&v, rparen_text);
						ZS_ONE_OUT(&v,space_text);
					}
					if ((int4)(tt_ptr->term_ctrl) & TRM_NOECHO)
					{
						ZS_PARM_SP(&v,zshow_noecho);
					}
					if (tt_ptr->term_ctrl & TRM_PASTHRU)
					{
						ZS_PARM_SP(&v,zshow_past);
					} else
					{
						ZS_PARM_SP(&v,zshow_nopast);
					}
					if (!(tt_ptr->term_ctrl & TRM_ESCAPE))
					{
						ZS_PARM_SP(&v,zshow_noesca);
					}
					if (tt_ptr->term_ctrl & TRM_READSYNC)
					{
						ZS_PARM_SP(&v,zshow_reads);
					} else
					{
						ZS_PARM_SP(&v,zshow_noreads);
					}
					if (tt_ptr->term_ctrl & TRM_NOTYPEAHD)
					{
						ZS_PARM_SP(&v,zshow_notype);
					} else
					{
						ZS_PARM_SP(&v,zshow_type);
					}
					if (!l->iod->wrap)
					{
						ZS_PARM_SP(&v,zshow_nowrap);
					}
					mask_out = &tt_ptr->mask_term;
					if (mask_out->mask[0] != TERM_MSK)
					{
						ZS_PARM_EQU(&v,zshow_term);
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
									MV_FORCE_MVAL(&m,i * 32 + j) ;
									mval_write(output,&m,FALSE);
								}
						}
						ZS_ONE_OUT(&v, rparen_text);
						ZS_ONE_OUT(&v,space_text);
					}
					ZS_PARM_EQU(&v,zshow_width);
					MV_FORCE_MVAL(&m,(int) l->iod->width) ;
					mval_write(output, &m, FALSE);
					ZS_ONE_OUT(&v,space_text);
					ZS_PARM_EQU(&v,zshow_leng);
					MV_FORCE_MVAL(&m,(int) l->iod->pair.out->length) ;
					mval_write(output, &m, FALSE);
					ZS_ONE_OUT(&v,space_text);
					if (l->iod->write_filter)
					{
						bool twoparms = FALSE;

						ZS_PARM_EQU(&v,zshow_fil);
						if (l->iod->write_filter & CHAR_FILTER)
						{
							if (l->iod->write_filter & ESC1)
							{
								twoparms = TRUE;
								ZS_ONE_OUT(&v,lparen_text);
							}
							ZS_STR_OUT(&v,filchar_text);
							if (twoparms)
							{
								ZS_ONE_OUT(&v,comma_text);
								ZS_ONE_OUT(&v,space_text);
							}
						}
						if (l->iod->write_filter & ESC1)
							ZS_STR_OUT(&v,filesc_text);
						if (twoparms)
							ZS_ONE_OUT(&v,rparen_text);
						ZS_ONE_OUT(&v,space_text);
					}
					break;
				case rm:
					ZS_STR_OUT(&v,rmsfile_text);
					rm_ptr = (d_rm_struct*) l->iod->dev_sp;
					if (rm_ptr->fixed)
					{
						ZS_PARM_SP(&v,zshow_fixed);
					}
					if (rm_ptr->noread)
					{
						ZS_PARM_SP(&v,zshow_read);
					}
					if (l->iod->width != DEF_RM_WIDTH)
					{
						ZS_PARM_EQU(&v,zshow_rec);
						MV_FORCE_MVAL(&m,(int) l->iod->width);
						mval_write(output, &m, FALSE);
						ZS_ONE_OUT(&v,space_text);
					}
					if (!l->iod->wrap)
					{
						ZS_PARM_SP(&v,zshow_nowrap);
					}
					break;
				case mb:
					ZS_STR_OUT(&v,mailbox_text);
					mb_ptr = (d_mb_struct*) l->iod->dev_sp;
					if (mb_ptr->write_mask)
					{
						ZS_PARM_SP(&v,zshow_wait);
					}
					if (mb_ptr->prmflg)
					{
						ZS_PARM_SP(&v,zshow_prmmbx);
					}
					if (mb_ptr->maxmsg != DEF_MB_MAXMSG)
					{
						ZS_PARM_EQU(&v,zshow_bloc);
						MV_FORCE_MVAL(&m,mb_ptr->maxmsg) ;
						mval_write(output, &m, FALSE);
						ZS_ONE_OUT(&v,space_text);
					}
					if (mb_ptr->promsk & IO_RD_ONLY)
					{
						ZS_PARM_SP(&v,zshow_read);
					}
					if (mb_ptr->del_on_close)
					{
						ZS_PARM_SP(&v,zshow_dele);
					}
					if (mb_ptr->promsk & IO_SEQ_WRT)
					{
						ZS_PARM_SP(&v,zshow_write);
					}
					break;
				case gtmsocket:
					delim_buff_sm = (sm_uc_ptr_t) malloc(MAX_DELIM_LEN);
					delim.addr = (char *) malloc(MAX_DELIM_LEN * 7);
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
							v.str.addr = socketptr->remote.saddr_ip;
							v.str.len = strlen(socketptr->remote.saddr_ip);
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
		/* zbfsize */ 			ZS_STR_OUT(&v, zbfsize_text);
						MV_FORCE_MVAL(&m, socketptr->buffer_size);
						mval_write(output, &m, FALSE);
						ZS_ONE_OUT(&v, space_text);
		/* izbfsize */ 			ZS_STR_OUT(&v, zibfsize_text);
						MV_FORCE_MVAL(&m, socketptr->bufsiz);
						mval_write(output, &m, FALSE);
						ZS_ONE_OUT(&v, space_text);
		/* delimiters */ 		if (socketptr->n_delimiter > 0)
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
								zshow_output(output, &delim);
								ZS_ONE_OUT(&v, space_text);
							}
						} else
						{
							ZS_STR_OUT(&v, nodelimiter_text);
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
