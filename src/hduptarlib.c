/* hduptarlib.c, do all the tar things here
 *
 * $Id: hduptarlib.c,v 1.55 2004/09/25 19:39:16 miekg Exp $
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
 */

#include"hdup.h"
#include"prototype.h"

extern unsigned int ignore_tar_err;
extern unsigned int dryrun;

int 
hdup_dountar(phost_t host, struct argument_t * arg, char *restore) 
{
	/* handle all the untarring */
	char *out = NULL;
	char *tar = NULL;
	char *ssh = NULL;
	char *cat = NULL;
	char *decrypt = NULL;
	FILE *in = NULL;
	unsigned long long int bytes;
	int ret;

	pheader_t header = NULL;
	ret = 0;

	if (host->remote == NULL) {
		/* stay local */
		if (hdup_compression(restore) == CRYPT) {
			/* is something is encrpypted I must have 
			 * the decryption key */
			if (host->alg == NULL ) { 
				VERBOSE("%s", "Archive appears to be encrypted - skipping"); 
				return 1 ; 
			}

			decrypt = setup_decryptcmd(host);

			tar = setup_untarcmd(host,arg->extractdir, arg->extractfile, hdup_compression_crypt(restore));
			out = g_strdup_printf("%s | %s", decrypt, tar);
		} else {
			tar = setup_untarcmd(host, arg->extractdir, arg->extractfile, hdup_compression(restore));
			out = tar;
		}
	} else {
		/* untar from remote sideside */
		/* allocate space, here and only here */
		header = (pheader_t) g_malloc (sizeof(header_t));
		hdup_fillheader(header, host, RESTORE, arg->extractdir);
		ssh = setup_transportcmd(host);
		if (hdup_compression(restore) == CRYPT ) {
			/* there is no special access function... ugly.. */
			VERBOSE("%s", "Archive is encrypted - adding to header"); 
			header->encryption = "y";
		}
		out = ssh;
	}

	/* out contains the OTHER side of the pipe, in is the archive we're dealing 
	 * with everything is setup, now check for compression stuff */
	if (hdup_chunk(restore) == SPLIT) {
		/* oh no, split up archive */
		/* remote or local doesn't matter */
		cat = setup_unsplitcmd(restore);
		LOGDEBUG("%s",cat);

		if (dryrun) {
			VVERBOSE("Faking %s", cat);
			return 0;
		}
		
		if (!ignore_tar_err) {
			ret = pipe2pipe(cat, out, header, &bytes);
			host->bytes = bytes;
			return ret;
		} else {
			/* discard any errors */
			pipe2pipe(cat, out, header, &bytes);
			host->bytes = bytes;
			return 0;
		}

	} else {
		/* ok, normal people still exist */
		in = fopen(restore, "r");
		if (in == NULL ){
			VERBOSE("%s", "Could not open the archive for reading");
			perror(NULL);
			return 1;
		} else 
			if (dryrun) {
				VVERBOSE("Faking %s", restore);
				return 0;
			}
			if (!ignore_tar_err) {
				ret = stream2pipe(in, out, header, &bytes);
				host->bytes = bytes;
				return ret;
			} else {
				stream2pipe(in, out, header, &bytes);
				host->bytes = bytes;
				return 0;
			}
	}
	/* should not reached this */
	VERBOSE("%s", "restore: nothing to do?!");
	host->bytes = 0;
	return 1;
}

