/**
 * Copyright (c) 2023 Jacob Martin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * Echos back on the same socket anything received on a socket. For example,
 * in one window `./echo_server 5000`. In another, `netcat localhost 5000`.
 * Everything submitted through netcat will be echoed back.
 */

/**
 * Compile with, e.g.:
 * gcc -O2 -Wall -Wextra -Wconversion -g3 -fsanitize=undefined -o io_uring_echo_server io_uring_echo_server.c -luring
 * Depending on your kernel and liburing version, you might have to compile statically.
 */

#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>
#include <error.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <liburing.h>

#define MAXREQUESTS 32
#define BUFSZ 128
#define BUFS_IN_GROUP 64

struct io_uring ring;
struct io_uring_buf_ring *br;
char *bufs[BUFS_IN_GROUP];

#define DIE_ERRNO(err, ...) do {			\
    error_at_line(EXIT_FAILURE,				\
		  errno,				\
		  __FILE__,				\
		  __LINE__,				\
		  err __VA_OPT__(,)			\
		  __VA_ARGS__); } while (0)

#define DIE_STRERR(err, errno, ...) do {			\
    error_at_line(EXIT_FAILURE,					\
		  errno,					\
		  __FILE__,					\
		  __LINE__,					\
		  err __VA_OPT__(,)				\
		  __VA_ARGS__); } while (0)

#define DIE(err, ...) do {			\
    error_at_line(EXIT_FAILURE,			\
		  0,				\
		  __FILE__,			\
		  __LINE__,			\
		  err __VA_OPT__(,)		\
		  __VA_ARGS__); } while (0)

struct request_data {
  enum {
    ACCEPT,
    RECV,
    SEND,
    CLOSE
  } type;
  int fd;
  int buf_id;
};

void setup_buffer_ring()
{
  struct io_uring_buf_reg reg = {};
  short unsigned int i;
  int ret;
  
  /* allocate mem for sharing buffer ring */
  if ((ret = posix_memalign((void **) &br, 4096,
			    BUFS_IN_GROUP * sizeof(struct io_uring_buf_ring))) != 0)
      DIE_STRERR("Couldn't align buffer group\n", ret);
  
  /* assign and register buffer ring */
  reg.ring_addr = (unsigned long) br;
  reg.ring_entries = BUFS_IN_GROUP;
  reg.bgid = 1;
  if ((ret = io_uring_register_buf_ring(&ring, &reg, 0)) != 0)
    DIE_STRERR("Error registering buffers", -errno);
  
  /* add initial buffers to the ring */
  io_uring_buf_ring_init(br);
  for (i = 0; i < BUFS_IN_GROUP; i++) {
    bufs[i] = malloc(BUFSZ);
    /* add each buffer, we'll use i buffer ID */
    io_uring_buf_ring_add(br, bufs[i], BUFSZ, i,
			  io_uring_buf_ring_mask(BUFS_IN_GROUP), i);
  }
  
  /* we've supplied buffers, make them visible to the kernel */
  io_uring_buf_ring_advance(br, BUFS_IN_GROUP);
}

