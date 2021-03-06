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
#include <stdbool.h>

#define HTTP_GET "GET"
#define HTTP_POST "POST"
#define HTTP_HEAD "HEAD"

#define HTML_MAX_SIZE 1024
#define URL_SIZE 256
#define PORT_SIZE 6
#define TYPE_SIZE 12
#define HEAD_MAX_SIZE 1024
#define SEGMENT_MAX_SIZE 2048
#define PARAMETER 24
#define VALUE 16
#define COOKIE_SIZE 24
#define MAX_CLIENTS 5
#define CONNECTION_TIME 10

struct ClientInfo {
	int connfd; // if connfd is set to -1 then the client is inactive
	time_t time;
	int keepAlive;
	struct sockaddr_in socket; 
};

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
 * This function gets the URL requested by the client.
 */ 
void getRequestUrl(char url[], char message[]) {
	gchar ** split = g_strsplit(message, " ", -1);
	strcat(url, "http://localhost");
	if (split[1] != NULL) {
		strcat(url, split[1]);
	}
	g_strfreev(split);
}

/**
 * This function splits a message and gets the head from it.
 */
void getHeadField(char message[], char head[]){
	gchar ** split = g_strsplit(message, "\r\n\r\n", -1);
	if(split[1] != NULL){
		strcat(head, split[0]);
	} 
	g_strfreev(split);
}

/**
 * This funstion splits a message and gets the data from it.
 */
void getDataField(char message[], char data[]){
	gchar ** split = g_strsplit(message, "\r\n\r\n", -1);
	if(split[1] != NULL){
		strcat(data, split[1]);
	} 
	g_strfreev(split);
}

/**
 * This function splits the URI and gets the query part of it.
 */
void getQueryString(char url[], char queryString[]){
	gchar ** split = g_strsplit(url, "?", -1);
	strcat(queryString, split[1]);
	g_strfreev(split);
}

/**
 * This function gets the parameter and it's value from a query string.
 */
void getParameters(char parameters[PARAMETER][PARAMETER], char queryString[]){
	gchar ** splitAmpersand = g_strsplit(queryString, "&", -1);
	int i = 0;
	while(splitAmpersand[i] != NULL){
		gchar ** keyValue = g_strsplit(splitAmpersand[i], "=", -1);
		if(keyValue[0] == NULL || keyValue[1] == NULL){
			break;
		} else {
			strcpy(parameters[i], splitAmpersand[i]);
		}
		i++;
	}		
	g_strfreev(splitAmpersand);
}

/**
 * This function generates a simple header.
 */
void headGenerator(char head[], char cookie[], int contentLength){
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
	// set the cookie
	if (cookie[0] != '\0') {
		strcat(head, "\r\n");
		strcat(head, "Set-Cookie: ");
		strcat(head, cookie);
	}
	/*The string \r\n\r\n distinguishes between the head and data field.*/
	strcat(head, "\r\n\r\n");
}

/**
 * Html is the array to append the html code 
 * color is a string like "bg=red"
 */
void generateHtmlBody(char html[], char color[]) {
	if (color[0] != '\0') {
		strcat(html, "<!DOCTYPE html>\n<html>\n\t<body");
		strcat(html, " style='background-color:");

		gchar ** keyValue = g_strsplit(color, "=", -1);
		if(keyValue[0] == NULL || keyValue[1] == NULL) {
			strcat(html, "white");
		} else {
			strcat(html, keyValue[1]);
		}		
		g_strfreev(keyValue);
		strcat(html,"'>");
	} else {
		strcat(html, "<!DOCTYPE html>\n<html>\n\t<body>");
	}
}

/**
 * appends the html containg the url, ip address and port number of the request into
 * the html.
 */ 
void generateHtmlRequestInfo(char html[], char url[], char ip[], int port) {
	strcat(html, "\n\t\t<h1>");
	strcat(html, url);
	strcat(html, "</h1>");
	strcat(html, "\n\t\t<p>");
	strcat(html, "ClientIP: ");
	strcat(html, ip);
	strcat(html, " Port: ");
 	// convert the port number to a string format	
	char portBuff[PORT_SIZE];
	memset(&portBuff, 0, sizeof(portBuff));
	snprintf(portBuff, PORT_SIZE, "%d", port);
	strcat(html, portBuff);
	strcat(html, "</p>");	
}

