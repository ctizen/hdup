/* hduplib.c
 * lib with some general function handy for hdup
 * 
 * $Id: hduplib.c,v 1.95 2004/09/27 07:42:38 miekg Exp $
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
 */

#include"hdup.h"
#include"prototype.h"

extern unsigned int dryrun;

char *dots   = "......................................"; 
char *spaces = "                                      ";
char *overview[OVERVIEW];

int 
hdup_getdate(char * d, int format, int history) 
{
	/* return the date in d
	 * in format. With 0-padding
	 */
	time_t currenttime;
	struct tm *ltime;

	if (history == 1) {
		g_strlcpy(d, STATIC, strlen(STATIC) + 1);
		return FALSE;
	}

	(void)time(&currenttime);
	ltime = localtime(&currenttime);
	switch (format) {
		default:
		case DATE_DEF:
			/* DD-MM-YYYY */
			g_snprintf(d,11, "%.2d-%.2d-%4d",ltime->tm_mday,
					(ltime->tm_mon+1), (ltime->tm_year+1900));
			break;
		case DATE_ISO:
			/* YYYY-MM-DD */
			g_snprintf(d,11, "%4d-%.2d-%.2d",(ltime->tm_year+1900),
					(ltime->tm_mon+1), ltime->tm_mday);
			break;
		case DATE_US:
			/* MM-DD-YYYYY */
			g_snprintf(d,11, "%.2d-%.2d-%4d",(ltime->tm_mon+1),
					ltime->tm_mday,
					(ltime->tm_year+1900));
			break;
	}
	return TRUE;
}

int 
hdup_parsedate(char *d, int format, int *day, int *mon, int *year) 
{
	/* parse a date according to format */
	switch (format) {
		default:
		case DATE_DEF:
			return sscanf(d,"%2d-%2d-%4d",day,mon,year);
		case DATE_ISO:
			return sscanf(d,"%4d-%2d-%2d",year,mon,day);
		case DATE_US:
			return sscanf(d,"%2d-%2d-%4d",mon,day,year);
	}
}

char * 
hdup_makedate(int format, int day, int mon, int year) 
{
	/* return the date in d */
	char *d = g_malloc(11);

	switch (format) {
		default:
		case DATE_DEF:
			/* DD-MM-YYYY */
			g_snprintf(d,11, "%.2d-%.2d-%4d",day,mon,year);
			break;
		case DATE_ISO:
			/* YYYY-MM-DD */
			g_snprintf(d,11, "%4d-%.2d-%.2d",year, mon, day);
			break;
		case DATE_US:
			/* MM-DD-YYYYY */
			g_snprintf(d,11, "%.2d-%.2d-%4d",mon, day, year);
			break;
	}
	return d;
}

/* 5 Oct 2005, Glib, miekg */
int 
hdup_setup(phost_t host[], int which) 
{
	/* create /vol/backup/<hostname>
	 *        /vol/backup/<hostname>/etc
	 *        /vol/backup/<hostname>/<currentdate>
	 */
	char *basename;
	char *d_etc;
	char *d_date;

	/* create the path of the directory */
	basename = g_strconcat(host[which]->archive, host[which]->name, NULL);
	host[which]->basename = basename;

	/* create dir_date */
	d_date = g_strconcat(basename, "/", host[which]->date, NULL);
	host[which]->dirname_date = d_date;

	d_etc = g_strconcat(basename, "/etc", NULL);
	host[which]->dirname_etc = d_etc;

	(void)hdup_mkdir ( host[which]->basename , 0750, host[which]->user,
			   host[which]->group);
	(void)hdup_mkdir ( host[which]->dirname_etc, 0750, host[which]->user,
			   host[which]->group);
	(void)hdup_mkdir ( host[which]->dirname_date, 0750, host[which]->user,
			   host[which]->group);
	return TRUE;
}

void 
hdup_archive(phost_t host, int s) 
{
	/* fill host entry with the archive name */
	if (g_str_equal(host->compression, TAR_BZIP)) {
		host->archivename = g_strdup_printf("%s/%s.%s.%s%s", 
				host->dirname_date, host->name,host->date,
				scheme[s],BZIP_EXT);
		return;
	}
	if (g_str_equal(host->compression, TAR_LZOP)) {
		host->archivename = g_strdup_printf("%s/%s.%s.%s%s",host->dirname_date,
				host->name,host->date,scheme[s],LZOP_EXT);
		return;
	}

	if (g_str_equal(host->compression, TAR_NONE)) {
		host->archivename = g_strdup_printf("%s/%s.%s.%s%s",host->dirname_date,
				host->name,host->date,scheme[s],NONE_EXT);
		return;
	}
	host->archivename = g_strdup_printf("%s/%s.%s.%s%s",host->dirname_date,
			host->name,host->date,scheme[s],GZIP_EXT);
}

