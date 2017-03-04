/* hduprun.c, 
 *
 * functions for running the pre and post run scripts.
 *
 * $Id: hduprun.c,v 1.12 2004/06/01 12:17:40 miekg Exp $
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

int 
hdup_runpre(phost_t host, char *scheme[], int which_scheme)
{
	/* run the pre script and subsitute variables */
	/* hostname */
	char *tmp = hdup_subst(host->prerun,host->name,subvar[0]);
	char *command = hdup_subst(host->prerun,host->name,subvar[0]);
	int c;
	LOGDEBUG("%s %s","prerun",command);

	/* we must do some prerun stuff
	 * start with 1, as we have done 0 above
	 */
	for (c = 1; c < SUBS; c++) {
		switch(c) {
			case 1:
				command = hdup_subst(tmp,host->archivename,subvar[1]); 
				break;
			case 2:
				/* scheme */
				command = hdup_subst(tmp,scheme[which_scheme],subvar[2]);
				break;
			case 3:
				/* encryption */
				if (host->alg != NULL) 
					command = hdup_subst(tmp,"yes", subvar[3]);
				else 
					command = hdup_subst(tmp,"no", subvar[3]);
				break;
			case 4:
				/* user */
				command = hdup_subst(tmp,host->user,subvar[4]);
				break;
			case 5:
				if (host->chunksize != NULL)
					command = hdup_subst(tmp,"yes", subvar[5]);
				else 
					command = hdup_subst(tmp,"no", subvar[5]);
				break;
			case 6:
				/* group */
				command = hdup_subst(tmp,host->group,subvar[6]);
				break;
		}
		tmp = command; /* for the next run */
	}
	command = tmp; /* last thing to do */

	LOGDEBUG("%s %s","prerun",command);

	if ( hdup_system(command) != 0 ) {
		g_free(command);
		/* something went wrong */
		return 1;
	}
	VERBOSE("%s","Succesfully ran pre script");
	return 0;
}

int
hdup_runpost(phost_t host, char *scheme[], int which_scheme) 
{
	/* we must do some postrun stuff */
	/* hostname */
	char *command = hdup_subst(host->postrun,host->name,subvar[0]);
	char *tmp = hdup_subst(host->postrun,host->name,subvar[0]);
	int c;

	/* we must do some prerun stuff
	 * start with 1, as we have done 0 above
	 */
	for (c = 1; c < SUBS; c++) {
		switch(c) {
			case 1:
				command = hdup_subst(tmp,host->archivename,subvar[1]); 
				break;
			case 2:
				/* scheme */
				command = hdup_subst(tmp,scheme[which_scheme],subvar[2]);
				break;
			case 3:
				/* encryption */
				if ( host->alg != NULL ) 
					command = hdup_subst(tmp,"yes", subvar[3]);
				else 
					command = hdup_subst(tmp,"no", subvar[3]);
				break;
			case 4:
				/* user */
				command = hdup_subst(tmp,host->user,subvar[4]);
				break;
			case 5:
				if (host->chunksize != NULL)
					command = hdup_subst(tmp,"yes", subvar[5]);
				else 
					command = hdup_subst(tmp,"no", subvar[5]);
				break;
			case 6:
				/* group */
				command = hdup_subst(tmp,host->group,subvar[6]);
				break;
		}
		tmp = command;
	}
	command = tmp;
	LOGDEBUG("%s %s","postrun",command);

	if ( hdup_system(command) != 0 ) {
		g_free(command);
		return 1;
	}
	VERBOSE("%s","Succesfully ran post script");
	return 0;
}
