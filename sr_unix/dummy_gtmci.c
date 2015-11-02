/****************************************************************
 *								*
 *	Copyright 2009 Fidelity Information Services, Inc 	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

 int gtm_ci(const char *c_rtn_name, ...);
 void gtm_zstatus(char *msg, int len);
 void ci_ret_code_quit(void);

 int gtm_ci(const char *c_rtn_name, ...)
 {
 	return 0;
 }

 void gtm_zstatus(char *msg, int len)
 {
 	return;
 }

 void ci_ret_code_quit(void)
 {
 	return;
 }