int 
hdup_inclist(phost_t host, int s) 
{
	/* 
	 * month        -> delete month/week/day -> use month
	 * weekly       -> if no weekly -> cp month to weekly
	 *              | then use weekly + rm daily
	 * daily        -> if no daily -> cp weekly to daily
	 *              | then use daily
	 */
	char *inc_day, *inc_week, *inc_month = NULL;

	if (s == REMOTE) { 
		LOG("%s","Internal error: wrong function call...");
		hdup_cleanup(1, host);
	}

	inc_month = g_strdup_printf("%s/inclist.%s", host->dirname_etc,scheme[MONTHLY]);
	inc_week = g_strdup_printf("%s/inclist.%s", host->dirname_etc,scheme[WEEKLY]);
	inc_day = g_strdup_printf("%s/inclist.%s", host->dirname_etc, scheme[DAILY]);

	switch (s) {
		case MONTHLY:
			host->inclist = inc_month;
			/* remove it */
			hdup_unlink(host->inclist);
			hdup_unlink(inc_week);
			hdup_unlink(inc_day);
			g_free (inc_week); g_free(inc_day);
			break;
		case WEEKLY:
			host->inclist = inc_week;
			if (g_file_test(inc_month, G_FILE_TEST_IS_REGULAR)) {
				/* does exist cp monthly to weekly */
				if (hdup_cp(inc_month,inc_week) != 0 ) 
					WARN("%s", "Failed to copy, performing full dump");
				hdup_unlink(inc_day);
			} else {
				if ( host->always == 0 )  {
					/* complain and do not go a level up */
					WARN("%s", "Could not find previous backup list");
					WARN("%s", "Backups must be made in order: monthly, weekly and then a daily");
					(void)rmdir(host->dirname_date);
					hdup_cleanup(-1,host);
				} else {
					/* We're forcing a monthly */
					VERBOSE("%s", "Scheme override weekly -> monthly");
					hdup_overview("Scheme","override weekly -> monthly");
					return hdup_inclist(host, MONTHLY);
				}
			}
			g_free(inc_month); g_free(inc_day);
			break;
		case DAILY:
			host->inclist = inc_day;
			if (g_file_test(inc_week, G_FILE_TEST_IS_REGULAR)) {
				VERBOSE("Copying %s over to daily", inc_week);
				/* does exist, cp weekly to daily */
				if ( hdup_cp(inc_week,inc_day ) != 0 )
					WARN("%s","failed to copy, performing full dump");
			} else {
				if ( host->always == 0 ) {
					/* complain and do not go a level up */
					WARN("%s","could not find previous backup list");
					rmdir(host->dirname_date);
					hdup_cleanup(-1,host);
				} else {
					/* Go look for a weekly */
					VERBOSE("%s", "Scheme override daily -> weekly");
					hdup_overview("Scheme","override daily -> weekly");
					return hdup_inclist(host, WEEKLY);
				}
			}
			g_free(inc_month); g_free(inc_week);
			break;
	}
	/* set the user */
	(void)hdup_chown(host->inclist, host->user, host->group);
	VERBOSE("Using inclist %s", host->inclist);
	return s;
}

int
hdup_cp(char *from, char *to) 
{
	int pid, status;
	char *command;

	/* check args */
	if ( from == NULL || to == NULL )
		return 1;

	command = g_strdup_printf("/bin/cp -f %s %s 2>/dev/null", from, to);

	if ((pid = fork()) < 0) {
		perror("fork");
		hdup_cleanup(1,NULL);
	}
	if ( pid == 0 ) {
		/* child */
		execl("/bin/sh", "sh", "-c",
				command,
				(char *)NULL);
		hdup_cleanup(1,NULL);
	}

	while (wait(&status) != pid)
		/* empty */ ;

	if ( status != 0 ) {
		/* shit hit the fan */
		LOG("%s %s %s %s", "Could not perform the copy:", from, "to", to );
		return 1;
	}        
	return 0;
}

int 
hdup_lock(phost_t host, int l) 
{
	/* set/unset a LOCK FILE 
	 * in host->basename/LOCK
	 */
	time_t locktime;
	int lockfd;
	char *lockfile = NULL;

	lockfile = g_strconcat(host->basename, "/LOCK", NULL);
	if (!lockfile) {
		SYSLOG_H("%s", "Internal error");
		FATAL("%s", "Internal error. Could not get LOCK");
	}

	if (dryrun) {
		switch(l) {
			case LOCK:
				VVERBOSE("%s","Faking LOCK creation");
				break;
			case UNLOCK:
				VVERBOSE("%s","Faking LOCK removal");
				break;
		}
		return 0;
	}

	switch (l) {
		case LOCK:
			lockfd = open(lockfile, O_EXCL | O_CREAT, S_IRUSR | S_IWUSR);
			if (-1 == lockfd) {
				if (g_file_test(lockfile , G_FILE_TEST_IS_REGULAR)) {
					/* check the creation time */
					locktime = hdup_file_time(lockfile);
					if (locktime != (time_t) 0) {
						SYSLOG_H("LOCK in %s FOUND (%s)", host->basename, strtok(ctime(&locktime),"\n") );
						FATAL("%s: LOCK in %s found (%s)", host->name, host->basename, strtok(ctime(&locktime), "\n") );
					}
					else {
						SYSLOG_H("LOCK in %s FOUND (%s)", host->basename, "unknown");
						FATAL("%s: LOCK in %s found (%s)", host->name, host->basename, "unknown");
					}
				} else {
					SYSLOG_H("%s","Could not get LOCK");
					FATAL("%s: %s",host->name, "Could not get LOCK");
				}
			}
			/* dirty hack, check how much room there is */
			if (host->free != NULL) {
				if (hdup_free(lockfile) < hdup_convert(host->free)) {
					WARN("%s %s","Need more free space:",host->free);
					SYSLOG_H("Need more free space: %s",host->free);
					hdup_cleanup(-1,host);
				}
			}
			VVERBOSE("%s","Creating LOCK");
			break;

		case UNLOCK:
			if (!g_file_test(lockfile , G_FILE_TEST_IS_REGULAR)) {
				/* hmmm, nothing */
				WARN("%s", "No lockfile found");
				g_free(lockfile);
				return 0;
			} else {
				/* lock found */
				hdup_unlink(lockfile);
			}

			VVERBOSE("%s","Removing LOCK");
			break;
	}
	g_free(lockfile);
	return 0;
}

