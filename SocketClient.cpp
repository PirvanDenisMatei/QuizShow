# define _CRT_SECURE_NO_WARNINGS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <iostream>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "27105"
#define DEFAULT_BUFLEN 512

using namespace std;

int main()
{
    WSADATA wsaData;
    int iresult;
    iresult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iresult != 0) {
        cout << "WSAStartup failed: " << iresult << endl;
        return 1;
    }

    struct addrinfo* result = NULL, * ptr = NULL, hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    cout << "Getting address..." << endl;
    iresult = getaddrinfo("127.0.0.1", DEFAULT_PORT, &hints, &result);
    if (iresult != 0) {
        cout << "Getaddrinfo failed: " << iresult;
        WSACleanup();
        return 1;
    }
    cout << "Address obtained successfully!" << endl;

    SOCKET ConnectSocket = INVALID_SOCKET;

    cout << "Connecting to server..." << endl;
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

        if (ConnectSocket == INVALID_SOCKET) {
            cout << "Error at socket(): " << WSAGetLastError() << endl;
            WSACleanup();
            return 1;
        }

        iresult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iresult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);
    if (ConnectSocket == INVALID_SOCKET) {
        cout << "Unable to connect to server" << endl;
        WSACleanup();
        return 1;
    }
    cout << "Connection established successfully!" << endl;

    char sendbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;
    char recvbuf[DEFAULT_BUFLEN];
    int iSendResult, iReceiveResult;
    bool exiting = false;


    do {
        //primire mesaj
        iReceiveResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
        if (iReceiveResult >= 0) {
            recvbuf[iReceiveResult] = '\0';
            cout << "Message received: " << recvbuf << endl;
        }
        else {
            cout << "Recv failed: " << WSAGetLastError() << endl;
        }

        //trimitere mesaj
        cout << "Sending to server: ";
        cin.getline(sendbuf, DEFAULT_BUFLEN - 1);
        iSendResult = send(ConnectSocket, sendbuf, (int)strlen(sendbuf), 0);

        if (iSendResult == SOCKET_ERROR) {
            cout << "Send failed: " << WSAGetLastError() << endl;
            closesocket(ConnectSocket);
            WSACleanup();
            return 1;
        }

        //logica
        if (strcmp(sendbuf, "exit") == 0) {
            cout << "Exiting..." << endl;
            exiting = true;
        }
    } while (exiting == false);

    iSendResult = shutdown(ConnectSocket, SD_SEND);
    if (iSendResult == SOCKET_ERROR) {
        cout << "Shutdown failed: " << WSAGetLastError() << endl;
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    closesocket(ConnectSocket);
    WSACleanup();
    return 0;
}