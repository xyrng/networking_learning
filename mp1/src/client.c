/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define PORT "3490" // the port client will be connecting to

#define MAXDATASIZE 100 // max number of bytes we can get at once


typedef struct http_request {
	char *method;					// req line
	char *request_uri;		// req line
	char *version;				// req line
	char *ua;							// header
	char *accept;					// header
	char *host;						// header
	char *port;
	bool keepalive;				// header
} http_request;

typedef struct http_response {
	char *version;
	char *status;
	char *reason;
	size_t cl;
} http_response;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes;
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	if (argc != 2) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
	    perror("recv");
	    exit(1);
	}

	buf[numbytes] = '\0';

	printf("client: received '%s'\n",buf);

	close(sockfd);

	return 0;
}

// check argc first please
int build_request(http_request *req, char *input) {
	if (strnstr(input, "http://", 7) != input) {
		fprintf(stderr, "No Protocol Specify.\n", );
		return -1;
	}
	char *host_ptr = input + 7;
	char *port_ptr = strchr(host_ptr, ':');
	char *file_path_ptr = strstr(host_ptr, "/");
	char *end_ptr;
	int str_size;

	// parse file_path, host and port
	// port <= 65535
	char *file_path;
	char *host;
	char *port;
	if (file_path_ptr == NULL || file_path_ptr + 1 == 0) { // no file_path
		file_path = malloc(2);
		strcpy(file_path, "/");
		if (file_path_ptr == NULL) {
			file_path_ptr = strchr(host_ptr, '\0');
		}
	} else {
		end_ptr = strchr(file_path_ptr, '\0');
		str_size = end - file_path_ptr;
		file_path = malloc(str_size + 1);
		strcpy(file_path, file_path_ptr);
	}

	// parse host and port
	if (port_ptr == NULL) { // http://127.0.0.1/index.html
		port = malloc(3);
		strcpy(port, "80");
		str_size = file_path_ptr - host_ptr;
		host = malloc(str_size + 1);
		strcpy(host, host_ptr);
	} else {
		if (port_ptr < file_path_ptr) {
			// host
			str_size = port_ptr - host_ptr; // ':' - '[a-z]'
			host = malloc(str_size + 1);
			strncpy(host, host_ptr, str_size);
			host[str_size] = '\0';
			// port
			str_size = file_path - port_ptr - 1; // '/' - ':'
			port = malloc(str_size + 1);
			strncpy(port, port_ptr + 1, str_size);
			*(port + str_size) = '\0';
			if (atoi(port) > 65535) {
				fprintf(stderr, "[Error] Wrong Port Number! It should be less than 65536.\n");
				return -1;
			}
		} else {
			fprintf(stderr, "[Error] File path cannot contain `column` signal.\n");
			return -1;
		}
	}

	// initialize and assign the request
	req->method = malloc(4);
	strcpy(req->method, "GET");
	req->request_uri = file_path;
	req->version = malloc(9);
	strcpy(req->version, "HTTP/1.1");
	req->ua = malloc(22);
	strcpy(req->ua, "Wget/1.12 (linux-gnu)");
	req->accept = malloc(4);
	strcpy(req->accept, "*/*");
	req->host = host;
	req->port = port;
	req->keepalive = 1;

	return 1;
}


size_t generate_request(char **request_str, http_request *new_request) {
	size_t retval = request_str_size(new_request);
	char *request = malloc(retval + 1);
	// request line
	strcat(request_str, req->method);
	strcat(request_str, " ");
	strcat(request_str, req->request_uri);
	strcat(request_str, " ");
	strcat(request_str, req->version);
	strcat(request_str, "\r\n");
	// header
	strcat(request_str, "User-Agent: ");
	strcat(request_str, req->ua);
	strcat(request_str, "\r\n");
	strcat(request_str, "Host: ");
	strcat(request_str, req->host);
	strcat(request_str, ":");
	strcat(request_str, req->port);
	strcat(request_str, "\r\n");
	strcat(request_str, "Accept: ");
	strcat(request_str, req->accept);
	strcat(request_str, "\r\n");
	strcat(request_str, "Connection: Keep-Alive");
	strcat(request_str, "\r\n");
	strcat(request_str, "\r\n");
	*request_str = request;
	return retval;
}

size_t request_str_size(http_request *new_request) {
	size_t retval = 0;
	retval += strlen(req->method) + 1 + strlen(req->request_uri) + 1 + strlen(req->version) + 2 +
						12 + strlen(req->ua) + 2 + 8 + strlen(req->accept) + 2 + 6 + strlen(req->host) + 1 +
						strlen(req->port) + 2 + 22 + 4;
	return retval;
}

// write the response to file
int parse_response_header(char *response_str, http_response *rep) {
	char *str_ptr = response_str;
	char *end_ptr;
	int str_size;
	// Status-Line Part
	if (str_ptr == NULL) {
		fprintf(stderr, "[Error] Wrong Response Message (Status-Line).\n");
		return -1;
	} else {
		// HTTP-Version										 str_ptr -> [a-z]
		end_ptr = strchr(str_ptr, " "); // end_ptr -> ' '
		if (end_ptr == NULL) {
			fprintf(stderr, "[Error] Wrong Response Message (HTTP-Version end).\n");
			return -1;
		}
		str_size = end_ptr - str_ptr;
		strndup(rep->version, str_ptr, str_size + 1);

		// Status-Code
		str_ptr = end_ptr + 1;
		if (*str_ptr == 0) {
			fprintf(stderr, "[Error] Wrong Response Message (Status-Code start).\n");
			return -1;
		}
		end_ptr = strchr(str_ptr, " ");
		if (end_ptr == NULL) {
			fprintf(stderr, "[Error] Wrong Response Message (Status-Code end).\n");
			return -1;
		}
		str_size = end_ptr - str_ptr;
		strndup(rep->status, str_ptr, str_size + 1);

		// Reason-Phrase
		str_ptr = end_ptr + 1;
		if (*str_ptr == 0) {
			fprintf(stderr, "[Error] Wrong Response Message (Reason-Phrase start).\n");
			return -1;
		}
		end_ptr = strchr(str_ptr, "\r"); // end_ptr -> '\r'
		if (end_ptr == NULL) {
			fprintf(stderr, "[Error] Wrong Response Message (Reason-Phrase end).\n");
			return -1;
		}
		str_size = end_ptr - str_ptr;
		strndup(rep->reason, str_ptr, str_size + 1);
	}

	// header part
	str_ptr = end_ptr + 2;
	if (*str_ptr == 0) {
		fprintf(stderr, "[Error] Wrong Response Message: no header part.\n");
		return -1;
	}
	char *cl_ptr = NULL;
	while ((end_ptr = strstr(str_ptr, "\r\n")) != NULL) {
		if ((cl_ptr = strstr(str_ptr, "Content-Length: ")) != NULL) {
			break;
		}
		str_ptr = end_ptr + 2;
		if (*str_ptr == 0) {
			break;
		}
	}
	if (cl_ptr == NULL) {
		fprintf(stderr, "[Error] No Content-Length.\n");
		return -1;
	}
	end_ptr = strstr(cl_ptr, "\r\n");
	if (end_ptr != NULL) {
		fprintf(stderr, "[Error] Wrong Response Message Protocol.\n");
		return -1;
	}
	str_size = end_ptr - cl_ptr;
	strndup(rep->cl, cl_ptr, str_size + 1);

	return 1;
}