int 
hdup_system(char * command) 
{
	/* generic command executioner */
	int status = 0;
	char *my_command;
	
	if (quiet > 1) {
		my_command = g_strconcat(command, MUTE, NULL);
	} else {
		my_command = g_strdup(command);
	}

	LOGDEBUG("command: [%s]", my_command);

	if (dryrun) {
		VVERBOSE("Faking: %s", my_command);
		return 0;
	}
	else {
		VVERBOSE("Running: %s", my_command);

		status  = system(my_command);
		if ( status != 0 ) {
			WARN("%s %s","Could not perform the command:", my_command);
			return 1;
		} else 
			return 0;
	}
}

void 
hdup_cleanup(int exitcode, phost_t host) 
{
	/* exit and do some cleanups */
	/* host != NULL -> unlock */

	/* Oh, btw this is a mess */
	/* exitcode = 0 -> print-overview + exit(0)
	 * exitcode =-1 -> no overview + exit(1)
	 * exitcode =-2 -> no overview + exit(0)
	 * exitcode =>0 -> errors + overview + exit(exitcode) */

	if (host != NULL)
		hdup_lock(host, UNLOCK);

	if (hdup_log == 1) 
		closelog();

	/* newline */
	hdup_overview(NULL, NULL);

	/* no explicit status error */
	if ( exitcode > 0 ) {
		WARN("%s", "AN ERROR OCCURED");
		hdup_overview(NULL, NULL);
		hdup_overview("AN ERROR OCCURED!", NULL);
		hdup_overview("NOTHING WAS DONE!", NULL);
		hdup_overview(NULL, NULL);
	}

	/* no overview - abnormal exit */
	if ( exitcode == -1 ) {
		/* extra empty line - readability */
		fprintf(stderr, "\n");
		exit(1);
	}

	/* no overview - normal exit */
	if ( exitcode == -2 )
		exit(0);

	hdup_print();
	exit (exitcode);
}

int 
hdup_overview (char *info, char *value) 
{
	/* fill the overview */
	int li, lv, i;
	static int row_count = 0;

	if (row_count > OVERVIEW )
		FATAL("%s","overview overflow");

	if (info == NULL && value == NULL ){
		/* insert newline */
		overview[row_count] = g_strdup("\n");
		row_count++;
		return 0;
	}

	/* a info.....: value message */
	li = strlen(info); lv = strlen(value);
	i = DOTEND - li;

	if (!value) {
		fprintf(stdout, "%.*s%s\n", DOTEND + 2, spaces, info);
		return 0;
	}

	
	if (i < 0) {
		WARN("%s %s","Overview \'dotend\' underflow with", info);
		return 1;
	}
	overview[row_count] = g_strdup_printf("%s%.*s:  %s\n",
			info,
			i, dots,
			value);

	row_count++;
	return 0;
}

void 
hdup_print(void)
{
	int i = 0;
	if ( quiet != 3 ) {
		/* no 3 -q's */
		while (overview[i] != NULL) {
			fprintf(stdout,"%s", overview[i]);
			i++;
		}
	}
}

/* return size as a multiple of k, M or G */
char *
hdup_humansize(long long size) 
{
	/* give back size in 5k, 5M, 5G,etc */
	float s = 0;
	char *filesize;

	if ( size > GIGABYTE ) {
		s = (float) size / (float) GIGABYTE;
		filesize = g_strdup_printf("%.1f%c",s, 'G');
		return filesize;
	}
	else if (size > MEGABYTE) {
		s = (float) size / (float) MEGABYTE;
		filesize = g_strdup_printf("%.1f%c",s, 'M');
		return filesize;
	}
	else if (size > KILOBYTE) {
		s = (float) size / (float) KILOBYTE;
		filesize = g_strdup_printf("%.0f%c",s, 'k');
		return filesize;
	}
	if (size == 0) {
		return "0";
	} else {
		filesize = g_strdup_printf("%d%c",(int)size, 'B');
		return filesize;
	}
}
	
time_t 
hdup_file_time (char *filepath) 
{
	/* give back the st_ctime, which I (wrongly) treat
	 * as the file creation time
	 */
	struct stat buf;

	if ( filepath == NULL )
		return (time_t) 0;

	if ( stat(filepath, &buf) == 0 ) 
		return buf.st_ctime;
	else 
		return (time_t) 0;
}

