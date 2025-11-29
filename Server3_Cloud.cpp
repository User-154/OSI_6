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
#include <pthread.h>
#endif
#include <cstring>
#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#define THREAD_RETURN DWORD WINAPI
#define THREAD_CALL __stdcall
#else
#define THREAD_RETURN void*
#define THREAD_CALL
#endif

using namespace std;

const int MAX_CLIENTS = 50;
const int BUFFER_SIZE = 1024;

vector<int> clients;
vector<string> clientNames;
#ifdef _WIN32
CRITICAL_SECTION clientsMutex;
#else
pthread_mutex_t clientsMutex = PTHREAD_MUTEX_INITIALIZER;
#endif

void BroadcastMessage(const string& message, int senderSocket = -1) {
#ifdef _WIN32
    EnterCriticalSection(&clientsMutex);
#else
    pthread_mutex_lock(&clientsMutex);
#endif

    for (size_t i = 0; i < clients.size(); i++) {
        if (clients[i] != senderSocket && clients[i] != -1) {
            send(clients[i], message.c_str(), message.length(), 0);
        }
    }

#ifdef _WIN32
    LeaveCriticalSection(&clientsMutex);
#else
    pthread_mutex_unlock(&clientsMutex);
#endif
}

struct ClientParams {
    int socket;
    sockaddr_in addr;
};

#ifdef _WIN32
DWORD WINAPI HandleClient(LPVOID arg) {
#else
void* HandleClient(void* arg) {
#endif
    ClientParams* params = (ClientParams*)arg;
    int clientSocket = params->socket;
    sockaddr_in clientAddr = params->addr;
    delete params;

    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
    int clientPort = ntohs(clientAddr.sin_port);

    printf("New client connected: %s:%d\n", clientIP, clientPort);

    string nameRequest = "Enter your name: ";
    send(clientSocket, nameRequest.c_str(), nameRequest.length(), 0);

    char username[32];
    int bytesReceived = recv(clientSocket, username, 31, 0);
    if (bytesReceived <= 0) {
#ifdef _WIN32
        closesocket(clientSocket);
#else
        close(clientSocket);
#endif
        return NULL;
    }
    username[bytesReceived] = '\0';

    if (strlen(username) > 0 && username[strlen(username) - 1] == '\n') {
        username[strlen(username) - 1] = '\0';
    }

#ifdef _WIN32
    EnterCriticalSection(&clientsMutex);
#else
    pthread_mutex_lock(&clientsMutex);
#endif
    clients.push_back(clientSocket);
    clientNames.push_back(username);
#ifdef _WIN32
    LeaveCriticalSection(&clientsMutex);
#else
    pthread_mutex_unlock(&clientsMutex);
#endif

    string joinMessage = string(username) + " joined the chat!\n";
    printf("%s", joinMessage.c_str());
    fflush(stdout);
    BroadcastMessage(joinMessage, clientSocket);

    char buffer[BUFFER_SIZE];

    while (true) {
        bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);

        if (bytesReceived <= 0) {
            break;
        }

        buffer[bytesReceived] = '\0';

        if (strncmp(buffer, "exit", 4) == 0) {
            break;
        }

        string clientName;
#ifdef _WIN32
        EnterCriticalSection(&clientsMutex);
#else
        pthread_mutex_lock(&clientsMutex);
#endif
        for (size_t i = 0; i < clients.size(); i++) {
            if (clients[i] == clientSocket) {
                clientName = clientNames[i];
                break;
            }
        }
#ifdef _WIN32
        LeaveCriticalSection(&clientsMutex);
#else
        pthread_mutex_unlock(&clientsMutex);
#endif

        string messageText = buffer;
        if (!messageText.empty() && messageText[messageText.length() - 1] == '\n') {
            messageText = messageText.substr(0, messageText.length() - 1);
        }

        string fullMessage = clientName + ": " + messageText + "\n";
        printf("%s", fullMessage.c_str());
        fflush(stdout);
        BroadcastMessage(fullMessage, clientSocket);
    }

    string clientName;
#ifdef _WIN32
    EnterCriticalSection(&clientsMutex);
#else
    pthread_mutex_lock(&clientsMutex);
#endif
    for (size_t i = 0; i < clients.size(); i++) {
        if (clients[i] == clientSocket) {
            clientName = clientNames[i];
            clients.erase(clients.begin() + i);
            clientNames.erase(clientNames.begin() + i);
            break;
        }
    }
#ifdef _WIN32
    LeaveCriticalSection(&clientsMutex);
#else
    pthread_mutex_unlock(&clientsMutex);
#endif

    string leaveMessage = clientName + " left the chat!\n";
    printf("%s", leaveMessage.c_str());
    fflush(stdout);
    BroadcastMessage(leaveMessage);

#ifdef _WIN32
    closesocket(clientSocket);
#else
    close(clientSocket);
#endif
    printf("Client disconnected: %s:%d\n", clientIP, clientPort);
    fflush(stdout);

    return NULL;
}

int main() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }
    InitializeCriticalSection(&clientsMutex);
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
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        printf("Unable to create socket\n");
        WSACleanup();
        return 1;
    }
#else
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        printf("Unable to create socket\n");
        return 1;
    }
#endif

    int enable = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&enable, sizeof(enable));

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    int retVal = bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
#ifdef _WIN32
    if (retVal == SOCKET_ERROR) {
        printf("Unable to bind socket\n");
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }
#else
    if (retVal == -1) {
        printf("Unable to bind socket\n");
        close(serverSocket);
        return 1;
    }
#endif

    retVal = listen(serverSocket, 10);
#ifdef _WIN32
    if (retVal == SOCKET_ERROR) {
        printf("Unable to listen\n");
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }
#else
    if (retVal == -1) {
        printf("Unable to listen\n");
        close(serverSocket);
        return 1;
    }
#endif

    printf("Chat server started. Waiting for connections on port %d...\n", serverPort);
    fflush(stdout);

    while (true) {
        sockaddr_in clientAddr;
#ifdef _WIN32
        int clientAddrSize = sizeof(clientAddr);
#else
        socklen_t clientAddrSize = sizeof(clientAddr);
#endif

#ifdef _WIN32
        SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrSize);
        if (clientSocket == INVALID_SOCKET) {
            printf("Unable to accept connection\n");
            continue;
        }
#else
        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrSize);
        if (clientSocket == -1) {
            printf("Unable to accept connection\n");
            continue;
        }
#endif

        ClientParams* params = new ClientParams;
        params->socket = clientSocket;
        params->addr = clientAddr;

#ifdef _WIN32
        HANDLE clientThread = CreateThread(NULL, 0, HandleClient, params, 0, NULL);
        if (clientThread == NULL) {
            printf("Unable to create thread\n");
            closesocket(clientSocket);
            delete params;
            continue;
        }
        CloseHandle(clientThread);
#else
        pthread_t clientThread;
        if (pthread_create(&clientThread, NULL, HandleClient, params) != 0) {
            printf("Unable to create thread\n");
            close(clientSocket);
            delete params;
            continue;
        }
        pthread_detach(clientThread);
#endif
    }

#ifdef _WIN32
    closesocket(serverSocket);
    WSACleanup();
    DeleteCriticalSection(&clientsMutex);
#else
    close(serverSocket);
    pthread_mutex_destroy(&clientsMutex);
#endif
    return 0;
}