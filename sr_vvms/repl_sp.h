/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include <descrip.h>

#ifndef _REPL_SP_H
#define _REPL_SP_H

#define ERRNO			((EVMSERR != errno)? errno : vaxc$errno)
#define FORMAT_STR		"PID %X %s is%s alive\n"

#define MAX_COMMAND_LINE_LENGTH 512
#define DUMMY_START_QUAL	"/DUMMY_START"
#define NOWAIT			0x1
#define SERVER_UP		1
#define MUPIP_CMD		"MUPIP "
#define PROC_NAME_MAXLEN	15
#define SOURCE_PROMPT_START_QUAL	"REPLICATE/SOURCE/START"
#define RECV_PROMPT_START_QUAL		"REPLICATE /RECEIVER/START"
#define BUFF_QUAL			"/BUFFSIZE="
#define CONNECT_QUAL			"/CONNECTPARAMS="
#define FILTER_QUAL			"/FILTER="
#define SECONDARY_QUAL			"/SECONDARY="
#define LISTENPORT_QUAL			"/LISTENPORT="
#define LOG_QUAL			"/LOG="
#define LOGINTERVAL_QUAL		"/LOG_INTERVAL="
#define PASSIVE_QUAL			"/PASSIVE"
#define $DESCR(name,string)	struct dsc$descriptor_s name = { strlen(string), DSC$K_DTYPE_T, DSC$K_CLASS_S, string }
#define SET_PRIV(X, Y)	\
{				\
	prvadr[1] = 0;		\
	prvadr[0] = (X);	\
	Y = sys$setprv(TRUE, prvadr, FALSE, prvprv);	\
}
#define REL_PRIV	\
if (0 != (prvadr[0] &= ~prvprv[0]))	\
{				\
	sys$setprv(FALSE, prvadr, FALSE, NULL);	\
}

uint4 get_proc_name(unsigned char *prefix, uint4 prefix_size, uint4 pid, unsigned char *buff);
uint4 get_proc_info(uint4 pid, uint4 *time, uint4 *icount);
int repl_fork_rcvr_server(uint4 *pid, uint4 *cmd_channel);
int4 repl_mbx_wr(uint4 channel, sm_uc_ptr_t msg, int len, uint4 err_code);
int4 repl_trnlnm(struct dsc$descriptor_s *d_tbl_srch_list, struct dsc$descriptor_s *d_logical,
		 struct dsc$descriptor_s *d_expanded, struct dsc$descriptor_s *d_foundin_tbl);
int4 get_mbx_devname(struct dsc$descriptor_s *d_cmd_mbox, struct dsc$descriptor_s *d_cmd_dev);
int4 repl_create_server(struct dsc$descriptor_s *d_cmd, char *mbx_prefix, char *mbx_suffix, uint4 *cmd_channel, uint4 *server_pid,
			uint4 err_code);

/*----- FILE I/O related -----*/
#define F_CLOSE(CHANNEL, RC)			(RC) = sys$dassgn(CHANNEL)
#define F_COPY_GDID(to, from)	                \
{\
	memcpy(&(to).dvi, &(from).dvi, SIZEOF((to).dvi));\
	memcpy(&(to).did, &(from).did, SIZEOF((to).did));\
	memcpy(&(to).fid, &(from).fid, SIZEOF((to).fid));\
}

#define F_COPY_GDID_FROM_STAT(to, nam)		\
{\
	memcpy(&(to).dvi, &(nam).nam$t_dvi, SIZEOF((to).dvi));\
	memcpy(&(to).did, &(nam).nam$w_did, SIZEOF((to).did));\
	memcpy(&(to).fid, &(nam).nam$w_fid, SIZEOF((to).fid));\
}

#define F_READ_BLK_ALIGNED(channel, from, buff, size, status) \
{\
	status = sys$qiow(EFN$C_ENF, channel, IO$_READVBLK, &iosb[0], 0, 0,\
			  (sm_uc_ptr_t)buff, size, 1+DIVIDE_ROUND_DOWN(from, DISK_BLOCK_SIZE), 0, 0, 0);\
	if (status == SS$_NORMAL) \
		status = iosb[0];\
}

#define F_WRITE_BLK_ALIGNED(channel, to, buff, size, status) \
{\
	status = sys$qiow(EFN$C_ENF, channel, IO$_WRITEVBLK, &iosb[0], 0, 0,\
			  (sm_uc_ptr_t)buff, size, 1+DIVIDE_ROUND_DOWN(to, DISK_BLOCK_SIZE), 0, 0, 0);\
	if (status == SS$_NORMAL) \
		status = iosb[0];\
}
#endif
