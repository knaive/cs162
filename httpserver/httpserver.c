#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

#include "libhttp.h"
#include "util.h"
#include "wq.h"

/*
 * Global configuration variables.
 */
wq_t work_queue;
int num_threads;
int server_port;
char *server_files_directory;
char *server_proxy_hostname;
int server_proxy_port;

/*
 * Reads an HTTP request from stream (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 */

void handle_files_request(int fd) {
    struct http_request *request = http_request_parse(fd);

    if(request == NULL) goto EXIT;

    char *full_path = join_path(server_files_directory, request->path, NULL);
    /* printf("%s\n %s\n %s\n", server_files_directory, request->path, full_path); */

    struct stat sb;
    stat(full_path, &sb);
    if (S_ISDIR(sb.st_mode)) {

        DIR *dirp;
        struct dirent *dp;
        dirp = opendir(full_path);
        while((dp = readdir(dirp))!=NULL) {
            if(strcmp(dp->d_name, "index.html")==0) {
                char *filename = join_path(full_path, "/index.html", NULL);
                reply_with_file(fd, filename);
                free(filename);
                free(full_path);
                closedir(dirp);
                goto EXIT;
            }
        }
        closedir(dirp);

        char *template = "<html><body><center><p><a href=\"%s\">%s</a></p>";
        size_t template_len = strlen(template), parent_path_len = strlen(request->path);
        char *temp = NULL, *saved, *send_buffer = (char *)malloc(1);
        send_buffer[0] = '\0';
        dirp = opendir(full_path);
        char *display_name, *url;
        while((dp = readdir(dirp))!=NULL) {
            if(!strcmp(dp->d_name, ".")) continue;
            else if(!strcmp(dp->d_name, "..")) {
                display_name = "Parent Directory";
                url = get_parent_name(request->path);
            } else {
                display_name = dp->d_name;
                url = join_path(request->path, dp->d_name, NULL);
            }
            temp = (char *) malloc(template_len + parent_path_len + 2*dp->d_reclen);
            sprintf(temp, template, url, display_name);
            saved = send_buffer;
            send_buffer = join_string(send_buffer, temp, NULL);
            free(saved);
            free(temp);
            free(url);
        }
        saved = send_buffer;
        size_t content_len;
        send_buffer = join_string(send_buffer,  "</center></body></html>", &content_len);
        free(saved);

        http_start_response(fd, 200);
        http_send_header(fd, "Content-Type", "text/html");
        dprintf(fd, "Content-Length: %lu\r\n", content_len);
        http_end_headers(fd);

        http_send_string(fd, send_buffer);
        closedir(dirp);
        free(send_buffer);
    } else {
        if (access(full_path, F_OK) == -1) {
            http_start_response(fd, 404);
            goto EXIT;
        }

        reply_with_file(fd, full_path);
    }
EXIT: close(fd);
}

void* file_request_handler(void* unused) {
    while(1){
        int req_fd = wq_pop(&work_queue);
        printf("Process %d, thread %lu will handle request.\n", getpid(),  pthread_self());
        handle_files_request(req_fd);
    }
}

void dispatch_file_request(int fd) {
    wq_push(&work_queue, fd);
}


/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target. HTTP requests from the client (fd) should be sent to the
 * proxy target, and HTTP responses from the proxy target should be sent to
 * the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 */
/* create a socket connected to remote server */
int connect_to_host(char *hostname, int port) {
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("Failed to create a new socket");
        exit(errno);
    }

    /* get ip address of hostname */
    struct hostent* ent = gethostbyname(hostname);
    if(ent == NULL || ent->h_addr_list == NULL) {
        perror("Failed to get host ip");
        exit(errno);
    }
    struct in_addr** addr = (struct in_addr**)ent->h_addr_list;

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr= *addr[0];
    server_addr.sin_port = htons(port);

    if((connect(fd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)))!=0) {
        perror("Failed to connect server");
        exit(errno);
    }

    return fd;
}

struct fd_pair {
    int *read_fd;
    int *write_fd;
    pthread_cond_t* cond;
    int *finished;
    char* type;
    unsigned long id;
};