int 
hdup_dotar(phost_t host, int s) 
{
	/* this handles all the tars we should ever have to run. Everything is
	 * done with pipes */
	/* we are tarring to stdout, catch it with a pipe and putting it
	 * somewhere different*/

	char *out = NULL;
	char *in  = NULL;
	char *tar = NULL; /* hold entire tar command */
	char *ssh = NULL; /* hold entire ssh command */
	char *crypt = NULL; /* hold entire mcrypt command */
	char *split = NULL; /* hold entire split command */
	pheader_t header = NULL;
	unsigned int t = 0; 
	unsigned long long int bytes;

	/* get tar ready */
	tar = setup_tarcmd(host);
	LOGDEBUG("tar: %s",tar);

	if (host->remote != NULL) {
		/* allocate space here, keeps header NULL otherwise */
		header = (pheader_t) g_malloc (sizeof(header_t));
		ssh = setup_transportcmd(host);
	}

	if (host->alg != NULL)
		crypt = setup_cryptcmd(host);

	if (host->chunksize != NULL)
		split = setup_splitcmd(host);

	/* normal case: backup locally */
	if (host->alg == NULL) {
		/* no crypto */
		if (host->remote == NULL) {
			/* tar cvfz - > archive */
			if (host->chunksize == NULL) {
				out = host->archivename;
				if (dryrun) {
					VVERBOSE("Faking %s", tar);
					return 0;
				}
				if (pipe2file(tar, out, &bytes) != 0) 
					return 1;
				
				host->bytes = bytes;
				(void)hdup_chown (out, host->user, host->group);
				(void)hdup_chmod (out);
				return 0;
			} else {
				/* split */
				out = split;
				LOGDEBUG("split: %s",split);
				if (dryrun) {
					VVERBOSE("Faking %s", tar);
					return 0;
				}
				if (pipe2pipe(tar, out, header, &bytes) != 0)
					return 1;

				host->bytes = bytes;
				t = hdup_chown_dir(host->dirname_date, s, host->user, 
						host->group);
				hdup_overview("Chunks", g_strdup_printf("%d", t));
				hdup_chmod_dir(host->dirname_date, s);
				return 0;
			}

			/* remote: tar cvf - | ssh hdup ... */
			/* no splitting up - happens at the remote side */
		} else	{
			if (dryrun) {
				VVERBOSE("Faking %s", tar);
				return 0;
			}
			hdup_fillheader(header, host, s, NULL);

			out = ssh;
			if (pipe2pipe(tar, out, header, &bytes) != 0)
				return 1;

			host->bytes = bytes;

			return 0;
		}
	} else {
		/* crypto */
		if ( host->remote == NULL ) {
			if ( host->chunksize == NULL ) {
				/* tar cvfz - | crypto - > archive */
				/* add .nc extension */
				host->archivename = g_strconcat(host->archivename, MCRYPT_EXT, NULL);

				/* Flawfinder: ignore */
				out = g_strdup_printf("%s > %s", crypt, host->archivename);
				
				if (dryrun) {
					VVERBOSE("Faking %s", tar);
					return 0;
				}
				if (pipe2pipe(tar,out, header, &bytes) != 0) 
					return 1;

				host->bytes = bytes;
				(void)hdup_chown (host->archivename, host->user, host->group);
				(void)hdup_chmod (host->archivename);
				return 0;
			} else {
				/* also do splitting */
				host->archivename = g_strconcat(host->archivename, MCRYPT_EXT, NULL);

				/* refill spit -- archivename has changed */
				g_free(split); split = setup_splitcmd(host);
				if ( split == NULL ) 
					return 1;

				/* Flawfinder: ignore */
				out = g_strdup_printf("%s | %s", crypt, split);
				LOGDEBUG("out: %s",out);

				if (dryrun) {
					VVERBOSE("Faking %s", out);
					return 0;
				}
				if (pipe2pipe(tar, out, header, &bytes) != 0 ) 
					return 1;

				host->bytes = bytes;
				/* it could be that the whole dir is too much */
				t = hdup_chown_dir(host->dirname_date, s, host->user,
						host->group);
				hdup_overview("Chunks", g_strdup_printf("%d", t));
				hdup_chmod_dir(host->dirname_date, s);
			}
		} else	{
			/* tar cvfz - | crypto | ssh */
			/* no splitting up - happens at the remote side */
			hdup_fillheader(header, host, s, NULL);

			/* Flawfinder: ignore */
			in = g_strdup_printf("%s | %s", tar, crypt);
			if (dryrun) {
				VVERBOSE("Faking %s", tar);
				return 0;
			}
			if (pipe2pipe(in,ssh, header, &bytes) != 0) 
				return 1;

			host->bytes = bytes;
			return 0;
		}
	}
	host->bytes = 0;
	return 0;
}

