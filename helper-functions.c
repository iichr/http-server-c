#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define BUFSIZE 4096
#define MAXLINE 4096
#define MAXBUF  4096

typedef struct {
	int fd;				/* descriptor for this internal buf */
	int unread;			/* unread bytes in internal buf */
	char *p_buf;		/* next unread byte in internal buf */
	char ibuf[BUFSIZE];	/* internal read buffer */
} fd_internalbuf;


/**
 * Displays error messages from system calls.
 * @param msg The message to display
 */
void error(char *msg)
{
	fprintf(stderr, "%s: %s\n", msg, strerror(errno));
	exit(0);
}

/**
 * Read n bytes using read(). Transferred directly between memory
 * and file, no buffer
 * @param  fd  a file descriptor
 * @param  buf a pointer to a buffer
 * @param  n   max size to read
 * @return     how many bytes were read
 */
ssize_t read_n_bytes(int fd, void *buf, size_t n) {
	size_t bytesleft = n;
	ssize_t bytesread;
	char *p_buf = buf;
	/* while bytes to be read */
	while(bytesleft > 0) {
		bytesread = read(fd, p_buf, bytesleft);
		// do error handling below: if(...)
		if(bytesread < 0) {
			error("Error in read_n_bytes");
		}
		else if(bytesread == 0) {
			break;
		}
		// update bytes left thus far and buffer pointer
		bytesleft = bytesleft - bytesread;
		p_buf = p_buf + bytesread; 
	}
	return n - bytesleft;
}

/**
 * Write n bytes using write(). Transferred directly between memory
 * and file, no buffer
 * @param  fd  a file descriptor
 * @param  buf a pointer to a buffer
 * @param  n   bytes to write
 * @return     how many bytes were written
 */
void write_n_bytes(int fd, void *buf, size_t n) 
{
	size_t bytesleft = n;
	ssize_t byteswritten;
	char *p_buf = buf;

	while (bytesleft > 0) {
		byteswritten = write(fd, p_buf, bytesleft);
		if(byteswritten <= 0) {
			error("Error in read_n_bytes");
		}
	bytesleft = bytesleft - byteswritten;
	p_buf = p_buf + byteswritten;
	}
	if(n != byteswritten) {
		error("Error in read_n_bytes");
	}
}


// adapted from Bryant,R. and O'Hallaron, D. Computer Systems A Programmers Perspective 3rd Edition
// pp 898 - ...

/**
 * Associate a file descriptor with a read buffer from the structure
 * @param rp the address of a structure with an internal buffer to be
 * mapped to a file descriptor 
 * @param fd a file descriptor
 */
void init_internalbuf(fd_internalbuf *rp, int fd) 
{
	rp->fd = fd;  
	rp->unread = 0;  
	rp->p_buf = rp->ibuf; /* point to first element of buffer */
}


/**
 * A buffered version of the Linux read() function.
 * - adapted from Bryant,R. and O'Hallaron, D.
 * - Computer Systems A Programmers Perspective 3rd Edition
 * 
 * When read() is called with a request to read n bytes
 * there are rp->unread unread bytes in the read buffer.
 * If the buffer is empty, then it is replenished with a call to read. 
 * Receiving a short count == partially filling the read buffer.
 * Once the buffer is nonempty, read() copies the minimum of n and rp->unread bytes from the
 * read buffer to buf and returns the number of bytes copied.
 * 
 * @param  rp  [description]
 * @param  buf [    ]
 * @param  n   [description]
 * @return     [description]
 */
static ssize_t readb(fd_internalbuf *rp, char *buf, size_t n) {

	int count = n;

	/* if buffer is empty */
	while (rp->unread <= 0) {
		rp->unread = read(rp->fd, rp->ibuf, 
			   sizeof(rp->ibuf));
		if (rp->unread < 0) {
		}
		else if (rp->unread == 0)  /* EOF */
			return 0;
		else
			/* reset buffer pointer to internal buffer */
			rp->p_buf = rp->ibuf;
	}

	/* Copy min(n, rp->rio_cnt) bytes from internal buf to user buf */        
	if (rp->unread < n)   
		count = rp->unread;
	memcpy(buf, rp->p_buf, count);
	rp->p_buf = rp->p_buf + count;	/* update pointer to the next unread element */
	rp->unread = rp->unread - count;
	return count;		/* number of bytes copied to supplied buffer */
}


