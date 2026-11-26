#pragma once

#include <winsock2.h>
#include <thread>
#include <atomic>

class UDPServer
{
public:
    UDPServer();
    ~UDPServer();

    bool start(unsigned short port = 12345);
    void stop();

private:
    SOCKET serverSocket;
    std::thread serverThread;
    std::atomic<bool> running;
    bool winsockStarted;

    void receiveLoop();
};