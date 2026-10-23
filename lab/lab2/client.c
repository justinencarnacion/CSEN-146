//
// COEN 146L - Lab 2
// TCP file-transfer client
// Student starter: complete each numbered TODO.
//
// Usage:
//   ./client <server_ip> <server_port> <source_file> <destination_file>
//

#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define BUFFER_SIZE 4096
#define FILENAME_SIZE 1024
#define ACK_MESSAGE "received!"
#define ACK_SIZE (sizeof(ACK_MESSAGE))
#define REPORT_INTERVAL_SECONDS 1.0

/*
 * Parse and validate a command-line port value.
 * Accepts only integers in the valid TCP/UDP range of 1..65535.
 */
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
 * Read exactly the requested number of bytes from a socket.
 * This helper keeps reading until the full payload arrives or the
 * connection is closed.
 */
static ssize_t read_all(int socket_fd, void *buffer, size_t length)
{
    size_t total = 0;
    char *position = buffer;

    while (total < length) {
        ssize_t bytes_read =
            read(socket_fd, position + total, length - total);

        if (bytes_read < 0) {
            if (errno == EINTR)
                continue;

            return -1;
        }

        if (bytes_read == 0)
            break;

        total += (size_t)bytes_read;
    }

    return (ssize_t)total;
}

/*
 * Send a complete buffer over the socket.
 * TCP may deliver partial writes, so we continue until all bytes are sent.
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
 * Wait for the server to close its write side.
 * This indicates the server has finished sending any remaining data.
 */
static int wait_for_server_close(int socket_fd)
{
    char buffer[64];

    while (1) {
        ssize_t bytes_read = read(socket_fd, buffer, sizeof(buffer));

        if (bytes_read > 0)
            continue;

        if (bytes_read == 0)
            return 0;

        if (errno != EINTR)
            return -1;
    }
}

/*
 * Main client entry point.
 * The program validates input, sets up a TCP connection, sends the
 * destination filename, streams the file contents, and waits for the
 * server to finish the transfer.
 */