/**
 * Reads the next text line from file rp (including the terminating newline character)
 * copies it to memory location buf, and terminates the text line with \0. 
 * It reads at most maxlen-1 bytes, leaving room for the terminating NULL character. 
 * Text lines that exceed maxlen-1 bytes are truncated and terminated with a NULL character.
 *
 * - adapted from Bryant,R. and O'Hallaron, D.
 * - Computer Systems A Programmers Perspective 3rd Edition
 * 
 * @param  rp     file descriptor with buffer to read from
 * @param  buf    where the line will be stored
 * @param  maxlen how many bytes is the maximum limit to read from a line
 * @return        number of bytes read
 */
ssize_t readline(fd_internalbuf *rp, void *buf, size_t maxlen) 
{
	int n;			/* number of bytes read in total */
	int bytesread;	/* bytes read thus far */
	char c;			/* keeps track of current buffer pointer */
	char *bufp = buf;

	for (n = 1; n < maxlen; n++) { 
		if ((bytesread = readb(rp, &c, 1)) == 1) {
			*bufp++ = c; // move pointer by one character position
			if (c == '\n')
			break;
		} else if (bytesread == 0) {
			if (n == 1)
				return 0; /* EOF, no data read */
			else
				break;    /* EOF, some data was read */
		} else
			error("Error in readline");
			return -1;    /* error */
	}
	*bufp = 0;
	return n;
}


/**
 * Read up to n bytes from file rp to the memory location buf
 * - adapted from Bryant,R. and O'Hallaron, D.
 * - Computer Systems A Programmers Perspective 3rd Edition
 * 
 * @param  rp  file descriptor with buffer to read from
 * @param  buf [description]
 * @param  n   [description]
 * @return     [description]
 */
ssize_t read_n_bytes_buffered(fd_internalbuf *rp, void *buf, size_t n) 
{
	size_t bytesleft = n;
	ssize_t bytesread;
	char *bufp = buf;
	
	while (bytesleft > 0) {
		if ((bytesread = readb(rp, bufp, bytesleft)) < 0) {
			if (errno == EINTR)     /* interrupted by sig handler return */
				bytesread = 0;      /* call read() again */
			else
				error("Error in read_n_bytes_buffered");
				return -1;          /* errno set by read() */ 
		}
		else if (bytesread == 0)
			break;                  /* EOF */

		// update bytes left thus far and buffer pointer
		bytesleft = bytesleft - bytesread;
		bufp = bufp + bytesread;
	}
	return n - bytesleft;       /* return >= 0 */
}


/**
 * HTTP HELPER FUNCTIONS
 */

// General approach:
// If formatted output needed -> sprintf
// send the formatted string to the socket using write_n_bytes
// 
// If formatted input needed -> readline to read an entire text line
// then use sscanf to extract different fields from the text line.

/**
 * Read request headers and print them
 * @param rp a struct with an internal buffer to read from
 */
void read_request_headers(fd_internalbuf *rp)
{
	char buf[MAXLINE];
	// first line
	readline(rp, buf, MAXLINE);
	printf("%s", buf);
	// request headers: Host, Accept, Content-Length etc.
	// a blank line separates them from the body
	// protocol requires \r\n
	while(strcmp(buf, "\r\n")) {
		readline(rp, buf, MAXLINE);
		printf("%s", buf);
	}
	return;
}

/**
 * Convert a given URI into a relative pathname e.g ./index.html
 * Error handling e.g whether file exists or not is done separately.
 * 
 * @param uri      the full URI
 * @param filename the relative pathname of the resource at the URI
 */
