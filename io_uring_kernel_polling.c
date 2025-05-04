#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <liburing.h>

#define PORT 8080
#define QUEUE_DEPTH 2000
#define BUFFER_SIZE 1024
#define READ_SZ 8192
#define RESPONSE "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-length:15\r\n\r\nHello, world!\r\n"

#define EVENT_TYPE_ACCEPT       0
#define EVENT_TYPE_READ         1
#define EVENT_TYPE_WRITE        2

struct io_data {
    int fd;
    char buffer[BUFFER_SIZE];
};

struct request {
                int event_type;
                int iovec_count;
                int client_socket;
                struct iovec iov[];
};

struct io_uring ring;

int add_accept(int server_fd, struct sockaddr_in *client_addr, socklen_t *client_len) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    io_uring_prep_accept(sqe, server_fd, (struct sockaddr *)client_addr, client_len, 0);
    io_uring_sqe_set_flags(sqe, 0);
    struct request *req = malloc(sizeof(*req));
    req->event_type = EVENT_TYPE_ACCEPT;
    io_uring_sqe_set_data(sqe, req);
    //io_uring_submit(&ring);
    return 0;
}

int add_read(int client_socket) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    struct request *req = malloc(sizeof(*req) + sizeof(struct iovec));
    req->iov[0].iov_base = malloc(READ_SZ);
    req->iov[0].iov_len = READ_SZ;
    req->event_type = EVENT_TYPE_READ;
    req->client_socket = client_socket;
    req->iovec_count = 1;
    memset(req->iov[0].iov_base, 0, READ_SZ);
    io_uring_prep_readv(sqe, client_socket, &req->iov[0], 1, 0);
    io_uring_sqe_set_data(sqe, req);
    
    //io_uring_submit(&ring);
    return 0;
}

int add_write(struct request *req) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (!sqe) {
        perror("io_uring_get_sqe failed");
        //close(data->fd);
        //free(data);
        return 1;
    }
    req->event_type = EVENT_TYPE_WRITE;
    io_uring_prep_write(sqe, req->client_socket, RESPONSE, strlen(RESPONSE), 0);
    io_uring_sqe_set_data(sqe, req);
    //io_uring_submit(&ring);
    return 0;
}

int main() {
    int server_fd;
    struct sockaddr_in server_addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, QUEUE_DEPTH) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    struct io_uring_params params;
    memset(&params, 0, sizeof(params));
    params.flags = IORING_SETUP_SQPOLL;
    params.sq_thread_idle = 2000;

    io_uring_queue_init_params(QUEUE_DEPTH, &ring, &params);
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    add_accept(server_fd, &client_addr, &client_len);
    io_uring_submit(&ring);
    struct io_uring_cqe *cqe;

    while (1) {
        io_uring_wait_cqe(&ring, &cqe);
        struct request *req = (struct request *) cqe->user_data;
        switch (req->event_type) {
          case EVENT_TYPE_ACCEPT:
            add_accept(server_fd, &client_addr, &client_len);
            add_read(cqe->res);
            free(req);
            break;
          case EVENT_TYPE_READ:
            if (cqe->res <= 0) {
              for (int i = 0; i < req->iovec_count; i++) {
                if (req->iov[i].iov_base){
                  free(req->iov[i].iov_base);
                }
              }
              close(req->client_socket);
              free(req);
            }
            else {
              add_write(req);
            }
            //io_uring_submit(&ring);
            break;
          case EVENT_TYPE_WRITE:
            add_read(req->client_socket);
            for (int i = 0; i < req->iovec_count; i++) {
                if (req->iov[i].iov_base){
                  free(req->iov[i].iov_base);
                }
            }
            free(req);
            break;
        }
        io_uring_submit(&ring);
        io_uring_cqe_seen(&ring, cqe);
    }

    io_uring_queue_exit(&ring);
    close(server_fd);
    return 0;
}

