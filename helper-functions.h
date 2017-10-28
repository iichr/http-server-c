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

/* file descriptor with an internal buffer */
typedef struct {
	int fd;		/* descriptor for this internal buf */
	int unread;		/* unread bytes in internal buf */
	char *p_buf;	/* next unread byte in internal buf */
	char ibuf[BUFSIZE];	/* internal read buffer */
} fd_internalbuf;

/* General I/O */
void error(char *msg);
ssize_t read_n_bytes(int fd, void *buf, size_t n);	/* read n bytes from file descriptor */
ssize_t write_n_bytes_a(int fd, void *buf, size_t n);	/* write n bytes to file descriptor */
void write_n_bytes(int fd, void *buf, size_t n);	/* write n bytes to file descriptor */

void init_internalbuf(fd_internalbuf *rp, int fd);	/* associate file descriptor with an internal buffer struct */

ssize_t readb(fd_internalbuf *rp, char *buf, size_t n);	/* buffered version of UNIX read() */

ssize_t readline_a(fd_internalbuf *rp, void *usrbuf, size_t maxlen); 
ssize_t readline(fd_internalbuf *rp, void *buf, size_t maxlen); /* read a text line from a file descriptor struct */

ssize_t read_n_bytes_buffered(fd_internalbuf *rp, void *buf, size_t n);	/* read n bytes to a buffer from a file descriptor struct */

/* HTTP Handling  */
void read_request_headers(fd_internalbuf *rp); /* read HTTP request headers from file descriptor struct */
void parse_uri(char *uri, char *filename); /* convert URI into a relative path name */
void getfiletype(char *filename, char *filetype); /* get the MIME type given a filename string */
void raise_http_err(int fd, char *err, char *error_code, char *shortmsg, char *longmsg); /* raise an HTTP error code with a message to the client */

void serve_static(int fd, char *filename, int filesize); /* send an HTTP response from the server */
