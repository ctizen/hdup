/* hdupconf.c
 * parses a hdup config file
 * 
 * $Id: hdupconf.c,v 1.57 2004/10/03 13:00:07 miekg Exp $
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

#include"config.h"
#include"hdup.h"
#include"prototype.h"

extern unsigned int ignore_conf_err;

char *keyword[] =  {
	"dir","exclude", "proto option", "user",
	"compression","prerun","postrun","proto","include",
	"date spec", "algorithm", "key",
	"overwrite", "skip", "force", "sparse", "tar", "find",
	"always backup", "allow remote", "remote hdup", "remote hdup option",
	"archive dir", "mcrypt", "no history", "one filesystem", "chunk size",
	"free", "log", "inherit","compression level", "gpg", "group",
	"tar option", "nobackup"
};

/* This doesn't do quoting, escaping, etc. I don't need that actually */
int 
readline(char *line, FILE *from, int lim, int *linecnt)
{
	char *l = line;
	int c;
	unsigned int escape_seen = 0;

	while ((c = getc (from)) != EOF) {
		/* make possible to escape \n in config */	
		/* escaped , is handled in set_list. Ugly
		 * but I don't want to bring yacc/lex to the table */
		if (escape_seen == 1) {
			if (c == '\n') {
				LOGDEBUG("%s", "Escaped \\n seen");
				*linecnt++;
				escape_seen = 0;
				continue;
			} else {
				/* re-add the \ we chopped off */
				*l++ = '\\'; lim--;
				escape_seen = 0;
			}
		}

		if (c == '\\') {
			escape_seen = 1;
			continue;
		} 
		
		if (c != '\n') {
			*l++ = c; lim--;
		} else {
			*l = '\0'; return 0;
		}
		if (lim < 0) {
			WARN("Config: line %d: %s", *linecnt, "Maximum line length exceeded.");
			return 1;
		}

	}
	return 1;
}