char * 
hdup_lastname(char * filepath) 
{
	char *w, *last, *t, *pos;

	if ( filepath == NULL) 
		return NULL;

	/* if this is a split archive 
	 * *__split__ is added to the archive name
	 * We have to chop that off again 
	 */
	pos = strstr(filepath, SPLIT_WILDCARD_EXT);
	if ( pos != NULL )
		*pos='\0'; /* cut string short */

	t = strrchr (filepath, '/');
	if ( t != NULL ) {
		/* advance 1 further */
		t++;
		last = (char*) g_malloc (strlen(filepath));
		w = last;
		while ( *t != '\0' ) {
			*last = *t;
			last++; t++;
		}
		*last = '\0';
		last = w; /* reset */
		return last;
	}
	return NULL;
}

int 
hdup_checkalg(char *algorithm, struct tab *t)
{

	while (t->name != NULL) {
		if (g_str_equal(t->name, algorithm)) {
			return t->sym;
		}
		t++;
	}
	return 0;
}

int 
hdup_globfilecheck(char *globpath, char **firstmatch) 
{
	/* use glob(3) to search for files */

	/* returns the number of found matches */

	glob_t globbuf;
	if (globpath == NULL ) 
		return 0;

	/* doit */
	glob(globpath,0,NULL,&globbuf);
	/* return value the same on all OSses? Look inside structure */
	if ( globbuf.gl_pathc > 0 ) {
		/* we have a match */
		if ( firstmatch != NULL ) {
			*firstmatch = g_strdup(globbuf.gl_pathv[0]);
		}
		globfree(&globbuf);
		return 1;
	} else {
		globfree(&globbuf);
		return 0;
	}
}

int 
hdup_mkdir(char *dirpath, mode_t mode, char *user, char *group) 
{
	char *t, *d;
	struct stat buf;
	size_t l = strlen(dirpath);

	if ( dirpath == NULL ) 
		return -1;

	/* quardian: / */
	d = g_strconcat(dirpath, "/", NULL);

	/* we can go 1 step too far with t++ */
	for (t = d ; *t != '\0' && (unsigned int)(t-d) <= l; t++) {
		if ( *t == '/' ) {
			*t = '\0'; /* make mkdir think the string stops here */
			/* with an absolute path,we have the d is empty on the
			 * first hit... */
			if ( t - d != 0 ) {
				/* first check if the directory already exists */
				if ( stat(d, &buf) != 0 ) {
					/* ok, the stat failed? Could be EEXIST */
				} else {
					/* succesfull stat, something is there
					 */
					if (S_ISDIR(buf.st_mode)) { 
						/* the dir exist, move on */
						*t = '/'; /* go on with the loop */
						continue;
					}
					if (S_ISREG(buf.st_mode)) { 
						/* it's a FILE??? */
						FATAL("%s: %s","Is a file", d);
					}
				}

				if (!dryrun) {
					if (mkdir(d, mode) != 0) {
						if ( errno != EEXIST )
							/* some error has happened - bail out */
							FATAL("%s: %s","Could not create directory", d);
					}
					hdup_chown(d,user, group);
				} else {
					VVERBOSE("Faking mkdir %s", d);
				}
			}
			*t = '/'; /* go on with the loop */
		}
	}
	g_free(d);
	return 0;
}

int 
hdup_chown(char *path, char *user, char *group) 
{
	struct passwd *entry = NULL;
	struct group  *grp = NULL;
	int fd;
	uid_t uid; gid_t gid;

	/* group can be NULL, in that cause use the default
	 * for this user */

	if  (user == NULL || path == NULL) 
		/* no user given */
		return -1;

	if (group) {
		if ((grp = getgrnam(group)) == NULL) 
			return -1;
	}

	if ((entry = getpwnam(user)) != NULL) {
		if (!group)
			gid = entry->pw_gid;
		else
			gid = grp->gr_gid;

		uid = entry->pw_uid;

		fd = open (path, O_RDONLY | O_NONBLOCK | O_NOCTTY);
		if (fd == -1) {
			return 1;
		}
		
		if (fchown(fd, uid, gid) == 0 ) {
			close(fd);
			return 0;
		}
		else {
			/* this give a little too much errors */
			close(fd);
			/* perror("chown"); */
			return 1;
		}
	} else 
		return -1;
}

int 
hdup_chmod(char *path) 
{
	/* set the of a file to 640 */
	mode_t mode;
	if (path == NULL ) 
		return 1;

	mode = S_IRUSR | S_IWUSR | S_IRGRP;
	if ( chmod(path, mode) == 0 ) 
		return 0;
	else {
		perror("hdup_chmod");
		return 1;
	}
}

int 
hdup_chmod_dir(char *dirpath, int s) 
{
	/* go into a dir and give all files in there
	 * a chmod. This does not mean to go in recursively
	 * and do subdirs
	 */
	struct dirent *content;
	DIR * dir; int t = 0;

	dir = opendir(dirpath);

	if ( dir == NULL ) 
		return 0;

	chdir(dirpath);
	while ( (content = readdir (dir))  != NULL ) {
		if ( strstr(content->d_name, SPLIT_EXT) != NULL &&
				strstr(content->d_name, scheme[s]) != NULL ) {
			hdup_chmod(content->d_name);
			t++;
		}
	}

	return t;
}

int 
hdup_chown_dir(char *dirpath, int s, char *user, char *group) 
{
	/* see hdup_chmod_dir */
	struct dirent * content;
	DIR * dir; int t = 0;

	dir = opendir(dirpath);

	if ( dir == NULL ) 
		return 0;

	chdir(dirpath);
	while ( (content = readdir (dir))  != NULL ) {
		if ( strstr(content->d_name, SPLIT_EXT) != NULL &&
				strstr(content->d_name, scheme[s]) != NULL ) {
			hdup_chown(content->d_name, user, group);
			t++;
		}
	}
	return t;
}

