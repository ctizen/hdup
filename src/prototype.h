/* $Id: prototype.h,v 1.36 2004/09/29 09:11:28 miekg Exp $ */
/* function prototypes */
#include <regex.h>

#include "walker.h"

void backup_local(phost_t host[], int which, struct argument_t *);
void backup_restore(phost_t host[], int which, struct argument_t *);
void backup_remote(phost_t hdhost[], int which);
void hdup_cleanup (int , phost_t ); 
void hdup_print(void);
void hdup_sigpipe(int);
void hdup_sigint(int);
void hdup_sigalrm(int);
void hdup_archive(phost_t , int);
int config(char *, phost_t host[]);
int setlist(char *, char *hostpath[], int);
int addlist(char *org[], char *cur[]);
int setvar(char *, char **, int);
int addvar(char *, char **);
int whatkey(char *);
int findhost (char *, phost_t host[]);
int yesno(char *, char *, int);
int addyesno(int, int *);
int inherit(char *, phost_t, phost_t host[], int);
int hdup_mkfake(phost_t);
int hdup_checkalg (char *, struct tab *);
int hdup_globfilecheck (char *, char **);
int hdup_chown (char *, char *, char *); 
int hdup_chmod (char *); 
int hdup_chown_dir (char *, int, char *, char *); 
int hdup_chmod_dir (char *, int); 
int hdup_unlink (char *); 
int hdup_hash (phost_t );
int hdup_getdate(char * , int, int );
int hdup_parsedate(char * , int , int *, int *, int *);
int hdup_inclist (phost_t , int );
int hdup_mkdir(char *, mode_t , char *, char *);
int hdup_cp (char *,char *); 
int hdup_setup ( phost_t host[], int );
int hdup_tarlist (phost_t );
int hdup_lock(phost_t , int );
int hdup_overview(char *, char *);
int hdup_compression(char *);
int hdup_compression_crypt(char *);
int hdup_chunk(char *);
int hdup_split(char *);
int hdup_grep(char *, char );
int hdup_system(char *);
int version(void);
int readline (char *, FILE *, int, int *);
int usage (void);
int find_restore (struct argument_t *, phost_t , char **, int);
int hdup_runpre (phost_t , char *scheme[], int);
int hdup_runpost (phost_t , char *scheme[], int);
char * hdup_time(time_t);
int hdup_putheader(pheader_t, FILE * );
int hdup_fillheader(pheader_t, phost_t, int, char *);
int hdup_getheader(pheader_t, FILE *);
int hdup_checkheader(pheader_t);
int hdup_readword(char *, FILE *, int );
int hdup_scheme(char *);
int hdup_dotar(phost_t, int);
int hdup_dountar(phost_t, struct argument_t *, char *);
int hdup_ok_char(char *);
int pure_whitespace(char *);
long long int hdup_free(char *);    
long long int hdup_convert(char *); 

int pipe2file(char *, char *, unsigned long long int *);
int pipe2pipe(char *, char *, pheader_t, unsigned long long int *);
int stream2file(FILE *, char *, unsigned long long int *);
int stream2pipe(FILE *, char *, pheader_t, unsigned long long int *);

char * hdup_subst(char *, char *, char *);
char * hdup_humansize(long long);
char * hdup_lastname(char * );         /* ala basename */
char * day(char *, int , int );
char * setup_tarcmd(phost_t);
char * setup_untarcmd(phost_t, char *, char *, int);
char * setup_unsplitcmd(char *);
char * setup_transportcmd(phost_t);
char * setup_cryptcmd(phost_t);
char * setup_decryptcmd(phost_t);
char * setup_splitcmd(phost_t);
char * hdup_makedate(int , int , int , int );
time_t hdup_file_time(char *);
/* 
 * walker.c 
 */
gboolean hash_slash(char *);
int do_dir_walk(phost_t);
int walk_dir(char *, FILE *, phost_t,
	reg_exp *dexclude, reg_exp *fexclude,
	reg_exp *dinclude, reg_exp *finclude);
gboolean reg_do_match(reg_exp *reg, char *);
