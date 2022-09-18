#! /bin/sh
#
#	@(#)indxbib.sh	4.2	(Berkeley)	89/04/28
#
#	indxbib sh script
#
if test $1
	then /usr/libexec/mkey $* | /usr/libexec/inv _$1
	mv _$1.ia $1.ia
	mv _$1.ib $1.ib
	mv _$1.ic $1.ic
else
	echo 'Usage:  indxbib database [ ... ]
	first argument is the basename for indexes
	indexes will be called database.{ia,ib,ic}'
fi
