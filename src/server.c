#include "helper-functions.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <netinet/in.h>
#include <ctype.h>
#include <assert.h>
#include <sys/stat.h>

static void *connection_handler(void *); /* the function that handles a connection */
pthread_mutex_t lock; /* a lock for accesing files */
char *myname = "unknown";


int main(int argc, char *argv[]) {
	int port, sock, result;
	char *endp;
	struct sockaddr_in6 my_address;

	assert (argv[0] && *argv[0]);
	myname = argv[0];

	if (argc != 2) {
		fprintf (stderr, "%s: usage is %s port\n", myname, myname);
		exit (1);
	}

	/* convert a string to a number, endp gets a pointer to the last character converted */
	port = strtol (argv[1], &endp, 10);
	if (*endp != '\0') {
	fprintf (stderr, "%s: %s is not a number\n", myname, argv[1]);
	exit (1);
	}

	/* less than 1024 you need to be root, >65535 isn't valid */
	if (port < 1024 || port > 65535) {
		fprintf (stderr, "%s: %d should be in range 1024..65535 to be usable\n",
		myname, port);
		exit (1);
	}

	fprintf (stderr, "binding  to port %d\n", port);

	memset (&my_address, '\0', sizeof (my_address));
	my_address.sin6_family = AF_INET6; /* this is an ipv6 address */
	my_address.sin6_addr = in6addr_any;  /* bind to all interfaces */
	my_address.sin6_port = htons (port); /* network order */

	sock = socket(PF_INET6, SOCK_STREAM, 0);
	if(sock < 0) {
		fprintf (stderr, "Socket error\n");
		return -1;
	}

	const int one = 1;
	if(setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof (one)) != 0) {
		fprintf (stderr, "Set socket options error, but we carry on.\n");
	}

	if (bind (sock, (struct sockaddr *) &my_address, sizeof (my_address)) != 0) {
		fprintf (stderr, "Bind error.\n");
		return -1;
	}

	if (listen (sock, 5) != 0) {
		fprintf (stderr, "Listen error.\n");
		return -1;
	}

	// now we have s from above
	int client;
	struct sockaddr_in6 their_address;
	socklen_t their_address_size = sizeof (their_address);
	
	while(1) {
		pthread_t t;
		int *newsockfd; /* allocate memory for each instance to avoid race condition */
		pthread_attr_t pthread_attr; /* attributes for newly created thread */

		newsockfd  = malloc (sizeof (int));
		if (!newsockfd) {
			error ("Memory allocation failed!\n");
			exit (1);
		}

		/* waiting for connections and thread creation */
		*newsockfd = accept( sock, 
			  (struct sockaddr *) &their_address, 
			  &their_address_size);
		
		if (*newsockfd < 0) {
			error ("ERROR on accept");
		}

		/* create separate thread for processing */
		if (pthread_attr_init (&pthread_attr)) {
			error ("Creating initial thread attributes failed!\n");
			exit (1);
		}

		if (pthread_attr_setdetachstate (&pthread_attr, PTHREAD_CREATE_DETACHED)) {
			error ("Setting thread attributes failed!\n");
			exit (1);
		}
	 
		result = pthread_create (&t, &pthread_attr, connection_handler, (void *) newsockfd);
		
		if (result != 0) {
			error ("Thread creation failed!\n");
			exit (1);
		}
	}

	if(client < 0) {
		error ("Accept error.\n");
		return -1;
	}
	close(sock);
	return 0;
}


/**
 * Handles one HTTP transaction, including an HTTP response containing the contents of 
 * a local file.
 * @param  args socket
 */
static void *connection_handler(void *args) {
	
	int *fd = (int*)args;
	
	/* for extracting file info */
	struct stat file_info;	/* the struct where stat() will store file information */

	char buf[MAXLINE];	/* buffer */
	char method[MAXSMALL];	/* for storing the method - GET or POST */
	char uri[MAXLINE];	/* for storing the URI */
	char version[MAXSMALL]; /* for storing the version - HTTP 1.x/ */
	char filename[MAXLINE];	/* for storing the filename */
	fd_internalbuf fdbuf;	/* internal buffer */

	init_internalbuf(&fdbuf, *fd);	/* assign an internal buffer to fd by using struct */
	
	if(!readline(&fdbuf, buf, MAXLINE)) {	/* read the first request line */
		error("Error in readline");
	}
	printf("%s", buf);	/* print first line */
	/* Obtain the HTTP request method, uri and version */
	sscanf(buf, "%s %s %s", method, uri, version);	/* read formatted input from string */

	/* check if method is GET/POST, only supported ones so far */
	if (strcasecmp(method, "GET") && strcasecmp(method, "POST") ) { /* returns 0 if they are one of GET or POST */
		raise_http_err(*fd, method, "501", "Not implemented", "This method is not yet supported");
	}

	read_request_headers(&fdbuf);	/*read request headers from internal buffer */
	parse_uri(uri, filename);	/* parse uri and store in filename buffer */

	/* Obtain file information using the stat function annd store in file_info */
	if(stat(filename, &file_info) < 0) { /* check if successful i.e returns 0 */
		raise_http_err(*fd, filename, "404", "Not found", "File not found or couldn't be opened");
	}

	/* Get file size from the stat struct */
	int filesize = file_info.st_size;

	pthread_mutex_lock(&lock);
	serve_http(*fd, filename, filesize);	/* serve file requested */
	pthread_mutex_unlock(&lock);
	
	close (*fd); /* important to avoid memory leak */  
	free (fd);
	pthread_exit (NULL);
}

