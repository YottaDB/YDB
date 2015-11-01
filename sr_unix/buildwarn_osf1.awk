# Tru64   : Start at line beginning with "cc:" Print all following lines until
#		the only word in the line is of the form --*\^
BEGIN	{ state = 0;}
	{
		if ("cc:" == $1)
			state = 1;
		if (1 == state)
		{
			print $0;
			if (1 == NF && $1 ~ /--*\^/)
				state = 0;
		}
	}
END	{}