int 
pipe2file(char *tar, char *out, unsigned long long int *b) 
{
	/* write the tar to a file */
	FILE *pin;
	FILE *archivefile;
	unsigned int c; int stat;
	char buf[BUFSIZE + 1];
	*b = 0;

	archivefile = fopen(out,"w");
	pin = popen(tar,"r");

	LOGDEBUG("full tar: %s",tar);
	LOGDEBUG("out file: %s",out);
	VVERBOSE("Running: %s > %s",tar, out);

	if (archivefile == NULL) { 
		VERBOSE("%s","Could not open archive for writing");
		perror(NULL);
		return 1; 
	}
	if (pin == NULL) { 
		VERBOSE("%s","Could not setup tar for archiving"); 
		perror(NULL);
		return 1; 
	}
	/* Processing loop */
        while(!feof(pin)) {
                c = fread(buf, sizeof(char), BUFSIZE, pin);
                if (fwrite(buf, sizeof(char), c, archivefile) != c) {
                        WARN("%s", "Writing of the archive failed");
                        perror(NULL);
                        return 1; 
                }
		*b += c;
        }
	fclose(archivefile); 

	stat = pclose(pin);
	LOGDEBUG("%s: %d", "tar exit status",stat);
	/* 512; /bin/tar: Error is not recoverable: exiting now */
	/* 512 seems to mean that a zero size archived has been
	 * created
	 */
	if (stat == -1) 
		return 1;

	LOGDEBUG("%s: %d", "tar WIFEXITED exit status",WIFEXITED(stat));
	LOGDEBUG("%s: %d", "tar WEXITSTATUS exit status",WEXITSTATUS(stat));
	if (WIFEXITED(stat) == 0)
		if (WEXITSTATUS(stat) > 0)
			return 1;
	return 0;
}

/* cat a stream to a file
 * return 0 on error
 */
int
stream2file(FILE *in, char *file, unsigned long long int *b)
{
	/* read from the stream and write to file */
	FILE *handle;
	unsigned int c;
	char buf[BUFSIZE + 1];
	*b = 0;

	handle = fopen(file,"w");

	if (handle == NULL) { 
		VERBOSE("%s","Could not open archive for writing"); 
		perror(NULL);
		return 0; 
	}
	if (in == NULL) { 
		VERBOSE("%s", "Could not read from stream"); 
		perror(NULL);
		return 0; 
	}

	/* Processing loop */
	while(!feof(in)) {
                c = fread(buf, sizeof(char), BUFSIZE, in);
                if (fwrite(buf, sizeof(char), c, handle) != c) {
                        WARN("%s", "Writing of the archive failed");
                        perror(NULL);
                        return 1;
                }
                *b += c;
        }

	fclose(handle);
	return 0;
}

int
stream2pipe(FILE *in, char *out, pheader_t head, unsigned long long int *b) 
{
	/* write from the stream and to the pipe */
	FILE *pout;
	unsigned int c; int stat;
	char buf[BUFSIZE + 1];
	*b = 0;

	fflush(stdin);
	fflush(stdout);
	pout = popen(out,"w");

	if (pout == NULL) { 
		VERBOSE("%s","Could not open pipe for writing"); 
		perror(NULL);
		return 1; 
	}
	if (in == NULL) { 
		VERBOSE("%s", "Could not setup stream for reading"); 
		perror(NULL);
		return 1;
	}

	/* is sigalrm helpfull here */

	if (head != NULL) {
		/* print the header first */
		hdup_putheader(head, pout);
	}

	/* Processing loop */
        /* if nothing is waiting on the other side, a SIGPIPE is thrown */
        while(!feof(in)) {
                if (hdup_pipe == 1)
                        return 1;

                c = fread(buf, sizeof(char), BUFSIZE, in);
                if (fwrite(buf, sizeof(char), c, pout) != c) {
                        WARN("%s", "Writing to the pipe failed");
                        perror(NULL);
                        return 1;
                }
		*b += c; 
        }

	stat = pclose(pout);
	if ( stat == -1 ) 
		return 1;

	if ( WIFEXITED(stat) == 0 )
		return 1;
	if ( WEXITSTATUS(stat) > 0 ) 
		return 1;

	return 0;
}

