/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_CASECONV_included
#define GTM_CASECONV_included

void lower_to_upper(uchar_ptr_t d, uchar_ptr_t s, int4 len);
void upper_to_lower(uchar_ptr_t d, uchar_ptr_t s, int4 len);
void str_to_title(uchar_ptr_t d, uchar_ptr_t s, int4 len);

#endif /*GTM_CASECONV_included*/