/* todo: check for equal host names in the same config file */
int 
config(char *path, phost_t host[]) 
{
	/* eats a list of host, and fills them up 
	 * parse a hdup.conf2 file:
	 * put the config file in a HOST struct
	 */

	FILE *conf;        
	char *line     = (char *)g_malloc(MAXPATHLEN);
	char *left     = (char *)g_malloc(MAXPATHLEN);
	char *right    = (char *)g_malloc(MAXPATHLEN);
	char *h        = (char *)NULL;
	int i, len;
	int linecnt;
	int slash;

	len = 0; i = -1; linecnt = 0;

	if ((conf = fopen(path,"r"))) {
		while (readline(line, conf, MAXPATHLEN, &linecnt) == 0) {
			len = strlen(line);
			linecnt++;

			/* comment lines */
			if ( *line == '#' || *line == '\n' || *line == '\0' )
				continue; /* next line */

			/* check to see if there is still a '#' in there
			 * somewhere/ Added 3 Oct 2004 MG */
			if (strchr(line, '#') != NULL) 
				*strchr(line, '#') = '\0';

			if ( *line == '[' ) {
				/* new host found */
				if ( ++i > MAXHOST ) {
					FATAL("Config: line %d: %s", linecnt, "Too many hosts defined in the configuration file");
				}

				host[i] = (phost_t) g_malloc (sizeof(host_t));

				/* Solaris complains */
				h = (char*)rindex(line,']');

				if (h == NULL) {
					FATAL("Config: line %d: %s", linecnt, "Could not find closing \']\'");
				}
				
				/* not the first '[' */
				line++;
				host[i]->name = g_strdup(line);
				/* and not the last */
				*(host[i]->name + ( h - line) ) = '\0';

				/* sanitize */
				hdup_ok_char(host[i]->name);

				/* the first entry MUST be [global], hdup depends on this */
				if ( i == 0 && (!g_str_equal(host[0]->name, "global"))) {
					/* SYSLOG is not opened defined here (yet) */
					FATAL("Config: line %d: %s", linecnt, "First host in config file must be named [global]");
				}

				/* fill her up */
				host[i]->date           = NULL;   /* current date */
				host[i]->tar            = NULL;   /* what tar to use */
				host[i]->archive        = NULL;   /* where to store the archives */
				host[i]->datespec       = NULL;
				host[i]->compression    = NULL;   
				host[i]->user           = NULL;   /* default user */
				host[i]->group          = NULL;   /* default user */
				host[i]->basename       = NULL;
				host[i]->dirname_date   = NULL;
				host[i]->dirname_etc    = NULL;
				host[i]->filelist       = NULL;
				host[i]->excludelist    = NULL;
				host[i]->inclist        = NULL;
				host[i]->archivename    = NULL;
				host[i]->keypath        = NULL;
				host[i]->alg            = NULL;
				host[i]->mcrypt         = NULL;
				host[i]->gpg	        = NULL;
				host[i]->chunksize      = NULL;
				host[i]->free           = NULL;
				host[i]->complevel      = NULL;
				host[i]->nobackup	= NULL;
				host[i]->tar_conf_opt	= NULL;

				/* protocol stuff */
				host[i]->proto_opt      = NULL;
				host[i]->proto          = NULL;
				host[i]->remote_hdup	= NULL;
				host[i]->remote_hdup_opt = NULL;
				/* it's not in the config file anymore */
				host[i]->remote         = NULL; 

				host[i]->prerun         = NULL;
				host[i]->postrun        = NULL;

				/* options that are now in the config files */
				host[i]->skip      	= -1;
				host[i]->force     	= -1;
				host[i]->overwrite 	= -1;
				host[i]->sparse 	= -1;
				host[i]->always		= -1;
				host[i]->allow_remote = -1;
				host[i]->history    = -1;
				host[i]->onefile    = -1;
				host[i]->log        = -1;
				/* Thanks to Wouter, bug fixes */
				host[i]->path[0]        = NULL;
				host[i]->exclude[0]     = NULL;
				/* always have this default include 
				 * if we don't do this - the include
				 * program code will include nothing - AND
				 * NO BACKUPS WILL BE MADE
				 */
				host[i]->include[0]     = g_strdup(".*");
				continue; /* next line */
			}
			if ( *line != '[' ) {
				/* should be config line: blah = blih
				 * left part is keyword, right part is data */
				/* char* Solaris fix */

				if (i == -1)
					FATAL("Config: line %d: %s", linecnt, "No [host] statement seen");
				
				h = (char*)index(line,'=');
				if ( h == NULL ) {
					VERBOSE("Config: line %d: %s", linecnt, "Crap seen in config file - skipping");
					continue;
				} else {
					/* actual line lenght is thus
					 * 256+1536 + some spaces */
					sscanf(line, "%256[^=]=%1536[^\n]", left, right);

					/* get ride of spaces */
					g_strstrip(left);
					g_strstrip(right);
					switch (whatkey(left)) {
						case KEY_INHERIT:
							inherit(right, host[i], host, linecnt);
							break;
						case KEY_BACKUP:
							setlist(right, host[i]->path, linecnt);
							break;
						case KEY_EXCLUDE:
							setlist(right, host[i]->exclude, linecnt);
							break;
						case KEY_INCLUDE:
							setlist(right, host[i]->include, linecnt);
							break;
						case KEY_ARCHIVE:
							setvar(right, &host[i]->archive, linecnt);
							slash = strlen(host[i]->archive);

							if ((host[i]->archive)[slash - 1 ] != '/') {
								host[i]->archive = g_realloc(host[i]->archive, slash + 2);
								/* add a closing / if not already there */
								VVERBOSE("Config: line %d: %s", linecnt, "Adding closing \'/\' to archive dir");
								(host[i]->archive)[slash] = '/';
								(host[i]->archive)[slash + 1] = '\0';
							}
							break;
						case KEY_PROTO_OPT:
							setvar(right, &host[i]->proto_opt, linecnt);
							break;
						case KEY_USER:
							setvar(right, &host[i]->user, linecnt);
							break;
						case KEY_GROUP:
							setvar(right, &host[i]->group, linecnt);
							break;
						case KEY_COMP:
							setvar(right, &host[i]->compression, linecnt);
							break;
						case KEY_PRE:
							setvar(right, &host[i]->prerun, linecnt);
							break;
						case KEY_POST:
							setvar(right, &host[i]->postrun, linecnt);
							break;
						case KEY_PROTO:
							setvar(right, &host[i]->proto, linecnt);
							break;
						case KEY_REMOTE_HDUP:
							setvar(right, &host[i]->remote_hdup, linecnt);
							break;
						case KEY_REMOTE_HDUP_OPT:
							setvar(right, &host[i]->remote_hdup_opt, linecnt);
							break;
						case KEY_DATESPEC:
							setvar(right, &host[i]->datespec, linecnt);
							break;
						case KEY_ALG:
							setvar(right, &host[i]->alg, linecnt);
							break;
						case KEY_KEYFILE:
							setvar(right, &host[i]->keypath, linecnt);
							break;
						case KEY_OVERWRITE:
							host[i]->overwrite = yesno(right, keyword[KEY_OVERWRITE],linecnt);
							break;
						case KEY_SKIP:
							host[i]->skip = yesno(right, keyword[KEY_SKIP], linecnt);
							LOG("Config: line %d: %s", linecnt, "Skip is always enabled");
							break;
						case KEY_FORCE:
							host[i]->force = yesno(right, keyword[KEY_FORCE], linecnt);
							break;
						case KEY_ONEFILE:
							host[i]->onefile = yesno(right, keyword[KEY_ONEFILE], linecnt);
							break;
						case KEY_SPARSE:
							host[i]->sparse = yesno(right, keyword[KEY_SPARSE], linecnt);
							LOG("Config: line %d: %s", linecnt, "Sparse is always enabled");
							break;
						case KEY_ALWAYS:
							host[i]->always = yesno(right, keyword[KEY_ALWAYS], linecnt);
							break;
						case KEY_TAR:
							setvar(right, &host[i]->tar, linecnt);
							break;
						case KEY_FIND:
							LOG("Config: line %d: %s", linecnt, "Find is depreciated");
							break;
						case KEY_ALLOW:
							host[i]->allow_remote = yesno(right, keyword[KEY_ALLOW], linecnt);
							break;
						case KEY_HISTORY:
							host[i]->history = yesno(right, keyword[KEY_HISTORY], linecnt);
							break;
						case KEY_LOG:
							host[i]->log = yesno(right, keyword[KEY_LOG], linecnt);
							break;
						case KEY_MCRYPT:
							setvar(right, &host[i]->mcrypt, linecnt);
							break;
						case KEY_CHUNKSIZE:
							/* need lowercase m of k */
							setvar(right, &host[i]->chunksize, linecnt);
							break;
						case KEY_FREE:
							/* need lowercase m of k */
							setvar(right, &host[i]->free, linecnt);
							break;
						case KEY_COMPLEVEL:
							setvar(right, &host[i]->complevel, linecnt);
							break;
						case KEY_GPG:
							setvar(right, &host[i]->gpg, linecnt);
							break;
						case KEY_TAR_OPT:
							setvar(right, &host[i]->tar_conf_opt, linecnt);
							break;
						case KEY_NOBACKUP:
							setvar(right, &host[i]->nobackup, linecnt);
							break;
						default:
							/* not a keyword */
							LOG("Config: line %d: %s %s",linecnt, left," : is not a keyword");
							if (!ignore_conf_err) {
								g_free(left);
								g_free(right);
								return 1;
							}
							break;
					}
				}
			}
		} /* end while */
		g_free(left);
		g_free(right);
		
		host[i+1] = (phost_t) g_malloc (sizeof(host_t));
		host[i+1]->name = NULL; /* possible error: i+1 */
	} else {
		perror(NULL);
		/* not good */
		return 1;
	}
	return 0;
}

