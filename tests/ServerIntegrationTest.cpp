#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

std::string sendRequest(const std::string& request) {
    int socketFd = socket(AF_INET, SOCK_STREAM, 0);

    EXPECT_NE(socketFd, -1);

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);

    inet_pton(
        AF_INET,
        "127.0.0.1",
        &serverAddress.sin_addr
    );

    EXPECT_EQ(
        connect(
            socketFd,
            reinterpret_cast<sockaddr*>(&serverAddress),
            sizeof(serverAddress)
        ),
        0
    );

    ssize_t bytesSent = send(
        socketFd,
        request.c_str(),
        request.size(),
        0
    );

    EXPECT_EQ(
        bytesSent,
        static_cast<ssize_t>(request.size())
    );

    char buffer[4096];
    std::string response;

    while (true) {
        ssize_t bytesReceived = recv(
            socketFd,
            buffer,
            sizeof(buffer),
            0
        );

        if (bytesReceived <= 0) {
            break;
        }

        response.append(buffer, bytesReceived);
    }

    close(socketFd);

    return response;
}

// Connect to the running server and return the socket.
int connectToServer() {
    int socketFd =
        socket(AF_INET, SOCK_STREAM, 0);

    EXPECT_NE(socketFd, -1);

    sockaddr_in serverAddress{};

    serverAddress.sin_family =
        AF_INET;

    serverAddress.sin_port =
        htons(8080);

    inet_pton(
        AF_INET,
        "127.0.0.1",
        &serverAddress.sin_addr
    );

    int result =
        connect(
            socketFd,
            reinterpret_cast<sockaddr*>(
                &serverAddress
            ),
            sizeof(serverAddress)
        );

    EXPECT_EQ(result, 0);

    return socketFd;
}

TEST(ServerIntegrationTest, PutThenGet) {
    std::string putRequest =
        "PUT /cache/test HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "Hello";

    std::string putResponse = sendRequest(putRequest);

    EXPECT_NE(
        putResponse.find("200 OK"),
        std::string::npos
    );

    std::string getRequest =
        "GET /cache/test HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    std::string getResponse = sendRequest(getRequest);

    EXPECT_NE(
        getResponse.find("200 OK"),
        std::string::npos
    );

    EXPECT_NE(
        getResponse.find("Hello"),
        std::string::npos
    );
}

TEST(ServerIntegrationTest, GetMissingKeyReturns404) {
    std::string getRequest =
        "GET /cache/missing HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    std::string response = sendRequest(getRequest);

    EXPECT_NE(
        response.find("404 Not Found"),
        std::string::npos
    );
}

TEST(ServerIntegrationTest, DeleteThenGetReturns404) {
    std::string putRequest =
        "PUT /cache/delete-test HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "Hello";

    std::string putResponse = sendRequest(putRequest);

    EXPECT_NE(
        putResponse.find("200 OK"),
        std::string::npos
    );

    std::string deleteRequest =
        "DELETE /cache/delete-test HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    std::string deleteResponse = sendRequest(deleteRequest);

    EXPECT_NE(
        deleteResponse.find("200 OK"),
        std::string::npos
    );

    std::string getRequest =
        "GET /cache/delete-test HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    std::string getResponse = sendRequest(getRequest);

    EXPECT_NE(
        getResponse.find("404 Not Found"),
        std::string::npos
    );
}

TEST(ServerIntegrationTest, UnsupportedMethodReturns405) {
    std::string request =
        "POST /cache/test HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    std::string response = sendRequest(request);

    EXPECT_NE(
        response.find("405 Method Not Allowed"),
        std::string::npos
    );
}

