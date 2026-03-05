#include "asgn2_helper_funcs.h"
#include "connection.h"
#include "response.h"
#include "request.h"
#include "queue.h"

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/file.h>

#define INT2VOID(i) (void *) (uintptr_t) (i)
#define VOID2INT(p) (int) (uintptr_t) (p)

void handle_connection(int);
void handle_get(conn_t *);
void handle_put(conn_t *);
void handle_unsupported(conn_t *);

// Global queue for thread communication
static queue_t *que;

// Mutexes for thread safety
pthread_mutex_t audit_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t file_op_mutex = PTHREAD_MUTEX_INITIALIZER;

void *worker_thread(void *arg) {
    (void) arg;
    while (true) {
        void *conn_ptr;
        queue_pop(que, &conn_ptr);
        int connfd = VOID2INT(conn_ptr);
        handle_connection(connfd);
    }
    return NULL;
}

int main(int argc, char **argv) {
    int opt = 0;
    int num_threads = 4; // Default thread count

    while ((opt = getopt(argc, argv, "t:")) != -1) {
        switch (opt) {
        case 't':
            num_threads = atoi(optarg);
            if (num_threads <= 0) {
                warnx("Invalid number of threads: %s", optarg);
                return EXIT_FAILURE;
            }
            break;
        default:
            fprintf(stderr, "usage: ./httpserver [-t threads] <port>\n");
            return EXIT_FAILURE;
        }
    }

    if (optind >= argc) {
        warnx("missing port number");
        fprintf(stderr, "usage: ./httpserver [-t threads] <port>\n");
        return EXIT_FAILURE;
    }

    char *endptr = NULL;
    size_t port = (size_t) strtoull(argv[optind], &endptr, 10);
    if (endptr && *endptr != '\0') {
        warnx("invalid port number: %s", argv[optind]);
        return EXIT_FAILURE;
    }

    signal(SIGPIPE, SIG_IGN);

    // Initialize the queue and threads
    que = queue_new(num_threads);
    pthread_t threads[num_threads];
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, worker_thread, NULL);
    }

    Listener_Socket sock;
    if (listener_init(&sock, port) != 0) {
        return EXIT_FAILURE;
    }

    while (true) {
        int connfd = listener_accept(&sock);
        if (connfd >= 0) {
            queue_push(que, INT2VOID(connfd));
        }
    }

    return EXIT_SUCCESS;
}

void audit(const char *req, char *file_name, uint16_t response_num, char *Req_id) {
    if (Req_id == NULL) {
        Req_id = "0";
    }
    pthread_mutex_lock(&audit_mutex);
    fprintf(stderr, "%s,/%s,%d,%s\n", req, file_name, response_num, Req_id);
    pthread_mutex_unlock(&audit_mutex);
}

void handle_connection(int connfd) {
    conn_t *conn = conn_new(connfd);
    const Response_t *res = conn_parse(conn);

    if (res != NULL) {
        conn_send_response(conn, res);
    } else {
        const Request_t *req = conn_get_request(conn);
        if (req == &REQUEST_GET) {
            handle_get(conn);
        } else if (req == &REQUEST_PUT) {
            handle_put(conn);
        } else {
            handle_unsupported(conn);
        }
    }

    conn_delete(&conn);
    close(connfd);
}

void handle_get(conn_t *conn) {
    char *uri = conn_get_uri(conn);
    char *Req_id = conn_get_header(conn, "Request-Id");
    const Response_t *res = NULL;
    int fd = -1;

    // Use a mutex to safely check file existence and open
    pthread_mutex_lock(&file_op_mutex);
    fd = open(uri, O_RDONLY);
    if (fd < 0) {
        if (errno == ENOENT) res = &RESPONSE_NOT_FOUND;
        else if (errno == EACCES) res = &RESPONSE_FORBIDDEN;
        else res = &RESPONSE_INTERNAL_SERVER_ERROR;
        pthread_mutex_unlock(&file_op_mutex);
        goto out;
    }

    // Advisory read lock
    flock(fd, LOCK_SH);
    pthread_mutex_unlock(&file_op_mutex);

    struct stat st;
    fstat(fd, &st);
    if (S_ISDIR(st.st_mode)) {
        res = &RESPONSE_FORBIDDEN;
        goto out;
    }

    res = &RESPONSE_OK;
    conn_send_file(conn, fd, st.st_size);

out:
    if (res != &RESPONSE_OK) conn_send_response(conn, res);
    audit("GET", uri, response_get_code(res), Req_id);
    if (fd >= 0) {
        flock(fd, LOCK_UN);
        close(fd);
    }
}

void handle_put(conn_t *conn) {
    char *uri = conn_get_uri(conn);
    char *Req_id = conn_get_header(conn, "Request-Id");
    const Response_t *res = NULL;
    int fd = -1;

    pthread_mutex_lock(&file_op_mutex);
    bool existed = (access(uri, F_OK) == 0);
    fd = open(uri, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fd < 0) {
        if (errno == EACCES || errno == EISDIR) res = &RESPONSE_FORBIDDEN;
        else res = &RESPONSE_INTERNAL_SERVER_ERROR;
        pthread_mutex_unlock(&file_op_mutex);
        goto out;
    }

    // Advisory exclusive lock for writing
    flock(fd, LOCK_EX);
    pthread_mutex_unlock(&file_op_mutex);

    res = conn_recv_file(conn, fd);
    if (res == NULL) {
        res = existed ? &RESPONSE_OK : &RESPONSE_CREATED;
    }

out:
    conn_send_response(conn, res);
    audit("PUT", uri, response_get_code(res), Req_id);
    if (fd >= 0) {
        flock(fd, LOCK_UN);
        close(fd);
    }
}

void handle_unsupported(conn_t *conn) {
    const Response_t *res = &RESPONSE_NOT_IMPLEMENTED;
    conn_send_response(conn, res);
    audit(request_get_str(conn_get_request(conn)), conn_get_uri(conn), 501, conn_get_header(conn, "Request-Id"));
}