void* relay_message(void* endpoints) {
    struct fd_pair* pair = (struct fd_pair*)endpoints;   
    
    char buffer[4096];
    int read_ret, write_ret;
    printf("%s thread %lu start to work\n", pair->type, pair->id);
    while((read_ret=read(*pair->read_fd, buffer, sizeof(buffer)-1))>0) {
        write_ret = http_send_data(*pair->write_fd, buffer, read_ret);
        if(write_ret<0) break;
    }
    
    if(read_ret<=0) printf("%s thread %lu read failed, status %d\n", pair->type, pair->id, read_ret);
    if(write_ret<=0) printf("%s thread %lu write failed, status %d\n", pair->type, pair->id, write_ret);

    *pair->finished = 1;
    pthread_cond_signal(pair->cond);

    printf("%s thread %lu exited\n", pair->type, pair->id);
    return NULL;
}

static unsigned long id;
pthread_mutex_t id_mutex = PTHREAD_MUTEX_INITIALIZER;

void* proxy_request_handler(void* unused) {
    unsigned long local_id;
    while(1){
        int req_fd = wq_pop(&work_queue);
        int target_fd = connect_to_host(server_proxy_hostname, server_proxy_port);

        pthread_mutex_lock(&id_mutex);
        local_id = id++;
        pthread_mutex_unlock(&id_mutex);

        printf("Thread %lu will handle proxy request %lu.\n", pthread_self(), local_id);

        struct fd_pair pairs[2];
        pthread_mutex_t mutex;
        pthread_cond_t cond;
        int finished = 0;
        pthread_mutex_init(&mutex, NULL);
        pthread_cond_init(&cond, NULL);

        pairs[0].read_fd = &req_fd;
        pairs[0].write_fd = &target_fd;
        pairs[0].finished = &finished;
        pairs[0].type = "request";
        pairs[0].cond = &cond;
        pairs[0].id = local_id;

        pairs[1].read_fd = &target_fd;
        pairs[1].write_fd = &req_fd;
        pairs[1].finished = &finished;
        pairs[1].type = "response";
        pairs[1].cond = &cond;
        pairs[1].id = local_id;

        pthread_t threads[2];
        pthread_create(threads, NULL, relay_message, pairs);
        pthread_create(threads+1, NULL, relay_message, pairs+1);

        if(!finished) pthread_cond_wait(&cond, &mutex);

        close(req_fd);
        close(target_fd);

        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&cond);

        printf("Socket closed, proxy request %lu finished.\n\n", local_id);
    }
    return NULL;
}

void dispatch_proxy_request(int fd) {
    wq_push(&work_queue, fd);
}

/* A simple proxy that only serves a request per connection */
void* simple_proxy(void* arg) {
    int fd = *(int *)arg;
    char buffer[4096];
    struct http_request *request = http_request_parse(fd);
    if(request == NULL) goto EXIT;

    int proxy_target_fd = connect_to_host(server_proxy_hostname, server_proxy_port);
    memset(buffer, '\0', sizeof(buffer));
    sprintf(buffer, "%s %s HTTP/1.0\r\nHost: %s:%d\r\nConnection: close\r\n\r\n", request->method, \
            request->path, server_proxy_hostname, server_proxy_port);
    http_send_string(proxy_target_fd, buffer);

    int size = 0;
    while((size=read(proxy_target_fd, buffer, sizeof(buffer)-1))>0) {
        buffer[size] = '\0';
        http_send_string(fd, buffer);
    }
    close(proxy_target_fd);

EXIT:
    close(fd);
    return NULL;
}

void dispatch_proxy_request_s(int fd) {
    pthread_t t;
    if(pthread_create(&t, NULL, simple_proxy, (void*) &fd)) {
        perror("Failed to create a thread");
        exit(1);
    }
}

/* create a server socket so that we can accept client connections */
int create_server_socket(int port) {
    struct sockaddr_in server_address;
    int socket_number = socket(PF_INET, SOCK_STREAM, 0);
    if (socket_number == -1) {
        perror("Failed to create a new socket");
        exit(errno);
    }

    int socket_option = 1;
    if (setsockopt(socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option,
                sizeof(socket_option)) == -1) {
        perror("Failed to set socket options");
        exit(errno);
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    if (bind(socket_number, (struct sockaddr *) &server_address,
                sizeof(server_address)) == -1) {
        perror("Failed to bind on socket");
        exit(errno);
    }
    return socket_number;
}

/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int *socket_number, void (*request_handler)(int)) {
    struct sockaddr_in client_address;
    size_t client_address_length = sizeof(client_address);
    int client_socket_number;

    *socket_number = create_server_socket(server_port);
    if (listen(*socket_number, 1024) == -1) {
        perror("Failed to listen on socket");
        exit(errno);
    }

    printf("Listening on port %d...\n", server_port);

    while (1) {
        client_socket_number = accept(*socket_number,
                (struct sockaddr *) &client_address,
                (socklen_t *) &client_address_length);
        if (client_socket_number < 0) {
            perror("Error accepting socket");
            continue;
        }

        printf("Accepted connection from %s on port %d\n",
                inet_ntoa(client_address.sin_addr),
                client_address.sin_port);

        request_handler(client_socket_number);
    }

    shutdown(*socket_number, SHUT_RDWR);
    close(*socket_number);
}

