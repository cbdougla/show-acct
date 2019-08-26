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
  long ac_uid;
  long ac_btime, ac_utime, ac_stime, ac_etime;
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
int		opt_t=1, opt_d=0, opt_f=0, opt_H=0, opt_o=0, opt_D=0, opt_0=0, opt_u=0, opt_v=0, opt_e=-1;



int main(int argc, char *argv[])
{ FILE 		*acctfile, *outfile;
  char 		c, *acctfilename, *outfilename, delimiter=DELIMITER, *comtmp, flags[5], *pw_name;
  struct passwd  *passrec;
  unsigned long temp;
  struct tm	*timestuff;
  union acctunion *ptr_acct_union;
  struct myacct *ptr_myacct;
  int x, ver;

/* Option flags and other stuff for getopt 				*/
/* opt_t is not a choice on command line but an invisible option	*/ 
/* representing the default tabular output style			*/

  extern char 	*optarg;
  extern int 	optind, opterr;
  int 		length; 
  long          ac_utime, ac_stime, ac_etime, ac_mem;

  acctfilename=NULL;
  outfilename=NULL;

  opterr=0;

  while ((c=getopt(argc,argv,"f:d::eDhHo:u0v")) != EOF)
    switch(c)
    { case 'd': opt_d=1;	// delimeter
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
      case 'H': opt_H=1;	// supress header
                break;
      case '?': printf("\nInvalid argument\n");
                if(acctfilename != NULL) free(acctfilename);
                if(outfilename != NULL) free(outfilename);
      case 'h': printusage(); 	// usage summary
		exit(0);
		break;
      case 'o': opt_o=1;	// output file
                length=strlen(optarg);
                outfilename=calloc(length+1, sizeof(char));
                strcpy(outfilename,optarg);
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
    printf("Opened accounting file %s\n",acctfilename);

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
    printf("Opened output file %s\n",outfilename);

  ptr_acct_union=malloc(sizeof(struct acct));

  if (ptr_acct_union == NULL)
  { printf("Unable to allocate memory to ptr_acct_union.  Error %s\n",strerror(errno));
    exit(errno);
  }

  if (opt_D)
    printf("Trying to read initial record\n");

  if (opt_v)
  { length=fread(ptr_acct_union,sizeof(struct acct),1,acctfile);

    if ( length < 1 )
    { printf("Failed to read from accounting file, error %d (%s)\n",errno,strerror(errno));
      exit(errno);
    }
 
    if (opt_D)
      printf("Made it past the read command.  length=%d\n",length);

    printf("Accounting file %s is version %d\n",acctfilename,ptr_acct_union->acctv2.ac_version);
    exit(0);
  }

  if (opt_t)
  { if (!opt_H)
    { if(opt_u)
        fprintf(outfile,"%-8s ","user");

      fprintf(outfile,"%20s %10s %8s %6s %6s %11s %11s %8s %4s\n",
             "command","date","start","utime","stime",
	     "elapsed","average_mem","exitcode","flag");
  
      if(opt_u)
        fprintf(outfile,"%-8s ","----");
      fprintf(outfile,"%20s %10s %8s %6s %6s %11s %11s %8s %4s\n",
             "-------","----","-----","----","------",
	     "-------","-----------","--------","----");
    }
  }
  else if (opt_d)
  { if (!opt_H)
    { if(opt_u)
        fprintf(outfile,"%s%c","user",delimiter);
      fprintf(outfile,"%s%c%s%c%s%c%s%c%s%c%s%c%s%c%s\n",
             "command",delimiter,"date",delimiter,"start",delimiter,"utime",delimiter,"stime",delimiter,
	     "elapsed",delimiter,"average_mem",delimiter,"exitcode",delimiter,"flag");
    }
  }


  if (opt_D)
    printf("Starting real work\n");

  ptr_myacct=malloc(sizeof(struct myacct));

  while (length=fread(ptr_acct_union,sizeof(struct acct),1,acctfile) > 0)
  { 
    if (opt_D)
      printf("Read a record.  length=%d\n",length);

    if (opt_D)
      printf("About to try acct2myacct\n");

    acct2myacct(ptr_acct_union,ptr_myacct);

    if ( (ptr_myacct->ac_utime > opt_0) && (ptr_myacct->ac_exitcode > opt_e))
    { temp=ptr_myacct->ac_btime;
      timestuff=localtime(&temp);

      passrec=getpwuid(ptr_myacct->ac_uid);

      if (passrec != NULL)
      { pw_name=strdup(passrec->pw_name);
      }
      else
      { pw_name=calloc(7, sizeof(char));
        snprintf(pw_name,6*sizeof(char),"%d",ptr_myacct->ac_uid);
      }

      if ((passrec == NULL) && (errno > 0))
      { printf("There was a problem with the getpwuid function (error %s)\n",strerror(errno));
        printf("This will not affect anything but it is odd\n");
      }

      if (opt_t)
      { if (opt_u)
          fprintf(outfile,"%-8s ",pw_name);
        fprintf(outfile,"%20s ",ptr_myacct->ac_comm);
	fprintf(outfile,"  %04d%02d%02d ",timestuff->tm_year+1900, (timestuff->tm_mon)+1, timestuff->tm_mday);
	fprintf(outfile,"%02d:%02d:%02d ",timestuff->tm_hour, timestuff->tm_min, timestuff->tm_sec);
        fprintf(outfile,"%6u ",ptr_myacct->ac_utime);
        fprintf(outfile,"%6u ",ptr_myacct->ac_stime);
        fprintf(outfile,"%11u ",ptr_myacct->ac_etime);
        fprintf(outfile,"%11u ",ptr_myacct->ac_mem);
        fprintf(outfile,"%8lu",(unsigned long) (ptr_myacct->ac_exitcode));
	fprintf(outfile," ");
	fprintf(outfile,"%c", (ptr_myacct->ac_flag & AXSIG) ? 'X' : '-');
	fprintf(outfile,"%c", (ptr_myacct->ac_flag & ACORE) ? 'C' : '-');
	fprintf(outfile,"%c", (ptr_myacct->ac_flag & ASU) ? 'S' : '-');
	fprintf(outfile,"%c", (ptr_myacct->ac_flag & AFORK) ? 'F' : '-');
        fprintf(outfile,"\n");
      }
      else if (opt_d)
      { fprintf(outfile,"%s%c",pw_name,delimiter);
        fprintf(outfile,"%s%c",ptr_myacct->ac_comm,delimiter);
	fprintf(outfile,"%04d%02d%02d%c",timestuff->tm_year+1900, (timestuff->tm_mon)+1, timestuff->tm_mday,delimiter);
	fprintf(outfile,"%02d:%02d:%02d%c",timestuff->tm_hour, timestuff->tm_min, timestuff->tm_sec,delimiter);
        fprintf(outfile,"%u%c",ptr_myacct->ac_utime,delimiter);
        fprintf(outfile,"%u%c",ptr_myacct->ac_stime,delimiter);
        fprintf(outfile,"%u%c",ptr_myacct->ac_etime,delimiter);
        fprintf(outfile,"%u%c",ptr_myacct->ac_mem,delimiter);
        fprintf(outfile,"%u%c",ptr_myacct->ac_exitcode,delimiter);
	fprintf(outfile,"%c", (ptr_myacct->ac_flag & AFORK) ? 'F' : '-');	// Executed fork but did not exec
	fprintf(outfile,"%c", (ptr_myacct->ac_flag & ASU) ? 'S' : '-');		// User super-user privileges
	fprintf(outfile,"%c", (ptr_myacct->ac_flag & AXSIG) ? 'X' : '-');	// Was killed by a signal
	fprintf(outfile,"%c", (ptr_myacct->ac_flag & ACORE) ? 'C' : '-');	// Dumped core
	fprintf(outfile,"%c",delimiter);
        fprintf(outfile,"\n");
      }
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
}

// convert acct structure to internal acct structure
// returns version number of acct record
int acct2myacct(union acctunion *ptr_acct_union, struct myacct *ptr_myacct)
{  
    if (opt_D)
    { printf("ptr_acct_union->acctv2.ac_utime %d\n",ptr_acct_union->acctv2.ac_utime);
    }
   
    if ( (ptr_acct_union->acctv2.ac_version < 2) || (ptr_acct_union->acctv2.ac_version > 3) )
    { printf("Accounting file is version %d\n",ptr_acct_union->acctv2.ac_version);
      printf("This program is too stupid to deal with anything but 2 or 3\n");
      exit(EINVAL);
    }

    if (ptr_acct_union->acctv2.ac_version == 2)
    {
      ptr_myacct->ac_utime=compt2ulong(ptr_acct_union->acctv2.ac_utime);
      ptr_myacct->ac_stime=compt2ulong((ptr_acct_union->acctv2).ac_stime);
      ptr_myacct->ac_etime=compt2ulong((ptr_acct_union->acctv2).ac_etime);
      ptr_myacct->ac_mem=compt2ulong((ptr_acct_union->acctv2).ac_mem);
      ptr_myacct->ac_flag=(ptr_acct_union->acctv2).ac_flag;
      ptr_myacct->ac_version=2;
      ptr_myacct->ac_btime=(ptr_acct_union->acctv2).ac_btime;
      ptr_myacct->ac_mem=compt2ulong((ptr_acct_union->acctv2).ac_mem);
      ptr_myacct->ac_exitcode=(ptr_acct_union->acctv2).ac_exitcode;
      ptr_myacct->ac_uid=(ptr_acct_union->acctv2).ac_uid;
      strcpy(ptr_myacct->ac_comm,(ptr_acct_union->acctv2).ac_comm);
    }
    if (ptr_acct_union->acctv2.ac_version == 3)
    {
      ptr_myacct->ac_utime=compt2ulong(ptr_acct_union->acctv3.ac_utime);
      ptr_myacct->ac_stime=compt2ulong(ptr_acct_union->acctv3.ac_stime);
      ptr_myacct->ac_etime=compt2ulong(ptr_acct_union->acctv3.ac_etime);
      ptr_myacct->ac_mem=compt2ulong(ptr_acct_union->acctv3.ac_mem);
      ptr_myacct->ac_flag=ptr_acct_union->acctv3.ac_flag;
      ptr_myacct->ac_version=3;
      ptr_myacct->ac_btime=ptr_acct_union->acctv3.ac_btime;
      ptr_myacct->ac_mem=compt2ulong(ptr_acct_union->acctv3.ac_mem);
      ptr_myacct->ac_exitcode=(ptr_acct_union->acctv3).ac_exitcode;
      ptr_myacct->ac_uid=(ptr_acct_union->acctv3).ac_uid;
      strcpy(ptr_myacct->ac_comm,ptr_acct_union->acctv3.ac_comm);
    }
    return(ptr_acct_union->acctv2.ac_version);
}
