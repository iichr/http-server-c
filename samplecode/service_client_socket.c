#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>

#include <memory.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <assert.h>

#include "service_client_socket.h"

/* why can I not use const size_t here? */
#define buffer_size 8096


struct {
	char *ext;
	char *filetype;
} extensions [] = {
	{"gif", "image/gif" },
	{"jpg", "image/jpg" },
	{"jpeg","image/jpeg"},
	{"png", "image/png" },
	{"ico", "image/ico" },
	{"zip", "image/zip" },
	{"gz",  "image/gz"  },
	{"tar", "image/tar" },
	{"htm", "text/html" },
	{"html","text/html" },
	{"pdf", "application/pdf"},
	{0,0} };


int
service_client_socket (const int s, const char *const tag) {
	long i, len;
	int buflen, fd;
	char *file_type_mime;
	
	char buffer[buffer_size];
	size_t bytes;
	size_t filebytes;

	printf ("new connection from %s\n", tag);

	/* repeatedly read a buffer load of bytes, leaving room for the
	terminating NUL we want to add to make using printf() possible */
	while ((bytes = read (s, buffer, buffer_size - 1)) > 0) {
		/* this code is not quite complete: a write can in this context be
		partial and return 0<x<bytes.  realistically you don't need to
		deal with this case unless you are writing multiple megabytes */
		//if (write (s, buffer, bytes) != bytes) {
		//	perror ("write");
		//	return -1;
		//}
		/* NUL-terminal the string */
		buffer[bytes] = '\0';

		/* remove \n and \r characters */
		for(i=0;i<bytes;i++) {
			if(buffer[i] == '\r' || buffer[i] == '\n') {
				buffer[i] = '*';
			}
		}
		//if (strcasecmp(buffer, "GET")) {
		if(strncmp(buffer,"GET ",4)) {
			write(s, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested GET, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n",271);
		}
		

		// something to do with removeing after empty space
		for(i=4; i<buffer_size; i++) {
			if(buffer[i] == ' ') {
				buffer[i] = 0;
				break;
			}
		}

		// empty should point to index
		if( !strncmp(&buffer[0],"GET /\0",6) || !strncmp(&buffer[0],"get /\0",6) ) {
			strcpy(buffer,"GET /index.html");
		}

		// extensions
		buflen=strlen(buffer);
		file_type_mime = (char *)0;
		for(i=0;extensions[i].ext != 0;i++) {
			len = strlen(extensions[i].ext);
			if( !strncmp(&buffer[buflen-len], extensions[i].ext, len)) {
				file_type_mime =extensions[i].filetype;
				break;
			}
		}

		if(file_type_mime == 0) {
			write(s, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n",271);
		}


		// if(!strncmp(&buffer[buflen-len], "jpg", len)) {
		// 	file_type_mime = "image/jpg";
		// }
		
		
		if((fd = open(&buffer[5],O_RDONLY)) == -1) {
			write(s, "HTTP/1.1 404 Not Found\nContent-Length: 136\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>\n",224);
		}

		// get file length
		len = (long)lseek(fd, (off_t)0, SEEK_END);
		(void)lseek(fd, (off_t)0, SEEK_SET);
		// send response header
		(void)sprintf(buffer,"HTTP/1.1 200 OK\r\nServer: CSSERVER\r\nContent-Length: %ld\r\nConnection: close\r\nContent-Type: %s\r\n\r\n", len, file_type_mime); /* Header + a blank line */
		(void)write(s,buffer,strlen(buffer));

		while ((filebytes = read(fd, buffer, buffer_size - 1)) > 0 ) {
			(void)write(s,buffer,filebytes);
		}
		close(fd);
		



		/* special case for tidy printing: if the last two characters are
		\r\n or the last character is \n, zap them so that the newline
		following the quotes is the only one. */
		if (bytes >= 1 && buffer[bytes - 1] == '\n') {
			if (bytes >= 2 && buffer[bytes - 2] == '\r') {
				strcpy (buffer + bytes - 2, "..");
			} else {
				strcpy (buffer + bytes - 1, ".");
			}
		}
		
#if (__SIZE_WIDTH__ == 64 || __SIZEOF_POINTER__ == 8)
	printf ("echoed %ld bytes back to %s, \"%s\"\n", bytes, tag, buffer);
#else
	printf ("echoed %d bytes back to %s, \"%s\"\n", bytes, tag, buffer);
#endif
	}
	/* bytes == 0: orderly close; bytes < 0: something went wrong */
	if (bytes != 0) {
		perror ("read");
		write(s, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n",271);
		return -1;
	}
	printf ("connection from %s closed\n", tag);
	close (s);
	return 0;
}

