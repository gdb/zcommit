/*
   zsend.c  simple zephyr sender
   Copyright (C) 1994 Darrell Kindred

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <strings.h>
#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include <zephyr/zephyr.h>
#include <zephyr/zephyr_err.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/file.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#define DEFAULT_CLASS "MESSAGE"
#define DEFAULT_INSTANCE "PERSONAL"
#define URGENT_INSTANCE "URGENT"
#define DEFAULT_OPCODE ""
#define FILSRV_CLASS "FILSRV"
#ifdef CMU_INTERREALM
#define DEFAULT_REALM "ANDREW.CMU.EDU"
#endif

extern Code_t ZCancelSubscriptions(), ZUnsetLocation(), ZClosePort(),
  //ZRetrieveSubscriptions(), 
              ZGetSubscriptions(), ZSubscribeTo(),
              ZSendNotice(), ZInitialize(), ZOpenPort(), ZPending(),
              ZCompareUID(), ZReceiveNotice(), ZCheckAuthentication(),
              ZFreeNotice(), ZSetLocation();
#ifdef CMU_INTERREALM
extern char *ZExpandRealm();
#endif

typedef struct PendingReply PendingReply;
struct PendingReply {
   char *instance;
   char *recipient;
   ZUnique_Id_t	uid;
   PendingReply *next;
};

struct Globals {
   const char	*program;
   int          argc;
   const char   **argv;

   u_short	port;
   int		zfd;

   int		debug;

   /* linked list of messages sent which are waiting for replies */
   PendingReply *pending_replies;

};

struct Globals global_storage, *globals = &global_storage;

void usage(const char *progname) {
   fprintf(stderr, "usage: %s [options] [recipients]\n", progname);
   fprintf(stderr, "   options:\n");
   fprintf(stderr, "      -i <inst>      use instance <inst>\n");
   fprintf(stderr, "      -c <class>     use class <class>\n");
#ifdef CMU_INTERREALM
   fprintf(stderr, "      -r <realm>     use realm <realm>\n");
#endif
   fprintf(stderr, "      -s <sig>       use signature <sig>\n");
   fprintf(stderr, "      -S <sender>    use sender <sender>\n");
   fprintf(stderr, "      -O <opcode>    use opcode <opcode>\n");
   fprintf(stderr, "      -m <msg>       send msg instead of reading stdin (must be last arg)\n");
   fprintf(stderr, "      -d             print debugging information\n");
}

void exit_tzc() {
   ZClosePort();
   exit(0);
}		 

Code_t check(Code_t e, char *s) {
   if (e) {
      printf(";;; return code %d\n",(int) e);
      fflush(stdout);
      com_err(__FILE__, e, s);
      exit(1);
   }
   return e;
}

Code_t warn(Code_t e, char *s) {
   if (e)
      com_err(__FILE__, e, s);
   return e;
}

char *auth_string(int n) {
   switch (n) {
    case ZAUTH_YES    : return "yes";
    case ZAUTH_FAILED : return "failed";
    case ZAUTH_NO     : return "no";
    default           : return "bad-auth-value";
   }
}
 
char *kind_string(int n) {
   switch (n) {
    case UNSAFE:    return "unsafe";
    case UNACKED:   return "unacked";
    case ACKED:     return "acked";
    case HMACK:     return "hmack";
    case HMCTL:     return "hmctl";
    case SERVACK:   return "servack";
    case SERVNAK:   return "servnak";
    case CLIENTACK: return "clientack";
    case STAT:      return "stat";
    default:        return "bad-kind-value";
   }
}

/* warning: this uses ctime which returns a pointer to a static buffer
 * which is overwritten with each call. */
char *time_str(time_t time_num)
{
    char *now_name;
    now_name = ctime(&time_num);
    now_name[24] = '\0';	/* dump newline at end */
    return(now_name);
}

/* return time in the format "14:15:03" */
/* uses ctime, which returns a ptr to a static buffer */
char *debug_time_str(time_t time_num)
{
    char *now_name;
    now_name = ctime(&time_num);
    now_name[19] = '\0';	/* strip year */
    return now_name+11;		/* strip date */
}

void
setup()
{
   check(ZInitialize(), "ZInitialize");
   globals->port = 0;
   check(ZOpenPort(&globals->port), "ZOpenPort");

   globals->pending_replies = NULL;
}

int get_message(char **msg, char *sig) {
	/* XXX fix this to be dynamic */
	static char buf[65535];
	int c, len;
	char *p;
	strcpy(buf, sig);
	len = strlen(sig)+1;
	p = &(buf[len]);
	while ((c=getchar()) != EOF) {
		*p++ = c;
		len++;
	}
	len++;
	*p = '\0';
	*msg = buf;
	return len;
}

