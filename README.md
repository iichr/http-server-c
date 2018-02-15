# Documentation

[TOC]

## 1. File structure

The multithreaded server is composed of two C files and one header file, located in src. In
addition to that, there is a few html pages along with files they are dependent on, such as
images to be displayed; the latter are located in `src/assets`.

The main method and server code are located in the `server.c` file.
Most of the helper functions, to do with I/O and HTTP request handling are located in `helper-functions.c`

## 2. How to build

There is a makefile provided. Run with `make`.

Failing that, the following could be used:

```bash
 clang -D_POSIX_SOURCE -Wall -Werror -pedantic -std=c99 -D_GNU_SOURCE -pthread server.c helper-functions.c -o server
```



## 3. How to run

After compilation in a terminal window specify the **port number** you wish the server to be on:

```bash
./server PORTNUMBER &
```



## 4. Code structure

### 4.1 helper_functions.h and helper_functions.c

`helper_functions.h` defines a struct called `fd_internalbuf`, which associates a file
descriptor (used to access a file or other input/output resource, such as a _socket_) with an
internal buffer `ibuf` of size BUFSIZE. Along that the unread bytes are stored in an integer and there is a pointer `p_buf` to the next unread byte in the internal buffer.

The helper functions can broadly be split into two categories - **general I/O handling** and **HTTP specific**.

#### 4.1.1 General I/O handling

- error function that displays messages from system calls to stderr accompanied by an error number;

- ```c
  void init_internalbuf(fd_internalbuf *rp, int fd)
  ```

  associates a file descriptor(in our case a socket) with an internal buffer from the `fd_internalbuf` struct;


- `readb` a buffered version of the read() function. When `read()` is called with a request to read n bytes there are a number of unread bytes in the read buffer. If the buffer is empty, then it is replenished with a call to read. Once the buffer is nonempty, `read()` copies the minimum of `n` and `rp->unread` bytes from the read buffer to the user provided one and returns the number of bytes copied;
- `readline` reads the next text line from a file descriptor with internal buffer struct (including the terminating newline character). It is then copied to a user-defined memory location, and terminated with \0. Text lines that exceed maxlen-1 bytes are truncated and terminated with a NULL character. Returns number of bytes read. 
  It is used in parsing HTTP request headers.

#### 4.1.2 HTTP specific

- ```c
  void read_request_headers(fd_internalbuf *rp)
  ```

  reads HTTP requests in the following format and prints them:  1) first a request line `GET/index.html HTTP/1.1`  2) General and request headers such as *Connection, Host, Accept, Content-Length* etc.  3) `\r\n` to signal separation from the message body.

- ```C
  void parse_uri(char *uri, char *filename)
  ```

  convert a given URI (see RFC 3986) into a path on the server uses the current working
  directory if the URI ends with / then the client is redirected to the `index.html` page.

- `getfiletype` - obtain a MIME filetype given a filename. **The server supports .html,**
  **.jpeg, .jpg, .pdf and plain text files**. For reference check out https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Complete_list_of_MIME_types

- ```c
  void raise_http_err(int fd, char *err, char *error_code, 
                      char *error_title, char *error_descr)
  ```

  returns a given error message with an error code to the client. Handles building an HTML response body along with the HTTP response itself. Example usage:
  `raise_http_err(*fd, filename, "404", "Not found", "File not found or couldn't be opened");`

- ```c
  void serve_http(int fd, char *filename, int filesize) 
  ```

  Sends an HTTP response with the body being the file contents. If successful response is
  200 OK followed by the necessary headers. If the file is not found/can't be opened, a 404 response code is displayed instead with the body of the message rendered by the
  `raise_http_err` function.



### 4.2 Server

#### 4.2.1 main function

Retrieves the port number, checks if valid. A socket is then created, and bound to the server's
address. An accept() call is called in an infinite-loop which awaits incoming requests. An
incoming request is duplicated into a different socket to keep the main socket available for
more connections. Each incoming request should be dealt with in separate thread. Each thread is set in a detached state and `pthread_create` is then called with our `connection_handler` function and a pointer to the socket we called accept() on.

#### 4.2.2 static void *connection_handler(void *args)

Each thread calls this function in order to handle one HTTP transaction, which includes an HTTP response containing the contents of a local file.

We use the pointer to the socket, which is the result of the accept call in main() and initialise
an internal buffer to it. Char arrays are declared to store the requests, method, uri, version and filename.

From the first line of the HTTP request we extract the method, uri and version, and we check if the method is a GET or POST ( **for our purposes here they are both treated the same way, as we don't have an underlying database** ). If it is not, a 501 error code is raised.

Next the remaining request header are read and the uri is parsed and stored in a filename
buffer. We obtain file information - the file size, by calling `stat()` on the filename and store
the information in a `stat struct`. `serve_http` is then called to serve the content requested.
It is deemed to be a critical section so it is surrounded by a lock, which is released upon exit. We close and free the pointer to the file descriptor (socket) at the end to avoid memory leaks.

## 5. HTML files

`index.html` is the main landing page, containing links that redirect to `images.html` , which
contains 4 *.jpg* images in order to test different file-type handling, plus `sad.html`. On the
index page there is a also a link to a `factsheet.pdf` for testing how a larger static file is
served.

## 6. Testing

In `test.sh` we use _curl_ to call the index page and the factsheet.pdf file 500 times in
parallel.
