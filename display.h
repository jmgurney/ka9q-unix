#ifndef	_DISPLAY_H
#define	_DISPLAY_H

#ifndef	_STDIO_H
#include <stdio.h>
#endif

#define	MAXARGS	5

struct display {
	uint32 cookie;	/* Magic cookie to detect bogus pointers */
#define	D_COOKIE	0xdeadbeef
	int cols;	/* Screen width */
	int col;	/* cursor column, 0 to cols-1 */
	int savcol;	/* Saved cursor column */

	int rows;	/* Screen height */
	int row;	/* cursor row, 0 to rows-1 */
	int savrow;	/* Saved cursor row */

	int virttop;	/* Offset to start of virtual screen in buffer */
	int realtop;	/* Offset to start of real screen in buffer */
	int maxtop;	/* Max offset touched */
	int size;	/* Size of memory buffer in rows */

	int argi;	/* index to current entry in arg[] */
	int arg[MAXARGS];	/* numerical args to ANSI sequence */

	uint8 attrib;	/* Current screen attribute */
	enum {
		DISP_NORMAL,	/* No ANSI sequence in progress */
		DISP_ESCAPE,	/* ESC char seen */
		DISP_ARG	/* ESC[ seen */
	} state;	/* State of ANSI escape sequence FSM */
	struct {
		unsigned int dirty_screen:1;	/* Whole screen needs update */
		unsigned int dirty_cursor:1;	/* Cursor has moved */
		unsigned int no_line_wrap:1;	/* Don't wrap past last col */
		unsigned int no_scroll:1;	/* Set for wrap-scrolling */
		unsigned int scrollbk;		/* Scrollback is active */ 
	} flags;	/* Status flags */

	uint8 *buf;	/* Internal screen image */

	/* "Dirty" info. Keeps track of which lines (and parts of lines)
	 * have changed since the last screen update. lcol is initialized
	 * to the right edge, while rcol is initialized to the left edge.
	 * Whenever lcol <= rcol, the line is considered to be dirty.
	 */
	struct dirty {
		uint8 lcol;	/* Leftmost dirty column */
		uint8 rcol;	/* Rightmost dirty column */
	} *dirty;		/* One per line */
	uint8 *tabstops;	/* Tab stops */
};
#define	COLS	80
#define	ROWS	25
extern uint8 Statbuf[];
extern struct dirty Stdirt;

struct display *newdisplay(int rows,int cols,int noscrol,int sfsize);
void displaywrite(struct display *dp,const void *buf,int cnt);
void dupdate(struct display *dp);
void closedisplay(struct display *dp);
void statwrite(int col,void *buf,int cnt,int attrib);
void dscrollmode(struct display *dp,int flag);
void dhome(struct display *dp);
void dend(struct display *dp);
void dpgup(struct display *dp);
void dpgdown(struct display *dp);
void dcursup(struct display *dp);
void dcursdown(struct display *dp);
void debug(char *s);

#endif _DISPLAY_H

