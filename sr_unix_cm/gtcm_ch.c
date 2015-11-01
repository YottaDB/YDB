/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cmidef.h"
#include "cmmdef.h"
#include "error.h"
#include "fao_parm.h"
#include <fcntl.h>
#include "cmi.h"
#include "preemptive_ch.h"


GBLREF connection_struct	*curr_entry;
GBLDEF bool			gtcm_errfile = FALSE;
GBLDEF bool			gtcm_firsterr = TRUE;
GBLDEF int			gtcm_errfd;
GBLREF int			outparm[MAX_FAO_PARMS];
LITREF err_ctl			*master_msg[];
GBLREF short			gtcm_ast_avail;		/* actually represents number of file descriptors */

CONDITION_HANDLER(gtcm_ch)
{
	char		*c1ptr, *c2ptr, *buffptr, sev;
	char		tempbuf[2048], outbuf[2048];
	unsigned char	*tempptr, *mbfptr, *endptr;
	err_ctl		*fac, *ctl;
	err_msg		*msg;
	int		i, *argptr, arg_cnt, count, msg_len, len, prev_count;
	bool		first;
	void		gtcm_write_ast();

	START_CH;

	if (gtcm_firsterr)
	{	if ((gtcm_errfd = open("$gtm_dist/cmerr.log", O_RDWR | O_APPEND)) != -1)
		{	gtcm_errfile = FALSE;
			gtcm_firsterr = FALSE;
			gtcm_ast_avail--;
		}
	}
	if (IS_GTM_ERROR(SIGNAL))
		fac = &merrors_ctl;
	else
	{
		for (i = 0; fac = master_msg[i]; i++)
		{
			if ((SIGNAL & FACMASK(fac->facnum)) && (MSGMASK(SIGNAL, fac->facnum) < fac->msg_cnt))
				break;
		}
	}
	c1ptr = outbuf;
	if (fac)
	{
		*c1ptr++ = '%';
		memcpy(c1ptr, STR_AND_LEN(fac->facname));
		c1ptr += strlen(fac->facname);
		*c1ptr++ = '-';
	}
	else
	{
		memcpy(c1ptr, "%NONAME-", sizeof("%NONAME-"));
		c1ptr += sizeof("%NONAME-");
	}

	switch(severity)
	{	case SUCCESS : sev = 'S'; break;
		case INFO : sev = 'I'; break;
		case WARNING : sev = 'W'; break;
		case ERROR : sev = 'E'; break;
		case SEVERE : sev = 'F'; break;
		default: sev = 'U'; break;
	}
	*c1ptr++ = sev;
	*c1ptr++ = ',';
	*c1ptr++ = ' ';
	sprintf(c1ptr, util_outbuff, outparm[0], outparm[1], outparm[2], outparm[3],
		outparm[4], outparm[5], outparm[6], outparm[7], outparm[8], outparm[9], outparm[10], outparm[11]);

	if (gtcm_errfile)
	{
#ifdef DP
	fprintf(gtcm_errfd,outbuf);
#endif
	}
	if (curr_entry)
	{	mbfptr = curr_entry->clb_ptr->mbf;
		endptr = mbfptr + curr_entry->clb_ptr->mbl;
		argptr = (int*)arg;
		arg_cnt = *argptr++;
		*mbfptr++ = CMMS_E_ERROR;
		*mbfptr++ = 0;
		*mbfptr++ = arg_cnt;
		buffptr = &tempbuf[0];
		prev_count = 0;
		for (count = 0; count < arg_cnt; )
		{	count++;
			msg_len = 0;
			if (!(ctl = err_check(*argptr)))
			{	*mbfptr++ = 'L';
				*(int4*)mbfptr = *argptr++;
				mbfptr += sizeof(int4);
				if (count < arg_cnt)
				{	assert(*argptr == 0);
					*mbfptr++ = 'L';
					*(int4*)mbfptr = *argptr++;
					mbfptr += sizeof(int4);
					count++;
				}
			}else
			{
				assert((*argptr & FACMASK(ctl->facnum)) && (MSGMASK(*argptr, ctl->facnum) <= ctl->msg_cnt));
				msg = ctl->fst_msg + MSGMASK(*argptr, ctl->facnum) - 1;
				*mbfptr++ = 'L';
				*(int4*)mbfptr = *argptr++;
				mbfptr += sizeof(int4);
				c1ptr = msg->msg;
				first  = TRUE;	/* Mark this arg as the first after a signal, either fao count or next arg */
				for (c2ptr = c1ptr + strlen(msg->msg); c1ptr < c2ptr; )
				{	if (*c1ptr++ == '!')
					{	if (*c1ptr == 'A') /* ascii */
						{	if (first)
							{	first = FALSE;	/* fao count */
								if (endptr - mbfptr < sizeof(int4) + 1)
								{
									curr_entry->clb_ptr->cbl =
										mbfptr - curr_entry->clb_ptr->mbf;
									curr_entry->clb_ptr->ast = 0;
									tempptr = curr_entry->clb_ptr->mbf + 1;
									*tempptr++ = 1;
									*tempptr = count - prev_count;
									prev_count = count;
									cmi_write(curr_entry->clb_ptr);
									mbfptr = curr_entry->clb_ptr->mbf;
									*mbfptr++ = CMMS_E_ERROR;
									*mbfptr++ = 0;
									*mbfptr++ = arg_cnt - count;
								}
								*mbfptr++ = 'L';
								count++;
								*(int4*)mbfptr = *argptr++;
								mbfptr += sizeof(int4);
							}
							if (*++c1ptr == 'D')	/* string, only type used at moment */
							{	if (endptr - mbfptr < sizeof(int4) + 1)
								{
									curr_entry->clb_ptr->cbl =
										mbfptr - curr_entry->clb_ptr->mbf;
									curr_entry->clb_ptr->ast = 0;
									tempptr = curr_entry->clb_ptr->mbf + 1;
									*tempptr++ = 1;
									*tempptr = count - prev_count;
									prev_count = count;
									cmi_write(curr_entry->clb_ptr);
									mbfptr = curr_entry->clb_ptr->mbf;
									*mbfptr++ = CMMS_E_ERROR;
									*mbfptr++ = 0;
									*mbfptr++ = arg_cnt - count;
								}
								*mbfptr++ = 'L';
								len = *argptr++;
								*(int4*)mbfptr = len;
								mbfptr += sizeof(int4);
								count++;
								*buffptr++ = 'A';
								*(short*)buffptr = len;
								buffptr += sizeof(short);
								tempptr = (unsigned char *)*argptr++;
								memcpy(buffptr,tempptr, len);
								buffptr += len;
							}
							if (endptr - mbfptr < buffptr - &tempbuf[0])
							{	curr_entry->clb_ptr->cbl = mbfptr - curr_entry->clb_ptr->mbf;
								curr_entry->clb_ptr->ast = 0;
								tempptr = curr_entry->clb_ptr->mbf + 1;
								*tempptr++ = 1;
								*tempptr = count - prev_count;
								prev_count = count;
								cmi_write(curr_entry->clb_ptr);
								mbfptr = curr_entry->clb_ptr->mbf;
								*mbfptr++ = CMMS_E_ERROR;
								*mbfptr++ = 0;
								*mbfptr++ = arg_cnt - count;
							}
							memcpy(mbfptr,&tempbuf[0],buffptr - &tempbuf[0]);
							mbfptr += buffptr - &tempbuf[0];
							buffptr = &tempbuf[0];
							count++;
							c1ptr++;

						}else if (*(c1ptr+1) == 'L') /* a longword */
						{	if (*c1ptr == 'U' || *c1ptr == 'X' || *c1ptr == 'S')
							{	if (first)
								{	first = FALSE;
									if (endptr - mbfptr < sizeof(int4) + 1)
									{
										curr_entry->clb_ptr->cbl =
											mbfptr - curr_entry->clb_ptr->mbf;
										curr_entry->clb_ptr->ast = 0;
										tempptr = curr_entry->clb_ptr->mbf + 1;
										*tempptr++ = 1;
										*tempptr = count - prev_count;
										prev_count = count;
										cmi_write(curr_entry->clb_ptr);
										mbfptr = curr_entry->clb_ptr->mbf;
										*mbfptr++ = CMMS_E_ERROR;
										*mbfptr++ = 0;
										*mbfptr++ = arg_cnt - count;
									}
									*mbfptr++ = 'L';
									count++;
									*(int4*)mbfptr = *argptr++;
									mbfptr += sizeof(int4);
								}
								if (endptr - mbfptr < sizeof(int4) + 1)
								{
									curr_entry->clb_ptr->cbl =
										mbfptr - curr_entry->clb_ptr->mbf;
									curr_entry->clb_ptr->ast = 0;
									tempptr = curr_entry->clb_ptr->mbf + 1;
									*tempptr++ = 1;
									*tempptr = count - prev_count;
									prev_count = count;
									cmi_write(curr_entry->clb_ptr);
									mbfptr = curr_entry->clb_ptr->mbf;
									*mbfptr++ = CMMS_E_ERROR;
									*mbfptr++ = 0;
									*mbfptr++ = arg_cnt - count;
								}
								*mbfptr++ = 'L';
								count++;
								*(int4*)mbfptr = *argptr++;
								mbfptr += sizeof(int4);
								c1ptr++;

							}
						}
					}
				}
			}
		}
	}
	if (curr_entry && mbfptr > curr_entry->clb_ptr->mbf + 3)
	{	curr_entry->clb_ptr->cbl = mbfptr - curr_entry->clb_ptr->mbf;
		curr_entry->clb_ptr->ast = gtcm_write_ast;
		cmi_write(curr_entry->clb_ptr);
	}
	if (SEVERITY == SUCCESS || SEVERITY == INFO)
	{	CONTINUE;
	}
	if (SEVERITY == WARNING || SEVERITY == ERROR)
	{/*	UNWIND;*/
	  preemptive_ch(SEVERITY);
	  active_ch++;
	  ctxt = active_ch;
	  longjmp(active_ch->jmp,-1);
	}
	NEXTCH;
}
