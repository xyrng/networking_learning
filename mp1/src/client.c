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

#define MAXDATASIZE 1024 // max number of bytes we can get at once

typedef struct http_request {
	char *method;					// req line
	char *request_uri;		// req line
	char *version;				// req line
	char *ua;							// header
	char *accept;					// header
	char *host;						// header
	char *port;
	int keepalive;				// header
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

int recv_header(int sock_fd, char** response_header, char* buf, size_t* content_received) {
    size_t bytes_received = 0;
    size_t response_length = 512;
    *response_header = malloc(response_length);
    while (1) {
        size_t newly_received = recv(sock_fd, buf, MAXDATASIZE - 1, 0);
        buf[newly_received] = 0;
        char* end = strstr(buf, "\r\n\r\n");
		size_t to_copy = end ? end - buf + 4 : newly_received;
        if (bytes_received + to_copy + 1 > response_length) {
            do {
				response_length *= 2;
			} while (bytes_received + 1 > response_length);
            char* new_addr = realloc(*response_header, response_length);
            if (new_addr != NULL) {
                *response_header = new_addr;
            } else {
                fprintf(stderr, "Error allocating buffer for header");
                return -1;
            }
        }
        strncpy(*response_header, buf, to_copy);
		(*response_header)[to_copy] = 0;
		if (end) {
			*content_received = newly_received - to_copy + 1;
            memmove(buf, buf + to_copy, *content_received);
            buf[*content_received] = 0;
            return 0;
        }
    }
}

int main(int argc, char *argv[])
{
    int sockfd, numbytes;
    char buf[MAXDATASIZE] = 0;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    if (argc != 2) {
        fprintf(stderr,"usage: client hostname\n");
        exit(1);
    }

    http_request new_request = {0};
    if (build_request(&new_request, argv[1])) {
        fprintf(stderr, "Error parsing request.\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char* host = new_request.host;
    char* port = new_request.port;
    if ((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
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

    // Send HTTP Request
    char* request_str = NULL;
    size_t request_length = generate_request(&request_str, &new_request);

    size_t bytes_sent = 0;
    do {
        size_t newly_sent = send(sockfd, request_str + bytes_sent,
                request_length - bytes_sent, 0);
        if (newly_sent == -1) {
            fprintf(stderr, "Error sending request.\n");
            close(sockfd);
            exit(1);
        }
        bytes_sent += newly_sent;
    } while (request_length > bytes_sent);

    shutdown(sockfd, SHUT_WR);

    char* response_header = NULL;
    size_t content_received = 0;
    if (recv_header(sockfd, &response_header, buf, &content_received)) {
        fprint(stderr, "Error parsing header.\n");
        close(sockfd);
        exit(1);
    }

    http_response response = {0};
    if (parse_response_header(&response, response_header)) {
        fprintf(stderr, "Error parsing response");
        exit(1);
    }

    size_t content_length = response.content_length;
    FILE* output = fopen("output", "w");
    if (content_received != 0) {
        fwrite(buf, 1, content_received, output);
    }

    while (content_length > content_received) {
        size_t newly_received = recv(sockfd, buf, MAXDATASIZE, 0);
        if (newly_received == -1) {
            perror("Receving file error");
            close(sockfd);
            exit(1);
        }
        fwrite(buf, 1, newly_received, output);
        content_received += newly_received;
    }
    fclose(output);

    shutdown(sockfd, SHUT_RD);
    close(sockfd);

    return 0;
}

// check argc first please
int build_request(http_request *req, char *input) {
	if (strnstr(input, "http://", 7) != input) {
		fprintf(stderr, "No Protocol Specify.\n");
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
	strcat(request_str, new_request->method);
	strcat(request_str, " ");
	strcat(request_str, new_request->request_uri);
	strcat(request_str, " ");
	strcat(request_str, new_request->version);
	strcat(request_str, "\r\n");
	// header
	strcat(request_str, "User-Agent: ");
	strcat(request_str, new_request->ua);
	strcat(request_str, "\r\n");
	strcat(request_str, "Host: ");
	strcat(request_str, new_request->host);
	strcat(request_str, ":");
	strcat(request_str, new_request->port);
	strcat(request_str, "\r\n");
	strcat(request_str, "Accept: ");
	strcat(request_str, new_request->accept);
	strcat(request_str, "\r\n");
	strcat(request_str, "Connection: Keep-Alive");
	strcat(request_str, "\r\n");
	strcat(request_str, "\r\n");
	*request_str = request;
	return retval;
}

size_t request_str_size(http_request *new_request) {
	size_t retval = 0;
	retval += strlen(new_request->method) + 1 + strlen(new_request->request_uri) + 1 + strlen(new_request->version) + 2 +
						12 + strlen(new_request->ua) + 2 + 8 + strlen(new_request->accept) + 2 + 6 + strlen(new_request->host) + 1 +
						strlen(new_request->port) + 2 + 22 + 4;
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