int 
hdup_unlink(char *path) 
{
	if ( unlink(path) == 0 ) 
		return 0;
	else 
		return 1;
}

/* create a fake archive
 * make an empty file and compress
 * that if needed
 */
int
hdup_mkfake(phost_t host)
{
        FILE *f;
        char *comp = NULL;
	/* Flawfinder: ignore */
        char *crypt;
        char *cryptcmd;
        unsigned int compression;
        char *arch_no_ext;
        char *n;

        compression = hdup_compression(host->archivename);

        /* cut of extension */
        arch_no_ext = g_strdup(host->archivename);
        switch(compression) {
                case NONE:
                        break;
                case GZIP:
                        n = strstr(arch_no_ext, ".gz");
                        if (n == NULL)
                                WARN("%s","No .gz extension on a gzip file");
                        *n = '\0';
                        return 1;
                case BZIP:
                        n = strstr(arch_no_ext, ".bz2");
                        if (n == NULL)
                                WARN("%s","No .gz extension on a gzip file");
                        *n = '\0';
                        return 1;
                case LZOP:
                        n = strstr(arch_no_ext, ".lzo");
                        if (n == NULL)
                                WARN("%s","No .gz extension on a gzip file");
                        *n = '\0';
                        return 1;
        }
        f = fopen(arch_no_ext, "w+");
        if (f == NULL) {
                WARN("%s", "Could not create empty archive");
		return 1;
	}

	/* use -f (force) on all compression tools */
        switch(compression) {
                case NONE:
                        comp = NULL;
                        break;
                case GZIP:
                        comp = g_strdup_printf("%s -%s -f %s",GZIP_PROG, host->complevel,
                                        arch_no_ext);
                        break;
                case BZIP:
                        comp = g_strdup_printf("%s -%s -f %s",BZIP_PROG, host->complevel,
                                        arch_no_ext);
                        break;
                case LZOP:
                        /* -U make lzop behave like gzip/bzip */
                        comp = g_strdup_printf("%s -U -%s -f %s",LZOP_PROG, host->complevel,
                                        arch_no_ext);
                        break;
        }

        if (comp != NULL) {
                /* compress the empty archive */
                if (hdup_system(comp) != 0) {
                        WARN("%s","Compression of empty archive failed");
			return 1;
		}
        }

        if (host->alg != NULL) {
		/* Flawfinder: ignore */
                crypt = setup_cryptcmd(host);
		/* Flawfinder: ignore */
                cryptcmd = g_strdup_printf("%s %s", crypt, host->archivename);

                if (hdup_system(cryptcmd) != 0) {
                        WARN("%s","Encryption of empty archive failed");
			return 1;
		}
        }
        return 0;
}

int 
hdup_compression(char *archivename) 
{
	/* return the compression used on an archive */
	/* 24 apr 2003: added split support */
	char *tmpstring; char *pos;

	if ( archivename == NULL )
		return -1;

	tmpstring = g_strdup(archivename);

	/* if archive name matches for __split__ we're
	 * stripping that of and leave the rest of the
	 * function untouched */
	if ( (pos = strstr(tmpstring, SPLIT_EXT)) != NULL )
		/* split extension found, put a \0 where __split__
		 * is found. This is after all the other extension.
		 * This way we skip that all together */
		*pos = '\0';

	/* check for encryption */
	if (strstr(tmpstring, MCRYPT_EXT) != NULL )
		return CRYPT;
	/* check for BZIP */
	if (strstr(tmpstring, BZIP_EXT) != NULL )
		return BZIP;
	if (strstr(tmpstring, GZIP_EXT) != NULL )
		return GZIP;
	if (strstr(tmpstring, LZOP_EXT) != NULL )
		return LZOP;
	/* should be none than */
	return NONE;
}

int 
hdup_compression_crypt (char *archivename) 
{
	/* return the compression used on an archive if
	 * that archive is also encrypted 
	 * 24 apr 2003: split support
	 */  
	char *tmpstring; char *pos;

	if ( archivename == NULL )
		return -1;

	tmpstring = g_strdup(archivename);

	if ( (pos = strstr(tmpstring, SPLIT_EXT)) != NULL )
		*pos = '\0';

	if (strstr(tmpstring, MCRYPT_BZIP_EXT) != NULL )
		return BZIP;
	if (strstr(tmpstring, MCRYPT_GZIP_EXT) != NULL )
		return GZIP;
	if (strstr(tmpstring, MCRYPT_LZOP_EXT) != NULL )
		return LZOP;
	if (strstr(tmpstring, MCRYPT_NONE_EXT) != NULL )
		return NONE;

	return -1;
}

int 
hdup_chunk(char *archivename) 
{
	/* check to see if the archive is a split archive */  
	if ( archivename == NULL )
		return -1;

	if ( strstr(archivename, SPLIT_EXT) != NULL )
		return SPLIT;

	return -1;
}

int 
hdup_grep (char *into, char c) 
{
	/* search for specific character in into */
	char *spot;

	if ( into == NULL ) 
		return -1;

	spot = index(into, c);

	if ( spot == NULL ) 
		/* not found */
		return 1;
	else
		return 0;
}

