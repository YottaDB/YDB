# Solaris : Among the C compilations, the moment NF != 1 ==> print that line.
BEGIN	{ state = 0; pattern = sprintf("\"%s\/.*\"", gtm_src); }
	{
		if (1 == state && "Start" == $1 && "of" == $2)	# reached beginning of archiving => end of compilation
			state = 0;
		else if ($1 ~ pattern)
			state = 1;
		if ((1 == state) && (1 != NF))
			print $0;
	}
END	{}