int 
pipe2pipe(char *tar, char *out, pheader_t head, unsigned long long int *b) 
{
	/* write the tar to a pipe */
	FILE *pin;
	FILE *pout;
	unsigned int c; int stat;
	char buf[BUFSIZE + 1];

	pout = popen(out,"w");
	pin = popen(tar,"r");
	*b = 0;

	VVERBOSE("Running: %s | %s", tar, out);

	if ( pout == NULL ) { 
		VERBOSE("%s", "Could not open pipe for writing"); 
		perror(NULL);
		return 1;
	}
	if ( pin == NULL ) { 
		VERBOSE("%s", "Could not setup tar for archiving"); 
		perror(NULL);
		return 1;
	}

	if ( head != NULL ) 
		/* print the header first */
		hdup_putheader(head, pout);

	/* Processing loop */
        while(!feof(pin)) {
                if (hdup_pipe == 1)
                        return 1;

                c = fread(buf, sizeof(char), BUFSIZE, pin);
                if (fwrite(buf, sizeof(char), c, pout) != c) {
                        WARN("%s", "Writing to the pipe failed");
                        perror(NULL);
                        return 1;
                }
		*b += c;
        }

	/* out pipe */
	stat = pclose(pout);
	if (stat == -1 ) 
		return 1;

	/* with ssh 255 is returned when something fails */
	/* otherwise hdup's error is returned, both are more than 0 */
	if ( WIFEXITED(stat) == 0 )
		return 1;
	if ( WEXITSTATUS(stat) > 0  ) 
		return 1;

	/* in pipe */
	stat = pclose(pin);
	if ( stat == -1 ) 
		return 1;

	if ( WIFEXITED(stat) == 0 )
		return 1;

	return 0;
}

char * 
setup_transportcmd(phost_t host) 
{
	/* create a string with the ssh command */
	/* ssh ssh_options user@host hdup remote <hostname> */
	char *ssh;
	ssh = g_strdup_printf("%s %s %s %s %s remote %s", host->proto, host->proto_opt,
			host->remote, host->remote_hdup, host->remote_hdup_opt, host->name);
	return ssh;
}

char * 
setup_cryptcmd(phost_t host) 
{
	/* create a string with the mcrypt command */
	/* mcrypt -a alg -f keypath or
	 * gpg -e blabla */

	/* Flawfinder: ignore */
	char *crypt;

	if ( strcasecmp(host->alg, GPG_ALG) == 0 ) {
		/* yep GPG */
		/* Flawfinder: ignore */
		crypt = g_strdup_printf("%s " GPG_CRYPT ,
			host->gpg,
			host->keypath);
		/* Flawfinder: ignore */
		LOGDEBUG("%s\n",crypt);

	} else {
		/* Flawfinder: ignore */
		crypt = g_strdup_printf("%s -u -q -a %s -f %s",
			host->mcrypt,
			host->alg,
			host->keypath);
	}

	/* f*cking mute still works, even in a pipe :) */
	if ( quiet > 0 ) {
		/* Flawfinder: ignore */
		crypt = g_strconcat(crypt, MUTE, NULL);
	}
	/* Flawfinder: ignore */
	return crypt;
}

char * 
setup_decryptcmd(phost_t host) 
{
	/* create a string with the mcrypt command */
	/* mcrypt -a alg -f keypath */

	/* Flawfinder: ignore */
	char *crypt;

	if ( strcasecmp(host->alg, GPG_ALG) == 0 ) {
		/* yep GPG */
		/* Flawfinder: ignore */
		crypt = g_strdup_printf("%s " GPG_DECRYPT ,
			host->gpg,
			host->keypath);
	} else {
		/* Flawfinder: ignore */
		crypt = g_strdup_printf("%s -d -q -a %s -f %s",
			host->mcrypt,
			host->alg,
			host->keypath);
	}
	/* Flawfinder: ignore */
	LOGDEBUG("%s\n",crypt);

	/* mute still works, even in a pipe */
	if ( quiet > 0 ) {
		/* Flawfinder: ignore */
		crypt = g_strconcat(crypt, MUTE, NULL);
	}

	/* Flawfinder: ignore */
	return crypt;
}