void generateHtmlData(char html[], char data[]) {
	strcat(html, "\n\t\t<p>");
	strcat(html, data);
	strcat(html, "</p>");
}

void getBackgroundColor(char parameters[PARAMETER][PARAMETER], char bg[]){
	int i = 0;
	while(parameters[i][0]) {
		gchar ** key = g_strsplit(parameters[i], "=", -1);
		if(key[0] && strcmp("bg", key[0]) == 0) {
			strcat(bg, parameters[i]);
		}
		g_strfreev(key);
		i++;
	}
}

/*
 * This function generates the html to list the query parameters sent from the client
 * examble : "bg=red" or "var=foo" 
 */
void generateHtmlParameters(char html[], char parameters[PARAMETER][PARAMETER]) {
	strcat(html, "\n\t\t<ul>");
	int j = 0;
	while(parameters[j][0]) {
		gchar ** keyValue = g_strsplit(parameters[j], "=", -1);
		if (keyValue[0] && keyValue[1]) { 
			strcat(html, "\n\t\t\t<li>");
			strcat(html, keyValue[0]);
			strcat(html, " = ");
			strcat(html, keyValue[1]);
			strcat(html, "</li>");
		}	
		g_strfreev(keyValue);
		j++;
	}
	strcat(html, "\n\t\t</ul>");
}

/**
 * This function handles request of type HEAD. 
 */
void headHandler(int connfd, char bg[]){
	char head[HEAD_MAX_SIZE];
	memset(&head, 0, HEAD_MAX_SIZE);
	headGenerator(head, bg, 0);	
	write(connfd, head, (size_t) sizeof(head));
}

/**
 * This function handles requests of type POST.
 */
void postHandler(int connfd, char url[], char bg[],  int port, char IP[], char message[]){
	char html[HTML_MAX_SIZE];
	char data[HTML_MAX_SIZE];
	char head[HTML_MAX_SIZE];
	char queryString[URL_SIZE];
	char parameters[PARAMETER][PARAMETER];
	char segment[SEGMENT_MAX_SIZE];
	memset(&html, 0, HTML_MAX_SIZE);
	memset(&head, 0, HEAD_MAX_SIZE);
	memset(&queryString, 0, URL_SIZE);
	memset(&data, 0, HTML_MAX_SIZE);
	memset(&parameters, 0, PARAMETER * sizeof(char *));
	memset(&segment, 0, SEGMENT_MAX_SIZE);
	
	if (strchr(url, '?')) {
		getQueryString(url, queryString);
		getParameters(parameters, queryString);
	}
	generateHtmlBody(html, bg);
	

	generateHtmlRequestInfo(html, url, IP, port);	
	if (strchr(url, '?')) {
		generateHtmlParameters(html, parameters);
	}
	getDataField(message,data);
	generateHtmlData(html, data);
	strcat(html, "\n\t</body>\n</html>\n");
	headGenerator(head, bg, strlen(html));	
	strcat(head, html);
	strcat(segment, head);
	write(connfd, segment, (size_t) sizeof(segment));
}

/**
 *	This function handles GET request. It constructs the html string and sends it 
 *	to the client. The html varies depending on if there is a query in the URL, 
 *	if it is to set a background color of the page or if it is just a regular URL.
 */
void getHandler(int connfd, char url[], char bg[], int port, char IP[]){
	char html[HTML_MAX_SIZE];
	char head[HEAD_MAX_SIZE];
	char parameters[PARAMETER][PARAMETER];
	char queryString[URL_SIZE];
	char segment[SEGMENT_MAX_SIZE];
	memset(&html, 0, HTML_MAX_SIZE);
	memset(&queryString, 0, URL_SIZE);
	memset(&head, 0, HEAD_MAX_SIZE);
	memset(&parameters, 0, PARAMETER * sizeof(char *));
	memset(&segment, 0, SEGMENT_MAX_SIZE);
	
	// Generate the html
	if (strchr(url, '?')) {
		getQueryString(url, queryString);
		getParameters(parameters, queryString);
	}

	generateHtmlBody(html, bg);
	generateHtmlRequestInfo(html, url, IP, port);
	if (strchr(url, '?')) {
		generateHtmlParameters(html, parameters);
	}
	strcat(html, "\n\t</body>\n</html>\n");

	headGenerator(head, bg, strlen(html));
	
	strcat(head, html);
	strcat(segment, head);
	write(connfd, segment, (size_t) sizeof(segment));
}

