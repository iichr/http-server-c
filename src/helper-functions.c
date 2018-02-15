#include "helper-functions.h"

/**
 * Displays error messages from system calls.
 * @param msg The message to display
 */
void error(char *msg) {
	fprintf(stderr, "%s: %s\n", msg, strerror(errno));
	exit(0);
}

/**
 * Associate a file descriptor with a read buffer from the structure
 * @param rp the address of a structure with an internal buffer to be
 * mapped to a file descriptor 
 * @param fd a file descriptor
 */
void init_internalbuf(fd_internalbuf *rp, int fd) {
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
ssize_t readb(fd_internalbuf *rp, char *buf, size_t n) {
	int count = n;

	/* if buffer is empty */
	while (rp->unread <= 0) {
		rp->unread = read(rp->fd, rp->ibuf, 
			   sizeof(rp->ibuf));
		if (rp->unread < 0) {
			if(errno != EINTR) {
				return -1;
			}
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
 * copies it to memory location usrbuf, and terminates the text line with \0. 
 * It reads at most maxlen-1 bytes, leaving room for the terminating NULL character. 
 * Text lines that exceed maxlen-1 bytes are truncated and terminated with a NULL character.
 *
 * - adapted from Bryant,R. and O'Hallaron, D.
 * - Computer Systems A Programmers Perspective 3rd Edition
 * 
 * @param  rp     file descriptor with buffer to read from
 * @param  usrbuf where the line will be stored
 * @param  maxlen how many bytes is the maximum limit to read from a line
 * @return        number of bytes read
 */
ssize_t readline_a(fd_internalbuf *rp, void *usrbuf, size_t maxlen) 
{
	int n, rc;
	char c, *bufp = usrbuf;

	for (n = 1; n < maxlen; n++) { 
	if ((rc = readb(rp, &c, 1)) == 1) {
		*bufp++ = c;
		if (c == '\n')
		break;
	} else if (rc == 0) {
		if (n == 1)
		return 0; /* EOF, no data read */
		else
		break;    /* EOF, some data was read */
	} else
		return -1;	  /* error */
	}
	*bufp = 0;
	return n;
}

/**
 * Wrapper for readline_a.
 * - adapted from Bryant,R. and O'Hallaron, D.
 * - Computer Systems A Programmers Perspective 3rd Edition
 */

ssize_t readline(fd_internalbuf *rp, void *usrbuf, size_t maxlen) 
{
    ssize_t rc;

    if ((rc = readline_a(rp, usrbuf, maxlen)) < 0)
	error("Readline error");
    return rc;
} 

/**
 * HTTP HANDLING
 */

/**
 * Read request headers and print them
 * @param rp a struct with an internal buffer to read from
 */
void read_request_headers(fd_internalbuf *rp) {
	char buf[MAXLINE];

	readline(rp, buf, MAXLINE); /* first line of the request */
	printf("%s", buf); /* print it */
	/**
	 * Keep reading until \r\n ecnountered
	 * request headers: Host, Accept, Content-Length etc.
	 * Separated from the the body by a blank line
	 * Specs require \r\n
	 */
	while(strcmp(buf, "\r\n")) {
		readline(rp, buf, MAXLINE);
		printf("%s", buf);
	}
	return;
}

/**
 * Convert a given URI into a pathname
 * Error handling e.g whether file exists or not is done separately.
 * 
 * @param uri      the full URI
 * @param filename the pathname to the resource at the URI
 */
void parse_uri(char *uri, char *filename) {
	char *dir;
	/* get current working directory */
	dir = getenv("PWD");
	char path_buffer[300];	/* buffer to store the path. Prone to overflow!! */

	int urilen = strlen(uri);
	/* if URI ends with / then redirect to the index page */
	if(uri[urilen - 1] == '/') {
		sprintf(path_buffer, "%s/index.html", dir);
		strcat(filename, path_buffer);
	} else {
		sprintf(path_buffer, "%s%s", dir, uri);
		strcat(filename, path_buffer);
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
void getfiletype(char *filename, char *filetype) {
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


/**
 * Return a given error message with an error code to the client
 * @param fd         	the file descriptor	
 * @param err 	name of object that caused the error
 * @param error_code	HTTP error code e.g 404
 * @param error_title	HTTP error code message e.g Forbidden
 * @param error_descr	A more thorough explanation
 */
void raise_http_err(int fd, char *err, char *error_code, char *error_title, char *error_descr) {
	char buf[BUFSIZE];	/* store response header */
	char body[BUFSIZE];	/* store the response body */

	/* RESPONSE BODY - to be diplsayed in the browser */
	sprintf(body, "<html><title>%s Error</title>", error_code);
    sprintf(body, "%s<body> <h1>\r\n", body);
    sprintf(body, "%s%s: %s</h1><h3>\r\n", body, error_code, error_title);
    sprintf(body, "%s %s: <em> %s <em> </h3>\r\n", body, error_descr, err);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.1 %s %s\r\n", error_code, error_title);
    write(fd, buf, strlen(buf));
    sprintf(buf, "%sConnection: close\r\n", buf);
	write(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    write(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    
    /* Write the responses */
    write(fd, buf, strlen(buf));
    write(fd, body, strlen(body));
}


/**
 * Send an HTTP response with the body being the contents of a file.
 * 
 * @param fd       the file descriptor to be written to
 * @param filename 
 * @param filesize 
 */
void serve_http(int fd, char *filename, int filesize) {
	FILE *src;
	char filetype[MAXLINE];
	char buf[MAXBUF];
	int bytesread;
 
 	src = fopen(filename, "r");
	if(src != NULL) {
		/* Print the response incl headers */
		getfiletype(filename, filetype);	/* determine file type */
		sprintf(buf, "HTTP/1.1 200 OK\r\n");
		sprintf(buf, "%sServer: CSERVER\r\n", buf);
		sprintf(buf, "%sConnection: close\r\n", buf);
		sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
		sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
		write(fd, buf, strlen(buf));
		printf("Response headers:\n");
		printf("%s\n", buf);

		/* HTTP response body - display file contents in browser */
		while((bytesread = fread(buf,1,MAXBUF, src)) > 0) {
			write(fd,buf,bytesread);
		}
		fclose(src);
	} else {
		/* FILE NOT FOUND */
		sprintf(buf, "HTTP/1.1 404 Not found\r\n");
		sprintf(buf, "%sServer: CSERVER\r\n", buf);
		sprintf(buf, "%sConnection: close\r\n", buf);
		sprintf(buf, "%sContent-length: %lu\r\n", buf, strlen(buf));
		write(fd, buf, strlen(buf));
		printf("Response headers:\n");
		printf("%s\n", buf);
		/* Raise HTTP 404 error */
		raise_http_err(fd, filename, "404", "Not found", "File not found or couldn't be opened.");
	}
}

