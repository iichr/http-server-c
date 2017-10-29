#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>

/* Constants */
#define BUFSIZE 4096
#define MAXLINE 4096
#define MAXBUF  4096
#define MAXSMALL 2048


/* File descriptor with an internal buffer */
typedef struct {
	int fd;		/* descriptor for this internal buf */
	int unread;		/* unread bytes in internal buf */
	char *p_buf;	/* next unread byte in internal buf */
	char ibuf[BUFSIZE];	/* internal read buffer */
} fd_internalbuf;

/**
 * GENERAL I/O
 */
void error(char *msg);	/* for error printing */

void init_internalbuf(fd_internalbuf *rp, int fd);	/* associate file descriptor with an internal buffer struct */

ssize_t readb(fd_internalbuf *rp, char *buf, size_t n);	/* buffered version of UNIX read() */

ssize_t readline_a(fd_internalbuf *rp, void *usrbuf, size_t maxlen); 
ssize_t readline(fd_internalbuf *rp, void *buf, size_t maxlen); /* read a text line from a file descriptor struct */

/**
 * HTTP HANDLING
 */
void read_request_headers(fd_internalbuf *rp); /* read HTTP request headers from file descriptor struct */
void parse_uri(char *uri, char *filename); /* convert URI into a path name */
void getfiletype(char *filename, char *filetype); /* get the MIME type given a filename string */
void raise_http_err(int fd, char *err, char *error_code, char *err_title, char *descr); /* raise an HTTP error code with a message to the client */

void serve_http(int fd, char *filename, int filesize); /* send an HTTP response from the server */