char * 
hdup_subst(char *into, char *what, char *id)
{
	/* substitute string what in the string into
	 * where string id is located
	 * returns pointer to the newly created string 
	 * or the originial string if nothing was found
	 * or *NULL is something went wrong
	 */
	/* There is a function in glib for this */
	char *lwhere, *rwhere;
	char *left, *right; /* the newly formed substrings */
	char *newstring;
	size_t lsize, rsize,id_len, into_len, what_len;

	/* are we called correctly */
	if (!into || !id)
		return NULL;

	if (!what)
		what = g_strdup("-empty-");

	id_len = strlen(id); into_len = strlen(into);
	what_len = strlen(what);

	lwhere = strstr(into, id); /* find where id is located */
	if (lwhere == NULL ) 
		/* nothing found return the original string */
		return into;

	/* left part before id */
	lsize = lwhere - into; 
	left = g_strdup(into);
	*(left + lsize) = '\0'; /* terminator */

	/* right part after id */
	rsize = into_len - ( lsize + id_len) ;
	right = (char*)g_malloc(rsize + 1);
	rwhere = into + into_len - rsize;
	strncpy(right, rwhere, rsize);
	*(right + rsize) = '\0'; /* terminiator */

	/* get *what in between the two parts */
	newstring = g_strdup_printf("%.*s%s%.*s",
			(int)lsize, left,
			what,
			(int)rsize, right);

	/* cannot free these values. See BUG #4 */
#ifdef DEBUG
	printf("newstring: %s\n",newstring);
#endif
	return newstring;
}

char *
hdup_time(time_t time) 
{
	/* print time as hh:mm:ss
	 * time print must be alloced 12 bytes */

	int hour,min,sec,mod;

	mod = time % 3600;
	hour = (time - mod) / 3600;
	time = time - (hour * 3600);

	mod = time % 60;
	min = (time - mod) / 60;

	sec = time - (min * 60);

	if ( hour > 1000 ) {
		return g_strdup(">1000:00:00s");
	} else {
		return g_strdup_printf("%d:%.2d:%.2d",hour,min,sec);
	}
}

int 
hdup_putheader(pheader_t head, FILE *stream) 
{
	/* write a hdup header to stream. This consists of the following:
	 * HDUP\nVERSION\nhostname\nscheme\ndate\ncompression\n
	 * encryption\nextractdir\n
	 */

	fprintf(stream, "%s",head->prot_name);
	fprintf(stream, "%s",PROT_SEP);
	fprintf(stream, "%s",head->prot_ver);
	fprintf(stream, "%s",PROT_SEP);
	fprintf(stream, "%s",head->hostname);
	fprintf(stream, "%s",PROT_SEP);
	fprintf(stream, "%s",head->scheme);
	fprintf(stream, "%s",PROT_SEP);
	fprintf(stream, "%s",head->date);
	fprintf(stream, "%s",PROT_SEP);
	fprintf(stream, "%s",head->compression);
	fprintf(stream, "%s",PROT_SEP);
	fprintf(stream, "%s",head->encryption);
	fprintf(stream, "%s",PROT_SEP);
	fprintf(stream, "%s",head->extractdir);
	fprintf(stream, "%s",PROT_SEP);
	fflush(stream); /* flush it before the archive comes */
	return 0;
}

int 
hdup_getheader(pheader_t head, FILE *stream) 
{
	/* read the header information */

	/* NAME\nVERSION\nHOST\nSCHEME\nDATE\COMPR\nTARFILE\EOF */
	hdup_readword(head->prot_name, stream, strlen(PROT_NAME));
	hdup_readword(head->prot_ver, stream, strlen(PROT_NAME));
	hdup_readword(head->hostname, stream, MAXPATHLEN);
	/* here 1 less the in hdup_fillheader */
	hdup_readword(head->scheme, stream, 8);
	hdup_readword(head->date, stream, 11);
	hdup_readword(head->compression, stream, 5);
	hdup_readword(head->encryption, stream, 1); /* y or n */
	hdup_readword(head->extractdir, stream, MAXPATHLEN); /* need for restore */

	/* sanitize */
	hdup_ok_char(head->prot_name); hdup_ok_char(head->prot_ver);
	hdup_ok_char(head->hostname); hdup_ok_char(head->scheme);
	hdup_ok_char(head->date); hdup_ok_char(head->compression);
	hdup_ok_char(head->encryption); hdup_ok_char(head->extractdir);
	return 0;
}

int 
hdup_fillheader(pheader_t head, phost_t host, int s, char *extractdir) 
{
	/* fill the header with the information from host */
	head->prot_name = g_strdup(PROT_NAME);
	head->prot_ver = g_strdup(PROT_VER);
	head->hostname = g_strdup(host->name);
	head->scheme = g_strdup(scheme[s]);
	head->date   = g_strdup(host->date);
	head->compression = g_strdup(host->compression);

	if (host->alg == NULL) 
		head->encryption = g_strdup("n");
	else 
		head->encryption = g_strdup("y");

	if (extractdir != NULL) 
		head->extractdir = g_strdup(extractdir);
	else 
		head->extractdir = g_strdup("\0");

	return 0;
}

