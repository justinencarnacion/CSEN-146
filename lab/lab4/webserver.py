
# Behnam Dezfouli
# CSEN, Santa Clara University

# This program implements a simple web server that serves html and jpg files

# Input arguments:
# argv[1]: Sever's port number


from socket import *  # Import socket module
import sys            # To terminate the program


if __name__ == "__main__":

    # check if port number is provided
    if len(sys.argv) != 2:
        print('Usage: python %s <PortNumber>' % (sys.argv[0]))
        sys.exit()
    
    
    # STUDENT WORK
    # Port number comes from the command line
    serverPort = int(sys.argv[1])

    # Create a TCP socket
    serverSocket = socket(AF_INET, SOCK_STREAM)

    # To avoid "address already in use" error
    serverSocket.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)

    # Bind to all interfaces on the given port and listen for one connection at a time
    serverSocket.bind(('', serverPort))
    serverSocket.listen(1)


    # Server should be up and running and listening to the incoming connections
    while True:
        print('The server is ready to respond to incoming requests...')


        # STUDENT WORK
        # Block until a browser connects; returns a new socket for this client
        connectionSocket, addr = serverSocket.accept()


        try:

            # STUDENT WORK
            # Read the HTTP request from the browser
            message = connectionSocket.recv(1024).decode()

            # Browsers sometimes open a connection without sending anything
            if not message:
                connectionSocket.close()
                continue

            print('Received HTTP header:')
            print(message)

            # Request line looks like: GET /index.html HTTP/1.1
            filename = message.split()[1].lstrip('/')
            # No file named in the request: default to index.html
            if filename == '':
                filename = 'index.html'

            file_extension = filename.split('.')[-1]


            if (file_extension == 'html'):
                response_headers = { 'Content-Type': 'text/html; encoding=utf8' }
            elif (file_extension == 'jpg'):
                response_headers = { 'Content-Type': 'image/jpeg; encoding=utf8' }
            else:
                print('Invalid file type, we only support html and jpg!')
                connectionSocket.close()
                continue

            
            # STUDENT WORK
            # Read the requested file (raises IOError if it does not exist)
            f = open(filename, 'rb')
            content = f.read()
            f.close()

            # Tell the browser how many bytes to expect, and that we close after this reply
            response_headers['Content-Length'] = len(content)
            response_headers['Connection'] = 'close'

            # Status line, header lines, then a blank line
            response = 'HTTP/1.1 200 OK\r\n'
            for key, value in response_headers.items():
                response += '%s: %s\r\n' % (key, value)
            response += '\r\n'

            print('Prepared HTTP header:')
            print(response)

            # Send the header, then the file bytes, then close this connection
            connectionSocket.send(response.encode())
            connectionSocket.sendall(content)
            connectionSocket.close()

            
            
        #Raised when an I/O operation (such as a print statement, the built-in open() function or a method of a file object) fails for an I/O-related reason, e.g., "file not found" or "disk full"
        except IOError:
            # STUDENT WORK
            # Requested file doesn't exist: reply with a 404 status and a small HTML error page
            body = '<html><body><h1>404 Not Found</h1></body></html>'

            response = 'HTTP/1.1 404 Not Found\r\n'
            response += 'Content-Type: text/html; encoding=utf8\r\n'
            response += 'Content-Length: %d\r\n' % len(body)
            response += 'Connection: close\r\n'
            response += '\r\n'

            print('Prepared HTTP header:')
            print(response)

            connectionSocket.send(response.encode())
            connectionSocket.send(body.encode())
            connectionSocket.close()


    # STUDENT WORK
    # Release the listening socket before exiting
    serverSocket.close()
    sys.exit()