int main(int argc, char *argv[])
{
    /* Make progress visible immediately, including when stdout is piped. */
    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc != 5) {
        fprintf(stderr,
                "Usage: %s <server_ip> <server_port> "
                "<source_file> <destination_file>\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    uint16_t port;

    /* Validate the port before creating the socket. */
    if (parse_port(argv[2], &port) < 0) {
        fprintf(stderr, "Invalid port number: %s\n", argv[2]);
        return EXIT_FAILURE;
    }

    if (strlen(argv[4]) >= FILENAME_SIZE) {
        fprintf(stderr,
                "Destination filename must be shorter than %d characters.\n",
                FILENAME_SIZE);
        return EXIT_FAILURE;
    }

    /* Open the input file in binary mode to preserve raw data bytes. */
    FILE *source_file = fopen(argv[3], "rb");

    if (source_file == NULL) {
        perror("Could not open source file");
        return EXIT_FAILURE;
    }

    /* Ignore SIGPIPE so a broken connection does not terminate the client. */
    signal(SIGPIPE, SIG_IGN);

    /* TODO 1: Create an IPv4 TCP socket for the file-transfer connection. */
    /* AF_INET = IPv4, SOCK_STREAM = reliable byte stream, 0 = default (TCP). */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("Could not create socket");
        fclose(source_file);
        return EXIT_FAILURE;
    }

    /* Fill in the server address structure for the IPv4 TCP connection. */
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));

    /* TODO 2: Set the IPv4 address family and port in network byte order. */
    /* htons() converts the 16-bit port to network (big-endian) byte order. */
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    /* TODO 3: Convert argv[1] from text to a binary IPv4 address. */
    /* inet_pton() turns "a.b.c.d" text into the binary network-order
     * address; it returns 1 only on success. */
    int address_result = inet_pton(AF_INET, argv[1], &serv_addr.sin_addr);

    if (address_result != 1) {
        fprintf(stderr, "Invalid IPv4 address: %s\n", argv[1]);
        close(sockfd); // close the socket before returning if inet_pton fails
        fclose(source_file);
        return EXIT_FAILURE;
    }

    /* TODO 4: Connect sockfd to the address stored in serv_addr. */
    /* Perform the TCP three-way handshake with the server. */
    int connect_result = connect(sockfd,
                                 (struct sockaddr *)&serv_addr,
                                 sizeof(serv_addr));

    if (connect_result < 0) {
        perror("Could not connect to server");
        close(sockfd); // close the socket before returning if connect fails
        fclose(source_file);
        return EXIT_FAILURE;
    }

    /* Send the destination filename to the server and wait for ACK. */
    size_t filename_bytes = strlen(argv[4]) + 1;

    // TODO 5: Send the destination filename, including the null terminator.
    // filename_bytes includes the trailing NUL so the server knows where
    // the filename ends in the byte stream; write_all() handles partial writes.
    ssize_t filename_result = write_all(sockfd, argv[4], filename_bytes);

    if (filename_result !=
        (ssize_t)filename_bytes) {
        perror("Could not send destination filename");
        close(sockfd); // close the socket before returning if write_all fails
        fclose(source_file);
        return EXIT_FAILURE;
    }

    char net_buff[ACK_SIZE + 1] = {0};

    /* TODO 6: Read exactly ACK_SIZE bytes into net_buff. */
    /* A single read() may return fewer than 10 bytes, so read_all() loops
     * until the full "received!" ACK (with NUL) arrives or the server closes. */
    ssize_t ack_bytes = read_all(sockfd, net_buff, ACK_SIZE);

    if (ack_bytes != (ssize_t)ACK_SIZE ||
        memcmp(net_buff, ACK_MESSAGE, ACK_SIZE) != 0) {
        fprintf(stderr,
                "The server did not confirm the destination filename.\n");
        if (ack_bytes > 0) {
            net_buff[ack_bytes < (ssize_t)sizeof(net_buff)
                     ? (size_t)ack_bytes
                     : sizeof(net_buff) - 1] = '\0';
            printf("Received ACK from server: %s\n", net_buff);
        } else {
            printf("Received no ACK from server.\n");
        }
        close(sockfd);
        fclose(source_file);
        return EXIT_FAILURE;
    }

    printf("Received ACK from server: %s\n", net_buff);
    printf("Server accepted the destination filename.\n");
    printf("Sending file content (throughput updates every %.1f s)...\n",
           REPORT_INTERVAL_SECONDS);
    fflush(stdout);

    /* Stream the file in fixed-size chunks to reduce memory usage. */
    char file_buff[BUFFER_SIZE];
    unsigned long long total_sent = 0;
    unsigned long long interval_sent = 0;
    struct timespec interval_start;

    if (clock_gettime(CLOCK_MONOTONIC, &interval_start) < 0) {
        perror("Could not start throughput timer");
        close(sockfd);
        fclose(source_file);
        return EXIT_FAILURE;
    }

    while (1) {
        /* 1. Read and send one chunk. write_all handles partial writes.
         * If the socket stalls, reporting waits for this write to finish. */
        /* TODO 7: Read the next block from source_file into file_buff. */
        /* fread() returns how many bytes were read; 0 means EOF (or an error,
         * which ferror() checks below). Size 1 keeps it byte-exact for binary. */
        size_t bytes_read =
            fread(file_buff, 1, sizeof(file_buff), source_file);
        if (ferror(source_file)) {
            perror("Could not read source file");
            close(sockfd);
            fclose(source_file);
            return EXIT_FAILURE;
        }

        /* TODO 8: Send the complete block with write_all(). */
        /* Send exactly bytes_read bytes; at EOF this is a harmless 0-byte send. */
        ssize_t data_write_result = write_all(sockfd, file_buff, bytes_read);

        if (data_write_result < 0) {
            perror("Could not send file data");
            close(sockfd);
            fclose(source_file);
            return EXIT_FAILURE;
        }

        /* TODO 9: Update total_sent and interval_sent. */
        /* total_sent is cumulative; interval_sent resets after each report. */
        total_sent += bytes_read;
        interval_sent += bytes_read;

        /* 3. Measure elapsed time using a monotonic clock. */
        struct timespec now;
        if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
            perror("Could not read throughput timer");
            close(sockfd);
            fclose(source_file);
            return EXIT_FAILURE;
        }

        /* TODO 10: Calculate elapsed seconds using now and interval_start. */
        /* Combine whole seconds and the nanosecond difference into one
         * floating-point value of elapsed seconds. */
        double elapsed =
            (double)(now.tv_sec - interval_start.tv_sec) +
            (double)(now.tv_nsec - interval_start.tv_nsec) / 1e9;

        /* 4. Report every 1 second, plus any remaining bytes at EOF. */
        if (elapsed >= REPORT_INTERVAL_SECONDS ||
            (bytes_read == 0 && interval_sent > 0)) {
            /* Mbps = bytes * 8 bits/byte / seconds / 1,000,000. */
            double mbps = elapsed > 0.0
                              ? (double)interval_sent * 8.0 / elapsed / 1e6
                              : 0.0;
            printf("Sent %llu bytes in %.3f s | Throughput: %.3f Mbps"
                   " | Total: %llu bytes\n",
                   interval_sent, elapsed, mbps, total_sent);
            fflush(stdout);
            interval_sent = 0;
            interval_start = now;
        }

        if (bytes_read == 0)
            break;
    }

    if (fclose(source_file) != 0) {
        perror("Could not close source file");
        close(sockfd); // close the socket before returning if fclose fails
        return EXIT_FAILURE;
    }

    /* Signal end-of-file to the server while still allowing reads. */
    if (shutdown(sockfd, SHUT_WR) < 0) {
        perror("Could not finish sending");
        close(sockfd); // close the socket before returning if shutdown fails
        return EXIT_FAILURE;
    }

    /* Wait until the server closes its side, confirming transfer completion. */
    printf("All file data submitted to TCP; waiting for server completion...\n");
    if (wait_for_server_close(sockfd) < 0) {
        perror("Error while waiting for server");
        close(sockfd); // close the socket before returning if wait_for_server_close fails
        return EXIT_FAILURE;
    }

    close(sockfd);

    printf("Sent %llu bytes.\n", total_sent);
    printf("Done.\n");
    return EXIT_SUCCESS;
}
