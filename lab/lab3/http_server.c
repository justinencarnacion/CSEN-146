//  Created by Behnam Dezfouli
//  CSEN, Santa Clara University
//

// This program implements a web server
//
// The input arguments are as follows:
// argv[1]: Sever's port number


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h> // to get file size


#define COMMAND_BUFF    15000       // Size of the buffer used to store the result of command execution
#define SOCK_READ_BUFF  4096        // Size of the buffer used to store the bytes read over socket
#define REPLY_BUFF      20000       // Size of the buffer used to store the message sent to client
#define FILE_READ       10

#define HTML_FILE       "index.html"


// Socket descriptors
int        socket_fd = 0;            // socket descriptor
int        connection_fd = 0;        // new connection descriptor


void  INThandler(int sig)
{
    char  input;
    
    signal(sig, SIG_IGN);
    printf("Did you hit Ctrl-C?\n"
           "Do you want to quit? [y/n] ");
    
    input = getchar();
    
    input = getchar();
    
    
    // STUDENT WORK

    // User confirmed: close the client connection and the listening socket, then exit
    if (input == 'y' || input == 'Y')
    {
        shutdown (connection_fd, SHUT_RDWR);
        close (connection_fd);
        
        shutdown (socket_fd, SHUT_RDWR);
        close (socket_fd);
        
        exit(0);
    }
    // Otherwise keep running and re-register the handler for the next Ctrl-C
    else
    {
        signal(SIGINT, INThandler);
    }

}


