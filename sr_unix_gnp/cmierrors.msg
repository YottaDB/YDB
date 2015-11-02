!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!								!
!	Copyright 2001, 2009 Fidelity Information Services, Inc	!
!								!
!	This source code contains the intellectual property	!
!	of its copyright holder(s), and is made available	!
!	under a license.  If you do not know the terms of	!
!	the license, please stop and do not read further.	!
!								!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	.FACILITY	CMI,250/PREFIX=CMI_
	.TITLE	CMIERRORS Error Messages for CMI ,GT.CM
DCNINPROG	<Attempt to initiate operation while disconnect was in progress>/fatal/fao=0
LNKNOTIDLE	<Attempt to initiate operation before previous operation completed>/fatal/fao=0
ASSERT		<Assert failed !AD line !UL>/error/fao=3
CMICHECK	<Internal CMI error. Report to your GT.M Support Channel.>/fatal/fao=0
NETFAIL		<Failure of Net operation>/error/fao=0
BADPORT		<Environment variable GTCM_TCP_PORT is not an integer>/error/fao=0
NOTND		<tnd argument to cmi_init is NULL>/fatal/fao=0
OVERRUN		<mbf argument in CLB is not large enough for packet>/error/fao=0
NOSERVENT	<Sevices data lookup failure>/error/fao=0
BADIPADDRPORT	<Bad specification of [ip address:port] in tnd>/error/fao=0
REASON_CONNECT	<Incoming connection>/error/fao=0
REASON_INTMSG	<Incoming urgent data>/error/fao=0
REASON_DISCON	<Disconnect encountered>/error/fao=0
REASON_ABORT	<Link aborted>/error/fao=0
REASON_EXIT	<Exit>/error/fao=0
REASON_PATHLOST	<Network path lost>/error/fao=0
REASON_PROTOCOL	<Protocol error>/error/fao=0
REASON_THIRDPARTY <Thirdparty error>/error/fao=0
REASON_TIMEOUT	<Network timeout>/error/fao=0
REASON_NETSHUT	<Shutdown received>/error/fao=0
REASON_REJECT	<Connection rejected>/error/fao=0
REASON_IODONE	<I/O done>/error/fao=0
REASON_OVERRUN	<Input overran buffer>/error/fao=0
REASON_STATUS	<Status>/error/fao=0
REASON_CONFIRM	<Confirm>/error/fao=0

	.end
