/* NETROM mailbox interface
 * Copyright 1991 Phil Karn, KA9Q
 *
 *	May '91	Bill Simpson
 *		move to separate file for compilation & linking
 */
#include "top.h"

#include <ctype.h>

#include "global.h"
#include "core/proc.h"
#include "core/socket.h"
#include "core/session.h"
#include "lib/util/cmdparse.h"
#include "commands.h"
#include "mailbox.h"

#include "net/netrom/netrom.h"
#include "net/netrom/nr4mail.h"


static int Nrsocket = -1;


int
nr4start(argc,argv,p)
int argc;
char *argv[];
void *p;
{
	int s,type;

	if (Nrsocket != -1)
		return -1;

	ksignal(Curproc,0);	/* Don't keep the parser waiting */
	chname(Curproc,"NETROM listener");
	Nrsocket = ksocket(kAF_NETROM,kSOCK_SEQPACKET,0);
	/* bind() is done automatically */
	if (klisten(Nrsocket,1) == -1) {
		close_s(Nrsocket);
		Nrsocket = -1;
		return -1;
	}
	for(;;){
		if((s = kaccept(Nrsocket,NULL,NULL)) == -1)
			break;	/* Service is shutting down */

		type = NRSESSION;
		newproc("mbox",2048,mbx_incom,s,(void *)type,NULL,0);
	}
	close_s(Nrsocket);
	Nrsocket = -1;
	return 0;
}
int
nr40(argc,argv,p)
int argc;
char *argv[];
void *p;
{
	close_s(Nrsocket);
	Nrsocket = -1;
	return 0;
}


static int dombnrident(int argc,char *argv[],void *p);
static int dombnrnodes(int argc,char *argv[],void *p);
static int dombnrconnect(int argc,char *argv[],void *p);

static char mbnrid[80];

static struct cmds Mbnrcmds[] = {
	{ "",		donothing,	0, 0, NULL },
	{ "connect",	dombnrconnect,	0, 0, NULL },
	{ "ident",	dombnrident,	0, 0, NULL },
	{ "nodes",	dombnrnodes,	0, 0, NULL },
	{ "users",	dombox,		0, 0, NULL },
	{ NULL,	NULL,		0, 0, NULL },
};

int
dombnetrom(argc,argv,p)
int argc;
char *argv[];
void *p;
{
	struct mbx *m;
	char *cp;

	if(Nrifaces[0].iface == NULL){
		kprintf("NET/ROM not activated.\n");
		return 0;
	}
	m = (struct mbx *) p;
	sprintf(mbnrid,"%s:%s",Nrifaces[0].alias,
	  pax25(m->line,Nrifaces[0].iface->hwaddr));
	kprintf("Connected to %s	",mbnrid);
	dombescape(1,NULL,p);

	while(mbxrecvline(m->user,m->line,MBXLINE,m->escape) >= 0) {
		for(cp = m->line; *cp; ++cp)	/* convert to lower case */
			if(isupper(*cp))
				*cp = tolower(*cp);
		if(cmdparse(Mbnrcmds,m->line,(void *)m) == -1)
			kprintf("%s> Invalid command (CONNECT IDENT NODES USERS)\n",
			  mbnrid);
	}
	return 0;
}

static int
dombnrident(argc,argv,p)
int argc;
char *argv[];
void *p;
{
	kprintf("%s> %s (%s)\n",mbnrid,Hostname,Version);
	return 0;
}

static int
dombnrnodes(argc,argv,p)
int argc;
char *argv[];
void *p;
{
	if(argc < 2)
		return doroutedump();
	return dorouteinfo(argc,argv,p);
}

static int
dombnrconnect(argc,argv,p)
int argc;
char *argv[];
void *p;
{
	struct mbx *m;
	uint8 *np;
	char buf[7];
	int s;
	struct ksockaddr_nr lsocket, fsocket;
	char alias[AXBUF];

	m = (struct mbx *) p;
	if(!(m->privs & NETROM_CMD)){
		kprintf(Noperm);
		return 0;
	}
	if((s = ksocket(kAF_NETROM,kSOCK_SEQPACKET,0)) == -1){
		kprintf(Nosock);
		return 0;
	}
	lsocket.nr_family = kAF_NETROM;
	/* Set up our local username, bind would use Mycall instead */
	if(strlen(m->name) > 6)
		strncpy(buf,m->name,6);
	else
		strcpy(buf,m->name);
	buf[6] = '\0';
	putalias(alias,buf,0);
	setcall(lsocket.nr_addr.user,alias);

	/* Putting anything else than Mycall here will not work */
	memcpy(lsocket.nr_addr.node,Mycall,AXALEN);
	kbind(s,(struct ksockaddr *)&lsocket,sizeof(struct ksockaddr_nr));

	/* See if the requested destination could be an alias, and
	 * find and use it if it is.  Otherwise assume it is an ax.25
	 * address.
	 */
	if(putalias(alias,argv[1],0) != -1 &&
		(np = find_nralias(alias)) != NULL){
		memcpy(fsocket.nr_addr.user,np,AXALEN);
		memcpy(fsocket.nr_addr.node,np,AXALEN);
	} else {	/* parse ax25 callsign */
		/* Only the user callsign of the remote station is never used by */
		/* NET/ROM, but it is needed for the psocket() call. */
		setcall(fsocket.nr_addr.user,argv[1]);
		setcall(fsocket.nr_addr.node,argv[1]);
	}
	fsocket.nr_family = kAF_NETROM;
	return gw_connect(m,s,(struct ksockaddr *)&fsocket, sizeof(struct ksockaddr_nr));
}

