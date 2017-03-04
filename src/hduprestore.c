/* hduprestore.c
 * restore a filesystem stored with hdup
 * 
 * $Id: hduprestore.c,v 1.40 2004/09/25 20:54:56 miekg Exp $
 *
 * Copyright:
 *
 *   This package is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; version 2 dated June, 1991.
 * 
 *   This package is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 * 
 *   You should have received a copy of the GNU General Public License
 *   along with this package; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *   02111-1307, USA.
 *
 *   Changelog:
 *    06 mar 2003: miekg: reworked the entire restore stuff 
 *    16 mar 2003: miekg: reworked must of it again,for version 1.6.6 
 *    17 mar 2003: miekg: match name: tar.* -> tar* (catches none compression)
 *    05 may 2003: miekg: did not add syslogging here: seems a bit pointless
 *
 */

#include"hdup.h"
#include"prototype.h"

void 
backup_restore(phost_t host[], int which, struct argument_t *arg ) 
{
	/* restore an archive, possibly a remote one */
	char *restore[MONTHLY + 1]; 
	int i=0; int stat=0; 
	int failure=1; 
	int date_format = 0;
	int how_many = -1;

	/* some basic values need to be filled in */
	hdup_setup(host,which);

	/* set the archive to null */
	restore[MONTHLY] = NULL; restore[WEEKLY] = NULL; restore[DAILY] = NULL;

	/* check for restoring to / */
	if (g_ascii_strncasecmp(arg->extractdir,"/",1) == 0 && strlen(arg->extractdir) == 1 ) {
		if ( host[which]->force == 0 ) 
			FATAL("%s", "Restoring to '/' is not a good idea");
		else 
			LOG("%s", "Forcing restore to '/'");
	}

	/* check the date format */
	if (g_ascii_strncasecmp(host[which]->datespec, "iso", 3) == 0) { 
		date_format = DATE_ISO;
	} else if (g_ascii_strncasecmp(host[which]->datespec, "american", 8) == 0) {
		date_format = DATE_US;
	} else {
		date_format = DATE_DEF;
	}

	if ((how_many = find_restore(arg,host[which], restore, date_format) ) == -1) 
		FATAL("%s", "No archives found: nothing restored");

	if (how_many > 3)
		FATAL("Found to many (%d) archives for restore", how_many);

	if (how_many < 2)
		LOG("%s", "No monthly archive found. Trying to restore from what I have");

	/* the find_restore function return the archive in restore[]. I must
	 * walk that array in the reverse order: 3 2 1. If it's not null I
	 * should extract it. */

	/* if only 1 stat is successfull we're in. If not there really is an error */

	if (host[which]->remote == NULL) {
		if (host[which]->alg != NULL) {
			/* when moving stuff to the other side - the encryption
			 * information in the config file is prob. bogus. MG 25
			 * sept 2004 */
			hdup_overview("Encryption", g_strdup_printf("yes (%s, %s)", host[which]->alg, host[which]->keypath));
		} else {
			hdup_overview("Encryption", "no");
		}
	}

	for (i = how_many; i >= 0; i--) {
		host[which]->date = arg->date;
		if ( restore[i] != NULL ) {
			VERBOSE("%s %s", "Restoring from",restore[i]);
			stat = hdup_dountar(host[which], arg, restore[i]);
			if (stat != 0 && host[which]->remote != NULL) {
				/* no point in trying do something next */
				break;
			}

			hdup_overview("Archives", hdup_lastname(restore[i]));
		}
		/* some tar has been lucky */
		if (stat == 0) 
			failure = 0;
	}

	if (failure == 0) 
		hdup_overview("Status","successfully restored backup");
	else 
		hdup_cleanup(1,host[which]);
}

int 
find_restore(struct argument_t *arg, phost_t host, char *restore[], int date_format) 
{
	/* restore strategy:
	 * first look for daily, if found look for weekly, if found look for
	 * monthly. This all goes backwards in time (yes we can do that)
	 * At most we will have 3 files to untar! Thank you Boris for pointing
	 * the obvious out to me...
	 */
	int t,p,h,j;char *d;
	char *directory,*backupdir;
	char *archivename;char *matchname;
	int found;

	backupdir = host->archive;
	p = strlen(backupdir); h = strlen(arg->host);
	d=(char*) g_malloc(12);
	directory = (char*)g_malloc( p + h + 11 + 5);

	if (host->history == 1 ) 
		d = g_strdup(STATIC);
	else 
		d = g_strdup(arg->date);

	found = -1;
	for(t = 0;t <= MAXRESTORE; t++) {
		/* create the dirname */
		directory = g_strdup_printf("%s%s/%s",backupdir,arg->host,d);

		/* the date dir at least exists */
		VERBOSE("%s %s","Restore: looking in",directory);
		if (g_file_test(directory, G_FILE_TEST_IS_DIR)) {

			/* found = -1 the first time around...MG 1-7-2004 */
			/* start with daily then weekly then monthly */
			for (j=found + 1; j <= MONTHLY; j++) {
				/* if a 'static' archive is upped to a remote machine and placed
				 * in a normal date directory hdup will not find it
				 * make the date in archive and it will keep on working
				 * Miek 16 Apr 2003
				 */
				matchname = g_strdup_printf("%s/%s.*.%s.tar*", directory,arg->host,scheme[j]);
				LOGDEBUG("matchname %s", matchname);
				if ( hdup_globfilecheck(matchname,&archivename) == 1) {
					if (restore[j] == NULL) {
						restore[j] = g_strdup(archivename);
						g_free(archivename);
						found = j;
					}
				}
				g_free(matchname);
			}
		}
		if (found == MONTHLY) {
			/* found them all, or at least a montly */
			return MONTHLY; /* found something */
		}
		if (host->history == 1) {
			/* no history only 1 dir to look in */
			return found;
		} else {
			/* the previous day */
			LOGDEBUG("Current day: %s", d);
			d = day(d, PREV, date_format);
			LOGDEBUG("Previous day: %s", d);
		}
	}
	return found; /* nothing found */
}

char * 
day(char *date, int direction, int date_format) 
{
	/* substract or add  one day from date and return the result */
	struct tm *ltime = (struct tm*)g_malloc(sizeof(struct tm));
	time_t abstime;
	char *mydate;
	int day,month,year;

	/* unwind it */
	hdup_parsedate(date,date_format,&day,&month,&year);
	if (year < 1990) 
		WARN("Unlikely year seen (%d). Is the date entered correctly?", year);

	/* do your calculations */
	ltime->tm_mday = day;
	ltime->tm_mon  = (month -1);
	ltime->tm_year = (year - 1900);

	ltime->tm_sec = 0; ltime->tm_min = 0;
	ltime->tm_hour = 0; ltime->tm_wday = 0;
	ltime->tm_yday = 0; ltime->tm_isdst = 0;

	abstime = mktime(ltime);        
	/* substract a day 
	 * if direction is NEXT (+1) this add one day
	 * if direction is PREV (-1) this substracts one day
	 */
	abstime = abstime + ( direction * SECDAY );
	ltime = localtime(&abstime);

	day = ltime->tm_mday;
	month = (ltime->tm_mon + 1);
	year = (ltime->tm_year + 1900);

	/* put is back together */
	mydate = hdup_makedate(date_format, day, month, year);

	return mydate;
}
