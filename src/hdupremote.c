/* hdupremote .c, a remotely receive the backup
 *
 * $Id: hdupremote.c,v 1.22 2004/09/25 20:41:24 miekg Exp $
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

void 
backup_remote(phost_t host[], int which) 
{
	/* receive a header from stdin, after that comes the tarfile.
	 * From the header we can deduce what to do with the archive.
	 * for daily, weekly and monthly we just put the archive in the
	 * correct place, and possibly add encryption
	 * remote is illegal
	 * restore is also special
	 */
	pheader_t header = NULL; 
	char *out = NULL; 
	char *decrypt = NULL;
	char *tar = NULL;
	char *split = NULL;
	unsigned int t = 0;
	unsigned long long int bytes;
	
	/* make the dirs */
        hdup_setup(host, which);

	/* do the allocation your self here */
	header = (pheader_t) g_malloc (sizeof(header_t));

	/* make some room */
	header->prot_ver  = (char*) g_malloc( strlen(PROT_VER) + 1);
	header->prot_name = (char*) g_malloc( strlen(PROT_NAME) + 1 );
	header->hostname   = (char *)g_malloc(MAXPATHLEN);
	header->scheme = (char *)g_malloc(8);  
	header->date   = (char *)g_malloc(11);
	header->compression = (char *)g_malloc(5);
	header->encryption = (char *)g_malloc(1);
	header->extractdir = (char *)g_malloc(MAXPATHLEN);

	hdup_getheader(header, stdin);
	if ( hdup_checkheader(header) != 0 ) {
		/* header is bogus */
		LOG("%s","Protocol: bogus header");
		hdup_cleanup(-1,host[which]);
	}

	if(!g_str_equal(header->hostname, host[which]->name)) {
		/* header->hostname MUST match host[which]->name */
		LOG("%s","Protocol: local hostname does not match remote hostname");
		hdup_cleanup(-1,host[which]);
	}

	host[which]->date = header->date; /* copy from stdin */
	/* forget the put something in hdup->compression [miek 26-06-2003] */
	host[which]->compression = header->compression;

	/* scheme is already checked */
	hdup_archive(host[which], hdup_scheme(header->scheme));
	hdup_overview("Scheme rcvd",header->scheme);

	if (g_ascii_strncasecmp(header->scheme,scheme[RESTORE], strlen(scheme[RESTORE])) == 0) {
		/* remote restore operation */
		/* tell this here, MG 18 May 2004 */
		LOG("%s: STARTING REMOTE RESTORE", host[which]->name);

		/* should check if the file is split */
		/* check the extract dir */
		if ( hdup_globfilecheck(header->extractdir,NULL) == 0 )
			FATAL("%s %s %s", "Extract dir: ", header->extractdir, " does not exist");

		/* select the right compression, from the header... */
		/* extractfile is NULL here - so it does not work remotely */
		if (g_ascii_strncasecmp(header->compression, TAR_BZIP, strlen(TAR_BZIP)) == 0) 
			tar = setup_untarcmd(host[which],header->extractdir,NULL, BZIP);
		if (g_ascii_strncasecmp(header->compression, TAR_GZIP, strlen(TAR_BZIP)) == 0) 
			tar = setup_untarcmd(host[which],header->extractdir,NULL, GZIP);
		if (g_ascii_strncasecmp(header->compression, TAR_NONE, strlen(TAR_BZIP)) == 0) 
			tar = setup_untarcmd(host[which],header->extractdir,NULL, NONE);

		if (host[which]->alg != NULL) {
			if (g_ascii_strncasecmp(header->encryption, "y",1) == 0 ) {
				VERBOSE("%s", "Encryption found - trying to decrypt");
				decrypt = setup_decryptcmd(host[which]);
				VVERBOSE("%s %s", "Decryption command: ", decrypt);
				out = g_strdup_printf("%s | %s", decrypt, tar);
			} else {
				VERBOSE("%s","Encryption specified in config - not using");
				out = tar;
			}
		} else 
			out = tar;

		/* our output command has been specified */
		/* dont lock for restore
		hdup_lock(host[which], LOCK);
		*/

		if (stream2pipe(stdin, out, NULL, &bytes) != 0) {
			LOG("%s", "Could not remotely restore the archive");
			hdup_cleanup(-1,host[which]);
		}
	} else if (g_ascii_strncasecmp(header->scheme,scheme[REMOTE], strlen(scheme[REMOTE])) == 0) {
		LOG("%s","Protocol: Illegal scheme: \'remote\'");
		hdup_cleanup(-1,host[which]);
	} else {
		/* place it on disk - check the chunksize in the config */
		LOG("%s: STARTING REMOTE BACKUP", host[which]->name);

		/* first check the encryption flag in the header */
		if (g_ascii_strncasecmp(header->encryption,"y",1) == 0) 
			host[which]->archivename = g_strconcat(host[which]->archivename, MCRYPT_EXT, NULL);

		if ( host[which]->chunksize != NULL ) 
			split = setup_splitcmd(host[which]);

		/* Skip crypto stuff in the config file. This also allows the
		 * configfiles at both ends to be identical
		 */
		if ( host[which]->alg != NULL ) 
			VERBOSE("%s","Will not encrypt archives from remote hosts - encrypt them locally");

		/* set the lock */
		(void)hdup_lock(host[which], LOCK);
		if ( host[which]->chunksize == NULL ) {
			if (stream2file(stdin, host[which]->archivename, &bytes) == 0) {
				(void)hdup_chown (host[which]->archivename, host[which]->user,
						  host[which]->group);
				(void)hdup_chmod (host[which]->archivename);
				VERBOSE("wrote %s", hdup_humansize((long long)bytes));
			} else {
				LOG("%s", "Could not create archive");
				hdup_cleanup(-1,host[which]);
			}
		} else {
			/* splitting */
			if (stream2pipe(stdin, split, NULL, &bytes) == 0) {
				t = hdup_chown_dir(host[which]->dirname_date,
						hdup_scheme(header->scheme) , host[which]->user,
						host[which]->group);
				VERBOSE("Chunks created: %d", t);
				VERBOSE("Chunk size: %s",host[which]->chunksize);
				hdup_chmod_dir(host[which]->dirname_date,hdup_scheme(header->scheme));
			} else {
				LOG("%s", "Could not create archive");
				hdup_cleanup(-1,host[which]);
			}
		}
	}
}
