/*			Networked				*/
/*								*/
/* Written by:	Glenn Machin 2931				*/
/* Originated:  Nov 12, 1990					*/
/* Description:							*/
/*								*/
/*	This program/routine exits/returns with a status 1 if	*/
/*	the terminal associated with the current process is	*/
/*	connected from a remote host, otherwise exits/returns	*/
/*	with a value of 0.					*/
/*								*/
/*	This program/routine makes some basic assumptions about	*/
/*      utmp:							*/
/*      	*The login process, rcmd, or window application */
/*		 makes an entry into utmp for all currents 	*/
/*		 users.						*/
/*		*For entries in which the users have logged in  */
/*		 locally. The line name is not a pseudo tty     */
/*		 device. 					*/
/*		*For X window application in which 	       */
/*		 the device is a pseudo tty device but the      */
/*               display is the local system, then the ut_host  */
/*	         has the format system_name:0.0 or :0.0.        */
/*		 All other entries will be assumed to be        */
/*		 networked.				        */
/*								*/
/*	Changes:   11/15/90  Check for file /etc/krb.secure.    */
/*			     If it exists then perform network  */
/*			     check, otherwise return 0.		*/
/****************************************************************/
/* 
 * Sandia National Laboratories also makes no representations about the 
 * suitability of the modifications, or additions to this software for 
 * any purpose.  It is provided "as is" without express or implied warranty.
 */
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifndef _TYPES_
#include <sys/types.h>
#ifndef _TYPES_
#define _TYPES_
#endif
#endif
#include <utmp.h>
#include <pwd.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#else
extern char *malloc(), *calloc(), *realloc();
#endif

#ifndef MAXHOSTNAME
#define MAXHOSTNAME 64
#endif

#ifdef NO_UT_PID

static int utfile;

#include <fcntl.h>

static void kadmin_setutent()
{
  utfile = open("/etc/utmp",O_RDONLY);
}

static struct utmp * kadmin_getutline(utmpent)
struct utmp *utmpent;
{
 static struct utmp tmputmpent;
 int found = 0;
 while ( read(utfile,&tmputmpent,sizeof(struct utmp)) > 0 ){
	if ( strcmp(tmputmpent.ut_line,utmpent->ut_line) == 0){
#ifdef NO_UT_HOST
		if ( ( 1) &&
#else
		if ( (strcmp(tmputmpent.ut_host,"") == 0) && 
#endif
	     	   (strcmp(tmputmpent.ut_name,"") == 0)) continue;
		found = 1;
		break;
	}
 }
 if (found) 
	return(&tmputmpent);
 return((struct utmp *) 0);
}

static void kadmin_endutent()
{
  close(utfile);
}
#else
#define kadmin_setutent  setutent
#define kadmin_getutline getutline
#define kadmin_endutent  endutent
#endif /* defined(HAVE_GETUTENT) && !defined(NO_UT_PID) */


int network_connected()
{
struct utmp utmpent;
struct utmp retutent, *tmpptr;
char *display_indx;
char currenthost[MAXHOSTNAME];
char *username,*tmpname;


/* Macro for pseudo_tty */
#define pseudo_tty(ut) \
        ((strncmp((ut).ut_line, "tty", 3) == 0 && ((ut).ut_line[3] == 'p' \
                                                || (ut).ut_line[3] == 'q' \
                                                || (ut).ut_line[3] == 'r' \
                                                || (ut).ut_line[3] == 's'))\
				|| (strncmp((ut).ut_line, "pty", 3) == 0))

    /* Check to see if getlogin returns proper name */
    if ( (tmpname = (char *) getlogin()) == (char *) 0) return(1);
    username = (char *) malloc(strlen(tmpname) + 1);
    if ( username == (char *) 0) return(1);
    strcpy(username,tmpname);
    
    /* Obtain tty device for controlling tty of current process.*/
    strncpy(utmpent.ut_line,ttyname(0) + strlen("/dev/"),
	    sizeof(utmpent.ut_line));

    /* See if this device is currently listed in /etc/utmp under
       calling user */
#ifndef NO_UT_TYPE
    utmpent.ut_type = USER_PROCESS;
#define ut_name ut_user
#endif
    kadmin_setutent();
    while ( (tmpptr = (struct utmp *) kadmin_getutline(&utmpent)) 
            != ( struct utmp *) 0) {

	/* If logged out name and host will be empty */
	if ((strcmp(tmpptr->ut_name,"") == 0) &&
#ifdef NO_UT_HOST
	    ( 1)) continue;
#else
	    (strcmp(tmpptr->ut_host,"") == 0)) continue;
#endif
	else break;
    }
    if (  tmpptr   == (struct utmp *) 0) {
	kadmin_endutent();
	return(1);
    }
    memcpy((char *)&retutent,(char *)tmpptr,sizeof(struct utmp));
    kadmin_endutent();
#ifdef DEBUG
#ifdef NO_UT_HOST
    printf("User %s on line %s :\n",
		retutent.ut_name,retutent.ut_line);
#else
    printf("User %s on line %s connected from host :%s:\n",
		retutent.ut_name,retutent.ut_line,retutent.ut_host);
#endif
#endif
    if  (strcmp(retutent.ut_name,username) != 0) {
	 return(1);
    }


    /* If this is not a pseudo tty then everything is OK */
    if (! pseudo_tty(retutent)) return(0);

    /* OK now the work begins there is an entry in utmp and
       the device is a pseudo tty. */

    /* Check if : is in hostname if so this is xwindow display */

    if (gethostname(currenthost,sizeof(currenthost))) return(1);
#ifdef NO_UT_HOST
    display_indx = (char *) 0;
#else
    display_indx = (char *) strchr(retutent.ut_host,':');
#endif
    if ( display_indx != (char *) 0) {
        /* 
           We have X window application here. The host field should have
	   the form => local_system_name:0.0 or :0.0  
           if the window is being displayed on the local system.
         */
#ifdef NO_UT_HOST
	return(1);
#else
        if (strncmp(currenthost,retutent.ut_host,
                (display_indx - retutent.ut_host)) != 0) return(1);
        else return(0);
#endif
    }
    
    /* Host field is empty or is not X window entry. At this point
       we can't trust that the pseudo tty is not connected to a 
       networked process so let's return 1.
     */
    return(1);
}

#ifdef NOTKERBEROS
main(argc,argv)
int argc;
char **argv;
{
  if (network_connected()){
#ifdef DEBUG
	 printf("Networked\n");
#endif
	exit(1);
  }
  else {
#ifdef DEBUG
	printf("Not networked\n");
#endif
	exit(0);
  }
}
#else
int networked()
{
  return(network_connected());
}
#endif
