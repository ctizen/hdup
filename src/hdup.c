/* hdup.c, a backup program
 *
 * Copyright (c) 2000-2005, Miek Gieben. All right reserverd.
 *
 * See GPL-2 for the license
 *
 */

#include"hdup.h"
#include"prototype.h"

unsigned int quiet = 0;
int verbose = 2;
unsigned int debug = 0;
unsigned int hdup_log = 0;
unsigned int ignore_tar_err = 0;
unsigned int ignore_conf_err = 0;
unsigned int dryrun = 0;
unsigned int patched_tar = 0;
sig_atomic_t hdup_pipe = 0;
sig_atomic_t hdup_sig  = 0;
sig_atomic_t hdup_alrm = 0;
/* order of scheme's is important */
char *scheme[] = {"daily","weekly","monthly","remote","restore"};
char *subvar[] = {"%h","%a","%s","%e","%u","%c","%g"};
char *progname = NULL;
/* added % for subvar */
/* added & | for shells use */
const char ok_chars[] = ":1234567890@-_=+./\\ %\\&|\
	abcdefghijklmnopqrstuvwxyz\
	ABCDEFGHIJKLMNOPQRSTUVWXYZ";

void 
backup_local(phost_t host[], int which, struct argument_t *arg)
{
	char *what, *checkfile;
	size_t l; unsigned int newscheme; 
	int transfer = 0;

	/* setup the directory */
	if ( host[which]->remote == NULL ) 
		hdup_setup(host, which);
	else {
		/* if host[which]->remote is entered we must transfer the archive */
		transfer = 1;
		hdup_setup(host, which);
		/* hdup date dir is not needed when moving it to a
		 * remote location */
		rmdir (host[which]->dirname_date);
	}

	/* this also works with encryption */
	checkfile = g_strdup_printf("%s/%s.%s.%s.*",host[which]->dirname_date,
			host[which]->name,host[which]->date, scheme[arg->scheme]);

	if ( hdup_globfilecheck( checkfile, NULL) != 0 && transfer == 0 ) {
		/* there already is something??!? */
		if ( host[which]->overwrite == 0 ) {
			SYSLOG_HW("%s","There already is an archive identical to the one created right now");
			FATAL("%s: %s",host[which]->name,  "There already is an archive identical to the one created right now");
		}
		else 
			VERBOSE("%s","Overwriting old archive");
	}
	g_free(checkfile);


	/* set a lock, also when we're piping the tar to ssh  */
	/* 15-09-2003: do the locking here, before any copying, MG 
	 * at least before hdup_inclist() */
	(void)hdup_lock(host[which], LOCK);

	/* sort out the incremental files, hdup_inclist will return the scheme
	 * that is actually used. If there is no previous backup list, hdup
	 * try to create a "higher" backup: from daily to weekly to monthly.
	 * This means there will be always be made a backup! This can be toggled
	 * on and off with "always backup" in the config file
	 */
	newscheme = hdup_inclist(host[which], arg->scheme);
	if ( arg->scheme != newscheme ) 
		arg->scheme = newscheme;
	else
		hdup_overview("Scheme", scheme[arg->scheme]);

	/* setup the archivename */
	hdup_archive(host[which], arg->scheme);

	hdup_overview("Archive",(char*)hdup_lastname(host[which]->archivename));

	if ( host[which]->alg != NULL ) {
		/* 10 mar 2003: enable local encryption again */
		if ( transfer == 1 ) 
			VERBOSE("%s","Decryption key may not be present when restoring the archive");
		l = strlen(host[which]->alg) + strlen(host[which]->keypath);
		what = g_strdup_printf("yes (%s, %s)", host[which]->alg, host[which]->keypath);
		hdup_overview("Encryption", what);
		g_free(what);
	} else {
		hdup_overview("Encryption", "no");
	}


	/* generate a list of files and/or directories we should backup */
	if(do_dir_walk(host[which]) == 0) {
		/* nothing was found - if tar get's a empty files-from
		 * it will backup the working directory
		 */
		WARN("%s", "No files were found - I'm faking an empty tar archive");
		if (hdup_mkfake(host[which]) != 0) 
			hdup_cleanup(1, host[which]);
	} else {
		/* do your thing */
		if (hdup_dotar(host[which], arg->scheme) != 0)
			hdup_cleanup(1, host[which]);
	}
}

