/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "error.h"

#include <descrip.h>
#include <rms.h>
#include <opcdef.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"

typedef struct {
	char			req_code;
	char			target[3];
	int4			mess_code;
	char			text[255];
}oper_msg_struct;

#define MESSAGE_HDR_LEN 15

error_def(ERR_GVUNDEF);
error_def(ERR_SERVERERR);

GBLDEF	bool			gtcm_errfile=FALSE;
GBLDEF	struct FAB		gtcm_errfab;
GBLDEF	struct RAB		gtcm_errrab;
GBLDEF	bool			gtcm_firsterr=TRUE;

GBLREF	connection_struct	*curr_entry;
GBLREF	bool			undef_inhibit;

CONDITION_HANDLER(gtcm_ch)
{
	bool			first;
	int			prev_count,count;
	uint4		stat;
	int			i;
	oper_msg_struct		oper_msg;
	bool			severe;
	unsigned short		faolen,severe_len;
	char			outadr[4];
	char			*ptr,*end, *tempptr, *buffptr;
	uint4		flags, len, *argptr;
	unsigned short		msg_len;
	uint4		*c;
	char			buff[512],buff1[512],buff2[512],tempbuf[2048];
	cmi_descriptor		*tempdesc,*tempdesc1;
	void			gtcm_write_ast(), preemptive_db_clnup(int);
	char			errout[259],errbuff[300];
	char			filename[]="SYS$MANAGER:CMERR.LOG";
/*	char			filename[]="SYS$LOGIN:CMERR.LOG"; not defined by run/det */
	$DESCRIPTOR(msg,buff);
	$DESCRIPTOR(msg2,buff2);
	$DESCRIPTOR(err1,errout);
	$DESCRIPTOR(out,"");
	$DESCRIPTOR(out1,"");

	undef_inhibit = FALSE;	/* reset undef_inhibit to the default value in case it got reset temporarily and there was an error.
				 * currently, only $INCREMENT temporarily resets this (in gtcmtr_increment.c)
				 */
	severe = SEVERITY >= 4;	/* a severe error */
	preemptive_db_clnup(SEVERITY);
	if (gtcm_firsterr)				/* open error logging file */
	{	gtcm_errfab = cc$rms_fab;
		gtcm_errrab = cc$rms_rab;
		gtcm_errrab.rab$l_fab = &gtcm_errfab;
		gtcm_errrab.rab$l_rop = RAB$M_WBH;
		gtcm_errfab.fab$b_fns = SIZEOF(filename);
		gtcm_errfab.fab$l_fna = filename;
		gtcm_errfab.fab$b_shr = FAB$M_SHRGET | FAB$M_UPI;
		gtcm_errfab.fab$l_fop = FAB$M_MXV | FAB$M_CBT;
		gtcm_errfab.fab$b_fac = FAB$M_PUT;
		gtcm_errfab.fab$b_rat = FAB$M_CR;
		stat = sys$create(&gtcm_errfab);
		if (stat & 1)
		{	stat = sys$connect(&gtcm_errrab);
			if (stat == RMS$_NORMAL)
			{	gtcm_errfile = TRUE;
			}
		}
		gtcm_firsterr = FALSE;
	}
	if (curr_entry)
	{	ptr = curr_entry->clb_ptr->mbf;
		end = ptr + curr_entry->clb_ptr->mbl;
	}else
	{	ptr = &tempbuf[0];
		end = &tempbuf[2047];
	}
	argptr = &SIGNAL;
	*ptr++ = CMMS_E_ERROR;
	*ptr++ = 0;
	*ptr++ = sig->chf$l_sig_args - 2;	/* discount exception pc and psl */
	flags = 15;
	buffptr = &buff1[0];
	prev_count = 0;
	for (count = 0; count < sig->chf$l_sig_args - 2;)
	{	count++;
		msg_len = 0;
		msg.dsc$w_length = 512;
		sys$getmsg(*argptr,&msg_len,&msg,flags,outadr);
		if ((gtcm_errfile || severe) && *argptr != 0)
		{	msg.dsc$w_length = msg_len;
			out.dsc$w_length = 255;
			out.dsc$a_pointer = &oper_msg.text[MESSAGE_HDR_LEN];
			c = argptr + 1;
			faolen = 0;
			if (sig->chf$l_sig_args > 3)
			{	switch(*c)
				{
					case 0:
						sys$fao(&msg, &faolen, &out);
						break;
					case 1:
						sys$fao(&msg, &faolen, &out, *(c+1));
						break;
					case 2:
						sys$fao(&msg, &faolen, &out, *(c+1), *(c+2));
						break;
					case 3:
						sys$fao(&msg, &faolen, &out, *(c+1),*(c+2),*(c+3));
						break;
					case 4:
						sys$fao(&msg, &faolen, &out, *(c+1),*(c+2),*(c+3),*(c+4));
						break;
					case 5:
						sys$fao(&msg, &faolen, &out, *(c+1),*(c+2),*(c+3),*(c+4),*(c+5));
						break;
					case 6:
						sys$fao(&msg, &faolen, &out, *(c+1),*(c+2),*(c+3),*(c+4),*(c+5),*(c+6));
						break;
				}
			}else
			{	sys$fao(&msg, &faolen, &out);
			}
			severe_len = faolen;
			if (gtcm_errfile)
			{	memcpy(errout,"!%D ",4);
				memcpy(errout + 4,&oper_msg.text[MESSAGE_HDR_LEN],faolen);
				err1.dsc$w_length =  faolen + 4;
				out1.dsc$w_length = 300;
				out1.dsc$a_pointer = &errbuff[0];
				sys$fao(&err1, &faolen, &out1, 0);
				gtcm_errrab.rab$w_rsz = faolen;
				gtcm_errrab.rab$l_rbf = errbuff;
				sys$put(&gtcm_errrab);
				sys$flush(&gtcm_errrab);
			}
			/* Special case severe errors, because they will cause the GT.CM process to halt when they
				are signalled if they are sent normally.  Create ascii string of error and signal
				with SERVERERR.  Also signal severe errors to operators.
			*/

			if (severe)
			{	memcpy(&oper_msg.text[0],"GT.CM SERVER:  ",MESSAGE_HDR_LEN);
				msg2.dsc$a_pointer = &oper_msg;
				msg2.dsc$w_length = severe_len + 8 + MESSAGE_HDR_LEN;
				oper_msg.req_code = OPC$_RQ_RQST;
				*oper_msg.target = OPC$M_NM_CENTRL | OPC$M_NM_DEVICE | OPC$M_NM_DISKS;
				sys$sndopr(&msg2,0);
				ptr--;
				*ptr++ = 4;
				*ptr++ = 'L';
				*(int4*)ptr = ERR_SERVERERR;
				ptr += SIZEOF(int4);
				*ptr++ = 'L';
				*(int4*)ptr = 2;
				ptr += SIZEOF(int4);
				*ptr++ = 'L';
				*(int4*)ptr = severe_len;
				ptr += SIZEOF(int4);
				*ptr++ = 'A';
				*(short*)ptr = severe_len;
				ptr += SIZEOF(short);
				memcpy(ptr,&oper_msg.text[MESSAGE_HDR_LEN],severe_len);
				ptr += faolen;
				break;					/* skip further processing for severe errors */
			}
		}

		/* Mark this argument as the first argument after a signal.  If there are fao parameters
		   in the message, then this argument will be count, otherwise, it will be the next signal.
		*/
		first = TRUE;
		if (end - ptr < SIZEOF(int4) + 1)			/* need second buff, write out first one */
		{	if (curr_entry)
			{	curr_entry->clb_ptr->cbl = (unsigned char *)ptr - curr_entry->clb_ptr->mbf;
				curr_entry->clb_ptr->ast = 0;
				tempptr = curr_entry->clb_ptr->mbf + 1;
				*tempptr++ = 1;
				*tempptr = count - prev_count;
				prev_count = count;
				cmi_write(curr_entry->clb_ptr);
				ptr = curr_entry->clb_ptr->mbf;
				*ptr++ = CMMS_E_ERROR;
				*ptr++ = 0;
				*ptr++ = sig->chf$l_sig_args - 2 - count;
			}else
			{	ptr = &tempbuf[0];
			}
		}
		*ptr++ = 'L';
		*(int4*)ptr = *argptr++;
		ptr += SIZEOF(int4);
		for (i = 0; i < msg_len;)
		{	if (buff[i++] == '!')
			{	if (buff[i] == 'A') /* ascii */
				{	if (first)
					{	first = FALSE;				/* fao count */
						if (end - ptr < SIZEOF(int4) + 1)	/* need second buff, write out first one */
						{	if (curr_entry)
							{	curr_entry->clb_ptr->cbl = (unsigned char *)ptr
												- curr_entry->clb_ptr->mbf;
								curr_entry->clb_ptr->ast = 0;
								tempptr = curr_entry->clb_ptr->mbf + 1;
								*tempptr++ = 1;
								*tempptr = count - prev_count;
								prev_count = count;
								cmi_write(curr_entry->clb_ptr);
								ptr = curr_entry->clb_ptr->mbf;
								*ptr++ = CMMS_E_ERROR;
								*ptr++ = 0;
								*ptr++ = sig->chf$l_sig_args - 2 - count;
							}else
							{	ptr = &tempbuf[0];
							}
						}
						*ptr++ = 'L';
						count++;
						*(int4*)ptr = *argptr++;
						ptr += SIZEOF(int4);
					}
					if (buff[++i] == 'S')					/* descriptor */
					{	*buffptr++ = 'D';
						tempptr = buffptr;
						buffptr += SIZEOF(short);
						tempdesc = buffptr;
						tempdesc1 = *argptr++;
						*tempdesc = *tempdesc1;
						tempdesc->dsc$a_pointer = 0;
						buffptr += SIZEOF(cmi_descriptor);
						memcpy(buffptr,tempdesc1->dsc$a_pointer, tempdesc1->dsc$w_length);
						buffptr += tempdesc1->dsc$w_length;
						*(short*)tempptr = buffptr - tempptr - 2;
					}else if (buff[i] == 'D')			/* string */
					{	if (end - ptr < SIZEOF(int4) + 1)	/* need second buff, write out first one */
						{	if (curr_entry)
							{	curr_entry->clb_ptr->cbl = (unsigned char *)ptr
												- curr_entry->clb_ptr->mbf;
								curr_entry->clb_ptr->ast = 0;
								tempptr = curr_entry->clb_ptr->mbf + 1;
								*tempptr++ = 1;
								*tempptr = count - prev_count;
								prev_count = count;
								cmi_write(curr_entry->clb_ptr);
								ptr = curr_entry->clb_ptr->mbf;
								*ptr++ = CMMS_E_ERROR;
								*ptr++ = 0;
								*ptr++ = sig->chf$l_sig_args - 2 - count;
							}else
							{	ptr = &tempbuf[0];
							}
						}
						*ptr++ = 'L';
						len = *argptr++;
						*(int4*)ptr = len;
						ptr += SIZEOF(int4);
						count++;
						*buffptr++ = 'A';
						*(short*)buffptr = len;
						buffptr += 2;
						tempptr = *argptr++;
						memcpy(buffptr, tempptr, len);
						buffptr += len;
					}else if (buff[i] == 'C')				/* counted string */
					{	*buffptr++ = 'C';
						tempptr = *argptr++;
						memcpy(buffptr,tempptr, *tempptr);
						buffptr += *tempptr;
					}
                                        if (curr_entry && (end - ptr < buffptr - &buff1[0]))
					{
                                        	curr_entry->clb_ptr->cbl = (unsigned char *)ptr - curr_entry->clb_ptr->mbf;
						curr_entry->clb_ptr->ast = 0;
						tempptr = curr_entry->clb_ptr->mbf + 1;
						*tempptr++ = 1;
						*tempptr = count - prev_count;
						prev_count = count;
						cmi_write(curr_entry->clb_ptr);
						ptr = curr_entry->clb_ptr->mbf;
						*ptr++ = CMMS_E_ERROR;
						*ptr++ = 0;
						*ptr++ = sig->chf$l_sig_args - 2 - count;
					}
                                        else if (NULL == curr_entry)
                                                ptr = &tempbuf[0];
					memcpy(ptr,&buff1[0],buffptr - &buff1[0]);
					ptr += buffptr - &buff1[0];
					buffptr = &buff1[0];
					count++;
				}else if (buff[i + 1] == 'L')	/* a longword */
				{	if(buff[i] == 'U' || buff[i] == 'X' || buff[i] == 'S')
					{	if (first)
						{	first = FALSE;
							if (end - ptr < SIZEOF(int4) + 1)/* need second buff, write out first one */
							{	if (curr_entry)
								{	curr_entry->clb_ptr->cbl = (unsigned char *)ptr
													- curr_entry->clb_ptr->mbf;
									curr_entry->clb_ptr->ast = 0;
									tempptr = curr_entry->clb_ptr->mbf + 1;
									*tempptr++ = 1;
									*tempptr = count - prev_count;
									prev_count = count;
									cmi_write(curr_entry->clb_ptr);
									ptr = curr_entry->clb_ptr->mbf;
									*ptr++ = CMMS_E_ERROR;
									*ptr++ = 0;
									*ptr++ = sig->chf$l_sig_args - 2 - count;
								}else
								{	ptr = &tempbuf[0];
								}
							}
							*ptr++ = 'L';
							count++;
							*(int4*)ptr = *argptr++;
							ptr += SIZEOF(int4);
						}
						if (end - ptr < SIZEOF(int4) + 1)	/* need second buff, write out first one */
						{	if (curr_entry)
							{	curr_entry->clb_ptr->cbl = (unsigned char *)ptr
												- curr_entry->clb_ptr->mbf;
								curr_entry->clb_ptr->ast = 0;
								tempptr = curr_entry->clb_ptr->mbf + 1;
								*tempptr++ = 1;
								*tempptr = count - prev_count;
								prev_count = count;
								cmi_write(curr_entry->clb_ptr);
								ptr = curr_entry->clb_ptr->mbf;
								*ptr++ = CMMS_E_ERROR;
								*ptr++ = 0;
								*ptr++ = sig->chf$l_sig_args - 2 - count;
							}else
							{	ptr = &tempbuf[0];
							}
						}
						*ptr++ = 'L';
						count++;
						*(int4*)ptr = *argptr++;
						ptr += SIZEOF(int4);
					}
				}
			}
		}
	}
	if (curr_entry && (ptr > curr_entry->clb_ptr->mbf + 3))					/* unwritten message in buffer */
	{	curr_entry->clb_ptr->cbl = (unsigned char *)ptr - curr_entry->clb_ptr->mbf;
		curr_entry->clb_ptr->ast = gtcm_write_ast;
		cmi_write(curr_entry->clb_ptr);
	}
	if (severe)
		lib$signal(SIGNAL);
	mch->CHF_MCH_SAVR0 = CM_NOOP;					/* return NOOP, so that nothing more is done */
	if ((stat = sys$unwind(&mch->CHF_MCH_DEPTH, 0)) != SS$_NORMAL)
		NEXTCH;
}