TEST(ServerIntegrationTest, HandlesLargeRequestBody) {
    std::string largeBody(10000, 'A');

    std::string request =
        "PUT /cache/large HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: " +
        std::to_string(largeBody.size()) +
        "\r\n"
        "\r\n" +
        largeBody;

    std::string putResponse = sendRequest(request);

    EXPECT_NE(
        putResponse.find("200 OK"),
        std::string::npos
    );

    std::string getRequest =
        "GET /cache/large HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    std::string getResponse = sendRequest(getRequest);

    EXPECT_NE(
        getResponse.find("200 OK"),
        std::string::npos
    );

    EXPECT_NE(
        getResponse.find(largeBody),
        std::string::npos
    );
}

TEST(ServerIntegrationTest, HandlesMultipleClients) {
    std::string request1 =
        "PUT /cache/client1 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "One!!";

    std::string request2 =
        "PUT /cache/client2 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "Two!!";

    std::string response1 = sendRequest(request1);
    std::string response2 = sendRequest(request2);

    EXPECT_NE(
        response1.find("200 OK"),
        std::string::npos
    );

    EXPECT_NE(
        response2.find("200 OK"),
        std::string::npos
    );

    std::string getRequest1 =
        "GET /cache/client1 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    std::string getRequest2 =
        "GET /cache/client2 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    std::string getResponse1 = sendRequest(getRequest1);
    std::string getResponse2 = sendRequest(getRequest2);

    EXPECT_NE(
        getResponse1.find("One!!"),
        std::string::npos
    );

    EXPECT_NE(
        getResponse2.find("Two!!"),
        std::string::npos
    );
}

TEST(ServerIntegrationTest, EmptyCacheKeyReturns400) {
    std::string request =
        "GET /cache/ HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    std::string response = sendRequest(request);

    EXPECT_NE(
        response.find("400 Bad Request"),
        std::string::npos
    );
}

TEST(ServerIntegrationTest, UnknownRouteReturns404) {
    std::string request =
        "GET /does-not-exist HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    std::string response = sendRequest(request);

    EXPECT_NE(
        response.find("404 Not Found"),
        std::string::npos
    );
}

TEST(ServerIntegrationTest, InvalidHttpVersionReturns400) {
    std::string request =
        "GET /hello HTTP/2.0\r\n"
        "Host: localhost\r\n"
        "\r\n";

    std::string response = sendRequest(request);

    EXPECT_NE(
        response.find("400 Bad Request"),
        std::string::npos
    );
}

TEST(ServerIntegrationTest, MalformedRequestReturns400) {
    std::string request =
        "GET\r\n"
        "\r\n";

    std::string response = sendRequest(request);

    EXPECT_NE(
        response.find("400 Bad Request"),
        std::string::npos
    );
}

TEST(ServerIntegrationTest, StoresEmptyValue) {
    std::string putRequest =
        "PUT /cache/empty HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 0\r\n"
        "\r\n";

    std::string putResponse = sendRequest(putRequest);

    EXPECT_NE(
        putResponse.find("200 OK"),
        std::string::npos
    );

    std::string getRequest =
        "GET /cache/empty HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    std::string getResponse = sendRequest(getRequest);

    EXPECT_NE(
        getResponse.find("200 OK"),
        std::string::npos
    );
}

TEST(ServerIntegrationTest, UpdatesExistingValue) {
    std::string firstPut =
        "PUT /cache/update HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 3\r\n"
        "\r\n"
        "Old";

    std::string firstResponse = sendRequest(firstPut);

    EXPECT_NE(
        firstResponse.find("200 OK"),
        std::string::npos
    );

    std::string secondPut =
        "PUT /cache/update HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 3\r\n"
        "\r\n"
        "New";

    std::string secondResponse = sendRequest(secondPut);

    EXPECT_NE(
        secondResponse.find("200 OK"),
        std::string::npos
    );

    std::string getRequest =
        "GET /cache/update HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    std::string getResponse = sendRequest(getRequest);

    EXPECT_NE(
        getResponse.find("New"),
        std::string::npos
    );

    EXPECT_EQ(
        getResponse.find("Old"),
        std::string::npos
    );
}