// main function ---------------
int main (int argc, char *argv[])
{
    
    // Register a function to handle SIGINT ( SIGNINT is interrupt the process )
    signal(SIGINT, INThandler);
    
    
    int        net_bytes_read;                // numer of bytes received over socket
    struct     sockaddr_in serv_addr;         // Address format structure
    char       net_buff[SOCK_READ_BUFF];      // buffer to hold characters read from socket
    char       message_buff[REPLY_BUFF];      // buffer to hold the message to be sent to the client
    
    char       file_buff[FILE_READ];          // to hold the bytes read from source file
    FILE       *source_file;                  // pointer to the source file
    
    
    // pointer to the file that will include the received bytes over socket
    FILE  *dest_file;
    
    
    if (argc < 2) // Note: the name of the program is counted as an argument
    {
        printf ("Port number not specified!\n");
        return 1;
    }
    
    
    
    // STUDENT WORK

    // Clear the address structure and fill in the server address
    memset (&serv_addr, 0, sizeof (serv_addr));
    serv_addr.sin_family = AF_INET;                   // IPv4
    serv_addr.sin_addr.s_addr = htonl (INADDR_ANY);   // listen on all interfaces
    serv_addr.sin_port = htons (atoi (argv[1]));      // port number from command line

    // STUDENT WORK



    // Create socket
    if ((socket_fd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf ("Error: Could not create socket! \n");
        return 1;
    }
    
    // To prevent "Address in use" error
    // The SO_REUSEADDR socket option, which explicitly allows a process to bind to a port which remains in TIME_WAIT
    // STUDENT WORK
    // Turn on SO_REUSEADDR so the server can restart on the same port right away
    int reuse = 1;
    if (setsockopt (socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof (reuse)) < 0)
    {
        perror("setsockopt(SO_REUSEADDR) failed!");
        exit(EXIT_FAILURE);
    }

    
    // bind it to all interfaces, on the specified port number
    // STUDENT WORK
    // Attach the socket to the address/port in serv_addr; fails if the port is taken
    if (bind (socket_fd, (struct sockaddr *)&serv_addr, sizeof (serv_addr)) < 0)
    {
        perror("Bind failed!");
        exit(EXIT_FAILURE);
    }
    
    
    // Accept up to 1 connections
    if (listen(socket_fd, 1) < 0)
    {
        perror("Listen failed!");
        exit(EXIT_FAILURE);
    }
    
    printf ("Listening to incoming connections... \n");
    
    
    unsigned int option = 0; // Variable to hold user option
    printf("1: System network configuration \n2: Regular HTTP server\n");
    scanf("%u", &option);
    
    // The web server returns current processor and memory utilization
    if ( option == 1 )
    {
        printf("System network configuration (using ifconfig)... \n");
    }
    // The requested file is returned
    else if (option == 2)
    {
        printf("Regular server (only serving html files)... \n\n");
    }
    else
        printf ("Invalid Option! \n\n");
    
    
    while (1)
    {
        // Accept incoming connection request from client
        // STUDENT WORK
        // Block until a browser connects; accept returns a new socket for this client
        if ((connection_fd = accept (socket_fd, (struct sockaddr *)NULL, NULL)) < 0)
        {
            perror("Accept failed!");
            continue;
        }
        
        printf ("Incoming connection: Accepted! \n");
        
        memset (net_buff, '\0', sizeof (net_buff));
        
        // Read the HTTP request sent by the browser and print it
        net_bytes_read = read (connection_fd, net_buff, sizeof (net_buff) - 1);
        if (net_bytes_read > 0)
            printf ("Received request:\n%s\n", net_buff);
        
        // Return system utilization info
        if ( option == 1 )
        {
            // run a command -- we run ifconfig here (you can try other commands as well)
            FILE *system_info = popen("ifconfig", "r");
            
            
            
            // STUDENT WORK
            // Read the ifconfig output into a buffer, then send it as a text/plain HTTP reply
            char   command_buff[COMMAND_BUFF];    // output of ifconfig
            size_t command_len = 0;               // number of bytes of output
            size_t bytes;
            
            if (system_info != NULL)
            {
                // Read the command output until EOF or the buffer is full
                while (command_len < sizeof (command_buff) &&
                       (bytes = fread (command_buff + command_len, 1,
                                       sizeof (command_buff) - command_len, system_info)) > 0)
                    command_len += bytes;
                pclose (system_info);
            }
            
            // Header: status line, content type, content length, then a blank line
            int header_len = snprintf (message_buff, sizeof (message_buff),
                                       "HTTP/1.1 200 OK\r\n"
                                       "Content-Type: text/plain\r\n"
                                       "Content-Length: %zu\r\n"
                                       "Connection: close\r\n\r\n",
                                       command_len);
            
            // Append the command output after the header and send both in one write
            memcpy (message_buff + header_len, command_buff, command_len);
            write (connection_fd, message_buff, header_len + command_len);
            
            printf("Reply sent to the client!\n");
            
            shutdown (connection_fd, SHUT_RDWR);
            close (connection_fd);
        }
        
        else if (option == 2)
        {
            // To get the size of html file
            struct stat sbuf;      /* file status */
            
            // make sure the file exists
            // HTML_FILE is index.html and is statically defined
            if (stat(HTML_FILE, &sbuf) < 0) {
                // STUDENT WORK
                // File is missing: reply with a 404 status and a small HTML error page
                char *not_found = "<html><body><h1>404 Not Found</h1>"
                                  "<p>The requested file was not found on this server.</p></body></html>";
                
                int len = snprintf (message_buff, sizeof (message_buff),
                                    "HTTP/1.1 404 Not Found\r\n"
                                    "Content-Type: text/html\r\n"
                                    "Content-Length: %zu\r\n"
                                    "Connection: close\r\n\r\n"
                                    "%s",
                                    strlen (not_found), not_found);
                write (connection_fd, message_buff, len);
                
                printf ("Error: %s not found, 404 sent to the client!\n", HTML_FILE);

                // Close this connection and go back to waiting for the next client
                shutdown (connection_fd, SHUT_RDWR);
                close (connection_fd);
                continue;
            }
            
            
            // Open the source file
            if ((source_file = fopen (HTML_FILE, "rb")) == NULL)
            {
                printf ("Error: could not open the source file!\n");
                
                // STUDENT WORK
                // File exists but can't be opened: reply with 500, then shut the server down
                char *server_error = "<html><body><h1>500 Internal Server Error</h1></body></html>";
                
                int len = snprintf (message_buff, sizeof (message_buff),
                                    "HTTP/1.1 500 Internal Server Error\r\n"
                                    "Content-Type: text/html\r\n"
                                    "Content-Length: %zu\r\n"
                                    "Connection: close\r\n\r\n"
                                    "%s",
                                    strlen (server_error), server_error);
                write (connection_fd, message_buff, len);
                
                shutdown (connection_fd, SHUT_RDWR);
                close (connection_fd);
                close (socket_fd);
                
                return 1;
            }
            
            else
            {
                
                
                // STUDENT WORK
                size_t bytes;    // number of bytes read from the file in each chunk

                // Send the header first; the length comes from stat()
                int header_len = snprintf (message_buff, sizeof (message_buff),
                                           "HTTP/1.1 200 OK\r\n"
                                           "Content-Type: text/html\r\n"
                                           "Content-Length: %lld\r\n"
                                           "Connection: close\r\n\r\n",
                                           (long long) sbuf.st_size);
                write (connection_fd, message_buff, header_len);
                
                // Then send the file contents, FILE_READ bytes at a time
                while ((bytes = fread (file_buff, 1, sizeof (file_buff), source_file)) > 0)
                    write (connection_fd, file_buff, bytes);
                
                fclose (source_file);
                
                printf("Reply sent to the client!\n");
            }
            
            shutdown (connection_fd, SHUT_RDWR);
            close (connection_fd);
        }
    }
    
    close (socket_fd);
}
