/* Packet tracing - top level and generic routines, including hex/ascii
 * Copyright 1991 Phil Karn, KA9Q
 */
#include "top.h"

#include "lib/std/stdio.h"
#include <ctype.h>
#include <time.h>
#include "global.h"
#include <stdarg.h>
#include "net/core/mbuf.h"
#include "net/core/iface.h"
#include "commands.h"
#include "core/trace.h"
#include "core/session.h"

static void ascii_dump(kFILE *fp,struct mbuf **bpp);
static void ctohex(char *buf,uint c);
static void fmtline(kFILE *fp,uint addr,uint8 *buf,uint len);
void hex_dump(kFILE *fp,struct mbuf **bpp);
static void showtrace(struct iface *ifp);

/* Redefined here so that programs calling dump in the library won't pull
 * in the rest of the package
 */
static char nospace[] = "No space!!\n";

struct tracecmd Tracecmd[] = {
	{ "input",	IF_TRACE_IN,	IF_TRACE_IN },
	{ "-input",	0,		IF_TRACE_IN },
	{ "output",	IF_TRACE_OUT,	IF_TRACE_OUT },
	{ "-output",	0,		IF_TRACE_OUT },
	{ "broadcast",	0,		IF_TRACE_NOBC },
	{ "-broadcast",	IF_TRACE_NOBC,	IF_TRACE_NOBC },
	{ "raw",	IF_TRACE_RAW,	IF_TRACE_RAW },
	{ "-raw",	0,		IF_TRACE_RAW },
	{ "ascii",	IF_TRACE_ASCII,	IF_TRACE_ASCII|IF_TRACE_HEX },
	{ "-ascii",	0,		IF_TRACE_ASCII|IF_TRACE_HEX },
	{ "hex",	IF_TRACE_HEX,	IF_TRACE_ASCII|IF_TRACE_HEX },
	{ "-hex",	IF_TRACE_ASCII,	IF_TRACE_ASCII|IF_TRACE_HEX },
	{ "off",	0,		0xffff },
	{ NULL,	0,	0 },
};


void
dump(
struct iface *ifp,
int direction,
struct mbuf *bp
){
	struct mbuf *tbp;
	uint size;
	time_t timer;
	char *cp;
	struct iftype *ift;
	kFILE *fp;

	if(ifp == NULL || (ifp->trace & direction) == 0
	 || (fp = ifp->trfp) == NULL)
		return;	/* Nothing to trace */

	ift = ifp->iftype;
	switch(direction){
	case IF_TRACE_IN:
		if((ifp->trace & IF_TRACE_NOBC)
		 && ift != NULL
		 && (ift->addrtest != NULL)
		 && (*ift->addrtest)(ifp,bp) == 0)
			return;		/* broadcasts are suppressed */
		time(&timer);
		cp = ctime(&timer);
		cp[24] = '\0';
		kfprintf(fp,"\n%s - %s recv:\n",cp,ifp->name);
		break;
	case IF_TRACE_OUT:
		time(&timer);
		cp = ctime(&timer);
		cp[24] = '\0';
		kfprintf(fp,"\n%s - %s sent:\n",cp,ifp->name);
		break;
	}
	if(bp == NULL || (size = len_p(bp)) == 0){
		kfprintf(fp,"empty packet!!\n");
		return;
	}
	dup_p(&tbp,bp,0,size);
	if(tbp == NULL){
		kfprintf(fp,nospace);
		return;
	}
	if(ift != NULL && ift->trace != NULL)
		(*ift->trace)(fp,&tbp,1);
	if(ifp->trace & IF_TRACE_ASCII){
		/* Dump only data portion of packet in ascii */
		ascii_dump(fp,&tbp);
	} else if(ifp->trace & IF_TRACE_HEX){
		/* Dump entire packet in hex/ascii */
		free_p(&tbp);
		dup_p(&tbp,bp,0,len_p(bp));
		if(tbp != NULL)
			hex_dump(fp,&tbp);
		else
			kfprintf(fp,nospace);
	}
	free_p(&tbp);
}

/* Dump packet bytes, no interpretation */
void
raw_dump(ifp,direction,bp)
struct iface *ifp;
int direction;
struct mbuf *bp;
{
	struct mbuf *tbp;
	kFILE *fp;

	if((fp = ifp->trfp) == NULL)
		return;
	kfprintf(fp,"\n******* raw packet dump (%s)\n",
	 ((direction & IF_TRACE_OUT) ? "send" : "recv"));
	dup_p(&tbp,bp,0,len_p(bp));
	if(tbp != NULL)
		hex_dump(fp,&tbp);
	else
		kfprintf(fp,nospace);
	kfprintf(fp,"*******\n");
	kfflush(fp);
	free_p(&tbp);
}

/* Dump an mbuf in hex */
void
hex_dump(fp,bpp)
kFILE *fp;
struct mbuf **bpp;
{
	uint n;
	uint address;
	uint8 buf[16];

	if(bpp == NULL || *bpp == NULL || fp == NULL)
		return;

	address = 0;
	while((n = pullup(bpp,buf,sizeof(buf))) != 0){
		fmtline(fp,address,buf,n);
		address += n;
	}
}
/* Dump an mbuf in ascii */
static void
ascii_dump(fp,bpp)
kFILE *fp;
struct mbuf **bpp;
{
	int c;
	uint tot;

	if(bpp == NULL || *bpp == NULL || fp == NULL)
		return;

