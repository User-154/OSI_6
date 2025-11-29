#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include <cstring>
#include <stdio.h>
#include <iostream>
#include <string>

using namespace std;

int main() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }
#endif

#ifdef _WIN32
    SOCKET clientSock = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSock == INVALID_SOCKET) {
        printf("Unable to create socket\n");
        WSACleanup();
        return 1;
    }
#else
    int clientSock = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSock == -1) {
        printf("Unable to create socket\n");
        return 1;
    }
#endif

    string ip;
    int port;
    cout << "Enter server IP: ";
    getline(cin, ip);
    cout << "Enter server port: ";
    cin >> port;
    cin.ignore();

    struct sockaddr_in serverInfo;
    serverInfo.sin_family = AF_INET;
    serverInfo.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &serverInfo.sin_addr) <= 0) {
        printf("Invalid IP address: %s\n", ip.c_str());
#ifdef _WIN32
        closesocket(clientSock);
        WSACleanup();
#else
        close(clientSock);
#endif
        return 1;
    }

    printf("Connecting to %s:%d...\n", ip.c_str(), port);
    int retVal = connect(clientSock, (struct sockaddr*)&serverInfo, sizeof(serverInfo));
#ifdef _WIN32
    if (retVal == SOCKET_ERROR) {
        printf("Unable to connect to server %s:%d\n", ip.c_str(), port);
        closesocket(clientSock);
        WSACleanup();
        return 1;
    }
#else
    if (retVal == -1) {
        printf("Unable to connect to server %s:%d\n", ip.c_str(), port);
        close(clientSock);
        return 1;
    }
#endif

    printf("Connection established successfully\n");

    string message;
    cout << "Enter text to send: ";
    getline(cin, message);

    retVal = send(clientSock, message.c_str(), message.length(), 0);
#ifdef _WIN32
    if (retVal == SOCKET_ERROR) {
        printf("Unable to send\n");
        closesocket(clientSock);
        WSACleanup();
        return 1;
    }
#else
    if (retVal == -1) {
        printf("Unable to send\n");
        close(clientSock);
        return 1;
    }
#endif

    printf("Data sent. Waiting for response...\n");

    char buffer[1024];
    retVal = recv(clientSock, buffer, sizeof(buffer) - 1, 0);
#ifdef _WIN32
    if (retVal == SOCKET_ERROR) {
        printf("Unable to receive data\n");
    }
#else
    if (retVal == -1) {
        printf("Unable to receive data\n");
    }
#endif
    else if (retVal == 0) {
        printf("Server closed connection\n");
    }
    else {
        buffer[retVal] = '\0';
        printf("Server response: %s\n", buffer);
    }

#ifdef _WIN32
    closesocket(clientSock);
    WSACleanup();
#else
    close(clientSock);
#endif

    printf("Press Enter to exit...");
    getchar();
    return 0;
}