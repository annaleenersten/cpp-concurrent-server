#include "Server.h"
#include "HttpRequest.h"
#include "Router.h"
#include "HttpResponse.h"

#include <iostream>
#include <cstring>
#include <csignal>
#include <cerrno>

// Linux-specific headers for epoll and nonblocking sockets.
#include <sys/epoll.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>


Server* serverInstance = nullptr;


// -------------------------------------------------------------
// Handle Ctrl+C.
// -------------------------------------------------------------

void handleSignal(int signal) {
    if (signal == SIGINT &&
        serverInstance != nullptr) {

        serverInstance->stop();
    }
}


// -------------------------------------------------------------
// Close a client connection and remove it from epoll.
// -------------------------------------------------------------

void Server::closeConnection(
    int epollFd,
    int clientSocket
) {
    epoll_ctl(
        epollFd,
        EPOLL_CTL_DEL,
        clientSocket,
        nullptr
    );

    close(clientSocket);

    connections.erase(clientSocket);
}


// -------------------------------------------------------------
// Accept all clients currently waiting on the listening socket.
// -------------------------------------------------------------

void Server::acceptClients(
    int epollFd,
    int serverSocket
) {
    while (true) {

        sockaddr_in clientAddress{};
        socklen_t clientAddressLength =
            sizeof(clientAddress);

        int clientSocket =
            accept(
                serverSocket,
                reinterpret_cast<sockaddr*>(
                    &clientAddress
                ),
                &clientAddressLength
            );

        // No more connections are currently waiting.
        if (clientSocket == -1) {

            if (
                errno == EAGAIN ||
                errno == EWOULDBLOCK
            ) {
                break;
            }

            std::cerr
                << "Failed to accept connection\n";

            break;
        }


        std::cout
            << "Client connected!\n";


        // Make the client socket nonblocking.
        int clientFlags =
            fcntl(
                clientSocket,
                F_GETFL,
                0
            );

        if (
            clientFlags == -1 ||
            fcntl(
                clientSocket,
                F_SETFL,
                clientFlags | O_NONBLOCK
            ) == -1
        ) {

            std::cerr
                << "Failed to make client socket nonblocking\n";

            close(clientSocket);
            continue;
        }


        // Store the connection so we can accumulate
        // partial HTTP requests.
        connections.emplace(
            clientSocket,
            Connection(clientSocket)
        );


        // Tell epoll to monitor this client.
        epoll_event clientEvent{};

        clientEvent.events =
            EPOLLIN |
            EPOLLRDHUP;

        clientEvent.data.fd =
            clientSocket;


        if (epoll_ctl(
                epollFd,
                EPOLL_CTL_ADD,
                clientSocket,
                &clientEvent
            ) == -1
        ) {

            std::cerr
                << "Failed to register client socket\n";

            connections.erase(clientSocket);
            close(clientSocket);
        }
    }
}


// -------------------------------------------------------------
// Handle events from an existing client socket.
// -------------------------------------------------------------

void Server::handleClientEvent(
    int epollFd,
    int clientSocket,
    uint32_t events
) {
    // The client disconnected or the socket encountered an error.
    if (
        events &
        (
            EPOLLERR |
            EPOLLHUP |
            EPOLLRDHUP
        )
    ) {
        closeConnection(
            epollFd,
            clientSocket
        );

        return;
    }


    // The client has data available.
    if (events & EPOLLIN) {

        readFromClient(
            epollFd,
            clientSocket
        );
    }
}


// -------------------------------------------------------------
// Read available data from a client.
// -------------------------------------------------------------

