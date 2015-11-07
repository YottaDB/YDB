/****************************************************************
 *                                                              *
 *      Copyright 2006 Fidelity Information Services, Inc 	*
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#ifndef GTM_CONV_H
#define GTM_CONV_H

/* Define types for use by compilation in VMS in code paths that will never be
   used. This was the preferred method of making the Unicode modified UNIX code
   work in VMS rather than butchering it with ifdefs making maintenance more
   difficult. 9/2006 SE
*/

typedef int UConverter;

UConverter* get_chset_desc(const mstr *chset);
int gtm_conv(UConverter* from, UConverter* to, mstr* src, char* dstbuff, int* bufflen);
int verify_chset(const mstr *parm);

#endif /* GTM_CONV_H */
