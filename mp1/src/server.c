/*
 ** server.c -- a stream socket server demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>

#define PORT "3490"  // the port users will be connecting to

#define BACKLOG 10	 // how many pending connections queue will hold

typedef struct http_status {
    const char* status_code;
    const char* reason;
} http_status;

typedef struct http_response {
	char *version;
	char *status;
	char *reason;
	size_t cl;
} http_response;

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

const http_status HTTP_ERROR = { "400", "Bad Request" };
const http_status HTTP_OK = { "200", "OK" };
const http_status HTTP_NOT_FOUND = { "404", "NOT FOUND" };

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                        p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                    sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    pthread_t threads[MAX_THREADS];
    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
                get_in_addr((struct sockaddr *)&their_addr),
                s, sizeof s);
        printf("server: got connection from %s\n", s);

        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener

            char* request_header = NULL;
            char buf[MAX_BUFFER] = 0;
            size_t content_received = 0;
            http_request request = 0;
            if (recv_header(sockfd, &request_header, &buf, &content_received) ||
                    parse_request_header(&request, request_header)) {
                respond(HTTP_ERROR);
                shutdown(new_fd, SHUTDOWN_RDWR);
                close(new_fd);
                exit(1);
            }

            char* path = request.request_uri;
            struct stat fstat;
            if (access(path, R_OK) || stat(path, &fstat)) {
                respond(HTTP_NOT_FOUND);
                shutdown(new_fd, SHUTDOWN_RDWR);
                close(new_fd);
                exit(1);
            }

            int file_fd = open(path, O_RDONLY);
            size_t content_length = fstat.st_size;
            size_t header_length = generate_header();

            size_t bytes_to_send = content_length;
            do {
                size_t bytes_sent = sendfile(new_fd, file_fd, 0, bytes_to_send);
                if (bytes_sent == -1) {
                    perror("Error transmitting file.");
                    exit(1);
                }
                bytes_to_send -= bytes_sent;
            } while (bytes_to_send > 0);
            close(file_fd);
            shutdown(new_fd, SHUTDOWN_RDWR);
            close(new_fd);
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
    }

    return 0;
}

int assign_struct_var(char *str_ptr, char *end_ptr, char *dest) {
  end_ptr = strstr(str_ptr, " ");
  if (end_ptr == NULL) {
    fprintf(stderr, "[Error] Wrong Request Message(Method end).\n");
    return -1;
  }
  *end_ptr = 0;
  dest = strdup(rl_ptr);
  return 1;
}

// request_str -> struct http_request
int parse_request_header(http_request *req, char *request_str) {
  if (request_str == NULL || *request_str == 0) {
    fprintf(stderr, "[Error] NULL Request.\n");
    return -1;
  }
  char *request = strdup(request_str);
  char *end_ptr;
  int str_size;
  char *header_ptr = strstr(request, "\r\n");
  if (header_ptr == NULL) {
    fprintf(stderr, "[Error] Wrong Request Format.\n");
    return -1;
  }
  *header_ptr = 0;
  header_ptr += 2;
  // request-line
  char *rl_ptr = request;
  // Method
  if (assign_struct_var(rl_ptr, end_ptr, req->method) == -1) {
    return -1;
  }
  // Request-URI
  rl_ptr = end_ptr + 1;
  if (*rl_ptr == 0) {
    fprintf(stderr, "[Error] Wrong Request Message(Request-URI start).\n");
    return -1;
  }
  if (assign_struct_var(rl_ptr, end_ptr, req->request_uri) == -1) {
    return -1;
  }
  // HTTP-Version
  rl_ptr = end_ptr + 1;
  if (*rl_ptr == 0) {
    fprintf(stderr, "[Error] Wrong Request Message(HTTP-Version start).\n");
    return -1;
  }
  if (assign_struct_var(rl_ptr, end_ptr, req->version) == -1) {
    return -1;
  }

  // request_header
  // if (header_ptr == NULL || *header_ptr == 0) {
  //   fprintf(stderr, "[Error] Wrong Request Message(header).\n");
  //   return -1;
  // }
  // char *header_end = strstr(header_ptr, "\r\n\r\n");
  // if (header_end == NULL) {
  //   fprintf(stderr, "[Error] Wrong Request Message(header end).\n");
  //   return -1;
  // }
  // *header_end = '\0';


  free(request);
  return 1;
}

// http_response -> string
size_t generate_response(char **response_str, http_response *rep) {
  int cl_loc = strlen(rep->version) + 1 + strlen(rep->status) + 1 + strlen(rep->reason) + 2;
  int length = cl_loc + 30 + 4;
  char *response = malloc(length);
  strcat(response, rep->version);
  strcat(response, " ");
  strcat(response, rep->status);
  strcat(response, " ");
  strcat(response, rep->reason);
  strcat(response, "\r\n");

  sprintf(response + cl_loc, "Content-Length: %d", rep->cl);
  strcat(response, "\r\n\r\n");
  *response_str = response;
  return strlen(response);
}
