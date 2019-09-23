/* Low level AX.25 routines:
 *  callsign conversion
 *  control block management
 *
 * Copyright 1991 Phil Karn, KA9Q
 */
#include "top.h"

#include "stdio.h"
#include "global.h"
#include "mbuf.h"
#include "timer.h"
#include "ax25.h"
#include "lapb.h"
#include <ctype.h>

struct ax25_cb *Ax25_cb;

/* Default AX.25 parameters */
uint32 T2init = 1000;		/* 1000ms transmit delay */
uint32 T3init = 0;		/* No keep-alive polling */
uint Maxframe = 1;		/* Stop and wait */
uint N2 = 10;			/* 10 retries */
uint Axwindow = 2048;		/* 2K incoming text before RNR'ing */
uint Paclen = 256;		/* 256-byte I fields */
uint Pthresh = 128;		/* Send polls for packets larger than this */
uint32 Axirtt = 5000;		/* Initial round trip estimate, ms */
uint Axversion = V1;		/* Protocol version */
uint32 Blimit = 30;		/* Retransmission backoff limit */

/* Look up entry in connection table */
struct ax25_cb *
find_ax25(uint8 *addr)
{
	struct ax25_cb *axp;
	struct ax25_cb *axlast = NULL;

	/* Search list */
	for(axp = Ax25_cb; axp != NULL; axlast=axp,axp = axp->next){
		if(addreq(axp->remote,addr)){
			if(axlast != NULL){
				/* Move entry to top of list to speed
				 * future searches
				 */
				axlast->next = axp->next;
				axp->next = Ax25_cb;
				Ax25_cb = axp;
			}
			return axp;
		}
	}
	return NULL;
}

/* Remove entry from connection table */
void
del_ax25(struct ax25_cb *conn)
{
	struct ax25_cb *axp;
	struct ax25_cb *axlast = NULL;

	for(axp = Ax25_cb; axp != NULL; axlast=axp,axp = axp->next){
		if(axp == conn)
			break;
	}
	if(axp == NULL)
		return;	/* Not found */

	/* Remove from list */
	if(axlast != NULL)
		axlast->next = axp->next;
	else
		Ax25_cb = axp->next;

	/* Timers should already be stopped, but just in case... */
	stop_timer(&axp->t1);
	stop_timer(&axp->t2);
	stop_timer(&axp->t3);

	/* Free allocated resources */
	free_q(&axp->txq);
	free_q(&axp->rxasm);
	free_q(&axp->rxq);
	free(axp);
}

/* Create an ax25 control block. Allocate a new structure, if necessary,
 * and fill it with all the defaults. The caller
 * is still responsible for filling in the reply address
 */
struct ax25_cb *
cr_ax25(uint8 *addr)
{
	struct ax25_cb *axp;

	if(addr == NULL)
		return NULL;

	if((axp = find_ax25(addr)) == NULL){
		/* Not already in table; create an entry
		 * and insert it at the head of the chain
		 */
		axp = (struct ax25_cb *)callocw(1,sizeof(struct ax25_cb));
		axp->next = Ax25_cb;
		Ax25_cb = axp;
	}
	axp->user = -1;
	axp->state = LAPB_DISCONNECTED;
	axp->maxframe = Maxframe;
	axp->window = Axwindow;
	axp->paclen = Paclen;
	axp->proto = Axversion;	/* Default, can be changed by other end */
	axp->pthresh = Pthresh;
	axp->n2 = N2;
	axp->srt = Axirtt;
	set_timer(&axp->t1,2*axp->srt);
	axp->t1.func = recover;
	axp->t1.arg = axp;

	set_timer(&axp->t2, T2init);
	axp->t2.func = defer_lapb_send;
	axp->t2.arg = axp;

	set_timer(&axp->t3,T3init);
	axp->t3.func = pollthem;
	axp->t3.arg = axp;

	/* Always to a receive and state upcall as default */
	axp->r_upcall = s_arcall;
	axp->s_upcall = s_ascall;

	return axp;
}

/*
 * setcall - convert callsign plus substation ID of the form
 * "KA9Q-0" to AX.25 (shifted) address format
 *   Address extension bit is left clear
 *   Return -1 on error, 0 if OK
 */
int
setcall(uint8 *out,char *call)
{
	int i,csize;
	unsigned ssid;
	char *dp,c;

	if(out == NULL || call == NULL || *call == '\0')
		return -1;

	/* Find dash, if any, separating callsign from ssid
	 * Then compute length of callsign field and make sure
	 * it isn't excessive
	 */
	dp = strchr(call,'-');
	if(dp == NULL)
		csize = strlen(call);
	else
		csize = dp - call;
	if(csize > ALEN)
		return -1;
	/* Now find and convert ssid, if any */
	if(dp != NULL){
		dp++;	/* skip dash */
		ssid = atoi(dp);
		if(ssid > 15)
			return -1;
	} else
		ssid = 0;
	/* Copy upper-case callsign, left shifted one bit */
	for(i=0;i<csize;i++){
		c = *call++;
		if(islower(c))
			c = toupper(c);
		*out++ = c << 1;
	}
	/* Pad with shifted spaces if necessary */
	for(;i<ALEN;i++)
		*out++ = ' ' << 1;
	
	/* Insert substation ID field and set reserved bits */
	*out = 0x60 | (ssid << 1);
	return 0;
}
int
addreq(const uint8 *a,const uint8 *b)
{
	if(memcmp(a,b,ALEN) != 0 || ((a[ALEN] ^ b[ALEN]) & SSID) != 0)
		return 0;
	else
		return 1;
}
/* Convert encoded AX.25 address to printable string */
char *
pax25(char *e,uint8 *addr)
{
	int i;
	char c,*cp;

	cp = e;
	for(i=ALEN;i != 0;i--){
		c = (*addr++ >> 1) & 0x7f;
		if(c != ' ')
			*cp++ = c;
	}
	if((*addr & SSID) != 0)
		sprintf(cp,"-%d",(*addr >> 1) & 0xf);	/* ssid */
	else
		*cp = '\0';
	return e;
}

/* Figure out the frame type from the control field
 * This is done by masking out any sequence numbers and the
 * poll/final bit after determining the general class (I/S/U) of the frame
 */
uint
ftype(uint control)
{
	if((control & 1) == 0)	/* An I-frame is an I-frame... */
		return I;
	if(control & 2)		/* U-frames use all except P/F bit for type */
		return control & ~PF;
	else			/* S-frames use low order 4 bits for type */
		return control & 0xf;
}

void
lapb_garbage(int red)
{
	struct ax25_cb *axp;

	for(axp=Ax25_cb;axp != NULL;axp = axp->next){
		mbuf_crunch(&axp->rxq);
		mbuf_crunch(&axp->rxasm);
	}
}

