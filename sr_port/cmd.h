/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef CMD_INCLUDED
#define CMD_INCLUDED

int cmd(void);
int m_break(void);
int m_close(void);
int m_do(void);
int m_else(void);
int m_xecute(void);
int m_for(void);
int m_goto(void);
int m_halt(void);
int m_hang(void);
int m_hcmd(void);
int m_if(void);
int m_job(void);
int m_kill(void);
int m_lock(void);
int m_merge(void);
int m_new(void);
int m_open(void);
int m_quit(void);
int m_read(void);
int m_set(void);
int m_tcommit(void);
int m_trestart(void);
int m_trollback(void);
int m_tstart(void);
int m_use(void);
int m_view(void);
int m_write(void);
int m_xecute(void);
int m_zallocate(void);
int m_zattach(void);
int m_zbreak(void);
int m_zcompile(void);
int m_zcontinue(void);
int m_zdeallocate(void);
int m_zedit(void);
int m_zgoto(void);
int m_zhalt(void);
int m_zhelp(void);
int m_zinvcmd(void);
int m_zlink(void);
int m_zmessage(void);
int m_zprint(void);
int m_zshow(void);
int m_zstep(void);
int m_zsystem(void);
int m_ztcommit(void);
#ifdef GTM_TRIGGER
int m_ztrigger(void);
#endif
int m_ztstart(void);
int m_zwatch(void);
int m_zwithdraw(void);
int m_zwrite(void);

#endif
