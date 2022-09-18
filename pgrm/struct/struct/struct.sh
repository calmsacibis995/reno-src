#! /bin/sh
#
#	@(#)struct	4.3	(Berkeley)	89/05/10
#
trap "rm -f /tmp/struct*$$" 0 1 2 3 13 15
files=no
for i
do
	case $i in
	-*)	;;
	*)	files=yes
	esac
done

case $files in
yes)
	/usr/libexec/structure $* >/tmp/struct$$
	;;
no)
	cat >/tmp/structin$$
	/usr/libexec/structure /tmp/structin$$ $* >/tmp/struct$$
esac &&
	/usr/libexec/beautify</tmp/struct$$
