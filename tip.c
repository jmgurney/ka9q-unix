/* "Dumb terminal" session command for serial lines
 * Copyright 1991 Phil Karn, KA9Q
 *
 *	Feb '91	Bill Simpson
 *		rlsd control and improved dialer
 */
#include "top.h"

#include "global.h"
#include "stdio.h"
#include "mbuf.h"
#include "proc.h"
#include "iface.h"
#ifndef	UNIX
#include "n8250.h"
#endif
#include "asy.h"
#include "tty.h"
#include "session.h"
#include "socket.h"
#include "commands.h"
#include "devparam.h"


static void tip_out(int i,void *n1,void *n2);


/* Execute user telnet command */
int
dotip(argc,argv,p)
int argc;
char *argv[];
void *p;
{
	struct session *sp;
	char *ifn;
	int c;
	kFILE *asy;

	if((asy = asyopen(argv[1],"r+b")) == NULL){
		kprintf("Can't kopen %s\n",argv[1]);
		return 1;
	}
	ksetvbuf(asy,NULL,_kIONBF,0);
	/* Allocate a session descriptor */
	if((sp = newsession(Cmdline,TIP,1)) == NULL){
		kprintf("Too many sessions\n");
		return 1;
	}
	/* Put tty into raw mode */
	sp->ttystate.echo = 0;
	sp->ttystate.edit = 0;
	kfmode(kstdin,STREAM_BINARY);
	kfmode(kstdout,STREAM_BINARY);

	/* Now fork into two paths, one rx, one tx */
	ifn = malloc(strlen(argv[1]) + 10);
	sprintf(ifn,"%s tip out",argv[1]);
	sp->proc1 = newproc(ifn,256,tip_out,0,asy,NULL,0);
	free( ifn );

	ifn = malloc(strlen(argv[1]) + 10);
	sprintf(ifn,"%s tip in",argv[1]);
	chname( Curproc, ifn );
	free( ifn );

	while((c = kfgetc(asy)) != kEOF){
		kputchar(c);
		if(sp->record != NULL)
			kputc(c,sp->record);
	}
	kfflush(kstdout);

	killproc(&sp->proc1);
	kfclose(asy);
	keywait(NULL,1);
	freesession(&sp);
	return 0;
}


/* Output process, DTE version */
static void
tip_out(i,n1,n2)
int i;
void *n1,*n2;
{
	int c;
	kFILE *asy = (kFILE *)n1;

	while((c = kgetchar()) != kEOF){
		kfputc(c,asy);
	}
}


