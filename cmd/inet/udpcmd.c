/* UDP-related user commands
 * Copyright 1991 Phil Karn, KA9Q
 */
#include "top.h"

#include "lib/std/stdio.h"
#include "global.h"
#include "net/core/mbuf.h"
#include "lib/util/cmdparse.h"
#include "commands.h"

#include "lib/inet/netuser.h"
#include "net/inet/udp.h"
#include "net/inet/internet.h"

static int doudpstat(int argc,char *argv[],void *p);

static struct cmds Udpcmds[] = {
	{ "status",	doudpstat,	0, 0,	NULL },
	{ NULL },
};
int
doudp(argc,argv,p)
int argc;
char *argv[];
void *p;
{
	return subcmd(Udpcmds,argc,argv,p);
}
int
st_udp(udp,n)
struct udp_cb *udp;
int n;
{
	if(n == 0)
		kprintf("&UCB      Rcv-Q  Local socket\n");

	return kprintf("%09p%6u  %s\n",udp,udp->rcvcnt,pinet(&udp->socket));
}

/* Dump UDP statistics and control blocks */
static int
doudpstat(argc,argv,p)
int argc;
char *argv[];
void *p;
{
	register struct udp_cb *udp;
	register int i;

	for(i=1;i<=NUMUDPMIB;i++){
		kprintf("(%2u)%-20s%10lu",i,
		 Udp_mib[i].name,Udp_mib[i].value.integer);
		if(i % 2)
			kprintf("     ");
		else
			kprintf("\n");
	}
	if((i % 2) == 0)
		kprintf("\n");

	kprintf("    &UCB Rcv-Q  Local socket\n");
	for(udp = Udps;udp != NULL; udp = udp->next){
		if(st_udp(udp,1) == kEOF)
			return 0;
	}
	return 0;
}
