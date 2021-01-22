/****************************************************************
 *								*
 * showacct.c reads a system accounting file created with	*
 * the accton command and prints out detailed information 	*
 * about processes.						*
 * If you're looking for a summary style report, use the 	*
 * "sa" command instead.  This is designed really to dump the	*
 * accounting information to a database so it can be queried	*
 * and to provide a detailed trail of commands run		*
 *								*
 * Written by Collin Douglas some time in 2001			*
 *								*
 * Modified 9/15/2016 to handle v2 or v3 acct files		*
 *								*
 ****************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <linux/acct.h>
#include <time.h>
#include <limits.h>

#define ACCTFILE "/var/account/pacct"
#define DELIMITER '|'


/* With the intent of keeping the print area fairly clean
 * I'm going to convert the acct record to an internal 
 * structure so I don't have to worry about what version it is
 * when I start to print
 */

struct myacct { 
  char ac_flag;
  char ac_version;
  long ac_uid;					// userID
  unsigned long ac_btime, ac_utime, ac_stime, ac_etime, ac_endtime;  // creation, user, system and elapsed times
  long ac_mem, ac_exitcode;
  char ac_comm[ACCT_COMM+1];
} ;

union acctunion {
  struct acct acctv2;
  struct acct_v3 acctv3;
} ;

static unsigned long 	compt2ulong(comp_t comptime);
static unsigned long 	comp2t2ulong(comp2_t foo);
double 		     	comp2_t_2_double(comp2_t val);
double 			comp_t_2_double(comp_t c_num);
int		     	printusage();
int			acct2myacct(union acctunion *ptr_acct_union, struct myacct *ptr_myacct);
int		opt_t=1, opt_d=0, opt_f=0, opt_H=1, opt_n=0, opt_o=0, opt_D=0, 
                opt_0=0, opt_u=0, opt_v=0, opt_e=-1, opt_T=0;

long clocks;

