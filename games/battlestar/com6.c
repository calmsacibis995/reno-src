/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static char sccsid[] = "@(#)com6.c	5.5 (Berkeley) 6/1/90";
#endif /* not lint */

#include "externs.h"
#include "pathnames.h"

launch()
{
	if (testbit(location[position].objects,VIPER) && !notes[CANTLAUNCH]){
		if (fuel > 4){
			clearbit(location[position].objects,VIPER);
			position = location[position].up;
			notes[LAUNCHED] = 1;
			time++;
			fuel -= 4;
			puts("You climb into the viper and prepare for launch.");
			puts("With a touch of your thumb the turbo engines ignite, thrusting you back into\nyour seat.");
			return(1);
		}
		else
			puts("Not enough fuel to launch.");
	 }
	 else
		puts("Can't launch.");
	 return(0);
}

land()
{
	if (notes[LAUNCHED] && testbit(location[position].objects,LAND) && location[position].down){
		notes[LAUNCHED] = 0;
		position = location[position].down;
		setbit(location[position].objects,VIPER);
		fuel -= 2;
		time++;
		puts("You are down.");
		return(1);
	}
	else
		puts("You can't land here.");
	return(0);
}

die() 		/* endgame */
{
	printf("bye.\nYour rating was %s.\n", rate());
	post(' ');
	exit(0);
}

live()
{
	puts("\nYou win!");
	post('!');
	exit(0);
}

/*
 * sigh -- this program thinks "time" is an int.  It's easier to not load
 * <time.h> than try and fix it.
 */
#define KERNEL
#include <sys/time.h>
#undef KERNEL

post(ch)
char ch;
{
	FILE *fp;
	struct timeval tv;
	char *date, *ctime();
	int s = sigblock(sigmask(SIGINT));

	gettimeofday(&tv, (struct timezone *)0);	/* can't call time */
	date = ctime(&tv.tv_sec);
	date[24] = '\0';
	if (fp = fopen(_PATH_SCORE,"a")) {
		fprintf(fp, "%s  %8s  %c%20s", date, uname, ch, rate());
		if (wiz)
			fprintf(fp, "   wizard\n");
		else if (tempwiz)
			fprintf(fp, "   WIZARD!\n");
		else
			fprintf(fp, "\n");
	} else
		perror(_PATH_SCORE);
	sigsetmask(s);
}

char *
rate()
{
	int score;

	score = max(max(pleasure,power),ego);
	if (score == pleasure){
		if (score < 5)
			return("novice");
		else if (score < 20)
			return("junior voyeur");
		else if (score < 35)
			return("Don Juan");
		else return("Marquis De Sade");
	}
	else if (score == power){
		if (score < 5)
			return("serf");
		else if (score < 8)
			return("Samurai");
		else if (score < 13)
			return("Klingon");
		else if (score < 22)
			return("Darth Vader");
		else return("Sauron the Great");
	}
	else{
		if (score < 5)
			return("Polyanna");
		else if (score < 10)
			return("philanthropist");
		else if (score < 20)
			return("Tattoo");
		else return("Mr. Roarke");
	}
}

drive()
{
	if (testbit(location[position].objects,CAR)){
		puts("You hop in the car and turn the key.  There is a perceptible grating noise,");
		puts("and an explosion knocks you unconscious...");
		clearbit(location[position].objects,CAR);
		setbit(location[position].objects,CRASH);
		injuries[5] = injuries[6] = injuries[7] = injuries[8] = 1;
		time += 15;
		zzz();
		return(0);
	}
	else
		puts("There is nothing to drive here.");
	return(-1);
}

ride()
{
	if (testbit(location[position].objects,HORSE)){
		puts("You climb onto the stallion and kick it in the guts.  The stupid steed launches");
		puts("forward through bush and fern.  You are thrown and the horse gallups off.");
		clearbit(location[position].objects,HORSE);
		while (!(position = rnd(NUMOFROOMS+1)) || !OUTSIDE || !beenthere[position] || location[position].flyhere);
		setbit(location[position].objects,HORSE);
		if (location[position].north)
			position = location[position].north;
		else if (location[position].south)
			position = location[position].south;
		else if (location[position].east)
			position = location[position].east;
		else
			position = location[position].west;
		return(0);
	}
	else puts("There is no horse here.");
	return(-1);
}

light()		/* synonyms = {strike, smoke} */
{		/* for matches, cigars */
	if (testbit(inven,MATCHES) && matchcount){
		puts("Your match splutters to life.");
		time++;
		matchlight = 1;
		matchcount--;
		if (position == 217){
			puts("The whole bungalow explodes with an intense blast.");
			die();
		}
	}
	else puts("You're out of matches.");
}