int 
main(int argc, char *argv[]) 
{
	phost_t host[MAXHOST + 1];
	struct argument_t arg; int c = 0; int which;
	char *cp;
	char *logstring;
	struct group *grp;
	struct sigaction sa;
	struct tab algs[] = ALGORITHMS;

#ifdef HAVE_GETOPT_H
	static struct option long_options[] =
	{ 
		{"config", required_argument, 0, 'c'},
		{"specific", required_argument, 0, 's'},
		{"help", no_argument, 0, 'h'},
		{"quiet", no_argument, 0, 'q'},
		{"verbose", no_argument, 0, 'V'},
		{"version", no_argument, 0, 'v'},
		{"ignore-tar", no_argument, 0, 'i'},
		{"ignore-conf", no_argument, 0, 'I'},
		{"patched-tar", no_argument, 0, 'P'},
		{"dryrun", no_argument, 0, 'd'},
		{"debug", no_argument, 0, 'D'},
		{0,0,0,0}
	};
#endif /*HAVE_GETOPT_H*/

	progname = g_strdup(argv[0]);

	/* reset options */
	arg.configpath  = NULL; arg.scheme      = SCHEME_ILL;
	arg.host        = NULL; arg.date        = NULL;
	arg.extractdir  = NULL; arg.extractfile = NULL;

	/* catch SIGPIPE */
	sa.sa_handler = hdup_sigpipe;
	sa.sa_flags = 0;
	sigfillset(&sa.sa_mask);
	sigaction(SIGPIPE, &sa, (struct sigaction *)NULL);

	/* catch SIGINT  */
	sa.sa_handler = hdup_sigint;
	sigaction(SIGINT, &sa, (struct sigaction *)NULL);

	/* setup SIGALRM. Used transfering with ssh */
	sa.sa_handler = hdup_sigalrm;
	sigaction(SIGALRM, &sa, (struct sigaction *)NULL);

	/* check if we are suid of sgid, if so QUIT! hdup isn't
         * designed to do this kind of stuff
	 */
        if (((getuid() != geteuid()) || (getgid() != getegid()))) {
		FATAL("%s %s", progname, "is not designed to be ran suid/sgid");
	}

	/* This is stupid to have - but somewhere I do something
	 * stupid - this at least prevents core dumps.... :-( */
	/* check argument lengths */
	for(c = 0; c < argc; c++) {
		if (strlen(argv[c]) > MAXPATHLEN) {
			FATAL("%s", "Command line argument too long");
		}
	}
	
	/* in host[0] I put the global options, these can be overriden in the config file */
#ifdef HAVE_GETOPT_H
	while ((c = getopt_long(argc, argv, "c:s:hqvViIdDP", long_options, 0)) != -1) {
#else
	while ((c = getopt(argc, argv, "c:s:hqvViIdDP")) != -1) {
#endif /*HAVE_GETOPT_H*/

		switch (c) { 
			case 'c':
				arg.configpath = strdup(optarg);
				hdup_ok_char(arg.configpath);
				break;
			case 'h':
				usage();
				hdup_cleanup(0,NULL);
				break;
			case 'v':
				version();
				hdup_cleanup(0,NULL);
				break;
			case 'q':
				quiet += 1;
				/* quiet option */
				if (quiet > 3)
					FATAL("%s","You can specify up to three -q's");
				break;
			case 'V':
				verbose--;
				if (verbose < 0)
					verbose = 0;
				break;
			case 's':
				arg.extractfile = strdup(optarg);
				/* if the first char eq '/' we chop it off
				 * as tar does the same */
				if (arg.extractfile[0] == '/') 
					arg.extractfile++;
				hdup_ok_char(arg.extractfile);
				break;
			case 'i':
				ignore_tar_err = 1;
				break;
			case 'I':
				ignore_conf_err = 1;
				break;
			case 'P':
				patched_tar = 1;
				break;
			case 'd':
				dryrun = 1;
				break;
			case 'D':
				debug = 1;
				break;
			case '?':
			default:
				usage();
				hdup_cleanup(-1,NULL);
		}
	}

	argc -= optind;
	argv += optind;

	/* need at least two arguments: scheme and a hostname */
	if (argc < 2 ) {
		usage();
		hdup_cleanup(-1,NULL);
	}

	/* MIEK; not sure I want to keep this for 2.0.11 */
	/* sanitize the command line */
	for (c = 0; c < argc; c++ ) 
		hdup_ok_char(argv[c]);

	/* check the scheme */
	arg.scheme = hdup_scheme(argv[0]);
	if (arg.scheme == REMOTE)
		progname = g_strconcat(progname, " (remote) ", NULL);

	if (arg.scheme != REMOTE && arg.scheme != RESTORE) {
		/* useless when receiving a bunch of files */
		if (patched_tar == 0) {
			LOG("%s", "Running without -P and probably a non patched tar");
			LOG("%s", "This will lead to incomplete backups");
		} else {
			LOG("%s", "Running with -P, all is well");
		}
	}

	/* first the config file to set things up,  -c option */
	if ( arg.configpath == NULL ) {
		VERBOSE("%s %s", "No -c switch using default configfile:", ETCFILE);
		arg.configpath = ETCFILE;
	}
	
	if (g_file_test(arg.configpath, G_FILE_TEST_IS_REGULAR)) {
		/* access test purely here for better error output */
		if (access(arg.configpath, R_OK))
			FATAL("%s %s","Cannot access the config file:", arg.configpath);

		if (config(arg.configpath, host) != 0) 
			FATAL("%s","The configuration file had errors in it");

	}  else 
		/* there is no config file */
		FATAL("%s %s","Config file could not be opened:", arg.configpath);

	/* config checks */
	if (findhost("global", host) == -1) 
		FATAL("%s", "No [global] section in configuration file");

	/* still ILL? */
	if (arg.scheme == SCHEME_ILL)
		FATAL("%s", "Scheme must be: daily, weekly, monthly, remote or restore");

	if (arg.scheme != RESTORE && arg.extractfile != NULL) 
		VERBOSE("%s", "Scheme is not \'restore\', -s ignored");

	/* process the commandline arguments to see what hdup should do */
	/* LOCAL */
	if ( arg.scheme <= REMOTE ) {
		/* second arg: host to backup */
		arg.host = argv[1];

		if ( argv[2] != NULL ) {
			/* must start with @ */
			if ( ( strchr(argv[2], '@')) == argv[2] ) {
				/* do not copy the @ */
				/* 29 jun 2002, store everything in ->remote */
				host[0]->remote = g_strdup(++argv[2]);
				/* check if there is a ':' in ->remote 
				 * 10 mar 2003: colon is not allowed anymore */
				if ( hdup_grep (host[0]->remote, ':') == 0 ) 
					FATAL("%s","Colon found in @REMOTE_HOST");
			} else 
				FATAL("%s %s","No @ found at start of REMOTEHOST:", argv[2]);
		}
	}

	/* REMOTE */
	if ( arg.scheme == REMOTE ) 
		/* REMOTE */
		arg.host = argv[1];

	/* RESTORE */ 
	if (arg.scheme == RESTORE) {
		/* restore a backup */
		if (argv[1] != NULL) 
			arg.host = argv[1];
		else
			FATAL("%s","You must supply a hostname");

		if (argv[2] != NULL) 
			arg.date = argv[2];
		else
			FATAL("%s","You must supply a date in the right format");

		if (argv[4] != NULL) {
			/* must start with @ */
			if ( ( strchr(argv[4], '@')) == argv[4] ) { 
				/* do not copy the @ */
				host[0]->remote = ++argv[4];
				if ( hdup_grep (host[0]->remote, ':') == 0 ) 
					FATAL("%s", "Colon found in @REMOTE_HOST");
			} else 
				FATAL("%s %s","No @ found at start of REMOTEHOST:", argv[4]);
		}
		/* do this after 4, we want to know if we going to restore remotely */
		if (argv[3] != NULL) {
			arg.extractdir  = argv[3];
			if ( hdup_globfilecheck(arg.extractdir,NULL) == 0 && host[0]->remote == NULL ) {
				FATAL("%s %s %s", "Extract dir: ", arg.extractdir, " does not exist");
			}
		} else 
			FATAL("%s","You must supply a dir to extract to");
	}
	if (arg.scheme == RESTORE && arg.extractfile != NULL &&
			host[0]->remote != NULL)
		FATAL("%s", "Remote restore of a single file in a archive it not possible");


	/* check host in config file */
	which = findhost(arg.host, host);
	if (which == -1) 
		FATAL("%s %s: %s","Host:", arg.host, "Not defined in configuration file");

	/* we are now ready to start the backup. Log this, makes the gen. email 
	 * msgs more readable */
	/* don't talk about anything when scheme == REMOTE, as the remote
	 * header will tell us what we're doing, MG 18 May 2004 */
	if ( arg.scheme <= MONTHLY) 
		LOG("%s: STARTING BACKUP",host[which]->name);
	if ( arg.scheme == RESTORE ) 
		LOG("%s: STARTING RESTORE",host[which]->name);

	/* do the date, default, iso or american */
	if ( host[0]->datespec == (char *) NULL )
		host[0]->datespec = "default";

	/* default everything to 'off' */
	if ( host[0]->skip == -1 ) host[0]->skip = 0;
	if ( host[0]->force == -1 ) host[0]->force = 0;
	if ( host[0]->overwrite == -1 ) host[0]->overwrite = 0;
	if ( host[0]->sparse == -1 ) host[0]->sparse = 0;
	if ( host[0]->always == -1 ) host[0]->always = 0;
	if ( host[0]->allow_remote == -1 ) host[0]->allow_remote = 0;
	if ( host[0]->history == -1 ) host[0]->history = 0;
	if ( host[0]->onefile == -1 ) host[0]->onefile = 0;
	if ( host[0]->log == -1 ) host[0]->log = 0;

	if ( host[0]->complevel == NULL ) host[0]->complevel = "6"; /* seems to be sane default */

	/* Inherit important vars from host[0] (=global) if there are defined */
	if ( host[which]->compression ==  NULL )
		host[which]->compression = host[0]->compression;
	if ( host[which]->archive == NULL )
		host[which]->archive = host[0]->archive;
	if ( host[which]->chunksize == NULL )
		host[which]->chunksize = host[0]->chunksize;
	if ( host[which]->free == NULL )
		host[which]->free = host[0]->free;
	if ( host[which]->complevel == NULL )
		host[which]->complevel = host[0]->complevel;

	/* check compression, and reset for use in the rest of the program  */
	/* looks a bit difficult, but this wil select gzip by default
	 * and if not, it will take the default from the hdup.conf
	 */
	/* also check if the exe is on the system MG 27 Sept 2004 */
	if ( host[which]->compression != NULL ) {
		if (g_str_equal(host[which]->compression, "gzip")) {
			/* ok, found */
			host[which]->compression    = TAR_GZIP;
			if (!g_file_test(GZIP_PROG, G_FILE_TEST_IS_EXECUTABLE))
				FATAL("%s: compression %s %s", host[which]->name, GZIP_PROG, "not found");
		} else if (g_str_equal(host[which]->compression, "none")) {
			host[which]->compression  = TAR_NONE;
		} else if (g_str_equal(host[which]->compression, "bzip")) {
			host[which]->compression  = TAR_BZIP;
			if (!g_file_test(BZIP_PROG, G_FILE_TEST_IS_EXECUTABLE))
				FATAL("%s: compression %s %s", host[which]->name, BZIP_PROG, "not found");
		} else if (g_str_equal(host[which]->compression, "lzop")) {
			host[which]->compression  = TAR_LZOP;
			if (!g_file_test(LZOP_PROG, G_FILE_TEST_IS_EXECUTABLE))
				FATAL("%s: %s %s", host[which]->name, LZOP_PROG, "not found");
		} else {
			/* unknown */
			FATAL("%s: compression %s %s",host[which]->name, "Unknown compression: ",host[which]->compression );
		}
	} else {
		VVERBOSE("%s","No compression given, defaulting to: gzip");
		host[which]->compression = TAR_GZIP; /* default */
		host[0]->compression = TAR_GZIP; /* default */
		if (!g_file_test(GZIP_PROG, G_FILE_TEST_IS_EXECUTABLE))
			FATAL("%s: compression %s %s", host[which]->name, GZIP_PROG, "not found");
	}
	
	if ( host[which]->prerun == NULL )
		host[which]->prerun = host[0]->prerun;
	if ( host[which]->postrun == NULL )
		host[which]->postrun = host[0]->postrun;

	if ( host[which]->user == NULL ) {
		if ( host[0]->user == NULL ) {
			VVERBOSE("%s %s","No user given, defaulting to:", DEF_USER);
			host[which]->user = DEF_USER;
		} else
			host[which]->user = host[0]->user;
	}
	if ( host[which]->group == NULL )
		host[which]->group = host[0]->group;
	if ( host[which]->datespec == NULL ) 
		host[which]->datespec = host[0]->datespec;
	if ( host[which]->keypath == NULL )
		host[which]->keypath = host[0]->keypath;
	if ( host[which]->alg == NULL )
		host[which]->alg = host[0]->alg;

	/* although remote is not in the config file we should
	 * still copy it to the right host */
	if ( host[which]->remote ==  NULL )
		host[which]->remote = host[0]->remote;

	/* ssh or whatever (rsync?) */
	if (host[which]->proto == NULL) {
		if (host[0]->proto == NULL) {
			VVERBOSE("%s %s","No protocol path given, defaulting to:",DEFAULT_PROTO);
			host[which]->proto = DEFAULT_PROTO;
		} else 
			host[which]->proto = host[0]->proto;
	}

	/* copy the nobackup argument. After this is can be still be NULL */
	if (host[which]->nobackup == NULL) {
		host[which]->nobackup = host[0]->nobackup;
	}

	/* copy the dir paths if they are not defined */
	/* if we do not redefine include take the one from global */
	if(host[which]->path[0] == NULL) {
		for (c = 0; c < MAXDIR; c++ ) {
			if (host[0]->path[c] == NULL)
				break;
			host[which]->path[c] = host[0]->path[c];
		}
	}

	/* if we do not redefine include take the one from global */
	if(host[which]->include[0] == NULL) {
		for (c = 0; c < MAXDIR; c++ ) {
			if (host[0]->include[c] == NULL)
				break;
			host[which]->include[c] = host[0]->include[c];
		}
	}
	/* tar */
	if ( host[which]->tar == NULL ) {
		if ( host[0]->tar ==  NULL ) {
			VVERBOSE("%s %s", "No tar path given, defaulting to:",TAR);
			host[which]->tar = TAR;
		} else 
			host[which]->tar = host[0]->tar;
	}

	/* tar options */
	if ( host[which]->tar_conf_opt == NULL ) {
		if ( host[0]->tar_conf_opt ==  NULL ) {
			VVERBOSE("%s %s", "No tar option given, defaulting to:",TAR_CONF_OPT);
			host[which]->tar_conf_opt = TAR_CONF_OPT;
		} else
			host[which]->tar_conf_opt = host[0]->tar_conf_opt;
	}

	if ( host[which]->mcrypt == NULL ) {
		if ( host[0]->mcrypt ==  NULL ) {
			VVERBOSE("%s %s", "No mcrypt path given, defaulting to:",MCRYPT);
			host[which]->mcrypt = MCRYPT;
		} else 
			host[which]->mcrypt = host[0]->mcrypt;
	}

	if ( host[which]->gpg == NULL ) {
		if ( host[0]->gpg ==  NULL ) {
			VVERBOSE("%s %s", "No gpg path given, defaulting to:",GPG);
			host[which]->gpg = GPG;
		} else 
			host[which]->gpg = host[0]->gpg;
	}

	/* extra check proto, tar, find */
	/* 11-11-2003 Don't check for this when we are receiving a backup */
	if ( arg.scheme != REMOTE ) {
		if (!g_file_test(host[which]->tar, G_FILE_TEST_IS_EXECUTABLE)) {
			SYSLOG_HW("%s", "Tar command could not be found - check your \'tar\' option");
			FATAL("%s: %s", host[which]->name, "Tar command could not be found - check your \'tar\' option");
		}
	}
	/* only check for ->proto when doing a remote backup
	 * [bug 4] in bugzilla. 
	 */
	if ( arg.scheme == REMOTE ) {
		if (!g_file_test(host[which]->proto, G_FILE_TEST_IS_EXECUTABLE)) {
			SYSLOG_HW("%s", "Proto command could not be found - check your \'proto\' option");
			FATAL("%s: %s", host[which]->name, "Proto command could not be found - check your \'proto\' option");
		}
	}

	if ( host[0]->proto_opt == NULL )
		host[0]->proto_opt = PROTO_OPT;
	if ( host[which]->proto_opt == NULL )
		host[which]->proto_opt = host[0]->proto_opt;
	/* the remote hdup, default locations */
	if ( host[0]->remote_hdup == NULL ) 
		host[0]->remote_hdup = REMOTE_HDUP;
	if ( host[0]->remote_hdup_opt == NULL )
		host[0]->remote_hdup_opt = REMOTE_HDUP_OPT;

	if ( host[which]->remote_hdup == NULL )
		host[which]->remote_hdup = host[0]->remote_hdup;
	if ( host[which]->remote_hdup_opt == NULL )
		host[which]->remote_hdup_opt = host[0]->remote_hdup_opt;

	/* always copy these */
	if ( host[which]->skip == -1 ) host[which]->skip = host[0]->skip;
	if ( host[which]->force == -1 ) host[which]->force = host[0]->force;
	if ( host[which]->overwrite == -1 ) host[which]->overwrite = host[0]->overwrite;
	if ( host[which]->sparse == -1 ) host[which]->sparse = host[0]->sparse;
	if ( host[which]->always == -1 ) host[which]->always = host[0]->always;
	if ( host[which]->allow_remote == -1 ) host[which]->allow_remote = host[0]->allow_remote;
	if ( host[which]->history == -1 ) host[which]->history = host[0]->history;
	if ( host[which]->onefile == -1 ) host[which]->onefile = host[0]->onefile;
	if ( host[which]->log == -1 ) host[which]->log = host[0]->log;

	if ( host[which]->history == 1 )
		WARN("%s","\'no history\' is set, using \'static\' directory. This is not smart, but your wish is my command");

	if ( host[which]->log == 1 ) {
		openlog(basename(progname) , LOG_CONS || LOG_PID, LOG_DAEMON );
		hdup_log = 1;
	}

	/* if we do not redefine exclude take the one from global */
	if(host[which]->exclude[0] == NULL) {
		for (c = 0; c < MAXDIR; c++ ) {
			if (host[0]->exclude[c] == NULL)
				break;
			host[which]->exclude[c] = host[0]->exclude[c];
		}
	}
	/* always put the archive dir in the exclude list 
	 * this is what skip used to do */
	host[which]->exclude[c] = host[which]->archive;

	/* check algorithm and key path, both must be set, or none */
	if ( ( host[which]->alg == NULL && host[which]->keypath != NULL ) || 
			( host[which]->alg != NULL && host[which]->keypath == NULL ) ) {
		SYSLOG_HW("%s","Both algorithm and key must be set");
		FATAL("%s: %s",host[which]->name, "Both algorithm and key must be set");
	}
	
	/* check group name */
	if (host[which]->group != NULL) {
		grp = getgrnam(host[which]->group);
		if (!grp)
			FATAL("%s: %s %s",host[which]->name, 
					host[which]->group, "is not a valid group");
	}
	
	if (host[which]->alg != NULL) {
		/* is alg == gpg, we must check gpg instead */
		if ( strcasecmp(host[which]->alg, GPG_ALG) == 0 ) {
			/* yes, gpg */
			if (!g_file_test(host[which]->gpg, G_FILE_TEST_IS_EXECUTABLE)) {
				SYSLOG_HW("%s",
						"GPG command could not be found - check your \'gpg\' option");
				FATAL("%s: %s", host[which]->name, 
						"GPG command could not be found - check your \'gpg\' option");
			}
			goto out; /* my God, a goto, this program sucks! */
		}
		/* check mcrypt location */
		if (!g_file_test(host[which]->mcrypt, G_FILE_TEST_IS_EXECUTABLE)) {
			SYSLOG_HW("%s",
					"Mcrypt command could not be found - check your \'mcrypt\' option");
			FATAL("%s: %s", host[which]->name, 
					"Mcrypt command could not be found - check your \'mcrypt\' option");
		}

		/* check for valid algorithms */
		if (hdup_checkalg(host[which]->alg, algs) == 0) {
			SYSLOG_HW("Invalid algorithm: %s", host[which]->alg);
			FATAL("%s: Invalid algorithm: %s", host[which]->name, host[which]->alg);
		}
			
out:
		/* is key availible, disable check for GPG */
		if ( strcasecmp(host[which]->alg, GPG_ALG) != 0 ) {
			if (!g_file_test(host[which]->keypath, G_FILE_TEST_IS_REGULAR)) {
				SYSLOG_HW("%s %s %s", "Encryption key", host[which]->keypath," not found");
				FATAL("%s: %s %s %s", host[which]->name, 
						"Encryption key", host[which]->keypath," not found");
			}
		}
	}

	/* if history = no, hdup does not keep a history. Everything is stored
	 * in a static dir, I'm tweaking getdate for this, which is a bit of a hack, 
	 * but it will then work everywere */
	if ( arg.scheme == RESTORE ) {
		/* arg.date must be filled or we exited above */
		if (g_str_equal(arg.date, STATIC)) {
			/* equals 'static', we must set the history to one */
			host[which]->history = 1;
			WARN("%s","\'no history\' is set, using \'static\' directory, see hdup.conf(5)");
		}
	}

	/* check the compression level, it should be a single integer [0..9] */
	if ( atoi(host[which]->complevel) < 1 || atoi(host[which]->complevel) > 9 ) {
		/* somebody is trying to do something stupid */
		WARN("%s %s",host[which]->complevel," should be an integer between 1 and 9 (will use 6)");
		host[which]->complevel = "-6";
	}

	host[which]->date = (char*) g_malloc(11);
	if (g_str_equal(host[which]->datespec, "default")) {
		/* look for today */
		if ( arg.date != NULL )
			if (g_str_equal(arg.date, "today") == 0) {
				/* get new space for arg.date */
				arg.date = (char*)g_malloc(11);
				hdup_getdate(arg.date, DATE_DEF, 0 );
			}

		hdup_getdate(host[which]->date, DATE_DEF, host[which]->history);
	} else if (g_str_equal(host[which]->datespec, "iso")) {
		/* look for today */
		if (arg.date != NULL)
			if (g_str_equal(arg.date, "today")) {
				arg.date = (char*)g_malloc(11);
				hdup_getdate(arg.date, DATE_ISO, 0 );
			}

		hdup_getdate(host[which]->date, DATE_ISO, host[which]->history);
	} else if (g_str_equal(host[which]->datespec, "american")) {
		/* look for today */
		if ( arg.date != NULL )
			if (g_str_equal(arg.date, "today")) {
				arg.date = (char*) g_malloc(11);
				hdup_getdate(arg.date, DATE_US, 0 );
			}

		hdup_getdate(host[which]->date, DATE_US, host[which]->history);
	} else {
		FATAL("%s: %s", host[which]->name, "Date spec should be \'default\',\'iso\' or \'american\'");
	}

	if ( host[which]->archive == NULL ) {
		SYSLOG_HW("%s", "No directories specified to store the archives, use \'archive dir\'");
		FATAL("%s: %s", host[which]->name,  "No directories specified to store the archives, use \'archive dir\'");
	}

	if ( host[which]->path[0] == NULL && arg.scheme < REMOTE ) {
		SYSLOG_HW("%s", "No directories to backup specified, use \'dir\'");
		FATAL("%s: %s",host[which]->name, "No directories to backup specified, use \'dir\'");
	}

	/* if a chunksize is specified the optional size letter (m or k) must be 
	 * lowercase */
	/* FIXME: ok, i'm totally not sure about this, but it seems to work */
	if ( host[which]->chunksize != NULL ) {
		LOGDEBUG("chunk %s", host[which]->chunksize);
		for (cp = host[which]->chunksize; *cp != 0; cp++) {
			*cp = tolower( (int) *cp );
		}
		LOGDEBUG("chunk %s", host[which]->chunksize);
	}

	/* run the prerun script - do this way early. People like to mount their
	 * backup filesystem before doing anything */
	if ( host[which]->prerun != NULL ) {
		if ( hdup_runpre(host[which], scheme, arg.scheme) != 0 ) {
			SYSLOG_HW("%s","Prescript executed with errors");
			FATAL("%s: %s",host[which]->name, "Prescript executed with errors");
		}
	}

	/* setup the overview status message */
	hdup_overview(NULL, NULL);
	hdup_overview("Hdup version", VERSION);
	hdup_overview(NULL,NULL);

	/* time keeping */
	(void)time( &(host[which]->elapsed));

	if ( arg.scheme == DAILY || arg.scheme == WEEKLY || arg.scheme == MONTHLY) {
		hdup_overview("Host", host[which]->name);
		hdup_overview("Date", host[which]->date);

		/* okay all the options are parsed we can do some REAL work */

		backup_local(host, which, &arg);

	} else if (arg.scheme == REMOTE) {
		/* remote always reads from stdin  */
		/* fix hostname argument for REMOTE */
		host[which]->name = arg.host; /* FIXME: still necessary? */
		hdup_overview("Host", host[which]->name);
		hdup_overview("Date", host[which]->date);
		hdup_overview("Scheme", scheme[arg.scheme]);

		/* only complain when not receiving this via stdin */
		if ( host[which]->allow_remote == 0) {
			/* host not allowed to put stuff here */
			LOG("%s","Remote is \'off\'. This host is not allowed to upload archives");
			hdup_cleanup(-1, host[which]);
		}

		/* prerun scripts are run! 11-09-2003 */
		backup_remote(host, which);

	} else if (arg.scheme == RESTORE) {
		char *arch;
		arch = g_strdup_printf("%s/%s",host[which]->archive,arg.host);
		if ( hdup_globfilecheck(arch, NULL) == 0 ) 
			FATAL("%s","Cannot find any archives for this host");
		g_free(arch);

		hdup_overview("Host", host[which]->name);
		hdup_overview("Date", host[which]->date);

		if ( arg.extractfile == NULL )
			hdup_overview("Scheme", scheme[arg.scheme]);
		else
			hdup_overview("Scheme", "restore (specific)");

		/* no prerun scripts are run */
		backup_restore(host, which, &arg);
	}

	/* init the log string at this point */
	logstring = NULL;
	if (arg.scheme != RESTORE) {
		hdup_overview("Bytes written", (char*)hdup_humansize(host[which]->bytes));
	}
	if (host[which]->chunksize != NULL) {
		logstring = g_strdup_printf("%s (chunked)", (char*)hdup_humansize(host[which]->bytes));
	} else {
		logstring = g_strdup_printf("%s", (char*)hdup_humansize(host[which]->bytes));
	}


	/* first unlock THEN perform the postrun script, always lock, remote,
	 * restore or normal operations */
	if (arg.scheme != RESTORE) {
		(void)hdup_lock(host[which], UNLOCK);
	}

	if (hdup_sig) 
		hdup_cleanup(-1, NULL);

	/* post run script */
	/* some people use the LOCK file on a mounted medium which gets
	 * unmounted by a postrun script, so we must unlock before the
	 * postrun script runs. */
	if (host[which]->postrun != NULL) {
		/* we must do some postrun stuff */
		if ( hdup_runpost(host[which], scheme, arg.scheme) != 0 ) {
			/* something went wrong, non fatal error */
			WARN("%s","Postscript executed with errors");
			/*SYSLOG_HW("%s","Postscript executed with errors");*/
		}
	}

	/* time keeping */
	host[which]->elapsed = time(NULL) -  host[which]->elapsed;
	hdup_overview("Elapsed", hdup_time(host[which]->elapsed));
	SYSLOG("SUCCESS, %s (%s): %s, %s", host[which]->name, scheme[arg.scheme],
			logstring,  hdup_time(host[which]->elapsed));
	if (arg.scheme < REMOTE ) 
		hdup_overview("Status", "successfully performed backup");

	if (arg.scheme != REMOTE)
		hdup_cleanup(0,NULL);
	else 
		/* no overview, normal exit */
		hdup_cleanup(-2,NULL);

	return 0;
}