	tot = 0;
	while((c = PULLCHAR(bpp)) != -1){
		if((tot % 64) == 0)
			kfprintf(fp,"%04x  ",tot);
		kputc(isprint(c) ? c : '.',fp);
		if((++tot % 64) == 0)
			kfprintf(fp,"\n");
	}
	if((tot % 64) != 0)
		kfprintf(fp,"\n");
}
/* Print a buffer up to 16 bytes long in formatted hex with ascii
 * translation, e.g.,
 * 0000: 30 31 32 33 34 35 36 37 38 39 3a 3b 3c 3d 3e 3f  0123456789:;<=>?
 */
static void
fmtline(fp,addr,buf,len)
kFILE *fp;
uint addr;
uint8 *buf;
uint len;
{
	char line[80];
	char *aptr,*cptr;
	uint8 c;

	memset(line,' ',sizeof(line));
	ctohex(line,(uint)hibyte(addr));
	ctohex(line+2,(uint)lobyte(addr));
	aptr = &line[6];
	cptr = &line[55];
	while(len-- != 0){
		c = *buf++;
		ctohex(aptr,(uint)c);
		aptr += 3;
		*cptr++ = isprint(c) ? c : '.';
	}
	*cptr++ = '\n';
	kfwrite(line,1,(unsigned)(cptr-line),fp);
}
/* Convert byte to two ascii-hex characters */
static void
ctohex(buf,c)
char *buf;
uint c;
{
	static char hex[] = "0123456789abcdef";

	*buf++ = hex[hinibble(c)];
	*buf = hex[lonibble(c)];
}

/* Modify or displace interface trace flags */
int
dotrace(argc,argv,p)
int argc;
char *argv[];
void *p;
{
	struct iface *ifp;
	struct tracecmd *tp;
	struct session *sp;

	if(argc < 2){
		for(ifp = Ifaces; ifp != NULL; ifp = ifp->next)
			showtrace(ifp);
		return 0;
	}
	if((ifp = if_lookup(argv[1])) == NULL){
		kprintf("Interface %s unknown\n",argv[1]);
		return 1;
	}
	if(argc == 2){
		showtrace(ifp);
		return 0;
	}
	/* MODIFY THIS TO HANDLE MULTIPLE OPTIONS */
	if(argc >= 3){
		for(tp = Tracecmd;tp->name != NULL;tp++)
			if(strncmp(tp->name,argv[2],strlen(argv[2])) == 0)
				break;
		if(tp->name != NULL)
			ifp->trace = (ifp->trace & ~tp->mask) | tp->val;
		else
			ifp->trace = htoi(argv[2]);
	}
	if(ifp->trfp != NULL){
		/* Close existing trace file */
		kfclose(ifp->trfp);
		ifp->trfp = NULL;
	}
	if(argc >= 4){
		if((ifp->trfp = kfopen(argv[3],APPEND_TEXT)) == NULL){
			kprintf("Can't write to %s\n",argv[3]);
		}
		/* Make sure its line buffered */
		ksetvbuf(ifp->trfp,NULL,_kIOLBF,kBUFSIZ);
	} else if(ifp->trace != 0){
		/* Create trace session */
		sp = newsession(Cmdline,ITRACE,1);
		sp->cb.p = NULL;
		sp->proc = sp->proc1 = sp->proc2 = NULL;
		ifp->trfp = sp->output;
		showtrace(ifp);
		kgetchar();	/* Wait for the user to hit something */
		ifp->trace = 0;
		ifp->trfp = NULL;
		freesession(&sp);
	}
	return 0;
}
/* Display the trace flags for a particular interface */
static void
showtrace(struct iface *ifp)
{
	char *cp;

	if(ifp == NULL)
		return;
	kprintf("%s:",ifp->name);
	if(ifp->trace & (IF_TRACE_IN | IF_TRACE_OUT | IF_TRACE_RAW)){
		if(ifp->trace & IF_TRACE_IN)
			kprintf(" input");
		if(ifp->trace & IF_TRACE_OUT)
			kprintf(" output");

		if(ifp->trace & IF_TRACE_NOBC)
			kprintf(" - no broadcasts");

		if(ifp->trace & IF_TRACE_HEX)
			kprintf(" (Hex/ASCII dump)");
		else if(ifp->trace & IF_TRACE_ASCII)
			kprintf(" (ASCII dump)");
		else
			kprintf(" (headers only)");

		if(ifp->trace & IF_TRACE_RAW)
			kprintf(" Raw output");

		if(ifp->trfp != NULL && (cp = kfpname(ifp->trfp)) != NULL)
			kprintf(" trace file: %s",cp);
		kprintf("\n");
	} else
		kprintf(" tracing off\n");
}

/* shut down all trace files */
void
shuttrace()
{
	struct iface *ifp;

	for(ifp = Ifaces; ifp != NULL; ifp = ifp->next){
		kfclose(ifp->trfp);
		ifp->trfp = NULL;
	}
}

/* Log messages of the form
 * Tue Jan 31 00:00:00 1987 44.64.0.7:1003 open FTP
 */
void
trace_log(struct iface *ifp,char *fmt, ...)
{
	va_list ap;
	char *cp;
	time_t t;
	kFILE *fp;

	if((fp = ifp->trfp) == NULL)
		return;
	time(&t);
	cp = ctime(&t);
	rip(cp);
	kfprintf(fp,"%s - ",cp);
	va_start(ap,fmt);
	kvfprintf(fp,fmt,ap);
	va_end(ap);
	kfprintf(fp,"\n");
	kfflush(ifp->trfp);
}
int
tprintf(struct iface *ifp,char *fmt, ...)
{
	va_list ap;
	int ret = 0;

	if(ifp->trfp == NULL)
		return -1; 
	va_start(ap,fmt);
	ret = kvfprintf(ifp->trfp,fmt,ap);
	kfflush(ifp->trfp);
	va_end(ap);
	return ret;
}
