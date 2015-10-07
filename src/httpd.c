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
#define HTML_MAX_SIZE 1024
#define URL_SIZE 50
#define PORT_SIZE 6
#define TYPE_SIZE 10
#define HEAD_SIZE 512

/**
 * This function gets the request method from the message
 * returns NULL if the request method is not supported 
 */
void getRequestType(char request[], char message[]) {
	gchar ** split = g_strsplit(message, " ", -1);
	strcat(request, split[0]);
	g_strfreev(split);
}

/** 
 * This function does not leak memory
 */ 
void getRequestUrl(char url[], char message[]) {
	gchar ** split = g_strsplit(message, " ", -1);
	strcat(url, "http://localhost");
	strcat(url, split[1]);
	g_strfreev(split);
}

void getDataField(char message[], char data[]){
	gchar ** split = g_strsplit(message, "\r\n\r\n", -1);
	if(split[1] != NULL){
		strcat(data, split[1]);
	} 
	g_strfreev(split);
}

void headGenerator(char head[], int contentLength){
	char c_contentLength[8];
	time_t now;                                        			
    time(&now);
    char buf[sizeof "2011-10-08T07:07:09Z"];
    strftime(buf, sizeof buf, "%FT%TZ", gmtime(&now));
	// Write the content length as string into headLength
	snprintf(c_contentLength, 8, "%d", contentLength); 
	// \r\n is carriage return + line feed
	strcat(head, "HTTP/1.1 200 OK\r\n");
	strcat(head, "Date: ");
	strcat(head, buf);
	strcat(head, "\r\n");
	strcat(head, "Server: Angel server 1.0\r\n");
	strcat(head, "Content-Type: text/html\r\n");
	strcat(head, "Content-length: ");
	strcat(head, c_contentLength);
	strcat(head, "\r\n\r\n");
}
void headHandler(int connfd){
	char head[HEAD_SIZE];
	memset(&head, 0, HEAD_SIZE);
	headGenerator(head, 0);	
	write(connfd, head, (size_t) sizeof(head));
}

void postHandler(int connfd, char url[], int port, char IP[], char message[]){
	char html[HTML_MAX_SIZE];
	char data[HTML_MAX_SIZE];
	char portBuff[PORT_SIZE];
	char head[HTML_MAX_SIZE];
	memset(&html, 0, HTML_MAX_SIZE);
	memset(&head, 0, HTML_MAX_SIZE);
	memset(&portBuff, 0, HTML_MAX_SIZE);
	memset(&data, 0, HTML_MAX_SIZE);
	snprintf(portBuff, PORT_SIZE, "%d", port);
	getDataField(message,data);
	strcat(html, "<!DOCTYPE html>\n<html>\n\t<body>");
	strcat(html, "\n\t\t<p>\n\t\t\t");
	strcat(html, portBuff);
	strcat(html, "<br>");	
	strcat(html, "\n\t\t\tClientID: ");
	strcat(html, IP);
	strcat(html, "<br>\n\t\t\t");	
	strcat(html, data);
	strcat(html, "\n\t\t</p>\n\t</body>\n</html>\n");
	headGenerator(head, strlen(html));
	strcat(head, html);
	write(connfd, head, (size_t) sizeof(head));
}

void getQueryString(char url[], char queryString[]){
	gchar ** split = g_strsplit(url, "?", -1);
	strcat(queryString, split[1]);
	g_strfreev(split);

}

void getParameters(char parameter[], char value[], char queryString[]){
	gchar ** split = g_strsplit(queryString, "=", -1);
	strcat(parameter, split[0]);
	strcat(value, split[1]);
	g_strfreev(split);
}
/**
 *	This function handles GET request. It constructs the html string and sends it 
 *	to the client.
 */
