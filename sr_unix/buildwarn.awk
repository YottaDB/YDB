#################################################################
#								#
#	Copyright 2007 Fidelity Information Services, Inc.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
BEGIN	{ state = 0; inwarning = 0; pattern = sprintf("%s/.*", gtm_src); }
$0 == "End of C Compilation"	{ state = 2;}
state == 1	{
			if (NF > 0)
			{
				if ((NF == 1) && ($1 ~ pattern))
				{
					prev = $0;
					inwarning = 0;
				} else
				{
					if (!inwarning)
					{
						print prev;
						inwarning = 1;
					}
					print $0;
				}
			}
		}
$0 == "Start of C Compilation"	{ state = 1; prev = ""; inwarning = 0; }
END	{
		if (state != 2)
			printf "BUILDWARN_AWK-E-ERROR : Did not see Start or End of C compilation: state = %d\n", state;
	}
