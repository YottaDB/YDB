/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "cli.h"

int main(int argc, char **argv, char **envp)
{
<<<<<<< HEAD
	return dlopen_libyottadb(argc, argv, envp, "gtcm_gnp_server_main");
=======
	char			*ptr, service[512];
	char			time_str[CTIME_BEFORE_NL + 2];	/* for GET_CUR_TIME macro */
	cmi_descriptor		log_path_descr, service_descr;
	cmi_status_t		status;
	int			arg_index, eof, parse_ret;
	pid_t			pid;
	struct sigaction	act;
	uint4			timout;
	unsigned short		log_path_len, service_len;
	DCL_THREADGBL_ACCESS;

        static boolean_t no_fork = FALSE;

	GTM_THREADGBL_INIT;
	common_startup_init(GTCM_GNP_SERVER_IMAGE);
	gtmenvp = envp;
	err_init(stop_image_conditional_core);
	assert(0 == offsetof(gv_key, top)); /* for integrity of CM_GET_GVCURRKEY */
	assert(2 == offsetof(gv_key, end)); /* for integrity of CM_GET_GVCURRKEY */
	assert(4 == offsetof(gv_key, prev)); /* for integrity of CM_GET_GVCURRKEY */
	gtm_chk_dist(argv[0]);
	/* read comments in gtm.c for cli magic below */
	cli_lex_setup(argc, argv);
	if (1 < argc)
		cli_gettoken(&eof);
	cli_token_buf[0] = '\0';
	ptr = cli_lex_in_ptr->in_str;
	memmove(ptr + SIZEOF("GTCM_GNP_SERVER ") - 1, ptr, strlen(ptr) + 1); /* BYPASSOK */
	MEMCPY_LIT(ptr, "GTCM_GNP_SERVER ");
	cli_lex_in_ptr->tp = cli_lex_in_ptr->in_str;
	parse_ret = parse_cmd();
	if (parse_ret && (EOF != parse_ret))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) parse_ret, 2, LEN_AND_STR(cli_err_str));
	service_len = (unsigned short)SIZEOF(service);
	CMI_DESC_SET_POINTER(&service_descr, service);
	service[0] = '\0';
	if (CLI_PRESENT == cli_present("SERVICE") && cli_get_str("SERVICE", CMI_DESC_POINTER(&service_descr), &service_len))
		CMI_DESC_SET_LENGTH(&service_descr, service_len);
	else
		CMI_DESC_SET_LENGTH(&service_descr, 0);
	if (cli_get_int("TIMEOUT", (int *)&timout))
	{
		cm_timeout = TRUE;
		if (timout > (1 << 21))
			timout = (1 << 21);
		closewait = (timout << 10); /* s -> ms; approx */
	}
	log_path_len = (unsigned short)SIZEOF(gtcm_gnp_server_log) - 1;
	CMI_DESC_SET_POINTER(&log_path_descr, gtcm_gnp_server_log);
	if (CLI_PRESENT != cli_present("LOG") || !cli_get_str("LOG", CMI_DESC_POINTER(&log_path_descr), &log_path_len))
		log_path_len = 0;
	if (CLI_PRESENT == cli_present("TRACE"))
		trace_on = TRUE;
	gtcm_gnp_server_log[log_path_len] = '\0';
	gtcm_open_cmerrlog();
        assert(0 == EMPTY_QUEUE);
	licensed = TRUE;
	stp_init(STP_INITSIZE);
	rts_stringpool = stringpool;
	getzdir();
	sig_init(generic_signal_handler, null_handler, suspsigs_handler, continue_handler); /* should do be done before cmi_init */

	/* Redefine handler for SIGHUP to switch log file */
	memset(&act, 0, SIZEOF(act));
	act.sa_handler  = gtcm_gnp_switch_interrupt;
	sigaction(SIGHUP, &act, 0);
	act.sa_handler  = gtcm_gnp_trace_on;
	sigaction(SIGUSR1, &act, 0);
	act.sa_handler  = gtcm_gnp_trace_off;
	sigaction(SIGUSR2, &act, 0);

	procnum = 0;
        memset(proc_to_clb, 0, SIZEOF(proc_to_clb));

	/* child continues here */
	gtcm_connection = FALSE;
        if (!no_fork)
        {
		FORK(pid);
                if (0 > pid)
                {
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_TEXT, 2,
					LEN_AND_LIT("Error forking gnp server into the background"), errno);
                        EXIT(-1);
                }
                else if (0 < pid)
                        EXIT(0);
		getjobnum();
                (void) setpgrp();
        }
	/* Write argv and the process id for ease of admin */
	GET_CUR_TIME(time_str);
	util_out_print("!AD : ", FALSE, CTIME_BEFORE_NL, time_str);
	for (arg_index = 0; arg_index < argc; arg_index++)
		util_out_print("!AZ ", FALSE, argv[arg_index]);
	util_out_print("[pid : !UL]", TRUE, process_id);
	/*
       	 * the acc function pointer is NULL to prevent incoming connections
	 * until we are ready.
	 */
	status = cmi_init(&service_descr, '\0', gtcm_neterr, NULL, gtcm_link_accept, gtcm_urgread_ast, CM_CLB_POOL_SIZE,
			  SIZEOF(connection_struct), CM_MSG_BUF_SIZE + CM_BUFFER_OVERHEAD);
	if (CMI_ERROR(status))
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_NETFAIL, 0,
				ERR_TEXT, 2, LEN_AND_LIT("Network interface initialization failed"), status);
		EXIT(status);
	}
	atexit(gtcm_exi_handler);
	initialize_pattern_table();
	/* Pre-allocate some timer blocks. */
	prealloc_gt_timers();
	gt_timers_add_safe_hndlrs();
	SET_LATCH_GLOBAL(&action_que.latch, LOCK_AVAILABLE);
	mu_gv_cur_reg_init();
	cs_addrs = &FILE_INFO(gv_cur_region)->s_addrs;
	cs_data = cs_addrs->hdr;
	cs_addrs->nl = (node_local_ptr_t)malloc(SIZEOF(node_local));
	memset((char *)cs_addrs->nl, 0, SIZEOF(node_local));
	action_que_dummy_reg = gv_cur_region;
	OPERATOR_LOG_MSG;
	/* ... now we are ready! */
	ntd_root->crq = gtcm_init_ast;
	while (!cm_shutdown)
	{
		gtcm_gnp_server_actions();
	}
	return SS_NORMAL;
>>>>>>> 7a1d2b3e... GT.M V6.3-007
}