/**
 * This function checks what type of request is received from the client.
 * It handles the types GET,POST and HEAD. For each type, the function calls
 * the appropriate type handler, creates a timestamp and logs to the httpd.log file.
 */
void typeHandler(int connfd, char message[], struct sockaddr_in client){

	char requestType[TYPE_SIZE];
	char url[URL_SIZE];
	char queryString[URL_SIZE];
	char parameters[PARAMETER][PARAMETER];
	char head[HEAD_MAX_SIZE];
	memset(&requestType, 0, TYPE_SIZE);
	memset(&url, 0, URL_SIZE);
	memset(&queryString, 0, URL_SIZE);
	memset(&parameters, 0, PARAMETER * sizeof(char *));
	memset(&head, 0, sizeof(head));
	getRequestType(requestType, message);
	getRequestUrl(url, message);	
	
	fprintf(stdout, "Message is: \n%s\n", message);
	fflush(stdout);
	char bg[20];
	memset(&bg, 0, sizeof(bg));
	/* Check if the query string contains the bg varible, if so get the value for the background color */
	if (strchr(url, '?')) {
		getQueryString(url, queryString);
		getParameters(parameters, queryString);
		getBackgroundColor(parameters, bg);
	}  
	/* If the bg varible is not in the query string it is maybe set in the cookie */
	if (bg[0] == '\0') {
		getHeadField(message, head);
		fprintf(stdout, "Head is: %s\n", head);
		fflush(stdout);
		gchar ** split = g_strsplit(head, "Cookie: ", -1);
		if(split[1] != NULL){
			strcat(bg, split[1]);
		} else {
			bg[0] = '\0';
		}
		g_strfreev(split);
	}

	fprintf(stdout, "requestType: :%s:\n", requestType);
	fprintf(stdout, "REQUEST MESSAGE:\n%s\n\n", message);
	fflush(stdout);

	if (strcmp(HTTP_GET, requestType) == 0) {
		/* Handle get reqest from client */
		getHandler(connfd, url, bg, client.sin_port, inet_ntoa(client.sin_addr));
	} else if (strcmp(HTTP_POST, requestType) == 0) { 
		/* Handle post reqest from client */
		postHandler(connfd, url, bg, client.sin_port, inet_ntoa(client.sin_addr), message);
	} else if (strcmp(HTTP_HEAD, requestType) == 0) {
		/* Handle head reqest from client */
		headHandler(connfd, bg);
	} else {
		fprintf(stdout, "Request type invalid!\n");
		fflush(stdout);
	}

	/* Get the current time on ISO 8601 format */
	time_t now;                                        			
    time(&now);
    char buf[sizeof "2011-10-08T07:07:09Z"];
    strftime(buf, sizeof buf, "%FT%TZ", gmtime(&now));

	/* Log request from user */
	FILE *f = fopen("log.txt", "a");
	if (f == NULL) {
		fprintf(stdout, "Error when opening log file");
		fflush(stdout);
	} else {		
		fprintf(f, "%s : ", buf);
		fprintf(f, "%s:%d ", inet_ntoa(client.sin_addr), client.sin_port);
		fprintf(f, "%s ", requestType);			
		fprintf(f, "%s : ", url);
		fprintf(f, "200 OK \n");
	}
	fclose(f);
}

/*
 * This function checks if the requst header of the client contains the subsrings "keep-alive"
 * or "HTTP/1.1" indicating that he is requesting a persistent connection
 * */
int getPersistentConnection(char message[]) {
	char head[HEAD_MAX_SIZE];
	memset(&head, 0, sizeof(head));
	getHeadField(message, head);
	if (strstr(head, "keep-alive") != NULL || strstr(head, "HTTP/1.1") != NULL) {
		return 1;
	} 
	return 0;
}

