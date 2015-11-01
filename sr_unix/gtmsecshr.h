/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef _GTMSECSHR
#define _GTMSECSHR

#define MAX_SERVER_START_TRIES  25
#define MAX_SERVER_WAIT		256
#define CREATEFAILED		1000
#define CLIENT_ERROR_SLEEP_TIME 1000 /* 1 sec is the max time that a cleint will sleep
					before attempting to re-send a request */
#define GTMSECSHR_PING_TIMEOUT  60
#define GTMSECSHR_MESG_TIMEOUT  30

#define GTMSECSHR_PERMS		0666

/* Exit codes from gtmsecshr */
#define NORMALEXIT			0
#define SETUIDROOT			1
#define SETGIDROOT			2
#define UNABLETOOPNLOGFILE		3
#define UNABLETODUPLOG			4
#define INVTRANSGTMSECSHR		5
#define UNABLETOEXECGTMSECSHR		6
#define GNDCHLDFORKFLD  		7
#define UNABLETOOPNLOGFILEFTL		8
#define SEMGETERROR			9
#define SEMAPHORETAKEN			10

/* return codes with gtmsecshr*/
#define INVLOGNAME			20
#define BINDERR				21
#define SOCKETERR			22
#define UNLINKERR			23
#define FTOKERR				24

/* special flag from gtmsecshr_sock_init for client if could not get normal socket name */
#define ONETIMESOCKET			-1

/* arguments for gtmsecshr_sock_init */
#define SERVER				0
#define CLIENT				1

#define GTMSECSHR_LOG_DIR		GTM_LOG_ENV
#define DEFAULT_GTMSECSHR_LOG_DIR	DEFAULT_GTM_TMP
#define GTMSECSHR_LOG_PREFIX		"gtm_secshr_log"
#define GTMSECSHR_SOCK_DIR		GTM_TMP_ENV
#define DEFAULT_GTMSECSHR_SOCK_DIR	DEFAULT_GTM_TMP
#define GTMSECSHR_SOCK_PREFIX		"gtm_secshr"
#define GTMSECSHR_PATH			"$gtm_dist/gtmsecshr"

#define	ROOTUID				0
#define ROOTGID				0

#define NOTIFICATION_REQUIRED(X) 	(X)
#define ABSOLUTE_PATH(X)		(X[0] == '/')

#ifdef SHORT_GTMSECSHR_TIMEOUT
#define MAX_TIMEOUT_VALUE		30
#else
#define MAX_TIMEOUT_VALUE		6000
#endif

#define	MAX_ID_LEN			8

#define	MAX_MESG			2048

#define MAX_GTMSECSHR_FAIL_MESG_LEN	70

typedef struct ipcs_mesg_struct {
	int		semid;
	int		shmid;
	time_t		sem_ctime;
	time_t		shm_ctime;
	unsigned int	fn_len;
	char		fn[MAX_TRANS_NAME_LEN];
} ipcs_mesg;

typedef struct gtmsecshr_mesg_struct {
	int len;	/* this is the whole hdr (4 ints) plus the mesg data */
	int code;
	int ack;
	int pid;
	unsigned long seqno;
	union
	{
		long id;
		char path[MAX_TRANS_NAME_LEN];
		ipcs_mesg	db_ipcs;
	}mesg;
}gtmsecshr_mesg;

/* include <stddef.h> for offsetof() */
#define GTM_MESG_HDR_SIZE		offsetof(gtmsecshr_mesg, mesg.id)

enum gtmsecshr_mesg_type{
        WAKE_MESSAGE = 1,
        CHECK_PROCESS_ALIVE,
        REMOVE_SEM,
        REMOVE_SHMMEM,
        PING_MESSAGE,
	REMOVE_FILE,
	CONTINUE_PROCESS,
	PING_MESG_RECVD,
	FLUSH_DB_IPCS_INFO,
	GTMSECSHR_MESG_COUNT
};


enum gtmsecshr_ack_type{
        ACK_NOT_REQUIRED,
        ACK_REQUIRED
};

void	gtmsecshr_log(char *, int), gtmsecshr_exit(int, boolean_t), gtmsecshr_init(void), gtmsecshr_sig_init(void);
void	gtmsecshr_switch_log_file(int);
int	gtmsecshr_open_log_file(void), gtmsecshr_getenv(char *, char **);
int 	send_mesg2gtmsecshr (unsigned int code, unsigned int id, char *path, int path_len);
int 	service_request(gtmsecshr_mesg *);
int4	gtmsecshr_sock_init(int caller);
void	gtmsecshr_sock_cleanup(int);
int4	gtmsecshr_pathname_init(int caller);
int	continue_proc(pid_t pid);

#endif