int init_server(uint16_t port)
{
  struct sockaddr_in echoserver;
  int serversock;
  if ((serversock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    DIE_ERRNO("Couldn't open socket\n");
  
  echoserver.sin_family = AF_INET;
  echoserver.sin_addr.s_addr = htonl(INADDR_ANY);
  echoserver.sin_port = htons(port);

  if (bind(serversock, (struct sockaddr *) &echoserver, sizeof(echoserver)) < 0)
    DIE_ERRNO("Failed to bind to the socket\n");

  if (listen(serversock, MAXREQUESTS) < 0)
    DIE_ERRNO("Failed to listen on the socket\n");

  return serversock;
}

struct sockaddr_in echoclient = { 0 };
socklen_t socklen = sizeof(echoclient);

void submit_multishot_accept(int serversock)
{
  struct io_uring_sqe *sqe;
  struct request_data *sqe_data;
  
  if ((sqe = io_uring_get_sqe(&ring)) == NULL)
    DIE("sqe returned null");
  
  if ((sqe_data = malloc(sizeof(*sqe_data))) == NULL)
    DIE_ERRNO("Couldn't allocate\n");

  io_uring_prep_multishot_accept(sqe, serversock, (struct sockaddr *) &echoclient, &socklen, 0);
  sqe_data->type = ACCEPT;
  io_uring_sqe_set_data(sqe, sqe_data);
  io_uring_submit_and_wait(&ring, 1);
}

void submit_recv(int fd)
{
  struct io_uring_sqe *sqe;
  struct request_data *sqe_data;
  
  if ((sqe = io_uring_get_sqe(&ring)) == NULL)
    DIE("sqe returned null");
  
  if ((sqe_data = malloc(sizeof(*sqe_data))) == NULL)
    DIE_ERRNO("Couldn't allocate\n");

  io_uring_prep_recv(sqe, fd, NULL, BUFSZ, 0);
  io_uring_sqe_set_flags(sqe, IOSQE_BUFFER_SELECT);
  sqe->buf_group = 1;
  sqe_data->type = RECV;
  sqe_data->fd = fd;
  io_uring_sqe_set_data(sqe, sqe_data);
  io_uring_submit(&ring);
}

void submit_send(struct io_uring_cqe *cqe)
{
  struct io_uring_sqe *sqe;
  struct request_data *sqe_data;
  int buf_id;

  sqe_data = io_uring_cqe_get_data(cqe);
  
  if (!(cqe->flags & IORING_CQE_F_BUFFER))
    DIE("cqe had weirdness\n");
  
  if ((sqe = io_uring_get_sqe(&ring)) == NULL)
    DIE("sqe returned null");
  
  buf_id = ((int) cqe->flags) >> IORING_CQE_BUFFER_SHIFT;
  sqe_data->buf_id = buf_id;
  io_uring_prep_send(sqe, sqe_data->fd, bufs[buf_id], (size_t) cqe->res, 0);
  sqe_data->type = SEND;
  io_uring_sqe_set_data(sqe, sqe_data);
  io_uring_submit(&ring);
}

void submit_close(struct request_data *cqe_data)
{
  struct io_uring_sqe *sqe;
  struct request_data *sqe_data;
  
  if ((sqe = io_uring_get_sqe(&ring)) == NULL)
    DIE("sqe returned null");
  
  sqe_data = cqe_data;

  io_uring_prep_close(sqe, cqe_data->fd);
  sqe_data->type = CLOSE;
  io_uring_sqe_set_data(sqe, sqe_data);
  io_uring_submit(&ring);
}

void sigint_handler(int signo) {
  (void) signo;
  printf("^C pressed. Shutting down.\n");
  io_uring_queue_exit(&ring);
  exit(0);
}

int main(int argc, char *argv[])
{
  int serversock;
  int ret;

  struct io_uring_cqe *cqe;
  struct request_data *cqe_data;

  unsigned head;
  unsigned n_cqes = 0;

  if (argc != 2)
    DIE("Usage: %s <port>\n", argv[0]);

  io_uring_queue_init(128, &ring, 0);

  signal(SIGINT, sigint_handler);
  
  setup_buffer_ring();
  
  serversock = init_server((uint16_t) atoi(argv[1]));

  submit_multishot_accept(serversock);
  
  while (1) {
    n_cqes = 0;
    io_uring_for_each_cqe(&ring, head, cqe) {
      n_cqes++;
      
      cqe_data = io_uring_cqe_get_data(cqe);
      
      switch (cqe_data->type) {
      case ACCEPT:
	submit_recv(cqe->res);
	break;
	
      case CLOSE:
	free(cqe_data);
	break;
	
      case RECV:
	if (cqe->res <= 0) {
	  submit_close(cqe_data);
	} else {
	  submit_send(cqe);
	}
	break;
	
      case SEND:
	submit_recv(cqe_data->fd);
	io_uring_buf_ring_add(br, bufs[cqe_data->buf_id], BUFSZ, 1, io_uring_buf_ring_mask(BUFS_IN_GROUP), cqe_data->buf_id);
	io_uring_buf_ring_advance(br, 1);
	free(cqe_data);
	break;
      }
    }
    io_uring_cq_advance(&ring, n_cqes);
  }
  io_uring_queue_exit(&ring);
  return 0;
}

