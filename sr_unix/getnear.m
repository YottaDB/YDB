getnear(gbl)
	f i=$length(gbl,","):-1:1  q:(($get(@gbl)'="")!(i=1))  s gbl=$piece($piece(gbl,")"),",",1,i-1)_")"
	i $get(@gbl)'="" q $get(@gbl)
	s gbl=$piece(gbl,"(")
	q $select($get(@gbl)'="":$get(@gbl),1:1)
