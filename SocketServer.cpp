#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>

#include <iostream>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <vector>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "27105"
#define DEFAULT_BUFLEN 512

//using namespace std;

HANDLE gh_mutex;
mongocxx::v_noabi::cursor* results;
HANDLE* threads = new HANDLE[64];
int* scores = new int[64];
int thread_count = 0;
int current_question = 0;

struct ClientStructure {
    SOCKET socket;
    int id;
    ClientStructure(SOCKET clsck, int identificator) : socket(clsck), id(identificator) {}
};

DWORD WINAPI handleClient(LPVOID lpParam) {
    ClientStructure Client = *((ClientStructure*)lpParam);
    delete (ClientStructure*)lpParam;

    DWORD dw_count = 0, dw_wait_result;
    char recvbuf[DEFAULT_BUFLEN];//, sendbuf[DEFAULT_BUFLEN];
    int iReceiveResult, iSendResult, iResult;
    int recvbuflen = DEFAULT_BUFLEN;
    bool exiting = false;

    mongocxx::v_noabi::cursor::iterator it = results->begin();
    mongocxx::v_noabi::cursor::iterator end = results->end();

    while (exiting == false) {
        dw_wait_result = WaitForSingleObject(gh_mutex, INFINITE);
        if (dw_wait_result != WAIT_OBJECT_0) {
            break;
        }
        if (it == end) {
            ReleaseMutex(gh_mutex);
            break;
        }

        bsoncxx::document::view doc = *it;
        std::string doc_message = std::string(doc["question"].get_string().value);
        doc_message += "\na) " + std::string(doc["answer_a"].get_string().value);
        doc_message += "\nb) " + std::string(doc["answer_b"].get_string().value);
        doc_message += "\nc) " + std::string(doc["answer_c"].get_string().value);
        doc_message += "\nd) " + std::string(doc["answer_d"].get_string().value);

        if (Client.id == 1) {
            std::cout << "\nQuestion " << current_question + 1 << ":\n" << doc_message << std::endl;
        }
                
        const char* sendbuf = doc_message.c_str();
        iSendResult = send(Client.socket, sendbuf, (int)strlen(sendbuf), 0);
        if (iSendResult == SOCKET_ERROR) {
            std::cout << "Send failed: " << WSAGetLastError() << std::endl;
            closesocket(Client.socket);
            return false;
        }

        //primire mesaj
        iReceiveResult = recv(Client.socket, recvbuf, recvbuflen, 0);
        if (iReceiveResult >= 0) {
            recvbuf[iReceiveResult] = '\0';
            std::cout << "Answer from client " << Client.id << ": " << recvbuf << std::endl;
        }
        else {
            std::cout << "Recv failed: " << WSAGetLastError() << std::endl;
            closesocket(Client.socket);
            return false;
        }

        //logica
        std::string correct_answer = std::string(doc["correct_answer"].get_string().value);
        if (correct_answer[0] == recvbuf[0]) {
            //std::cout << "The answer is correct!!!";
            scores[Client.id - 1] += int(doc["points"].get_int32().value);
        }
        else {
            //std::cout << "The answer is wrong.";
        }

        if (strcmp(recvbuf, "exit") == 0) {
            std::cout << "Client " << Client.id << "is exiting..." << std::endl;
            exiting = true;
        }

        dw_count++;
        if (Client.id == thread_count) {
            std::cout << "\nCorrect answer: " << std::string(doc["correct_answer"].get_string().value) << "\n\n";
            current_question++;
            it++;
        }

        if (!ReleaseMutex(gh_mutex)) {
            std::cout << "Couldn't release mutex" << std::endl;
            return 1;
        }

    }

    //serverul de deconecteaza de la client
    iResult = shutdown(Client.socket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        std::cout << "Shutdown failed: " << WSAGetLastError() << std::endl;
        closesocket(Client.socket);
        return false;
    }

    closesocket(Client.socket);
    return true;
}