int main(int argc, char *argv[])
{ FILE 		*acctfile, *outfile;
  char 		c, *acctfilename, *outfilename, delimiter=DELIMITER, *comtmp, flags[5], 
  		hostname[HOST_NAME_MAX+1], *pw_name;
  struct passwd  *passrec;
  time_t temp;
  struct tm	*begintime_tm, *endtime_tm;
  union acctunion *ptr_acct_union;
  struct myacct *ptr_myacct;
  int x, ver, rc, hostlen;

/* Option flags and other stuff for getopt 				*/
/* opt_t is not a choice on command line but an invisible option	*/ 
/* representing the default tabular output style			*/

  extern char 	*optarg;
  extern int 	optind, opterr;
  int 		length; 
  long          ac_utime, ac_stime, ac_etime, ac_mem;

  acctfilename=NULL;
  outfilename=NULL;
  clocks=sysconf(_SC_CLK_TCK);

  opterr=0;

  while ((c=getopt(argc,argv,"f:d::neDhHo:Tu0v")) != EOF)
    switch(c)
    { case 'd': opt_d=1;	// delimiter
            	opt_t=0;
		if (optarg)
		  delimiter=optarg[0];
                break;
      case 'D': opt_D=1;	// debug
                break;
      case 'f': opt_f=1;	// input file
		length=strlen(optarg);
                acctfilename=calloc(length+1, sizeof(char));
                strcpy(acctfilename,optarg);
		break;
      case 'e': opt_e=1;	// supress exitcode=0
                break;
      case 'H': opt_H=0;	// supress header
                break;
      case '?': printf("\nInvalid argument\n");
                if(acctfilename != NULL) free(acctfilename);
                if(outfilename != NULL) free(outfilename);
      case 'h': printusage(); 	// usage summary
		exit(0);
		break;
      case 'n': opt_n=1;	// include hostname in output
      	 	rc=gethostname(hostname,HOST_NAME_MAX);
		if(rc != 0) // Something went wrong.  unset opt_n
		  opt_n=0;
		hostlen=strlen(hostname);
                break;
      case 'o': opt_o=1;	// output file
                length=strlen(optarg);
                outfilename=calloc(length+1, sizeof(char));
                strcpy(outfilename,optarg);
		break;
      case 'T': opt_T=1;	// show end time
                break;
      case 'u': opt_u=1;	// show user
                break;
      case '0': opt_0=-1;	// include processes that ran for 0 time
                break;
      case 'v': opt_v=1;	// show version of acct file and exit
                break;
    }

  // IF we're passed a file, use it.
  // If not, use the default

  if (opt_f)
  { if(acctfilename != NULL)
    { acctfile=fopen(acctfilename,"r");
    }
  }
  else
  { length=strlen(ACCTFILE);
    acctfilename=calloc(length+1, sizeof(char));
    strcpy(acctfilename,ACCTFILE);
  }


  acctfile=fopen(acctfilename,"r");

  if (acctfile == NULL)
  { printf("Error opening accounting file %s (%s)\n",acctfilename,strerror(errno));
    exit(errno);
  }

  if (opt_D)
    printf("\nDEBUG: Opened accounting file %s\n",acctfilename);

  // If we're passed an output file, use it
  // If not, use STDOUT
  if (opt_o)
    outfile=fopen(outfilename,"w");
  else
    outfile=fdopen(STDOUT_FILENO,"w");

  if (outfile == NULL)
  { printf("Error opening output (%s)\n",strerror(errno));
    exit(errno);
  }

  if (opt_D)
    printf("\nDEBUG: Opened output file %s\n",outfilename);

  ptr_acct_union=malloc(sizeof(struct acct));

  if (ptr_acct_union == NULL)
  { printf("Unable to allocate memory to ptr_acct_union.  Error %s\n",strerror(errno));
    exit(errno);
  }

  if (opt_D)
    printf("\nDEBUG: Trying to read initial record\n");

  if (opt_v)
  { length=fread(ptr_acct_union,sizeof(struct acct),1,acctfile);

    if ( length < 1 )
    { printf("Failed to read from accounting file, error %d (%s)\n",errno,strerror(errno));
      exit(errno);
    }
 
    if (opt_D)
      printf("DEBUG: Made it past the read command.  length=%d\n",length);

    printf("Accounting file %s is version %d\n",acctfilename,ptr_acct_union->acctv2.ac_version);
    exit(0);
  }

  if (opt_t)
  { if (opt_H)
    { if(opt_u)
        fprintf(outfile,"%-8s ","user");
      if(opt_n)
        fprintf(outfile,"%-12s ","hostname");

      fprintf(outfile,"%20s %10s %8s","command","date","start");
      if (opt_T)
      { fprintf(outfile,"%10s","date");
        fprintf(outfile,"%9s","end");
      }
      fprintf(outfile,"%7s %6s %11s %11s %8s %4s\n", "utime","stime", "elapsed","average_mem","exitcode","flag");
  
      if(opt_u)
        fprintf(outfile,"%-8s ","----");
      if(opt_n)
        fprintf(outfile,"%-12s ","--------");
      fprintf(outfile,"%20s %10s %8s", "-------","----","-----");
      if (opt_T)
      { fprintf(outfile,"%10s", "----");
        fprintf(outfile,"%9s","---");
      }
      fprintf(outfile,"%7s %6s %11s %11s %8s %4s\n",
	     "-----","------", "-------","-----------","--------","----");
    }
  }
  else if (opt_d)
  { if (opt_H)
    { if(opt_u)
        fprintf(outfile,"%s%c","user",delimiter);
      if(opt_n)
        fprintf(outfile,"%s%c","hostname",delimiter);
      fprintf(outfile,"%s%c%s%c%s%c","command",delimiter,"date",delimiter,"start",delimiter);
    }

    if (opt_T)
    { fprintf(outfile,"%s%c","date",delimiter);
      fprintf(outfile,"%s%c","end",delimiter);   // print calculated end time
    }

    fprintf(outfile,"%s%c%s%c%s%c%s%c%s%c%s\n",
           "utime",delimiter,"stime",delimiter,
           "elapsed",delimiter,"average_mem",delimiter,"exitcode",delimiter,"flag");
    
  }


  if (opt_D)
    printf("DEBUG: Starting real work\n");

  ptr_myacct=malloc(sizeof(struct myacct));
  begintime_tm=malloc(sizeof(struct tm));
  endtime_tm=malloc(sizeof(struct tm));

  while (length=fread(ptr_acct_union,sizeof(struct acct),1,acctfile) > 0)
  { 
    if (opt_D > 1)
      printf("DEBUG: Read a record.  length=%d\n",length);

    if (opt_D > 1)
      printf("DEBUG: About to try acct2myacct\n");

    acct2myacct(ptr_acct_union,ptr_myacct);

    if ( ((int) (ptr_myacct->ac_utime) > opt_0) && (ptr_myacct->ac_exitcode > opt_e))
    { 
      temp=ptr_myacct->ac_btime;      // "temp" is only used here
      if (opt_D)
      { printf("DEBUG: ac_btime: %ld\n",temp);
      }
      localtime_r(&temp,begintime_tm);     // and here

      temp=ptr_myacct->ac_endtime;
      if (opt_D)
      { printf("DEBUG: ac_endtime: %ld\n",temp);
      }
      localtime_r(&temp, endtime_tm);

      if (opt_u)
      { passrec=getpwuid(ptr_myacct->ac_uid);

        if (passrec != NULL)
        { pw_name=strdup(passrec->pw_name);
        }
        else
        { pw_name=calloc(7, sizeof(char));
          snprintf(pw_name,6*sizeof(char),"%ld",ptr_myacct->ac_uid);
        }
  
        if ((passrec == NULL) && (errno > 0))
        { printf("There was a problem with the getpwuid function (error %s)\n",strerror(errno));
          printf("show-acct may not be able to resolve user ID numbers to names\n");
        }
      }

      if (opt_t)
      { if (opt_u)
          fprintf(outfile,"%-8s ",pw_name);
        if (opt_n)
          fprintf(outfile,"%-12s ",hostname);
        fprintf(outfile,"%20s ",ptr_myacct->ac_comm);
	fprintf(outfile,"  %04d%02d%02d ",begintime_tm->tm_year+1900, (begintime_tm->tm_mon)+1, begintime_tm->tm_mday);
	fprintf(outfile,"%02d:%02d:%02d ",begintime_tm->tm_hour, begintime_tm->tm_min, begintime_tm->tm_sec);
	if (opt_T)
	{ 
	fprintf(outfile," %04d%02d%02d ",endtime_tm->tm_year+1900, (endtime_tm->tm_mon)+1, endtime_tm->tm_mday);
	fprintf(outfile,"%02d:%02d:%02d ",endtime_tm->tm_hour, endtime_tm->tm_min, endtime_tm->tm_sec);
	}
        fprintf(outfile,"%6.2f ",(float) (ptr_myacct->ac_utime)/clocks);
        fprintf(outfile,"%6.2f ",(float) (ptr_myacct->ac_stime)/clocks);
        fprintf(outfile,"%11.2f ",(float) (ptr_myacct->ac_etime)/clocks);
        //fprintf(outfile,"%6d ",(ptr_myacct->ac_utime));
        ////fprintf(outfile,"%6d ",(ptr_myacct->ac_stime));
        ////fprintf(outfile,"%11d ",(ptr_myacct->ac_etime));
        fprintf(outfile,"%11ld ",ptr_myacct->ac_mem);
        fprintf(outfile,"%8lu",(unsigned long) (ptr_myacct->ac_exitcode));
	fprintf(outfile," ");
	fprintf(outfile,"%c", (ptr_myacct->ac_flag & AXSIG) ? 'X' : '-');
	fprintf(outfile,"%c", (ptr_myacct->ac_flag & ACORE) ? 'C' : '-');
	fprintf(outfile,"%c", (ptr_myacct->ac_flag & ASU) ? 'S' : '-');
	fprintf(outfile,"%c", (ptr_myacct->ac_flag & AFORK) ? 'F' : '-');
        fprintf(outfile,"\n");
      }
      else if (opt_d)
      { if (opt_u)
          fprintf(outfile,"%s%c",pw_name,delimiter);
        if (opt_n)
          fprintf(outfile,"%s%c",hostname,delimiter);
        fprintf(outfile,"%s%c",ptr_myacct->ac_comm,delimiter);
	fprintf(outfile,"%04d%02d%02d%c",begintime_tm->tm_year+1900, (begintime_tm->tm_mon)+1, begintime_tm->tm_mday,delimiter);
	fprintf(outfile,"%02d:%02d:%02d%c",begintime_tm->tm_hour, begintime_tm->tm_min, begintime_tm->tm_sec,delimiter);
	if (opt_T)
	{ 
	fprintf(outfile,"%04d%02d%02d%c",endtime_tm->tm_year+1900, (endtime_tm->tm_mon)+1, endtime_tm->tm_mday,delimiter);
	fprintf(outfile,"%02d:%02d:%02d%c",endtime_tm->tm_hour, endtime_tm->tm_min, endtime_tm->tm_sec,delimiter);
	}
        fprintf(outfile,"%.2f%c",(float) (ptr_myacct->ac_utime)/clocks,delimiter);
        fprintf(outfile,"%.2f%c",(float) (ptr_myacct->ac_stime)/clocks,delimiter);
        fprintf(outfile,"%.2f%c",(float) (ptr_myacct->ac_etime)/clocks,delimiter);
        //fprintf(outfile,"%d%c",(ptr_myacct->ac_utime),delimiter);
        //fprintf(outfile,"%d%c",(ptr_myacct->ac_stime),delimiter);
        //fprintf(outfile,"%d%c",(ptr_myacct->ac_etime),delimiter);
        fprintf(outfile,"%ld%c",ptr_myacct->ac_mem,delimiter);
        fprintf(outfile,"%ldu%c",ptr_myacct->ac_exitcode,delimiter);
	fprintf(outfile,"%c", (ptr_myacct->ac_flag & AFORK) ? 'F' : '-');	// Executed fork but did not exec
	fprintf(outfile,"%c", (ptr_myacct->ac_flag & ASU) ? 'S' : '-');		// User super-user privileges
	fprintf(outfile,"%c", (ptr_myacct->ac_flag & AXSIG) ? 'X' : '-');	// Was killed by a signal
	fprintf(outfile,"%c", (ptr_myacct->ac_flag & ACORE) ? 'C' : '-');	// Dumped core
        fprintf(outfile,"\n");
      }
      if (opt_u)
        free(pw_name);
    }
  }

  printf("\n");

  fclose(acctfile);
  fclose(outfile);

  free(ptr_acct_union);
  free(ptr_myacct);
  free(acctfilename);
  free(outfilename);
  free(begintime_tm);
  free(endtime_tm);
}

