/* A UDP echo server with timeouts.
 *
 * Note that you will not need to use select and the timeout for a
 * tftp server. However, select is also useful if you want to receive
 * from multiple sockets at the same time. Read the documentation for
 * select on how to do this (Hint: Iterate with FD_ISSET()).
 */

#include <assert.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <arpa/inet.h>
#include <glib.h>

#define HTTP_GET "GET"
#define HTTP_POST "POST"
#define HTTP_HEAD "HEAD"

/**
 * This function gets the request method from the message
 * returns NULL if the request method is not supported 
 */
char * getRequestType(char * request) {
	if (strcmp(HTTP_GET, request) == 0) {
 		return HTTP_GET;
	} else if (strcmp(HTTP_POST, request) == 0) { 
		return HTTP_POST;
	} else if (strcmp(HTTP_HEAD, request) == 0) {
		return HTTP_HEAD;
	} else {
		return NULL;
	}
}

/** 
 * This function does not leak memory
 */ 
char * getRequestUrl(char * url) {
	char host[]= "http://localhost";
	strcat(host, url);
	url = strdup(host);
	return url;
}



int main(int argc, char **argv)
{
	int i = 0;
	fprintf(stdout, "Print out the params, nr of params=%d \n", argc);
	fflush(stdout);
	while (argv[i] != NULL) {
		fprintf(stdout, "argv[%d] : %s \n", i, argv[i]);
		fflush(stdout);
		i++;
	}
	int sockfd;
	struct sockaddr_in server, client;
	char message[512];
	int my_port = 0;
	my_port = atoi(argv[1]);

	/* Create and bind a UDP socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	/* Network functions need arguments in network byte order instead of
	   host byte order. The macros htonl, htons convert the values, */
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(my_port);
	bind(sockfd, (struct sockaddr *) &server, (socklen_t) sizeof(server));

	/* Before we can accept messages, we have to listen to the port. We allow one
	 * 1 connection to queue for simplicity.
	 */
	listen(sockfd, 1);

	for (;;) {
		fd_set rfds;
		struct timeval tv;
		int retval;

		/* Check whether there is data on the socket fd. */
		FD_ZERO(&rfds);
		FD_SET(sockfd, &rfds);

		/* Wait for five seconds. */
		tv.tv_sec = 5;
		tv.tv_usec = 0;
		retval = select(sockfd + 1, &rfds, NULL, NULL, &tv);

		if (retval == -1) {
			perror("select()");
		} else if (retval > 0) {
			/* Data is available, receive it. */
			assert(FD_ISSET(sockfd, &rfds));

			/* Copy to len, since recvfrom may change it. */
			socklen_t len = (socklen_t) sizeof(client);

			/* For TCP connectios, we first have to accept. */
			int connfd;
			connfd = accept(sockfd, (struct sockaddr *) &client,
							&len);

			/* Receive one byte less than declared,
			   because it will be zero-termianted
			   below. */
			ssize_t n = read(connfd, message, sizeof(message) - 1);

			/* Send the message back. */
			write(connfd, message, (size_t) n);

			/* We should close the connection. */
			shutdown(connfd, SHUT_RDWR);
			close(connfd);

			/* Zero terminate the message, otherwise
			   printf may access memory outside of the
			   string. */
			message[n] = '\0';
			/* Print the message to stdout and flush. */
			fprintf(stdout, "Received:\n%s\n", message);
			fflush(stdout);
		 
			gchar ** split = g_strsplit(message, " ", -1);
			guint size = g_strv_length(split);
				
			int i = 0;
			for (; i < size ; i++) {
				// fprintf(stdout, "%s\n", split[i]);
				// fflush(stdout);
			}
			
			char * requestType = getRequestType(split[0]);
			char * requestUrl = getRequestUrl(split[1]);	
			fprintf(stdout, "Request type  %s\n", requestType);
			fprintf(stdout, "Request url %s\n", requestUrl);
			g_strfreev(split);
			
			/* Log request from user */
			time_t now;                                        			
            time(&now);
            char buf[sizeof "2011-10-08T07:07:09Z"];
            strftime(buf, sizeof buf, "%FT%TZ", gmtime(&now));
			
			FILE *f = fopen("log.txt", "a");
			if (f == NULL) {
				fprintf(stdout, "Error when opening log file");
				fflush(stdout);
			} else {		
				fprintf(f, "%s : ", buf);
				fprintf(f, "%s:%d ", inet_ntoa(client.sin_addr), client.sin_port);
				fprintf(f, "%s\n", getRequestType(message));			
				fprintf(f, "connfd: %d\n", connfd);
			}
			fclose(f);
		} else {
			fprintf(stdout, "No message in five seconds.\n");
			fflush(stdout);
		}
	}
}
