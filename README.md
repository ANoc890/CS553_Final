# io_uring Web Server Optimization
This project aims to build a simple io_uring-based web server in C and build upon
it using a variety of performance optimizations. Each version is isolated from
each other to highlight the imact of the technique being implemented.

Files:                           | Description:
---------------------------------|--------------------------------
io_uring.c                       | Basic web server implementation using io_uring.
io_uring_batched.c               | Added batched submission optimization.
io_uring_kernel_polling.c        | Uses IORING_SETUP_SQPOLL to enable kernel-side submission polling.
io_uring_multishot.c             | Implements multishot accept to reduce repeated accept overhead.
io_uring_register_files.c        | Registers file descriptors for listening socket, for use in future references.
io_uring_register_files_multi.c  | Builds upon io_uring_register_files.c by using multiple sockets.

Building:
gcc <FILE_NAME> -o <EXECUTABLE_NAME> -luring

Running:
./<EXECUTABLE_NAME>

For benchmarking the results, use the tool of your choice. Results recorded in the
project writeup were achieved throught the use of wrk2 and the input parameters
listed.