/**
 * Main function, sets up and tears down a TCP connection.
 */
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

	int sockfd, highestConnfd;
	struct sockaddr_in server, client;
	char message[512];
	int my_port = 0;
	my_port = atoi(argv[1]);

	/* Create and bind a UDP socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	highestConnfd = sockfd;

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
	listen(sockfd, MAX_CLIENTS);

	/* Create an array of connecting clients */
	struct ClientInfo clients[MAX_CLIENTS];
	int clientIndex = 0;	
	for (; clientIndex < MAX_CLIENTS; clientIndex++) {
		clients[clientIndex].connfd = -1;
	}

	for (;;) {
		fd_set rfds;
		struct timeval tv;
		int retval;

		/* Check whether there is data on the socket fd. */
		FD_ZERO(&rfds);
		highestConnfd = sockfd;
		FD_SET(sockfd, &rfds);
		int ci = 0;
		for (; ci < MAX_CLIENTS; ci++) {
			if (clients[ci].connfd > highestConnfd) {
				highestConnfd = clients[ci].connfd; // Update the highest connfd
			} 
			if (clients[ci].connfd != -1) {
				FD_SET(clients[ci].connfd, &rfds);
			}
		}		

		/* Wait for five seconds. */
		tv.tv_sec = 5;
		tv.tv_usec = 0;

		retval = select(highestConnfd + 1, &rfds, NULL, NULL, &tv);
	
		if (retval == -1) {
			perror("select()");
		} else if (retval > 0) {
			
			/* Check if we have a new connecting client */
			if (FD_ISSET(sockfd, &rfds)) {
				socklen_t len = (socklen_t) sizeof(client);				
				int connfd = accept(sockfd, (struct sockaddr *) &client, &len);
				/* Check if there is space for a new client */
				int foundSpaceAtIndex = -1;
				int ci = 0; // client index 
				for(; ci < MAX_CLIENTS; ci++) {
					if (clients[ci].connfd == -1) {
						/* set the client in the empty spot */
						clients[ci].connfd = connfd;
						time_t now;
						clients[ci].time = time(&now);
						clients[ci].socket = client;
						foundSpaceAtIndex = ci;
						break;
					}
				}
				
				/* If there is not space we close on the client */
				if (foundSpaceAtIndex == -1) { 
					fprintf(stdout, "No space for more clients\n");
					fflush(stdout);
					shutdown(connfd, SHUT_RDWR);
					close(connfd);	
				} else {
					fprintf(stdout, "Waiting for client read: \n");
					fflush(stdout);
					memset(&message, 0, sizeof(message));
					ssize_t n = read(connfd, message, sizeof(message) - 1);
					message[n] = '\0';
					fprintf(stdout, "Message from new clinet\n%s\n", message);
					fflush(stdout);
					if (strlen(message) == 0) {
						fprintf(stdout, "Messeage is empty, closing connection...\n");
						fflush(stdout);
						close(connfd);	
						clients[foundSpaceAtIndex].connfd = -1;
					} else {
						typeHandler(connfd, message, client); // handle client's request
						/* Check if the clients wants to keep the connection alive */
						if (getPersistentConnection(message) == 1) {
							fprintf(stdout, "Keep alive\n");
							fflush(stdout);
							time_t now;
							clients[foundSpaceAtIndex].time = time(&now);
						} else {
							fprintf(stdout, "close\n");
							fflush(stdout);
							close(connfd);	
							clients[foundSpaceAtIndex].connfd = -1;
						}
					}
				}
			}
 
			/* Go throw all connected clients and handle their request */
			int ci = 0; // client index 
			for (; ci < MAX_CLIENTS; ci++) {
				/* Check if current client is active and is sending a request */
				if (clients[ci].connfd != -1 && FD_ISSET(clients[ci].connfd, &rfds)) {
					memset(&message, 0, sizeof(message));
					ssize_t n = read(clients[ci].connfd, message, sizeof(message) - 1);
					message[n] = '\0';
					if (strlen(message) == 0) {
						close(clients[ci].connfd);	
						clients[ci].connfd = -1;
 					} else {
						typeHandler(clients[ci].connfd, message, client); // handle client's request
						/* Check if the clients wants to keep the connection alive */
						if (getPersistentConnection(message) == 1) {
							time_t now;
							clients[ci].time = time(&now);
						} else {
							close(clients[ci].connfd);	
							clients[ci].connfd = -1;
						}
					}
				}
				time_t now;
				int clientConnectionSec = (int) difftime(time(&now), clients[ci].time);
				//fprintf(stdout, "client %d : %d sec \n", ci, clientConnectionSec);
				//fflush(stdout);
				/* Throw out clients that have been inactive to more then CONNECTION TIME */
				if ( clientConnectionSec >= CONNECTION_TIME) {
					close(clients[ci].connfd);	
					clients[ci].connfd = -1;
				} 	
			}
			fprintf(stdout, "ROUND DONE\n");
			fflush(stdout);

		} else {
			fprintf(stdout, "No message in five seconds.\n");
			fflush(stdout);
		}
	}
}
