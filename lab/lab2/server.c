//
// COEN 146L - Lab 2
// TCP file-transfer server
// Student starter: complete each numbered TODO.
//
// Usage:
//   ./server <port>
//

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE 4096
#define FILENAME_SIZE 1024
#define BACKLOG 10
#define ACK_MESSAGE "received!"
#define ACK_SIZE (sizeof(ACK_MESSAGE))


static int parse_port(const char *text, uint16_t *port)
{
    char *end = NULL;
    errno = 0;

    long value = strtol(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' ||
        value < 1 || value > 65535) {
        return -1;
    }

    *port = (uint16_t)value;
    return 0;
}


/*
 * Send the entire buffer to the client, retrying on interruption and
 * reporting a failure if the socket closes before all bytes are sent.
 */
static ssize_t write_all(int socket_fd,
                         const void *buffer,
                         size_t length)
{
    size_t total = 0;
    const char *position = buffer;

    while (total < length) {
        ssize_t bytes_written =
            write(socket_fd, position + total, length - total);

        if (bytes_written < 0) {
            if (errno == EINTR)
                continue;

            return -1;
        }

        if (bytes_written == 0) {
            errno = EPIPE;
            return -1;
        }

        total += (size_t)bytes_written;
    }

    return (ssize_t)total;
}


/*
 * Read the remote filename one byte at a time until a terminating NUL is
 * received. The loop bound reserves space for the final NUL and rejects
 * malformed or oversized input.
 */
static ssize_t read_filename(int socket_fd,
                             char *filename,
                             size_t capacity)
{
    size_t total = 0;

    while (total < capacity - 1) {
        // TCP is a byte stream, so the filename may arrive in several reads.
        // Reading one byte at a time makes it simple to stop exactly at the
        // terminating NUL. The loop condition, not the one-byte read itself,
        // prevents the filename buffer from overflowing.
        ssize_t bytes_read = read(socket_fd, &filename[total], 1);

        if (bytes_read < 0) {
            if (errno == EINTR)
                continue;

            return -1;
        }

        if (bytes_read == 0) {
            errno = ECONNRESET;
            return -1;
        }

        // Check for the terminating NUL character
        if (filename[total++] == '\0') {
            if (total == 1) { // empty filename is not allowed
                errno = EINVAL;
                return -1;
            }

            return (ssize_t)total;
        }
    }

    filename[capacity - 1] = '\0';
    errno = ENAMETOOLONG;
    return -1;
}


/*
 * Create the destination file, acknowledge the client, and stream all incoming
 * bytes into the file until the client signals the end of its byte stream.
 */
static int receive_file(int connection_fd, const char *filename)
{
    FILE *dest_file = fopen(filename, "wb");

    if (dest_file == NULL) {
        perror("Could not open destination file");
        return -1;
    }

    /* TODO 7: Send the complete acknowledgement to the client. */
    // The destination file opened successfully, so confirm the filename.
    // write_all() loops over partial writes so all ACK_SIZE (10) bytes,
    // including the terminating NUL, reach the client.
    ssize_t ack_result = write_all(connection_fd, ACK_MESSAGE, ACK_SIZE);

    if (ack_result !=
        (ssize_t)ACK_SIZE) {
        perror("Could not send filename confirmation");
        fclose(dest_file);
        return -1;
    }

    printf("Receiving data for %s...\n", filename);
    // flush stdout to ensure the message is printed before receiving data
    fflush(stdout);

    char net_buff[BUFFER_SIZE]; // buffer for reading from the TCP socket
    unsigned long long total_received = 0;
    unsigned long long last_report_received = 0;
    struct timeval last_report = {0, 0};
    int status = 0;

    while (1) {
        // Read from the socket into the buffer
        // net_bytes_read is the number of bytes read from the socket, or -1 on error.
        // read() is a blocking call, so it will wait until data is available or the connection is closed.
        // The loop continues until the connection is closed or an error occurs.
        /* TODO 8: Read the next block from connection_fd. */
        // Read up to BUFFER_SIZE bytes; TCP may return fewer than the client
        // sent in one write, so we just process whatever arrives.
        ssize_t net_bytes_read =
            read(connection_fd, net_buff, sizeof(net_buff));

        if (net_bytes_read < 0) {
            if (errno == EINTR)
                continue;

            perror("Could not read from socket");
            status = -1;
            break;
        }

        // A return value of 0 means the client has ended its writing side and
        // all previously sent bytes have been read.
        if (net_bytes_read == 0)
            break;

        /* TODO 9: Write exactly net_bytes_read bytes to dest_file. */
        // Data may be binary, so write the exact byte count received rather
        // than treating net_buff as a NUL-terminated string.
        size_t file_bytes_written =
            fwrite(net_buff, 1, (size_t)net_bytes_read, dest_file);

        if (file_bytes_written !=
            (size_t)net_bytes_read) {
            perror("Could not write destination file");
            status = -1;
            break;
        }

        total_received += (unsigned long long)net_bytes_read;

        struct timeval now;
        if (gettimeofday(&now, NULL) == 0) {
            if (last_report.tv_sec == 0 && last_report.tv_usec == 0) {
                last_report = now;
                last_report_received = total_received;
            } else {
                /* TODO 10: Calculate elapsed microseconds since last_report. */
                // Convert the seconds difference to microseconds and add the
                // microsecond difference (which may be negative; the sum is
                // still correct).
                long elapsed_us =
                    (long)(now.tv_sec - last_report.tv_sec) * 1000000L +
                    (long)(now.tv_usec - last_report.tv_usec);

                if (elapsed_us >= 1000000L) {
                    unsigned long long bytes_this_window =
                        total_received - last_report_received;
                    double seconds = (double)elapsed_us / 1000000.0;
                    double throughput_bps = (double)bytes_this_window / seconds;
                    double throughput_mbps = (throughput_bps * 8.0) / 1000000.0;

                    printf("Received %llu bytes so far; throughput = %.2f Mbps\n",
                           total_received,
                           throughput_mbps);
                    fflush(stdout);
                    last_report = now;
                    last_report_received = total_received;
                }
            }
        }
    }

    if (fclose(dest_file) != 0) {
        perror("Could not close destination file");
        status = -1;
    }

    if (status == 0) {
        printf("Received %llu bytes.\n", total_received);
        printf("Saved as %s\n", filename);
    }

    return status;
}