int 
findhost(char *hostname, phost_t host[]) 
{
	/* return -1 if none found, otherwise index from host */
	int i = 0;        

	while (host[i]->name != NULL) {
		if (g_str_equal(host[i]->name, hostname)) {
			/* found! */
			return i;
		}
		i++;
	}
	return -1;
}

int 
inherit(char *hostnames, phost_t curr, phost_t host[], int linecnt) 
{
	/* inherit all from hostname into host */
	char* hostlist[MAXDIR];
	int i = 0;

	i = setlist(hostnames, hostlist, linecnt);
	if (i < 0) {
		LOG("Config: line %d: Not able to parse inherit list [%s] so ignoring", linecnt, hostnames);
		return -1;
	}
	for (i = 0; hostlist[i] != NULL && hostlist[i][0] != '\0' && i < MAXDIR; i++) {
		int h = findhost(hostlist[i], host);	
		if (h < 0)
			FATAL("Config: line %d: Cannot find hostname to inherit from [%s]", linecnt, hostlist[i]);

		/* inherit from host[h] -> curr */
		addlist(host[h]->path, curr->path);
		addlist(host[h]->exclude, curr->exclude);
		addlist(host[h]->include, curr->include);
		addvar(host[h]->archive, & curr->archive);
		addvar(host[h]->proto_opt, & curr->proto_opt);
		addvar(host[h]->tar_conf_opt , & curr->tar_conf_opt);
		addvar(host[h]->user, & curr->user);
		addvar(host[h]->compression, & curr->compression);
		addvar(host[h]->prerun, & curr->prerun);
		addvar(host[h]->postrun, & curr->postrun);
		addvar(host[h]->proto, & curr->proto);
		addvar(host[h]->remote_hdup, & curr->remote_hdup);
		addvar(host[h]->remote_hdup_opt, & curr->remote_hdup_opt);
		addvar(host[h]->datespec, & curr->datespec);
		addvar(host[h]->alg, & curr->alg);
		addvar(host[h]->keypath, & curr->keypath);
		addyesno(host[h]->overwrite, & curr->overwrite);
		addyesno(host[h]->skip, & curr->skip);
		addyesno(host[h]->force, & curr->force);
		addyesno(host[h]->onefile, & curr->onefile);
		addyesno(host[h]->sparse, & curr->sparse);
		addyesno(host[h]->always, & curr->always);
		addvar(host[h]->tar, & curr->tar);
		addyesno(host[h]->allow_remote, & curr->allow_remote);
		addyesno(host[h]->history, & curr->history);
		addyesno(host[h]->log, & curr->log);
		addvar(host[h]->mcrypt, & curr->mcrypt);
		addvar(host[h]->gpg, & curr->gpg);
		addvar(host[h]->chunksize, & curr->chunksize);
		addvar(host[h]->free, & curr->free);
		addvar(host[h]->complevel, & curr->complevel);
		addvar(host[h]->nobackup, & curr->nobackup);
	}
	return 0;
}

