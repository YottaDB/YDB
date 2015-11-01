#
#	Start at line where NF = 1 and if next line's NF > 1.
#	Keep printing until you see a line with NF == 1	and $1 == "$gtm_src"
#	Until the start of the archiving section
#
BEGIN	{ state = 0; prev_line = ""; pattern = sprintf("%s\/.*", gtm_src); }
	{
		if (1 == state && "Start" == $1 && "of" == $2)	# reached beginning of archiving => end of compilation
			state = 0;
		else if ($1 ~ pattern)
			state = 1;
		if (1 == state)
		{
			if ((1 == NF) && ($1 ~ pattern))
				prev_line = $0;
			else
			{
				if ("" != prev_line)
					print prev_line;
				prev_line = "";
				print $0;
			}
		}
	}
END	{ }