int 
hdup_checkheader(pheader_t head) 
{
	/* check to see if the header is correctly formed
	 * if not, raise error and quit!
	 */
	int s = -1;

	if ( strncmp ( head->prot_name, PROT_NAME, 4 ) != 0 ) {
		WARN("%s", "Protocol: name mismatch");
		return 1;
	}
	if ( strncmp ( head->prot_ver, PROT_VER, 2 ) != 0 ) {
		WARN("%s", "Protocol: version mismatch");
		return 1;
	}
	if ( strncmp (head->encryption, "n", 1) != 0 &&
			strncmp(head->encryption, "y", 1) != 0 ) {
		WARN("%s", "Protocol: encryption must be \'n\' or \'y\'");
		return 1;
	}

	/*  head->hostname is not checked at the moment, but that is not needed
	 *  the normal parsing of the config file is already done.
	 *  head->hostname MUST be equal to arg.host
	 */
	s = hdup_scheme(head->scheme);

	/* check for unknown and remote. The rest is legal */
	if ( s == -1 || s == 3  ) {
		WARN("%s %s","Protocol: scheme error:", head->scheme);
		return 1;
	}

	/* head->date is not checked at the moment */

	/* head->compression is not check at the moment */
	return 0;
}

void 
hdup_sigpipe(int signal) 
{
	/* received a sigpipe, cleanup */

	/* reset handler */
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	sigfillset(&sa.sa_mask);
	sigaction(SIGPIPE, &sa, (struct sigaction *)NULL);

	WARN("%s","SIGPIPE received, cleaning up");
	hdup_pipe = 1;
	return;
}

void 
hdup_sigint(int signal) 
{
	/* received sigint, try to cleanup */
	/* no resetting needed - we're going to exit */
	WARN("%s","SIGINT received, cleaning up");
	hdup_sig = 1;
	return;
}

void
hdup_sigalrm(int signal)
{
	/* alarm went off - this is used during ssh transfers */
	WARN("%s","SIGALRM (timeout) received, cleaning up");
	hdup_alrm = 1;
	return;
}

int 
hdup_scheme(char *sch) 
{
	/* return the scheme sequence number */
	int s = -1;

	/* check the scheme */
	if ( strncmp (sch, scheme[DAILY], strlen(scheme[DAILY])) == 0 )
		s = DAILY;
	if ( strncmp (sch, scheme[WEEKLY], strlen(scheme[WEEKLY])) == 0 )
		s = WEEKLY;
	if ( strncmp (sch, scheme[MONTHLY], strlen(scheme[MONTHLY])) == 0 )
		s = MONTHLY;
	if ( strncmp (sch, scheme[REMOTE], strlen(scheme[REMOTE])) == 0 )
		s = REMOTE;
	if ( strncmp (sch, scheme[RESTORE],strlen(scheme[RESTORE])) == 0 )
		s = RESTORE;

	return s;
}


int 
hdup_readword(char *word, FILE *from, int lim) 
{

	char *l = word;
	int c;
	while ( ( c = getc (from)) != EOF ) {

		if ( c != '\n' ) {
			*l++ = c; lim--;
		} else {
			*l = '\0';
			return 0;
		}

		if ( lim < 0 )
			return 1;

	}
	return 1;
}

int 
hdup_ok_char(char *check)
{
	/* remove dangerous characters */
	/* see: http://www.colorado.edu/htmltools/sanitize.html */
	char * cp;

	for (cp = check; *(cp += strspn(cp, ok_chars)); /* */ ) {
		VERBOSE("%s: %s", "sanitizing", check);
		*cp = '_';
	}

	return 0;
}

