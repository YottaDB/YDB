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

#ifndef _GTMSECSHR
#define _GTMSECSHR

/* To enable debugging of gtmsecshr, uncomment #define immediately below */
/* #define DEBUG_GTMSECSHR */
#ifdef DEBUG_GTMSECSHR
# define LOGFLAGS (LOG_USER | LOG_INFO)
# define DBGGSSHR(x) syslog x
#else
# define DBGGSSHR(x)
#endif
#define ABSOLUTE_PATH(X)	('/' == X[0])
#define GTMSECSHR_MESG_TIMEOUT  30
#define GTMSECSHR_PERMS		0666

/* Exit codes from gtmsecshr - note matching text entries are in message table in secshr_client.c */
#define NORMALEXIT			0
#define SETUIDROOT			1
#define INVTRANSGTMSECSHR		2
#define UNABLETOEXECGTMSECSHR		3
#define GNDCHLDFORKFLD  		4
#define SEMGETERROR			5
#define SEMAPHORETAKEN			6
#define SYSLOGHASERRORDETAIL		7
#define UNABLETOCHDIR			8
#define UNABLETODETERMINEPATH		9
#define NOTGTMSECSHR			10
#define BADGTMDISTDIR			11
#define LASTEXITCODE			11	/* Should have same value as last error code */

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

#define GTMSECSHR_SOCK_DIR		GTM_TMP_ENV
#define DEFAULT_GTMSECSHR_SOCK_DIR	DEFAULT_GTM_TMP
#define GTMSECSHR_SOCK_PREFIX		"gtm_secshr"
#define GTMSECSHR_DIR_SUFFIX		"/gtmsecshrdir"
#define GTMSECSHR_EXECUTABLE		"gtmsecshr"
#define GTMSECSHR_PATH			GTM_DIST_LOG "/" GTMSECSHR_EXECUTABLE

#define	ROOTUID				0

#ifdef SHORT_GTMSECSHR_TIMEOUT
#    define MAX_TIMEOUT_VALUE		30
#else
#  ifdef DEBUG
#    define MAX_TIMEOUT_VALUE		60	/* Give secshr timeout/startup some excercise in DEBUG mode */
#  else
#    define MAX_TIMEOUT_VALUE		6000
#  endif
#endif

#define	MAX_ID_LEN			8
#define	MAX_MESG			2048
#define MAX_SECSHR_SOCKFILE_NAME_LEN	(SIZEOF(GTMSECSHR_SOCK_PREFIX) + MAX_DIGITS_IN_INT)

typedef struct ipcs_mesg_struct
{
	int		semid;
	int		shmid;
	time_t		gt_sem_ctime;
	time_t		gt_shm_ctime;
	unsigned int	fn_len;
	char		fn[GTM_PATH_MAX];
} ipcs_mesg;

typedef struct gtmsecshr_mesg_struct
{
	int		code;		/* To gtmsecshr:   requested gtmsecshr_mesg_type function code.
					 * From gtmsecshr: return code (0 or errno).
					 */
	unsigned int	comkey;		/* Unique key per version keeps from having cross-version issues */
	boolean_t	usesecshr;	/* Copy of client's gtm_usesecshr flag. Only used in debug build but always kept
					 * for alignment.
					 */
	pid_t		pid;		/* Process id of sender */
	unsigned long	seqno;		/* Used only by client to validate response is for message sent */
	union
	{
		int4 		id;	/* Can be pid, semid or shmid */
		char 		path[GTM_PATH_MAX];
		ipcs_mesg	db_ipcs;
	} mesg;
} gtmsecshr_mesg;

/* include <stddef.h> for offsetof() */
#define GTM_MESG_HDR_SIZE		offsetof(gtmsecshr_mesg, mesg.id)

/* Version V6.0-000 largely re-built the interface between gtmsecshr client and server. Later versions should strive to
 * not change the order or placement of the message codes below. If a message becomes obsolete, rename the code to be
 * prefixed with "UNUSED_". This is so for future versions, if a security bug is found, we can take the source, compile
 * it for the relevant version and refresh just this module (assuming the client doesn't have issues).
 */
enum gtmsecshr_mesg_type
{
	/* Starting here, these are request codes put in mesg.code. They are returned unchanged except in case of error */
        WAKE_MESSAGE = 1,
        REMOVE_SEM,
        REMOVE_SHM,
	REMOVE_FILE,
	CONTINUE_PROCESS,
	FLUSH_DB_IPCS_INFO,
	/* From here down are response codes. These codes are never processed but all except INVALID_COMMAND (for which there is
	 * no response) can be returned to client.
	 */
	INVALID_COMMAND = 0x8000,	/* No response given */
	INVALID_COMKEY

};

int		validate_receiver(gtmsecshr_mesg *buf, char *rundir, int rundir_len, int save_code);
void		service_request(gtmsecshr_mesg *buf, int msglen, char *rundir, int rundir_len);
int4		gtmsecshr_sock_init(int caller);
void		gtmsecshr_sock_cleanup(int);
int4		gtmsecshr_pathname_init(int caller, char *execpath, int execpathln);
int		continue_proc(pid_t pid);

#endif