char * 
setup_tarcmd(phost_t host) 
{
	/* create a string with the tar command */
	char *tar; char *comp;

	/* tar will always look like this 
	 * MG 28-7-2005 use --null as we delimit are list with \0's
	 */
	if (patched_tar == 1) {
		tar = g_strdup_printf("%s %s - %s --no-recursion --null --files-from %s --listed-incremental %s --ignore-failed-read %s",
				host->tar, TAR_NONE, SPARSE,
				host->filelist, host->inclist,
				host->tar_conf_opt);


	} else {
		tar = g_strdup_printf("%s %s - %s --files-from %s --null --listed-incremental %s --ignore-failed-read %s",
				host->tar, TAR_NONE, SPARSE,
				host->filelist, host->inclist,
				host->tar_conf_opt);
	}

	if (strncmp(host->compression, TAR_GZIP, strlen(TAR_GZIP)) == 0) {
		comp = g_strdup_printf(" |%s -%s",GZIP_PROG, host->complevel);

		/* add the onefile option */
		if (host->onefile == 1)
			tar = g_strconcat(tar, ONEFILE, NULL);
		if (quiet > 0)
			tar = g_strconcat(tar, MUTE, NULL);
		tar = g_strconcat(tar, comp, NULL);

		LOGDEBUG("tar prog: [%s]", tar);

		return tar;
	}
	if ( strncmp(host->compression, TAR_BZIP, strlen(TAR_BZIP)) == 0 ) {
		comp = g_strdup_printf(" |%s -%s",BZIP_PROG, host->complevel);

		if (host->onefile == 1)
			tar = g_strconcat(tar, ONEFILE, NULL);
		if (quiet > 0)
			tar = g_strconcat(tar, MUTE, NULL);
		tar = g_strconcat(tar, comp, NULL);

		LOGDEBUG("tar prog: [%s]", tar);
		return tar;
	}
	if ( strncmp(host->compression, TAR_LZOP, strlen(TAR_LZOP)) == 0 ) {
		comp = g_strdup_printf(" |%s -%s",LZOP_PROG, host->complevel);

		if (host->onefile == 1)
			tar = g_strconcat(tar, ONEFILE, NULL);
		if (quiet > 0)
			tar = g_strconcat(tar, MUTE, NULL);
		/* add the rest of the string */
		tar = g_strconcat(tar, comp, NULL);

		LOGDEBUG("tar prog: [%s]", tar);
		return tar;
	}

	/* no compression */
	if (host->onefile == 1)
		tar = g_strconcat(tar, ONEFILE, NULL);
	if (quiet > 0)
		tar = g_strconcat(tar, MUTE, NULL);

	LOGDEBUG("tar prog: [%s]", tar);
	VVERBOSE("Setup: %s", tar);
	return tar;
}

char * 
setup_untarcmd(phost_t host, char * extractdir, char *extractfile, int compression) 
{
	/* create a string with the untar command */
	char *tar; char *tar_opt = NULL;

	/* figure out what options to use */
	switch (compression) {
		case NONE:
			tar_opt = UNTAR_NONE;
			break;
		case GZIP:
			tar_opt = UNTAR_GZIP;
			break;
		case BZIP:
			tar_opt = UNTAR_BZIP;
			break;
		case LZOP:
			tar_opt = UNTAR_LZOP;
			break;
		default:
			tar_opt = UNTAR_GZIP;
			break;
	}    
	if (extractfile == NULL) {
		/* completely untar it */
		tar = g_strdup_printf("%s %s - %s %s %s --ignore-failed-read %s --incremental",
				host->tar, UNTAR_OPT, extractdir, SPARSE, tar_opt,
				host->tar_conf_opt);
	} else {
		/* extract a single file */
		tar = g_strdup_printf("%s %s - %s %s --ignore-failed-read --incremental %s %s %s",
				host->tar, UNTAR_OPT,  
				extractdir, SPARSE, tar_opt,
				host->tar_conf_opt, extractfile);
	}
	if (quiet > 0)
		tar = g_strconcat(tar, MUTE, NULL);

	LOGDEBUG("Untar %s\n",tar);
	VVERBOSE("Setup: %s", tar);

	return tar;
}

char * 
setup_splitcmd(phost_t host) 
{
	/* hold the split cmd */
	char *split;
	split = g_strdup_printf("%s -b %s - %s/%s%s",
			SPLIT_PROG,
			host->chunksize,
			host->dirname_date,
			hdup_lastname(host->archivename),
			SPLIT_EXT);
	return split;
}

char * 
setup_unsplitcmd (char * archive) 
{ 
	/* create a string with cat */

	/* lappie.2003-04-24.monthly.tar__split__aa
	 * must be transformed in
	 * lappie.2003-04-24.monthly.tar*__split__??
	 */

	char *pos;

	pos = strstr(archive, SPLIT_EXT);
	if ( pos != NULL ) 
		*pos='\0'; /* cut string short */
	else 
		return NULL;

	/* put new stuff behind it */
	archive = g_strconcat(archive, "*", SPLIT_EXT, NULL);

	return(g_strdup_printf("%s %s??", UNSPLIT_PROG, archive));
}
