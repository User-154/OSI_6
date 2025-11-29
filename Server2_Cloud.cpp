#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#endif
#include <cstring>
#include <stdio.h>
#include <iostream>
#include <string>
#include <cctype>

using namespace std;

string processString(const string& input) {
    string result;
    for (int i = input.length() - 1; i >= 0; i--) {
        result += input[i];
    }
    return result;
}

int main(void) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }
#endif

    int serverPort;
    cout << "Enter server port: ";
    cin >> serverPort;
    cin.ignore();

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        struct hostent* host = gethostbyname(hostname);
        if (host && host->h_addr_list[0]) {
            cout << "Server IP: " << inet_ntoa(*(struct in_addr*)host->h_addr_list[0]) << endl;
        }
    }
    cout << "Server port: " << serverPort << endl;

#ifdef _WIN32
    SOCKET servSock = socket(AF_INET, SOCK_STREAM, 0);
    if (servSock == INVALID_SOCKET) {
        printf("Unable to create socket\n");
        WSACleanup();
        return 1;
    }
#else
    int servSock = socket(AF_INET, SOCK_STREAM, 0);
    if (servSock == -1) {
        printf("Unable to create socket\n");
        return 1;
    }
#endif

    int opt = 1;
    if (setsockopt(servSock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) < 0) {
        printf("Setsockopt error\n");
#ifdef _WIN32
        closesocket(servSock);
        WSACleanup();
#else
        close(servSock);
#endif
        return 1;
    }

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(serverPort);
    sin.sin_addr.s_addr = INADDR_ANY;

    int retVal = bind(servSock, (struct sockaddr*)&sin, sizeof(sin));
#ifdef _WIN32
    if (retVal == SOCKET_ERROR) {
        printf("Unable to bind to port %d\n", serverPort);
        closesocket(servSock);
        WSACleanup();
        return 1;
    }
#else
    if (retVal == -1) {
        printf("Unable to bind to port %d\n", serverPort);
        close(servSock);
        return 1;
    }
#endif

    printf("Server started. Waiting for connections on port %d...\n", serverPort);
    fflush(stdout);

    retVal = listen(servSock, 10);
#ifdef _WIN32
    if (retVal == SOCKET_ERROR) {
        printf("Unable to listen\n");
        closesocket(servSock);
        WSACleanup();
        return 1;
    }
#else
    if (retVal == -1) {
        printf("Unable to listen\n");
        close(servSock);
        return 1;
    }
#endif

    while (true) {
        struct sockaddr_in from;
#ifdef _WIN32
        int fromlen = sizeof(from);
#else
        socklen_t fromlen = sizeof(from);
#endif

#ifdef _WIN32
        SOCKET clientSock = accept(servSock, (struct sockaddr*)&from, &fromlen);
        if (clientSock == INVALID_SOCKET) {
            printf("Unable to accept connection\n");
            continue;
        }
#else
        int clientSock = accept(servSock, (struct sockaddr*)&from, &fromlen);
        if (clientSock == -1) {
            printf("Unable to accept connection\n");
            continue;
        }
#endif

        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(from.sin_addr), clientIP, INET_ADDRSTRLEN);
        printf("New client connected from %s:%d\n", clientIP, ntohs(from.sin_port));
        fflush(stdout);

        char buffer[1024];
        retVal = recv(clientSock, buffer, sizeof(buffer) - 1, 0);
#ifdef _WIN32
        if (retVal == SOCKET_ERROR) {
            printf("Unable to receive data\n");
            closesocket(clientSock);
            continue;
        }
#else
        if (retVal == -1) {
            printf("Unable to receive data\n");
            close(clientSock);
            continue;
        }
#endif
        else if (retVal == 0) {
            printf("Client disconnected\n");
#ifdef _WIN32
            closesocket(clientSock);
#else
            close(clientSock);
#endif
            continue;
        }

        buffer[retVal] = '\0';
        printf("Received from %s: %s\n", clientIP, buffer);

        string receivedStr(buffer);
        string processedStr = processString(receivedStr);

        printf("Processed string (inverted): %s\n", processedStr.c_str());
        fflush(stdout);

        retVal = send(clientSock, processedStr.c_str(), processedStr.length(), 0);
#ifdef _WIN32
        if (retVal == SOCKET_ERROR) {
            printf("Unable to send response\n");
        }
#else
        if (retVal == -1) {
            printf("Unable to send response\n");
        }
#endif
        else {
            printf("Sent to %s: %s\n", clientIP, processedStr.c_str());
        }

#ifdef _WIN32
        closesocket(clientSock);
#else
        close(clientSock);
#endif
        printf("Connection with %s closed\n\n", clientIP);
        fflush(stdout);
    }

#ifdef _WIN32
    closesocket(servSock);
    WSACleanup();
#else
    close(servSock);
#endif
    return 0;
}