// Receive the complete response from the server.
std::string receiveResponse(int socketFd) {

    char buffer[4096];

    std::string response;

    while (true) {

        ssize_t bytesReceived =
            recv(
                socketFd,
                buffer,
                sizeof(buffer),
                0
            );

        if (bytesReceived <= 0) {
            break;
        }

        response.append(
            buffer,
            bytesReceived
        );
    }

    return response;
}


// -------------------------------------------------------------
// Test that a request can arrive in multiple pieces.
//
// This is important for the epoll design because TCP does not
// guarantee that one send() on the client corresponds to one
// recv() on the server.
// -------------------------------------------------------------

TEST(ServerIntegrationTest, HandlesPartialRequest) {

    int socketFd =
        connectToServer();


    // Send only part of the HTTP request.
    std::string part1 =
        "PUT /cache/partial HTTP/1.1\r\n";

    send(
        socketFd,
        part1.data(),
        part1.size(),
        0
    );


    // Give the server a chance to process the first piece.
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100)
    );


    // Send the rest of the headers.
    std::string part2 =
        "Host: localhost\r\n"
        "Content-Length: 5\r\n"
        "\r\n";

    send(
        socketFd,
        part2.data(),
        part2.size(),
        0
    );


    // Send the body separately.
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100)
    );

    std::string part3 =
        "hello";

    send(
        socketFd,
        part3.data(),
        part3.size(),
        0
    );


    std::string response =
        receiveResponse(socketFd);


    EXPECT_NE(
        response.find("200"),
        std::string::npos
    );


    close(socketFd);
}


// -------------------------------------------------------------
// Test multiple clients connecting and sending requests at
// approximately the same time.
//
// This verifies that the server can monitor multiple client
// sockets and process their requests concurrently.
// -------------------------------------------------------------

TEST(ServerIntegrationTest, HandlesMultipleConcurrentClients) {

    constexpr int CLIENT_COUNT = 5;

    std::vector<std::thread> clients;

    std::vector<std::string> responses(
        CLIENT_COUNT
    );


    for (int i = 0; i < CLIENT_COUNT; i++) {

        clients.emplace_back(
            [i, &responses]() {

                int socketFd =
                    connectToServer();


                std::string request =
                    "PUT /cache/client" +
                    std::to_string(i) +
                    " HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Content-Length: 5\r\n"
                    "\r\n"
                    "hello";


                send(
                    socketFd,
                    request.data(),
                    request.size(),
                    0
                );


                responses[i] =
                    receiveResponse(socketFd);


                close(socketFd);
            }
        );
    }


    // Wait for every client thread to finish.
    for (auto& client : clients) {
        client.join();
    }


    // Every client should have received a successful response.
    for (const auto& response : responses) {

        EXPECT_NE(
            response.find("200"),
            std::string::npos
        );
    }
}


// -------------------------------------------------------------
// Test that an idle client does not prevent another client
// from being handled.
//
// The first client connects but sends no data.
//
// The second client then connects and sends a complete request.
//
// With the epoll architecture, the server should continue
// monitoring both sockets rather than having a worker thread
// blocked waiting for the first client.
// -------------------------------------------------------------

TEST(ServerIntegrationTest, IdleClientDoesNotBlockServer) {

    // Connect the first client.
    int idleSocket =
        connectToServer();


    // Do not send anything from this client.
    //
    // The server should simply wait for an EPOLLIN event
    // on this socket.
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100)
    );


    // Connect a second client.
    int activeSocket =
        connectToServer();


    std::string request =
        "GET /hello HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";


    send(
        activeSocket,
        request.data(),
        request.size(),
        0
    );


    // The active client should still receive a response even
    // though the first client has sent nothing.
    std::string response =
        receiveResponse(activeSocket);


    EXPECT_NE(
        response.find("200"),
        std::string::npos
    );


    close(activeSocket);
    close(idleSocket);
}