/* The following function is from W. Richard Stevens' book */
/* _Advanced_Programming_in_the_UNIX_Environment       */

unsigned long compt2ulong(comp_t comptime)    /* convert comp_t to unsigned long */
{ unsigned long   val;
  int             exp;

  val = comptime & 0x1fff;    /* 13-bit fraction */
  exp = (comptime >> 13) & 7; /* 3-bit exponent (0-7) */
  while (exp-- > 0)
    val <<= 3;
  return(val);
}

int printusage()
{ printf("\nShow details of a process accounting data file\n");
  printf("\nUsage: show-acct -f <filename> -o <outfile> [-dhH]\n");
  printf("\tf: Filename (default %s)\n",ACCTFILE);
  printf("\tH: Supress header\n");
  printf("\th: This help screen\n");
  printf("\td: Delimited output (default=\"%c\")\n",DELIMITER);
  printf("\te: Supress processes with an exitcode of 0\n");
  printf("\tD: Turn on debug output\n");
  printf("\to: Specify output file\n");
  printf("\tT: Calculate and show ending time field\n");
  printf("\tu: Show user ID in output\n");
  printf("\t0: Include processes with a run time of zero (excluded by default)\n");
  printf("\tv: Show accounting file version and exit\n");
  printf("\n");
  printf("Flags: \n");
  printf("\tX: Killed by signal\n");
  printf("\tC: Dumped core\n");
  printf("\tS: Ran with super-user privileges\n");
  printf("\tF: Executed fork but did not exec\n");
  printf("\n");
  printf("\tsysconf clockticks: %ld\n",sysconf(_SC_CLK_TCK));
  printf("\n");
}

