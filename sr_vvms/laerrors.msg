!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!								!
!	Copyright 2001 Sanchez Computer Associates, Inc.	!
!								!
!	This source code contains the intellectual property	!
!	of its copyright holder(s), and is made available	!
!	under a license.  If you do not know the terms of	!
!	the license, please stop and do not read further.	!
!								!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	.FACILITY	LMU,240/PREFIX=LA_
	.TITLE	LAERRORS	 Messages for LMU, VMS EDITION
DBEXIS  <The configuration database already exists>/fao=1/warning
NOCNFDB <Invalid or missing configuration database>/fao=0/fatal
DBINIT	<Configuration database !AD successfully created>/fao=1/success
INVAL	<                       ^ Invalid value, reenter>/fao=0/warning
NOTUNIQ <                       ^ Configuration ID not unique>/fao=0/warning
NOSYS   <Node !AD not available, Please enter the node number - if known>/fao=1/info
HEAD	<!/*** Product Authorization Key ***!/>/fao=0/info
NOCNF   <No configuration created>/fao=0/info
BADENCR	<Bad check sum number>/fao=0/error
BADCS   <The configuration data is not correct>/fao=0/error

NAM	<* Product             :!AD>/fao=1/info
VER	<* Version             :!AD>/fao=1/info
X 	<* Configuration value :!SW>/fao=1/info
UNLX 	<* Configuration value :Unlimited jobs>/fao=0/info
T0	<* Activation date     :!11%D>/fao=1/info
T1	<* Expiration date     :!11%D>/fao=1/info
UNLT0	<* Activation date     :Not limited>/fao=1/info
UNLT1	<* Expiration date     :Not limited>/fao=1/info
LID	<* Configuration ID    :!SL>/fao=1/info
NID	<* Node number !SL       :!SL>/fao=2/info
L	<* Number of systems   :!SW>/fao=1/info
SID    	<* Hardware model !SL    :!AD>/fao=2/info
SIDX	<* Hardware model !SL    :!AD!2(_)(0x!AD)>/fao=5/info
UNSID	<* Hardware model !SL    :Not limited >/fao=1/info
CUST	<* Customer name       :!AD>/fao=1/info
ADR	<* Address line !SL      :!AD>/fao=2/info
COM	<* Comment             :!AD>/fao=1/info
CS	<* Check sum           :!SL-!AD-!AD-!AD-!AD>/fao=5/info
STD	<* Creation date       :!17%D>/fao=1/info
OID	<* Operator ID         :!AD>/fao=1/info

PNAM	<* Product             :>/fao=0/info
PVER	<* Version             :>/fao=0/info
PX 	<* Configuration value :>/fao=0/info
PT0	<* Activation date     :>/fao=0/info
PT1	<* Expiration date     :>/fao=0/info
PLID	<* Configuration ID    :>/fao=0/info
PNID	<* Node number    [dec]:>/fao=0/info
PL	<* Number of systems   :>/fao=0/info
PSID    <* Hardware model      :>/fao=0/info
PCUST	<* Customer name       :>/fao=0/info
PADR1	<* Address line 1      :>/fao=0/info
PADR2	<* Address line 2      :>/fao=0/info
PADR3	<* Address line 3      :>/fao=0/info
PADR4	<* Address line 4      :>/fao=0/info
PCOM	<* Comment             :>/fao=0/info
PENC	<* Security function # :>/fao=0/info
PCS	<* Check sum           :>/fao=0/info

SAVE    <Do you want to save, display again, edit or quit S/D/E/Q [D] ?:>/fao=0/info
MODCNF	<Configuration modified>/fao=0/success
NEWCNF	<New configuration stored in database>/fao=0/success
VOID    <Do you want to void Y/N , or quit Q [N] ?:>/fao=0/info
VOIDED  <Configuration voided>/fao=0/success
PAKHD	<!1(/)!3(_)GT.M PRODUCT AUTHORIZATION KEY!2(/)>/fao=0/info
ISSUR	<Sanchez Computer Associates!/Malvern, PA  19355>/fao=0/info
ISSUE  	<!2(/)Issued to:!7(_)Issue Date:!/>/fao=0/info
ISSDT	<!8(_)!11%D>/fao=1/info
ISADR 	<!_!AD>/fao=1/info
DELIM   <!3(/)!75**!/>/fao=0/info
EMPTY	<!_ >/fao=0/info

CHOOSE  <Choose a value from the list:>/fao=0/info
VALNAM  <GT.M,GT.CM,GT.CX,TEST>/fao=0/info
VALVER  <V1.0,V1.2,V2.3,V2.4,V2.5,V3.0>/fao=0/info

DESTR   <Delete, Next or Quit D/N/Q [N] ?:>/fao=0/info
MOD     <Modify, Next or Quit M/N/Q [M] ?:>/fao=0/info
DELETE	<Configuration deleted>/fao=0/success

CNFOK  <Configuration !SL is valid for local node>/fao=1/success

HEADCL <Node name!2(_)Node number!2(_) Hardware model>/fao=0/info
DELICL <---------!2(_)-----------!2(_) -------------->/fao=0/info
SHOWCL "!15<!AD!>!3(_)!UL!2(_)  !15<!AD!>"/fao=3/info
	.END