int server_fd;
void signal_callback_handler(int signum) {
    printf("Caught signal %d: %s\n", signum, strsignal(signum));
    printf("Closing socket %d\n", server_fd);
    if (close(server_fd) < 0) perror("Failed to close server_fd (ignoring)\n");
    exit(0);
}

char *USAGE =
"Usage: ./httpserver --files www_directory/ --port 8000 [--num-threads 5]\n"
"       ./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000 [--num-threads 5]\n";

void exit_with_usage() {
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    signal(SIGINT, signal_callback_handler);
    signal(SIGPIPE, SIG_IGN);

    /* Default settings */
    server_port = 8000;
    num_threads = 1;
    void (*request_handler)(int) = NULL;
    void* (*thread_pool_worker)(void*) = NULL;

    int i;
    int simple_proxy = 0;
    for (i = 1; i < argc; i++) {
        if (strcmp("--files", argv[i]) == 0) {
            server_files_directory = argv[++i];
            if (!server_files_directory) {
                fprintf(stderr, "Expected argument after --files\n");
                exit_with_usage();
            }
        } else if (strcmp("--proxy", argv[i]) == 0) {
            char *proxy_target = argv[++i];
            if (!proxy_target) {
                fprintf(stderr, "Expected argument after --proxy\n");
                exit_with_usage();
            }

            char *colon_pointer = strchr(proxy_target, ':');
            if (colon_pointer != NULL) {
                *colon_pointer = '\0';
                server_proxy_hostname = proxy_target;
                server_proxy_port = atoi(colon_pointer + 1);
            } else {
                server_proxy_hostname = proxy_target;
                server_proxy_port = 80;
            }
        } else if (strcmp("--port", argv[i]) == 0) {
            char *server_port_string = argv[++i];
            if (!server_port_string) {
                fprintf(stderr, "Expected argument after --port\n");
                exit_with_usage();
            }
            server_port = atoi(server_port_string);
        } else if (strcmp("--num-threads", argv[i]) == 0) {
            char *num_threads_str = argv[++i];
            if (!num_threads_str || (num_threads = atoi(num_threads_str)) < 1) {
                fprintf(stderr, "Expected positive integer after --num-threads\n");
                exit_with_usage();
            }
        } else if (strcmp("--simple-proxy", argv[i]) == 0){
            simple_proxy = 1;
        } else if (strcmp("--help", argv[i]) == 0) {
            exit_with_usage();
        } else {
            fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
            exit_with_usage();
        }
    }

    if (server_files_directory == NULL && server_proxy_hostname == NULL) {
        fprintf(stderr, "Please specify either \"--files [DIRECTORY]\" or \n"
                "                      \"--proxy [HOSTNAME:PORT]\"\n");
        exit_with_usage();
    } else if(server_files_directory != NULL){
        request_handler = dispatch_file_request;
        thread_pool_worker = file_request_handler;
    } else if(server_proxy_hostname != NULL) {
        request_handler = dispatch_proxy_request;
        thread_pool_worker = proxy_request_handler;
        if(simple_proxy) {
            request_handler = dispatch_proxy_request_s;
            thread_pool_worker = NULL;
        }
    }

    if(num_threads && thread_pool_worker) {
        pthread_t *threads = (pthread_t *) malloc(sizeof(pthread_t)*num_threads);
        pthread_t *p = threads;
        wq_init(&work_queue);
        for(int i=0 ; i<num_threads; i++, p++) {
            pthread_create(p, NULL, thread_pool_worker, NULL);
        }
    }

    serve_forever(&server_fd, request_handler);

    return EXIT_SUCCESS;
}
