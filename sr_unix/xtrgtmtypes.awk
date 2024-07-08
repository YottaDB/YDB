#################################################################
#								#
# Copyright (c) 2010-2024 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
#
# Part of gengtmdeftypes.
#
# Each GT.M header file, after de-commenting, is run through this extractor to generate a list of GT.M
# defined types we want to make sure we define later. This is done initially because once processing
# begins on the combined pre-processed header files, we cannot tell the difference between system
# supplied structures and GT.M supplied structures. This gives us the latter list which we want to
# concentrate on in later processing.
#
BEGIN \
{
	bracelevel = 0;
	bracketlevel = 0;
	parenlevel = 0;
	prevfield = "";
	intypedef = 0;
}

#
# Main
#
{
	gsub("\\[.*\\]", " ");		# Eliminate matched [*] stuff
	gsub("\".*\"", " ");		# Eliminate double-quoted strings
	gsub("'.*'", " ");		# Eliminate single-quoted strings
	gsub("\\[", " & ");		# Remaining [ gets space around it (separate due to issues on AIX)
	gsub("\\]", " & ");		# Remaining ] gets space around it
	gsub("[#;:,{}=*-//)(]", " & ");	# Other special chars get spaces around them so can be recognized
	gsub("unsigned int", "unsigned-int");		# Turn types into single-token types
	gsub("unsigned long long", "unsigned-long-long");
	gsub("long long", "long-long");
	gsub("unsigned long", "unsigned-long");
	gsub("unsigned short", "unsigned-short");
	gsub("unsigned char", "unsigned-char");
	gsub("unsigned int", "unsigned-int");
	gsub("unsigned int", "unsigned-int");
	gsub("signed int", "int");
	gsub("signed char", "char");

	if ("typedef" == $1 || intypedef)
	{	# Either have a new typedef or we are already in one - ignore anything else
		if ("typedef" == $1)
		{
			if (intypedef)
			{
				printf("Error: nested typedef - not supported at line %d\n", NR);
				exit(1); #BYPASSOK
			}
			intypedef = 1;
			tokenssincetypedef = -1;	# Since we increment it for the typedef as well
			typedeftype1 = "";
			typedeftype2 = "";
			parenssincetypedef = 0;
			isfnptr=0;
		}
		for (i=1; NF >= i; i++)
		{
			tokenssincetypedef++;	# allow us to track topside
			if ("{" == $i)
				bracelevel++;
			else if ("}" == $i)
				bracelevel--;
			else if ("[" == $i)
				bracketlevel++;
			else if ("]" == $i)
				bracketlevel--;
			else if ("(" == $i)
			{
				parenlevel++;
				if (2 == tokenssincetypedef && 0 == parenssincetypedef && "*" == $(i+1))
				{	# Probable function pointer definition
					i++;	# Get rid of "("
					i++;	# Get rid of "*"
					if ("volatile" == $i) i++;	# ignore volatile
					prevfield = $i;
					prevfldnum = i;
					isfnptr = 1
					typedeftype1 = "*fnptr"
				}
				parenssincetypedef++;
			} else if (")" == $i)
				parenlevel--;
			else if (";" == $i || "," == $i)
			{
				if (0 == bracelevel && 0 == bracketlevel && 0 == parenlevel)
				{
					if (prevfldnum == (i - 1) || isfnptr)
						# make sure aren't picking up garbage from earlier in line
						print prevfield,typedeftype1,typedeftype2;
					intypedef = 0;
					isfnptr = 0;
				}
			} else if (1 == tokenssincetypedef)
				typedeftype1 = $i;
			else if (2 == tokenssincetypedef)
				typedeftype2 = $i;
			if (0 == bracketlevel && 0 == parenlevel && "]" != $i && ")" != $i && "(" != $i)
			{
				prevfield = $i;
				prevfldnum = i;
			}
		}
	}
}