int 
setvar (char *dir, char **var, int linecnt) 
{
	if (!dir) {
		return 0;
	}
	
	/* set var to dir */
	if (strlen(dir) > MAXPATHLEN)
		return -1;

	if (pure_whitespace(dir) != -1) {
		*var = NULL;
		WARN("Config: line %d: Resetting keywords like this, will not work", linecnt);
		return 0;
	}

	*var = g_strdup(dir);
	hdup_ok_char(*var);
	g_strstrip(*var);
	return 0;
}

int 
addvar (char* org, char **cur) 
{
	/* add org over cur - already stripped + okayed */
	if (org != NULL) {
		if (*cur != NULL) {
			free(*cur);
			*cur = NULL;
		}
		*cur = g_strdup(org);
	}
	return 0;
}

int 
yesno (char *what, char *which, int linecnt) 
{
	/* there are four possible answer in such a config option
	 * no/off -> 0, or yes/on -> 1
	 */

	if ( strncmp(what, "no", 2) == 0 ) { return 0; }
	if ( strncmp(what, "false", 5) == 0 ) { return 0; }
	if ( strncmp(what, "off", 3) == 0 ) { return 0; }

	if ( strncmp(what, "yes", 3) == 0 ) { return 1; }
	if ( strncmp(what, "true", 4) == 0 ) { return 1; }
	if ( strncmp(what, "on", 2) == 0 ) { return 1; }

	LOG("Config: line %d: No \'on/yes\' or \'off/no\' specified, going with \'off/no\' for %s", linecnt, which);
	return 0;	/* default is off/no */
}

int 
addyesno (int org, int *cur) 
{
	/* add org over cur */
	if (org != -1) {
		*cur = org;
	}
	return 0;
}

int 
setlist (char *dirpath, char *hostpath[], int linecnt) 
{
	/* parse a comma seperate list of directories
	 * allow escaping of , by using \,
	 */

	int l;
	int t = 0;

	if (pure_whitespace(dirpath) != -1) {
		hostpath[0] = NULL;
		WARN("Config: line %d: Resetting keywords like this, will not work", linecnt);
		return 0;
	}
	
	for(l = strlen(dirpath); l >= 0 ; l--) {
		if (*(l + dirpath) == ',' ) {
			if (*(l + dirpath - 1) == '\\') {
				LOGDEBUG("%s", "Escaped \\, seen");
				/* skip this one and even more ugly turn
				 * the \ into a space */
				*(l + dirpath - 1) = ' ';
				continue;
			}
			/* not the ',' */
			hostpath[t] = g_strdup(dirpath + l + 1);
			g_strstrip(hostpath[t]);
			*(l+dirpath) = '\0';
			t++; 
			continue;
		}
		if (l == 0) {
			hostpath[t] = g_strdup(dirpath);
			/* may be optional */
			g_strstrip(hostpath[t]);
			t++;
		}
		if (t > MAXDIR) 
			return -1;
	}
	hostpath[t] = (char*)NULL;
	return 0;
}

int 
addlist (char *org[], char *cur[]) 
{
	/* append each of org onto cur */

	if (org != NULL && org[0] != NULL) {
		int t = 0, i = 0;
		while ( t < MAXDIR && cur[t] != NULL && cur[t][0] != '\0' )
			t++;
		while ( t < MAXDIR && org[i] != NULL && org[i][0] != '\0') {
			cur[t] = g_strdup(org[i]);
			t++; i++;
		}
		if (i > 0) {
			cur[t] = (char*)NULL;
		}
	}
	return 0;
}

int 
whatkey(char *key) 
{
	/* found out what keyword is found */
	int i;
	for (i = 0; i < KEYWORDS; i++) {
		if (g_str_equal(key, keyword[i])) {
			return i;
		} 
	}
	return -1; /* no keyword found */
}

/**
 * check if a string is only whitespace
 * if so - return 0
 * otherwise return -1
 */
int
pure_whitespace(char *str)
{
	char *s;
	
	if (!str) {
		return 0;
	}

	for(s = str; *s; s++) {
		/* avoid isblank because it's a gnu-ism. bug 22 */
		if (*s != '\t' || *s != ' ') {
			return -1;
		}
	}
	return 0;
}