void Server::readFromClient(
    int epollFd,
    int clientSocket
) {
    auto connection =
        connections.find(clientSocket);

    if (connection == connections.end()) {
        return;
    }


    char buffer[4096];


    // Read everything currently available.
    while (true) {

        ssize_t bytesReceived =
            recv(
                clientSocket,
                buffer,
                sizeof(buffer),
                0
            );


        // Data was received.
        if (bytesReceived > 0) {

            connection
                ->second
                .requestData
                .append(
                    buffer,
                    bytesReceived
                );

            continue;
        }


        // The client closed the connection.
        if (bytesReceived == 0) {

            closeConnection(
                epollFd,
                clientSocket
            );

            return;
        }


        // No more data is currently available.
        if (
            errno == EAGAIN ||
            errno == EWOULDBLOCK
        ) {
            break;
        }


        // An actual socket error occurred.
        std::cerr
            << "Failed to receive client data\n";

        closeConnection(
            epollFd,
            clientSocket
        );

        return;
    }


    // Check whether the connection still exists.
    connection =
        connections.find(clientSocket);

    if (connection == connections.end()) {
        return;
    }


    const std::string& requestData =
        connection
            ->second
            .requestData;


    // ---------------------------------------------------------
    // Check whether the HTTP headers are complete.
    // ---------------------------------------------------------

    std::size_t headerEnd =
        requestData.find(
            "\r\n\r\n"
        );


    if (headerEnd == std::string::npos) {

        // The request is incomplete.
        //
        // Keep the data and wait for another EPOLLIN event.
        return;
    }


    // ---------------------------------------------------------
    // Determine how much body data we expect.
    // ---------------------------------------------------------

    HttpRequest request(
        requestData
    );


    std::size_t contentLength =
        request.getContentLength();


    std::size_t bodyStart =
        headerEnd + 4;


    std::size_t bodyBytesReceived =
        requestData.size() -
        bodyStart;


    // The body has not arrived yet.
    if (
        bodyBytesReceived <
        contentLength
    ) {
        return;
    }


    // ---------------------------------------------------------
    // The complete request has arrived.
    // ---------------------------------------------------------

    std::string completeRequest =
        requestData;


    // This server handles one request per connection,
    // so the socket no longer needs to be monitored by epoll.
    epoll_ctl(
        epollFd,
        EPOLL_CTL_DEL,
        clientSocket,
        nullptr
    );


    connections.erase(
        clientSocket
    );


    // Give the complete request to the thread pool.
    threadPool.enqueue(
        [this, clientSocket, completeRequest]() {

            handleClient(
                clientSocket,
                completeRequest
            );
        }
    );
}


// -------------------------------------------------------------
// Process a complete HTTP request.
//
// At this point epoll has already handled the network I/O.
// The worker thread handles the actual request processing.
// -------------------------------------------------------------

void Server::handleClient(
    int clientSocket,
    const std::string& requestData
) {
    HttpRequest request(
        requestData
    );


    std::cout
        << "Received:\n"
        << requestData
        << '\n';


    std::cout
        << "Method: "
        << request.getMethod()
        << '\n';


    std::cout
        << "Path: "
        << request.getPath()
        << '\n';


    std::cout
        << "Version: "
        << request.getVersion()
        << '\n';


    // Route the request.
    Router router(cache);

    HttpResponse response =
        router.route(request);


    // Build the HTTP response.
    std::string responseData =
        "HTTP/1.1 " +
        std::to_string(
            response.getStatusCode()
        ) +
        " " +
        response.getStatusText() +
        "\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: " +
        std::to_string(
            response.getBody().size()
        ) +
        "\r\n"
        "\r\n" +
        response.getBody();


    // ---------------------------------------------------------
    // Send the response.
    //
    // The socket is nonblocking, so send() may write only
    // part of the response.
    // ---------------------------------------------------------

    std::size_t totalSent = 0;

    while (
        totalSent <
        responseData.size()
    ) {

        ssize_t bytesSent =
            send(
                clientSocket,
                responseData.data() + totalSent,
                responseData.size() - totalSent,
                0
            );


        if (bytesSent > 0) {

            totalSent +=
                static_cast<std::size_t>(
                    bytesSent
                );

            continue;
        }


        if (
            errno == EAGAIN ||
            errno == EWOULDBLOCK
        ) {

            std::cerr
                << "Socket not ready for sending\n";

            break;
        }


        std::cerr
            << "Failed to send response\n";

        break;
    }


    // This server handles one HTTP request per connection.
    close(clientSocket);
}