void parse_uri(char *uri, char *filename) {
	int urilen = strlen(uri);
	strcpy(filename, ".");
	strcat(filename, uri);
	// if URI ends with / then redirect to homepage
	if(uri[urilen - 1] == '/') {
		strcat(filename, "index.hmtl");
	}
	return;
}


/**
 * Obtain filetype given a filename and store the result in the filetype pointer passed as an argument
 * 
 * for reference see: https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Complete_list_of_MIME_types
 * 
 * @param filename the filename from which to extract the extension	
 * @param filetype where the filetype is going to be stored
 */
void getfiletype(char *filename, char *filetype) 
{
	if (strstr(filename, ".html"))
		strcpy(filetype, "text/html");

	else if (strstr(filename, ".jpeg"))
		strcpy(filetype, "image/jpeg");

	else if (strstr(filename, ".jpg"))
		strcpy(filetype, "image/jpg");

	else if (strstr(filename, ".pdf"))
		strcpy(filetype, "application/pdf");

	else
		strcpy(filetype, "text/plain");
}  



// Send an HTTP response with the body being the contents of a file
// First, inspect suffix of file -> determine file type
// Next, send response line and headers to client
// terminate every header with \r\n per the standard
// terminate headers section with \n
// Next, send the response body by copying file contents to fd
// p.961
// TEST!
void serve_static(int fd, char *filename, int filesize) {
	FILE *srcfd;
	char *srcp, filetype[MAXLINE], buf[MAXBUF];
 
	/* Send response headers to client */
	getfiletype(filename, filetype);
	sprintf(buf, "HTTP/1.1 200 OK\r\n");
	sprintf(buf, "%sServer: CSERVER\r\n", buf);
	sprintf(buf, "%sConnection: close\r\n", buf);
	sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
	sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
	/* double empty line between response line and response header as per protocol */
	write_n_bytes(fd, buf, strlen(buf));
	printf("Response headers:\n");
	printf("%s", buf);

	/* Send response body to client */

	// open filename and get its descriptor fd
	srcfd = fopen(filename, "r");

	// map requested file to virtual memory area, starting at address srcp
	//srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
	//mmap args
	//(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
	//addr = starting address, length = length of mapping,
	//prot = memory protection of the mapping (see open mode of file)
	//flags, offset in fd = 0
	// alternatively fread
	fread(buf,1,MAXBUF,srcfd);

	// close afterwards, no longer needed !!!!!
	fclose(srcfd);         
	// copy filsieze bytes starting from srcp to the descriptor fd
	write_n_bytes(fd, srcp, filesize);
}


/**
 * Return a given error message with an error code to the client
 * @param fd         the file descriptor	
 * @param err        name of object that caused the error
 * @param error_code HTTP error code e.g 404
 * @param shortmsg   HTTP error code message e.g Forbidden
 * @param longmsg    A more thorough explanation
 */
void raise_http_err(int fd, char *err, char *error_code, char *shortmsg, char *longmsg) 
{
	char buf[MAXLINE];	/* store response header */
	char body[MAXBUF];	/* store the response body */

	/* Print the HTTP response */
	sprintf(buf, "HTTP/1.1 %s %s\r\n", error_code, shortmsg);
	write_n_bytes(fd, buf, strlen(buf));
	
	sprintf(buf, "Server: CSERVER\r\n");
	write_n_bytes(fd, buf, strlen(buf));
	
	sprintf(buf, "Content-type: text/html\r\n");
	write_n_bytes(fd, buf, strlen(buf));
	
	sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
	write_n_bytes(fd, buf, strlen(buf));

	/* Build the HTTP response body */
	sprintf(body, "<html><title>Error</title>");
	sprintf(body, "%s <body>\r\n", body);
	sprintf(body, "%s %s: %s\r\n", body, error_code, shortmsg);
	sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, err);
	/* print the response body */
	write_n_bytes(fd, body, strlen(body));
}