long long int 
hdup_free(char *path) 
{
	/* check to see how many bytes are free on the backup file
	 * system */

#ifdef HAVE_SYS_STATVFS_H
	struct statvfs *buf = (struct statvfs *)g_malloc(sizeof(struct statvfs));
#else
	struct statfs *buf = (struct statfs *)g_malloc(sizeof(struct statfs));
#endif /*HAVE_SYS_STATVFS_H*/

	long long int bytes_free = 0;

	if ( path == NULL ) 
		return -1;

#ifdef HAVE_SYS_STATVFS_H
	if ( statvfs(path, buf)  == 0 ) {
#else
	if ( statfs(path, buf)  == 0 ) {
#endif /*HAVE_SYS_STATVFS_H*/
		bytes_free  = buf->f_bavail * buf->f_bsize;
		VERBOSE("Free partition space: %lld", bytes_free);
		return bytes_free;
	} else 
		/* hmm, something wicked happened */
		return -1;
}

long long int 
hdup_convert (char *value) 
{
	/* convert a value like 50k or 50K to a integer.
	 * 50k = 51200 etc */

	/* mul should be 1 instead of 0 MG 2004 21 apr */
	char *cp; long long int v = 0; long int mul = 1;
	cp = value;
	while ( *cp != (char)NULL ) {
		switch (*cp) {
			case 'k':
			case 'K':
				mul = KILOBYTE;
				*cp = '\0'; /* mark as ended */
				goto end;
			case 'm':
			case 'M':
				mul = MEGABYTE;
				*cp = '\0'; /* mark as ended */
				goto end;
			case 'g':
			case 'G':
				mul = GIGABYTE;
				*cp = '\0'; /* mark as ended */
				goto end;
			default:
				break;
		}
		cp++;
	}
end:
	v = strtol(value, NULL, 10);
	v = v * mul;
	/* re-insert size modifier */
	switch (mul) {
		case KILOBYTE:
			*cp = 'K';
			break;
		case MEGABYTE:
			*cp = 'M';
			break;
		case GIGABYTE:
			*cp = 'G';
			break;
	}
	return v;
}

int 
usage() 
{
	fprintf(stdout,"Usage:\n");
	fprintf(stdout,"    hdup [ OPTION ] SCHEME HOST [ @USER@REMOTE_HOST ] (1st format)\n");
	fprintf(stdout,"    hdup [ OPTION ] restore HOST DATE DIRECTORY [ @USER@REMOTE_HOST ] (2nd format)\n\n");
	fprintf(stdout,"    Default config file location: %s\n",ETCFILE);
	fprintf(stdout,"\n");
	fprintf(stdout,"1st format arguments:\n");
	fprintf(stdout,"    SCHEME");
	fprintf(stdout,"               daily, weekly, monthly, remote\n");
	fprintf(stdout,"    HOST");
	fprintf(stdout,"                 specify which host to backup\n");
	fprintf(stdout,"    @USER@REMOTE_HOST");
	fprintf(stdout,"    transfer the archive to this host using USER\n");

	fprintf(stdout,"\n2nd format arguments:\n");
	fprintf(stdout,"    HOST");
	fprintf(stdout,"                 specify which host to restore\n");
	fprintf(stdout,"    DATE");
	fprintf(stdout,"                 restore up to this date\n");
	fprintf(stdout,"    DIRECTORY");
	fprintf(stdout,"            restore to this directory\n");
	fprintf(stdout,"    @USER@REMOTE_HOST");
	fprintf(stdout,"    restore to this host using USER\n");

#ifdef HAVE_GETOPT_H
	fprintf(stdout,"\nOptions:\n");
	fprintf(stdout,"    -c, --config=config\t\tlocation of the configuration file\n");
	fprintf(stdout,"    -s, --specific=file\t\trestore this file from an archive\n");
	fprintf(stdout,"    -i, --igore_tar\t\tignore tar errors when restoring\n");
	fprintf(stdout,"    -I, --igore_conf\t\tignore errors in the config file\n");
	fprintf(stdout,"    -P, --patched-tar\t\ttar is patched, enables --no-recursion and more\n");
	fprintf(stdout,"    -d, --dryrun\t\tdo a dry run, don't touch the filesystem\n");
	fprintf(stdout,"    -V, --verbose\t\tbe more verbose\n");
	fprintf(stdout,"    -V -V\t\t\tbe even more verbose\n");
	fprintf(stdout,"    -q, --quiet\t\t\tsuppress output from subprocesses\n");
	fprintf(stdout,"    -q -q\t\t\tsuppress logging output\n");
	fprintf(stdout,"    -q -q -q\t\t\tno logging at all, even no overview message\n");
	fprintf(stdout,"    -h, --help\t\t\tthis help\n");
	fprintf(stdout,"    -v, --version\t\tshow version\n");
	fprintf(stdout,"    -D, --debug\t\t\tshow debugging information\n");
#else
	fprintf(stdout,"\nOptions:\n");
	fprintf(stdout,"    -c config\tlocation of the configuration file\n");
	fprintf(stdout,"    -s file\trestore this file from an archive\n");
	fprintf(stdout,"    -i\t\tignore tar errors when restoring\n");
	fprintf(stdout,"    -I\t\tignore errors in the config file\n");
	fprintf(stdout,"    -P\t\ttar is patched, enables --no-recursion and more\n");
	fprintf(stdout,"    -d\t\tdo a dry run, don't touch the filesystem\n");
	fprintf(stdout,"    -V\t\tbe more verbose\n");
	fprintf(stdout,"    -V -V\tbe even more verbose\n");
	fprintf(stdout,"    -q\t\tsuppress output from subprocesses\n");
	fprintf(stdout,"    -q -q\tsuppress logging output\n");
	fprintf(stdout,"    -q -q -q\tno logging at all, even no overview message\n");
	fprintf(stdout,"    -h\t\tthis help\n");
	fprintf(stdout,"    -v\t\tshow version\n");
	fprintf(stdout,"    -D\t\tshow debugging information\n");
#endif /*HAVE_GETOPT_H*/
	fprintf(stdout,"\n");
	fprintf(stdout,"Report bugs to <hdup-user@miek.nl>.\n");
	fprintf(stdout,"Or at http://www.miek.nl/cgi-bin/bugzilla/index.cgi\n");
	return 0;
}

int 
version() 
{
	/* print the version of hdup to stderr */
	fprintf(stdout,"%s %s\n",PROGNAME,VERSION);
	fprintf(stdout,"Written by Miek Gieben.\n\n");
	fprintf(stdout,"Copyright (C) 2001-2005 Miek Gieben. This is free software.\n");
	fprintf(stdout,"There is NO warranty; not even for MERCHANTABILITY or FITNESS\n");
	fprintf(stdout,"FOR A PARTICULAR PURPOSE.\n\n");
	fprintf(stdout,"Homepage can be found at http://www.miek.nl/projects/hdup2/\n");
	fprintf(stdout,"Report bugs to <hdup-user@miek.nl>.\n");
	fprintf(stdout,"Or at http://www.miek.nl/cgi-bin/bugzilla/index.cgi\n");
	return 0;
}