// -------------------------------------------------------------
// Start the server.
// -------------------------------------------------------------

void Server::start() {

    serverInstance = this;

    std::signal(
        SIGINT,
        handleSignal
    );


    // ---------------------------------------------------------
    // Create the TCP listening socket.
    // ---------------------------------------------------------

    int serverSocket =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );


    if (serverSocket == -1) {

        std::cerr
            << "Failed to create socket\n";

        return;
    }


    // Allow the address to be reused after restarting.
    int reuse = 1;

    if (setsockopt(
            serverSocket,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuse,
            sizeof(reuse)
        ) == -1
    ) {

        std::cerr
            << "Failed to configure socket\n";

        close(serverSocket);
        return;
    }


    // Make the listening socket nonblocking.
    int serverFlags =
        fcntl(
            serverSocket,
            F_GETFL,
            0
        );


    if (
        serverFlags == -1 ||
        fcntl(
            serverSocket,
            F_SETFL,
            serverFlags | O_NONBLOCK
        ) == -1
    ) {

        std::cerr
            << "Failed to make server socket nonblocking\n";

        close(serverSocket);
        return;
    }


    // Configure the server address.
    sockaddr_in serverAddress{};

    serverAddress.sin_family =
        AF_INET;

    serverAddress.sin_addr.s_addr =
        INADDR_ANY;

    serverAddress.sin_port =
        htons(8080);


    // Bind to port 8080.
    if (bind(
            serverSocket,
            reinterpret_cast<sockaddr*>(
                &serverAddress
            ),
            sizeof(serverAddress)
        ) == -1
    ) {

        std::cerr
            << "Failed to bind socket\n";

        close(serverSocket);
        return;
    }


    // Start listening.
    if (listen(
            serverSocket,
            10
        ) == -1
    ) {

        std::cerr
            << "Failed to listen on socket\n";

        close(serverSocket);
        return;
    }


    // ---------------------------------------------------------
    // Create the epoll instance.
    // ---------------------------------------------------------

    int epollFd =
        epoll_create1(0);


    if (epollFd == -1) {

        std::cerr
            << "Failed to create epoll instance\n";

        close(serverSocket);
        return;
    }


    // Register the listening socket.
    epoll_event serverEvent{};

    serverEvent.events =
        EPOLLIN;

    serverEvent.data.fd =
        serverSocket;


    if (epoll_ctl(
            epollFd,
            EPOLL_CTL_ADD,
            serverSocket,
            &serverEvent
        ) == -1
    ) {

        std::cerr
            << "Failed to register server socket with epoll\n";

        close(epollFd);
        close(serverSocket);

        return;
    }


    std::cout
        << "Server listening on port 8080...\n";


    constexpr int MAX_EVENTS = 64;

    epoll_event events[MAX_EVENTS];


    // ---------------------------------------------------------
    // Main event loop.
    // ---------------------------------------------------------

    while (running) {

        int eventCount =
            epoll_wait(
                epollFd,
                events,
                MAX_EVENTS,
                1000
            );


        if (eventCount == -1) {

            if (errno == EINTR) {
                continue;
            }

            std::cerr
                << "Failed to wait for epoll events\n";

            break;
        }


        for (int i = 0; i < eventCount; i++) {

            int socketFd =
                events[i].data.fd;


            // New connection.
            if (socketFd == serverSocket) {

                acceptClients(
                    epollFd,
                    serverSocket
                );

                continue;
            }


            // Existing client connection.
            handleClientEvent(
                epollFd,
                socketFd,
                events[i].events
            );
        }
    }


    // ---------------------------------------------------------
    // Clean up remaining client connections.
    // ---------------------------------------------------------

    for (auto& [socketFd, connection] :
         connections) {

        close(socketFd);
    }

    connections.clear();


    close(epollFd);
    close(serverSocket);


    std::cout
        << "Server shut down.\n";
}


// -------------------------------------------------------------
// Stop the server.
// -------------------------------------------------------------

void Server::stop() {
    running = false;
}