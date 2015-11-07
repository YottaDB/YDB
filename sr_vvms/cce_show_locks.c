/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "vmsdtype.h"
#include <lckdef.h>
#include <jpidef.h>
#include <descrip.h>
#include <lkidef.h>
#include <climsgdef.h>
#include <psldef.h>
#include <prvdef.h>
#include <ssdef.h>
#include <efndef.h>
#include "util.h"


void display_lock_mode(int n, char *out);

void cce_show_locks(void)
{
	struct
	{
		int4 length;
		struct
		{
			unsigned int group:16, rmod:8, status: 7, sysnam:1;
		} value;
	} namespace;
	struct
	{
		int4 length;
		int4 value;
	} lkidadr, remlkid, system, pid;

	struct
	{
		int4 length;
		char value[31];
	} resnam;
	struct
	{
		int4 length;
		char value[3];
	} state;

#define ITEM_DEF(A, B) SIZEOF((B).value), A, &((B).value), &((B).length)

	item_list_3	ilist[] = {
		ITEM_DEF(LKI$_LOCKID, lkidadr),
		ITEM_DEF(LKI$_PID, pid),
		ITEM_DEF(LKI$_REMLKID, remlkid),
		ITEM_DEF(LKI$_RESNAM, resnam),
		ITEM_DEF(LKI$_STATE, state),
		ITEM_DEF(LKI$_SYSTEM, system),
		ITEM_DEF(LKI$_NAMSPACE, namespace),
		0, 0, 0, 0};
	int4 in_id;
	uint4	status;
	bool show_all_locks;
	struct
	{
		char lockid[8];
		char sp0[1];
		char remoteid[8];
		char sp1[1];
		char que[4];
		char sp2[1];
		char rq[2];
		char sp2a[1];
		char gn[2];
		char sp3[1];
		char systemid[8];
		char sp4[1];
		char owner[8];
		char sp5[1];
		char procname[15];
		char sp6[1];
		char group[8];
		char sp7[1];
		char rmod[4];
		char sp8[1];
		char sysnam[3];
		char sp9[1];
		char resourcename[31];
	} print_line;
	struct dsc$descriptor_s dsc_procname;
	$DESCRIPTOR(all_qualifier, "ALL");
	$DESCRIPTOR(output_qualifier, "OUTPUT");
	unsigned char *cp, *ctop;
	uint4 prvprv1[2], prvprv2[2], prvadr[2];
	static readonly char heading_line[] =
		"LOCK ID  REMOTEID QUE  RQ GN SYSTEMID OWNER    PROCESS NAME    GROUP    MODE LVL RESOURCE NAME";
	error_def(ERR_CCENOSYSLCK);
	error_def(ERR_CCENOWORLD);
	error_def(ERR_CCENOGROUP);

	prvadr[1] = 0;
	prvadr[0] = PRV$M_SYSLCK;
	status = sys$setprv(TRUE, &prvadr[0], FALSE, &prvprv1[0]);
	if (status != SS$_NORMAL)
		lib$signal(ERR_CCENOSYSLCK);
	prvadr[0] = PRV$M_WORLD;
	status = sys$setprv(TRUE, &prvadr[0], FALSE, &prvprv2[0]);
	if (status != SS$_NORMAL)
	{
		lib$signal(ERR_CCENOWORLD);
		prvadr[0] = PRV$M_GROUP;
		status = sys$setprv(TRUE, &prvadr[0], FALSE, &prvprv2[0]);
		if (status != SS$_NORMAL)
			lib$signal(ERR_CCENOGROUP);
	}

	status = cli$present(&all_qualifier);
	show_all_locks = (status == CLI$_PRESENT);
	dsc_procname.dsc$w_length = SIZEOF(print_line.procname);
	dsc_procname.dsc$b_dtype = DSC$K_DTYPE_T;
	dsc_procname.dsc$b_class = DSC$K_CLASS_S;
	dsc_procname.dsc$a_pointer = print_line.procname;
	util_out_open(&output_qualifier);
	util_out_write(LIT_AND_LEN(heading_line));
	util_out_write(heading_line, 0);
	for (in_id = -1;  ;)
	{
		status = sys$getlkiw(EFN$C_ENF, &in_id, &ilist, 0, 0, 0, 0);
		if (status != 1)
			break;
		if (!show_all_locks && memcmp("GTM", &resnam.value, SIZEOF("GTM") - 2) != 0)
			continue;
		memset(&print_line, ' ', SIZEOF(print_line));
		i2hex(lkidadr.value, print_line.lockid, SIZEOF(print_line.lockid));
		i2hex(pid.value, print_line.owner, SIZEOF(print_line.owner));
		i2hex(system.value, print_line.systemid, SIZEOF(print_line.systemid));
		i2hex(remlkid.value, print_line.remoteid, SIZEOF(print_line.remoteid));
		i2hex(namespace.value.group, print_line.group, SIZEOF(print_line.group));
		display_lock_mode(state.value[0], &print_line.rq);
		display_lock_mode(state.value[1], &print_line.gn);
		memcpy(print_line.resourcename, resnam.value, resnam.length);
		switch(state.value[2])
		{
			case LKI$C_GRANTED:
				cp = "GRNT";
				break;
			case LKI$C_CONVERT:
				cp = "CONV";
				break;
			case LKI$C_WAITING:
				cp = "WAIT";
				break;
			default:
				cp = "????";
				break;
		}
		memcpy(print_line.que, cp, SIZEOF(print_line.que));
		status = lib$getjpi(&JPI$_PRCNAM, &pid.value, 0, 0, &dsc_procname, 0);
		switch(namespace.value.rmod)
		{
			case PSL$C_USER:
				cp = "USER";
				break;
			case PSL$C_EXEC:
				cp = "EXEC";
				break;
			case PSL$C_SUPER:
				cp = "SUPR";
				break;
			case PSL$C_KERNEL:
				cp = "KRNL";
				break;
			default:
				cp = "????";
				break;
		}
		memcpy(print_line.rmod, cp, SIZEOF(print_line.rmod));
		cp = namespace.value.sysnam ? "SYS" : "GRP";
		memcpy(print_line.sysnam, cp, SIZEOF(print_line.sysnam));
		for (cp = print_line.resourcename, ctop = cp + SIZEOF(print_line.resourcename);  cp < ctop;  cp++)
			if (*cp < 32 || *cp > 127)
				*cp = '.';
		for (cp = &print_line, ctop = cp + SIZEOF(print_line);  cp < ctop && *(ctop - 1) == ' ';  ctop--)
			;
		util_out_write(&print_line, ctop - cp);
	}
	util_out_close();
	if (prvprv2[0] & prvadr[0] == 0)
		sys$setprv(FALSE, &prvadr[0], FALSE, 0);
	prvadr[1] = 0;
	prvadr[0] = PRV$M_SYSLCK;
	if (prvprv1[0] & PRV$M_SYSLCK == 0)
		sys$setprv(FALSE, &prvadr[0], FALSE, 0);
	return;
}

void display_lock_mode(int n, char *out)
{
	char *cp;
	switch (n)
	{
		case LCK$K_NLMODE:
			cp = "NL";
			break;
		case LCK$K_CRMODE:
			cp = "CR";
			break;
		case LCK$K_CWMODE:
			cp = "CW";
			break;
		case LCK$K_PRMODE:
			cp = "PR";
			break;
		case LCK$K_PWMODE:
			cp = "PW";
			break;
		case LCK$K_EXMODE:
			cp = "EX";
			break;
		default:
			cp = "??";
			break;
	}
	*out++ = *cp++;
	*out = *cp;
	return;
}