int get_message_arg(char **msg, char *msgptr, char *sig) {
	/* XXX fix this to be dynamic */
	static char buf[65535];
	int c, len;
        int i = 0;
	char *p;
	strcpy(buf, sig);
	len = strlen(sig)+1;
        strcpy(&buf[len], msgptr);
        len += strlen(msgptr)+1;
        *msg = buf;
	return len;
}

int main(int argc, const char *argv[]) {
   const char *program;
   const char **recipient;
   char *msg;
   int broadcast, msglen;
   int n_recips = 0;
   int (*auth)();
   int use_zctl = 0, sw;
   int havemsg = 0;
   int haverealm = 0;
   extern char *optarg;
   extern int optind;
   char location[BUFSIZ];
   ZNotice_t notice;
   int retval;
   char *sender=NULL, *signature="", *instance=DEFAULT_INSTANCE,
     *class=DEFAULT_CLASS, *opcode=DEFAULT_OPCODE, *msgptr="";     
#ifdef CMU_INTERREALM
   char *realm=DEFAULT_REALM;
   char rlmrecip[BUFSIZ];
   char *cp;
#endif

   program = strrchr(argv[0], '/');
   if (program == NULL)
      program = argv[0];
   else
      program++;

   while ((sw = getopt(argc, argv, "di:s:c:S:m:O:r:")) != EOF)
      switch (sw) {
       case 'O':
         opcode = optarg;
	 break;
       case 'i':
	 instance = optarg;
	 break;
       case 'c':
	 class = optarg;
	 break;
       case 's':
         signature = optarg;
	 break;
       case 'S':
         sender = optarg;
	 break;
       case 'd':
	 /* debug = 1; */
	 break;
#ifdef CMU_INTERREALM
       case 'r':
	 realm = optarg;
	 haverealm = 1;
	 break;
#endif
       case 'm':
	 msgptr = optarg;
	 havemsg = 1;
	 break;
       case '?':
       default:
	 usage(program);
	 exit(1);
      }

    broadcast = (optind == argc);

    if (broadcast && !(strcmp(class, DEFAULT_CLASS) ||
		       (strcmp(instance, DEFAULT_INSTANCE) &&
			strcmp(instance, URGENT_INSTANCE)))) {
	/* must specify recipient if using default class and
	   (default instance or urgent instance) */
	fprintf(stderr, "No recipients specified.\n");
	usage(program);
	exit(1);
    }

    if(havemsg)
        msglen = get_message_arg(&msg, msgptr, signature);
    else
        msglen = get_message(&msg, signature);

    setup();

    for ( ; broadcast || optind < argc; optind++) {
      bzero((char *) &notice, sizeof(notice));

      notice.z_kind = UNACKED;
      notice.z_port = 0;
      notice.z_class = class;
      notice.z_opcode = opcode;
      notice.z_sender = sender;
      notice.z_class_inst = instance;
#ifdef CMU_INTERREALM
      if (!broadcast && (cp = strchr(argv[optind], '@'))) {
	(void) strcpy(rlmrecip, argv[optind]);
	cp = strchr(rlmrecip, '@');
	if (cp) {
	  cp++;
	  (void) strcpy(cp, (char *) ZExpandRealm(cp));
	}
	notice.z_recipient = rlmrecip;
      } else if(haverealm) {
	rlmrecip[0] = '@';
	(void) strcpy(&rlmrecip[1], (char *) ZExpandRealm(realm));
	notice.z_recipient = rlmrecip;
      } else
#endif
      notice.z_recipient = (char *) (broadcast ? "" : argv[optind]);
      notice.z_message = msg;
      notice.z_message_len = msglen;
      notice.z_default_format = "@bold(UNAUTHENTIC) Class $class, Instance $instance:\n$message";
      auth = ZNOAUTH;
      if (auth == ZAUTH) {
	notice.z_default_format = "Class $class, Instance $instance:\nTo: @bold($recipient)\n$message";
      }
      if ((retval = ZSendNotice(&notice, auth)) != ZERR_NONE) {
#if 1
	char bfr[BUFSIZ];
	(void) sprintf(bfr, "while sending notice to %s", 
		       notice.z_recipient);
	com_err(__FILE__, retval, bfr);
#endif
	fprintf(stderr, "error %d from ZSendNotice while sending to %s\n", 
		retval, notice.z_recipient);
	exit(1);
      }
      if (broadcast)
	break;
   }
   exit(0);
}
