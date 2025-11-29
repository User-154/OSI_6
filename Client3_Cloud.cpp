#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include <windows.h>
#define THREAD_RETURN DWORD WINAPI
#define THREAD_CALL __stdcall
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#define THREAD_RETURN void*
#define THREAD_CALL
#endif
#include <cstring>
#include <stdio.h>
#include <iostream>
#include <string>

using namespace std;

const int BUFFER_SIZE = 1024;

#ifdef _WIN32
CRITICAL_SECTION consoleMutex;
#else
pthread_mutex_t consoleMutex = PTHREAD_MUTEX_INITIALIZER;
#endif
bool isReceiving = true;

void SafePrint(const string& message) {
#ifdef _WIN32
    EnterCriticalSection(&consoleMutex);
#else
    pthread_mutex_lock(&consoleMutex);
#endif
    cout << message;
    cout.flush();
#ifdef _WIN32
    LeaveCriticalSection(&consoleMutex);
#else
    pthread_mutex_unlock(&consoleMutex);
#endif
}

void ClearInputLine() {
    cout << "\r\033[K";
    cout.flush();
}

void ShowPrompt() {
#ifdef _WIN32
    EnterCriticalSection(&consoleMutex);
#else
    pthread_mutex_lock(&consoleMutex);
#endif
    cout << "> ";
    cout.flush();
#ifdef _WIN32
    LeaveCriticalSection(&consoleMutex);
#else
    pthread_mutex_unlock(&consoleMutex);
#endif
}

string GetLocalIP() {
    char hostname[256];
    gethostname(hostname, sizeof(hostname));

    struct hostent* host = gethostbyname(hostname);
    if (host == NULL) return "127.0.0.1";

    return inet_ntoa(*(struct in_addr*)*host->h_addr_list);
}

#ifdef _WIN32
DWORD WINAPI ReceiveMessages(LPVOID arg) {
#else
void* ReceiveMessages(void* arg) {
#endif
    int clientSocket = *(int*)arg;
    char buffer[BUFFER_SIZE];

    while (true) {
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);

        if (bytesReceived <= 0) {
            SafePrint("\nDisconnected from server\n");
            isReceiving = false;
            break;
        }

        buffer[bytesReceived] = '\0';
        ClearInputLine();
        SafePrint(string(buffer));

        if (isReceiving) {
            ShowPrompt();
        }
    }

    return NULL;
}

int main() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }
    InitializeCriticalSection(&consoleMutex);
#endif

#ifdef _WIN32
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        printf("Unable to create socket\n");
        WSACleanup();
        return 1;
    }
#else
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        printf("Unable to create socket\n");
        return 1;
    }
#endif

    string localIP = GetLocalIP();
    printf("Your local IP: %s\n", localIP.c_str());

    string serverInput;
    cout << "Enter server IP (localhost for same PC): ";
    getline(cin, serverInput);

    int serverPort;
    cout << "Enter server port: ";
    cin >> serverPort;
    cin.ignore();

    string serverIP;
    if (serverInput == "localhost") {
        serverIP = "127.0.0.1";
    }
    else {
        serverIP = serverInput;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);

    if (inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr) <= 0) {
        printf("Invalid IP address: %s\n", serverIP.c_str());
#ifdef _WIN32
        closesocket(clientSocket);
        WSACleanup();
#else
        close(clientSocket);
#endif
        return 1;
    }

    printf("Connecting to %s:%d...\n", serverIP.c_str(), serverPort);
    int retVal = connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
#ifdef _WIN32
    if (retVal == SOCKET_ERROR) {
        printf("Unable to connect to server %s:%d\n", serverIP.c_str(), serverPort);
        printf("Make sure server is running and firewall allows connections\n");
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }
#else
    if (retVal == -1) {
        printf("Unable to connect to server %s:%d\n", serverIP.c_str(), serverPort);
        printf("Make sure server is running and firewall allows connections\n");
        close(clientSocket);
        return 1;
    }
#endif

    printf("Connected to server successfully!\n");

    char buffer[BUFFER_SIZE];
    int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        printf("%s", buffer);
    }

    string username;
    cout << "Enter your name: ";
    getline(cin, username);
    send(clientSocket, username.c_str(), username.length(), 0);

    printf("Welcome to the chat, %s!\n", username.c_str());
    printf("Type your messages (type 'exit' to quit):\n\n");

#ifdef _WIN32
    HANDLE receiveThread = CreateThread(NULL, 0, ReceiveMessages, &clientSocket, 0, NULL);
    if (receiveThread == NULL) {
        printf("Unable to create receive thread\n");
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }
#else
    pthread_t receiveThread;
    if (pthread_create(&receiveThread, NULL, ReceiveMessages, &clientSocket) != 0) {
        printf("Unable to create receive thread\n");
        close(clientSocket);
        return 1;
    }
#endif

    string message;
    ShowPrompt();

    while (true) {
        getline(cin, message);

        if (message == "exit") {
            message += "\n";
            send(clientSocket, message.c_str(), message.length(), 0);
            break;
        }

        ClearInputLine();
        SafePrint("You: " + message + "\n");

        message += "\n";
        int sent = send(clientSocket, message.c_str(), message.length(), 0);
#ifdef _WIN32
        if (sent == SOCKET_ERROR) {
#else
        if (sent == -1) {
#endif
            SafePrint("Send error - connection lost\n");
            break;
        }

        if (isReceiving) {
            ShowPrompt();
        }
        }

    isReceiving = false;
#ifdef _WIN32
    shutdown(clientSocket, SD_BOTH);
    WaitForSingleObject(receiveThread, INFINITE);
    CloseHandle(receiveThread);
    closesocket(clientSocket);
    WSACleanup();
    DeleteCriticalSection(&consoleMutex);
#else
    shutdown(clientSocket, SHUT_RDWR);
    pthread_join(receiveThread, NULL);
    close(clientSocket);
    pthread_mutex_destroy(&consoleMutex);
#endif

    printf("Press Enter to exit...");
    getchar();
    return 0;
    }