#ifndef	_FTPCLI_H
#define	_FTPCLI_H

#include "top.h"

#include "lib/std/stdio.h"
#include "service/ftp/ftp.h"

#include "core/session.h"

#define	LINELEN	256		/* Length of user command buffer */

/* Per-session FTP client control block */
struct ftpcli {
	kFILE *control;
	kFILE *data;

	char state;
#define	COMMAND_STATE	0	/* Awaiting user command */
#define	SENDING_STATE	1	/* Sending data to user */
#define	RECEIVING_STATE	2	/* Storing data from user */

	int verbose;		/* Transfer verbosity level */
	int batch;		/* Command batching flag */
	int abort;		/* Aborted transfer flag */
	int update;		/* Compare with MD5 during mput/mget */
	char type;		/* Transfer type */
	char typesent;		/* Last type command sent to server */
	int logbsize;		/* Logical byte size for logical type */
	kFILE *fp;		/* File descriptor being transferred */

	char buf[LINELEN];	/* Command buffer */
	char line[LINELEN];	/* Last response from server */
	struct session *session;
};
#endif	/* _FTPCLI_H */