// convert acct structure to internal acct structure
// returns version number of acct record
int acct2myacct(union acctunion *ptr_acct_union, struct myacct *ptr_myacct)
{  
    unsigned long temp;

   
    if ( (ptr_acct_union->acctv2.ac_version < 2) || (ptr_acct_union->acctv2.ac_version > 3) )
    { printf("Accounting file is version %d\n",ptr_acct_union->acctv2.ac_version);
      printf("This program is too stupid to deal with anything but 2 or 3\n");
      exit(EINVAL);
    }

    if (ptr_acct_union->acctv2.ac_version == 2)
    {
      ptr_myacct->ac_utime=compt2ulong(ptr_acct_union->acctv2.ac_utime);
      ptr_myacct->ac_stime=compt2ulong(ptr_acct_union->acctv2.ac_stime);
      ptr_myacct->ac_etime=compt2ulong(ptr_acct_union->acctv2.ac_etime);
      ptr_myacct->ac_mem=compt2ulong(ptr_acct_union->acctv2.ac_mem);
      ptr_myacct->ac_flag=(ptr_acct_union->acctv2).ac_flag;
      ptr_myacct->ac_version=2;
      ptr_myacct->ac_btime=ptr_acct_union->acctv2.ac_btime;
      ptr_myacct->ac_mem=compt2ulong(ptr_acct_union->acctv2.ac_mem);
      ptr_myacct->ac_exitcode=ptr_acct_union->acctv2.ac_exitcode;
      ptr_myacct->ac_uid=ptr_acct_union->acctv2.ac_uid;
      strcpy(ptr_myacct->ac_comm,ptr_acct_union->acctv2.ac_comm);
    }
    if (ptr_acct_union->acctv2.ac_version == 3)
    {
      ptr_myacct->ac_utime=compt2ulong(ptr_acct_union->acctv3.ac_utime);
      ptr_myacct->ac_stime=compt2ulong(ptr_acct_union->acctv3.ac_stime);
      //ptr_myacct->ac_etime=compt2ulong(ptr_acct_union->acctv3.ac_etime);
      ptr_myacct->ac_etime=(unsigned long) (ptr_acct_union->acctv3.ac_etime);
      ptr_myacct->ac_mem=compt2ulong(ptr_acct_union->acctv3.ac_mem);
      ptr_myacct->ac_flag=ptr_acct_union->acctv3.ac_flag;
      ptr_myacct->ac_version=3;
      ptr_myacct->ac_btime=ptr_acct_union->acctv3.ac_btime;
      ptr_myacct->ac_mem=compt2ulong(ptr_acct_union->acctv3.ac_mem);
      ptr_myacct->ac_exitcode=(ptr_acct_union->acctv3).ac_exitcode;
      ptr_myacct->ac_uid=(ptr_acct_union->acctv3).ac_uid;
      strcpy(ptr_myacct->ac_comm,ptr_acct_union->acctv3.ac_comm);
    }
    ptr_myacct->ac_endtime=ptr_myacct->ac_btime+(ptr_myacct->ac_etime/clocks);


    if (opt_D)
    { //printf("\nDEBUG: ac_endtime: %lu, ac_btime: %lu, ac_etime: %lu\n",ptr_myacct->ac_endtime,ptr_myacct->ac_btime,ptr_myacct->ac_etime);
      //printf("\nDEBUG: btime: %s, endtime: %s\n",asctime(ptr_myacct->ac_btime),asctime(ptr_myacct->ac_endtime));
    }
    return(ptr_acct_union->acctv2.ac_version);
}
