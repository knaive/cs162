/*
 * A simple HTTP library.
 *
 * Usage example:
 *
 *     // Returns NULL if an error was encountered.
 *     struct http_request *request = http_request_parse(fd);
 *
 *     ...
 *
 *     http_start_response(fd, 200);
 *     http_send_header(fd, "Content-type", http_get_mime_type("index.html"));
 *     http_send_header(fd, "Server", "httpserver/1.0");
 *     http_end_headers(fd);
 *     http_send_string(fd, "<html><body><a href='/'>Home</a></body></html>");
 *     reply_with_file(fd, "/path/to/index.html", 200);
 *
 *     close(fd);
 */

#ifndef LIBHTTP_H
#define LIBHTTP_H

/*
 * Functions for parsing an HTTP request.
 */
struct http_request {
  char *method;
  char *path;
};

struct http_request *http_request_parse(int fd);

/*
 * Functions for sending an HTTP response.
 */
void http_start_response(int fd, int status_code);
void http_send_header(int fd, char *key, char *value);
void http_end_headers(int fd);
int http_send_string(int fd, char *data);
int http_send_data(int fd, char *data, size_t size);
int http_send_file(int dst_fd, int src_fd);
int reply_with_file(int fd, char *path, int status); 

/*
 * Helper function: gets the Content-Type based on a file name.
 */
char *http_get_mime_type(char *file_name);

#endif