/*
 * Main server loop:
 *  1) parse and validate the port number,
 *  2) create and bind the listening socket,
 *  3) accept clients one at a time,
 *  4) receive a filename and then the file content.
 */
int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    uint16_t port;

    if (parse_port(argv[1], &port) < 0) {
        fprintf(stderr, "Invalid port number: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    signal(SIGPIPE, SIG_IGN);

    // TODO 1: Create an IPv4 TCP socket for listening.
    // AF_INET = IPv4, SOCK_STREAM = reliable byte stream, 0 = default (TCP).
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd < 0) {
        perror("Could not create socket");
        return EXIT_FAILURE;
    }

    int reuse_address = 1;

    // Allow the socket to be reused immediately after the program exits.
    if (setsockopt(socket_fd,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &reuse_address,
                   sizeof(reuse_address)) < 0) {
        perror("Could not set SO_REUSEADDR");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr)); // Zero out the structure

    // TODO 2: Set the address family, local address, and network-order port.
    // INADDR_ANY (0.0.0.0) accepts connections on every local interface.
    // Address and port must be stored in network (big-endian) byte order.
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    // TODO 3: Bind socket_fd to serv_addr.
    // Associate the listening socket with the chosen local address and port.
    int bind_result = bind(socket_fd,
                           (struct sockaddr *)&serv_addr,
                           sizeof(serv_addr));

    if (bind_result < 0) {
        perror("Could not bind socket");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    // TODO 4: Put socket_fd into the listening state using BACKLOG.
    // Up to BACKLOG established connections may queue while waiting for
    // accept(); clients are still handled one at a time.
    int listen_result = listen(socket_fd, BACKLOG);

    if (listen_result < 0) {
        perror("Could not listen on socket");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    printf("Server listening on port %u...\n", (unsigned int)port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_length = sizeof(client_addr);

        // TODO 5: Accept a client and store its descriptor in connection_fd.
        // accept() blocks until a client connects and returns a new socket
        // dedicated to that client; socket_fd keeps listening for others.
        int connection_fd = accept(socket_fd,
                                   (struct sockaddr *)&client_addr,
                                   &client_length);

        if (connection_fd < 0) {
            if (errno == EINTR)
                continue;

            perror("Could not accept connection");
            break;
        }

        char client_ip[INET_ADDRSTRLEN];
        memset(client_ip, 0, sizeof(client_ip));
        // Convert the client's IP address to a human-readable string
        if (inet_ntop(AF_INET,
                      &client_addr.sin_addr,
                      client_ip,
                      sizeof(client_ip)) != NULL) {
            printf("Connection from %s:%u\n",
                   client_ip,
                   (unsigned int)ntohs(client_addr.sin_port));
        }

        char filename[FILENAME_SIZE];
        memset(filename, 0, sizeof(filename));

        // TODO 6: Receive and validate the null-terminated filename.
        // read_filename() reads until the NUL byte and rejects empty or
        // oversized names, so filename is always a valid C string on success.
        ssize_t filename_result =
            read_filename(connection_fd, filename, sizeof(filename));

        if (filename_result < 0) {
            perror("Could not receive destination filename");
            close(connection_fd);
            continue;
        }

        // Receive the file content and save it to the specified filename
        receive_file(connection_fd, filename);
        close(connection_fd);
    }

    close(socket_fd);
    return EXIT_FAILURE;
}