void getHandler(int connfd, char url[], int port, char IP[]){
	char html[HTML_MAX_SIZE];
	char portBuff[PORT_SIZE];
	char head[HEAD_SIZE];
	char queryString[URL_SIZE];
	char parameter[URL_SIZE];
	char value[URL_SIZE];
	memset(&html, 0, HTML_MAX_SIZE);
	memset(&portBuff, 0, PORT_SIZE);
	memset(&head, 0, HEAD_SIZE);
	memset(&queryString, 0, URL_SIZE);
	memset(&parameter, 0, URL_SIZE);
	memset(&value, 0, URL_SIZE);
	// Check for a query string
	snprintf(portBuff, PORT_SIZE, "%d", port);

	// Generate the html
	if(strchr(url, '?')){
		getQueryString(url, queryString);
		getParameters(parameter, value, queryString);
		// if the string contains a query, get and inject the param and value
		// to the html document
		if(strcmp("bg", parameter) == 0){
			strcat(html, "<!DOCTYPE html>\n<html>\n\t<body");
			strcat(html, " style='background-color:");
			strcat(html, value);			
			strcat(html,"'>");
			strcat(html, "\n\t\t<p>\n\t\t\t");
			strcat(html, portBuff);
			strcat(html, "<br>\n\t\t\t");	
			strcat(html, IP);
			strcat(html, "<br>");	

		} else{
			strcat(html, "<!DOCTYPE html>\n<html>\n\t<body>");
			strcat(html, "\n\t\t<p>\n\t\t\t");
			strcat(html, portBuff);
			strcat(html, "<br>");	
			strcat(html, "\n\t\t\tClientID: ");
			strcat(html, IP);
			strcat(html, "<br>\n\t\t\t");	
			strcat(html, parameter);
			strcat(html, " = ");
			strcat(html, value);
		}
	} else{
		strcat(html, "<!DOCTYPE html>\n<html>\n\t<body>\n\t\t<h1>");
		strcat(html, url);
		strcat(html, "</h1>");
		strcat(html, "\n\t\t<p>\n\t\t\tPort: ");
		strcat(html, portBuff);
		strcat(html, "<br>");	
		strcat(html, "\n\t\t\tClientID: ");
		strcat(html, IP);
		strcat(html, "<br>");	
	}
	strcat(html, "\n\t\t</p>\n\t</body>\n</html>\n");
	headGenerator(head, strlen(html));
	strcat(head, html);

	write(connfd, head, (size_t) sizeof(head));
}
void typeHandler(int connfd, char message[], FILE *f, struct sockaddr_in client){

	char requestType[TYPE_SIZE];
	char requestUrl[URL_SIZE];
	memset(&requestType, 0, TYPE_SIZE);
	memset(&requestUrl, 0, URL_SIZE);
	//char htmlDoc[HTML_MAX_SIZE];
	getRequestType(requestType, message);
	getRequestUrl(requestUrl, message);	
	fprintf(stdout, "Request type  %s\n", requestType);
	fprintf(stdout, "Request url %s\n", requestUrl);
	fflush(stdout);

	if (strcmp(HTTP_GET, requestType) == 0) {
		getHandler(connfd, requestUrl, client.sin_port, inet_ntoa(client.sin_addr));
	} else if (strcmp(HTTP_POST, requestType) == 0) { 
	postHandler(connfd, requestUrl, client.sin_port, inet_ntoa(client.sin_addr), message);
	//the data section of the request is the text to inject into the html
	// TODO: we have to get the data, delimiter
	} else if (strcmp(HTTP_HEAD, requestType) == 0) {
		headHandler(connfd);
	} else {
		fprintf(stdout, "Request type invalid!\n");
		fflush(stdout);
	}

	/* Log request from user */
	time_t now;                                        			
    time(&now);
    char buf[sizeof "2011-10-08T07:07:09Z"];
    strftime(buf, sizeof buf, "%FT%TZ", gmtime(&now));

	//FILE *f = fopen("log.txt", "a");
	if (f == NULL) {
		fprintf(stdout, "Error when opening log file");
		fflush(stdout);
	} else {		
		fprintf(f, "%s : ", buf);
		fprintf(f, "%s:%d ", inet_ntoa(client.sin_addr), client.sin_port);
		fprintf(f, "%s\n", requestType);			
		fprintf(f, "connfd: %d\n", connfd);
	}
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

			/* Zero terminate the message, otherwise
			   printf may access memory outside of the
			   string. */
			message[n] = '\0';
			/* Print the message to stdout and flush. */
			fprintf(stdout, "Received:\n%s\n", message);
			fflush(stdout);

			FILE *f = fopen("httpd.log", "a");
			typeHandler(connfd, message, f, client);		 
			// close the connection
			shutdown(connfd, SHUT_RDWR);
			close(connfd);	
			fclose(f);
		} else {
			fprintf(stdout, "No message in five seconds.\n");
			fflush(stdout);
		}
	}
}
