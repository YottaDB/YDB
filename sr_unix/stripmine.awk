#################################################################
#								#
# Copyright (c) 2010-2018 Fidelity National Information		#
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
# Script to scrub preprocessor output of routine with all header files. Actions performed:
#   a. Eliminates all statements other than "typedef" statements.
#   b. Puts spaces around all tokens, all special chars.
#   c. Eliminates multiple adjacent white space chars to make easy parsing for $ZPiece().
#
BEGIN \
{
	curlylevel = 0;
	bracketlevel = 0;
	parenlevel = 0;
	typedefactv = 0;
	prevfield = "";
	#
	# Read in the list of structures we are interested in
	#
	while (0 < (getline < "gtmtypelist.txt"))
		structs[$1]=""
	#
	# Read in the list of excluded structures and remove them from structs
	#
	while (0 < (getline < "gtmexcludetypelist.txt"))
	{
		if ("#" == $1)
			continue;
		delete structs[$1]
	}
}

#
# Main
#
{
	gsub("\".*\"", " ");	# Eliminate double-quoted strings from consideration - don't need them for our parse
	gsub("'.*'", " ");	# Eliminate single-quoted strings from consideration - don't need them for our parse
	#
	# These gsubs are to isolate specific chars with surrounding spaces so our parse can recognize them
	#
	gsub("\\[", " & ");	# AIX awk doesn't (at least at one point) allow for putting [ within a [] block hence
	                        # this is separate from the following line(s).
	gsub("\\]", " & ");
	gsub("[#;:,)({}=*]", " & ");
	#
	# Change 2 word types to single word for more consistent (less complex) parsing
	#
	gsub("unsigned int", "unsigned-int");
	gsub("unsigned long", "unsigned-long");
	gsub("unsigned short", "unsigned-short");
	gsub("unsigned char", "unsigned-char");
	gsub("unsigned int", "unsigned-int");
	gsub("short unsigned", "unsigned-short");
	gsub("signed int", "int");
	gsub("short int", "short");
	gsub("signed short", "short");
	gsub("signed char", "char");
	#
	# Select only typedef types records
	#
	if ("#" == $1)
		next;
	if ("typedef" == $1)
	{
		if (1 == typedefactv)
		{
			print "ERROR - found typedef while typedefactv is set at record", NR;
			exit 1;
		}
		tokenssincetypedef = -1;	# Since we increment it for the typedef as well
		parenssincetypedef = 0;
		isfnptr=0;
		typedefactv = 1;
		foundstruct = 0;
		linecnt = 0;
	}
	if (typedefactv)
	{
		if ("" == $1)
			next;			# Ignore blank lines
		for (i = 1; NF >= i; ++i)	# Process fields of this line in streaming mode
		{
			tokenssincetypedef++;	# allow us to track topside
			if ("{" == $i)
				curlylevel++;	# So we know when we are done with this typedef
			else if ("}" == $i)
				curlylevel--;
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
					if ("volatile" == $i) i++;	# Ignore volatile in this definition
					prevfield = $i;
					prevfldnum = i;
					isfnptr = 1;	# Prevents prevfield from being updated thus maintaining the type we found
				}
				parenssincetypedef++;
			} else if (")" == $i)
				parenlevel--;
			else if (0 == curlylevel && 0 == bracketlevel && (";" == $i || "," == $i))
			{
				if (prevfield in structs)
					foundstruct = 1;
				if (";" == $i)
					typedefactv = 0;
			}
			if (0 == bracketlevel && "]" != $i && 0 == parenlevel && ")" != $i && !isfnptr)
				prevfield = $i;
		}
		#
		# space elimination for easier (and faster) GT.M parsing
		#
		line = "";
		for (i = 1; NF >= i; ++i)
		{
			if (1 != i)
				line = line " " $i
			else
				line = $i
		}
		lines[++linecnt] = line;
		if (0 == typedefactv)
		{	# end of typedef - flush out accumulated lines if this was a structure we care about
			if (foundstruct)
			{
				for (i = 1; linecnt >= i; ++i)
					print lines[i];
				printf("\n");
			}
		}
	}
}
