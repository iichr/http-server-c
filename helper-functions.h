#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define BUFSIZE 4096
#define MAXLINE 4096
#define MAXBUF  4096

typedef struct {
	int fd;		/* descriptor for this internal buf */
	int unread;		/* unread bytes in internal buf */
	char *p_buf;	/* next unread byte in internal buf */
	char ibuf[BUFSIZE];	/* internal read buffer */
} fd_internalbuf;

/* General I/O */
void error(char *msg); 
ssize_t read_n_bytes(int fd, void *buf, size_t n);
void write_n_bytes(int fd, void *buf, size_t n);
void init_internalbuf(fd_internalbuf *rp, int fd); 
static ssize_t readb(fd_internalbuf *rp, char *buf, size_t n);
ssize_t readline(fd_internalbuf *rp, void *buf, size_t maxlen);
ssize_t read_n_bytes_buffered(fd_internalbuf *rp, void *buf, size_t n);

/* HTTP Handling  */
void read_request_headers(fd_internalbuf *rp);
void parse_uri(char *uri, char *filename);
void getfiletype(char *filename, char *filetype);
void raise_http_err(int fd, char *err, char *error_code, char *shortmsg, char *longmsg);
void serve_static(int fd, char *filename, int filesize);