int main()
{
    mongocxx::instance instance;
    mongocxx::uri uri("mongodb://localhost:27017/");
    mongocxx::client client(uri);
    mongocxx::v_noabi::collection collection;
    int topic;

    mongocxx::v_noabi::database db = client["questions"];
    std::cout << "Welcome to QuizShow!\nPlease choose one of the following topics:\n";
    std::cout << "1) History" << std::endl;
    std::cout << "2) Geography" << std::endl;
    std::cin >> topic;
    switch (topic) {
    case 1: {
        collection = db["History_questions"];
        break;
    }
    case 2: {
        collection = db["Geography_questions"];
        break;
    }
    }

    results = new mongocxx::v_noabi::cursor(collection.find({}));

    WSADATA wsaData;
    int iResult;
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cout << "WSAStartup failed: " << iResult;
        return 1;
    }

    struct addrinfo* result = NULL, * ptr = NULL, hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    //se ia adresa de la port in functie de parametrii dati de hint
    std::cout << "Getting the address..." << std::endl;
    iResult = getaddrinfo("127.0.0.1", DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        std::cout << "getaddrinfo failed: " << iResult << std::endl;
        WSACleanup();
        return 1;
    }
    std::cout << "Address got successfully!" << std::endl;

    //se creaza un socket
    SOCKET ListenSocket = INVALID_SOCKET;
    std::cout << "Creating socket..." << std::endl;
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        std::cout << "Error at socket(): " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }
    std::cout << "Socket created successfully!" << std::endl;

    //se face bind la acel socket
    std::cout << "Binding the socket..." << std::endl;
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        std::cout << "Bind failed with error: " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }
    std::cout << "Socket binded successfully!" << std::endl;
    freeaddrinfo(result);

    //InitializeCriticalSection(&console_lock);
    gh_mutex = CreateMutex(NULL, false, NULL);
    if (gh_mutex == NULL) {
        std::cout << "CreateMutex failed: " << GetLastError() << std::endl;
        return 1;
    }

    //dam listen la socket
    std::cout << "Listening..." << std::endl;
    if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cout << "Listen failed with error: " << WSAGetLastError() << std::endl;
        closesocket(ListenSocket);
        WSACleanup();
    }

    bool there_were_clients = false;
    while (1)
    {
        SOCKET ClientSocket;
        ClientSocket = INVALID_SOCKET;

        //acceptam o conexiune la acel socket
        ClientSocket = accept(ListenSocket, NULL, NULL);
        if (ClientSocket == INVALID_SOCKET) {
            std::cout << "\nAccept failed: " << WSAGetLastError() << std::endl;
            continue;
        }

        //cout << "Connection acepted!" << endl;

        HANDLE client_thread;
        DWORD threadID;

        ClientStructure* client = new ClientStructure(ClientSocket, thread_count + 1);

        //SOCKET* ClientSocketPtr = new SOCKET(ClientSocket);
        client_thread = CreateThread(NULL, 0, handleClient, (LPVOID)client, 0, &threadID);

        if (client_thread == NULL) {
            std::cout << "Failed to create thread for client" << std::endl;
            closesocket(ClientSocket);
            continue;
        }

        threads[thread_count] = client_thread;
        scores[thread_count] = 0;
        thread_count++;
        if (thread_count >= 64) {
            std::cout << "Thread limit reached!" << std::endl;
            break;
        }

        if (GetAsyncKeyState(VK_ESCAPE)) {
            std::cout << "Exiting server...";
            break;
        }

        if (current_question == 5) {
            std::cout << "The quiz is over. These are the results";
            break;
        }
    }

    WaitForMultipleObjects(thread_count, threads, true, INFINITE);

    closesocket(ListenSocket);
    for (int i = 0; i < thread_count; i++) {
        CloseHandle(threads[i]);
    }

    WSACleanup();
    
    for (int client = 0; client < thread_count; client++) {
        std::cout << "Client " << client + 1 << ": " << scores[client] << std::endl;
    }

    CloseHandle(gh_mutex);
    delete[] threads;
    delete[] scores;
    delete results;
    return 0;
}
