#pragma once

#include "ThreadPool.h"
#include "Cache.h"

#include <atomic>
#include <string>
#include <unordered_map>

class Server {
public:
    void start();
    void stop();

private:

    ThreadPool threadPool{4};
    Cache cache{1000};
    std::atomic<bool> running{true};

    // Stores the state of one connected client.
    //
    // TCP data may arrive in multiple pieces, so we keep
    // everything received from the client here until the
    // complete HTTP request has arrived.
    struct Connection {
        int socket;
        std::string requestData;

        explicit Connection(int socket)
            : socket(socket) {}
    };

    // Keep track of all connected client sockets.
    //
    // The socket file descriptor is the key, and the
    // Connection stores that client's accumulated request data.
    std::unordered_map<int, Connection> connections;

    void acceptClients(int epollFd, int serverSocket);

    void handleClientEvent(int epollFd, int clientSocket, uint32_t events);

    void readFromClient(int epollFd, int clientSocket);

    void processRequest(int clientSocket, const std::string& requestData);

    void closeConnection(int epollFd, int clientSocket);

    void handleClient(int clientSocket, const std::string& requestData);
 
};