#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>

#define BUFFER_SIZE 1024

#pragma comment(lib, "ws2_32.lib")

std::mutex cout_mutex; // 用於保護 std::cout 的互斥鎖

// 連線到指定的伺服器IP與預設埠號
SOCKET connect_to_server(const std::string &server_ip, const std::string &port)
{
    WSADATA wsaData;
    int iResult;

    // 初始化 Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
        std::cerr << "WSAStartup failed: " << iResult << "\n";
        return INVALID_SOCKET;
    }

    struct addrinfo hints, *addr_result = NULL;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_protocol = IPPROTO_TCP;

    // 取得伺服器的位址資訊
    iResult = getaddrinfo(server_ip.c_str(), port.c_str(), &hints, &addr_result);
    if (iResult != 0)
    {
        std::cerr << "getaddrinfo failed: " << iResult << "\n";
        WSACleanup();
        return INVALID_SOCKET;
    }

    // 建立客戶端 socket
    SOCKET sock = socket(addr_result->ai_family, addr_result->ai_socktype, addr_result->ai_protocol);
    if (sock == INVALID_SOCKET)
    {
        std::cerr << "socket failed: " << WSAGetLastError() << "\n";
        freeaddrinfo(addr_result);
        WSACleanup();
        return INVALID_SOCKET;
    }

    // 連線到伺服器
    iResult = connect(sock, addr_result->ai_addr, (int)addr_result->ai_addrlen);
    if (iResult == SOCKET_ERROR)
    {
        std::cerr << "connect failed: " << WSAGetLastError() << "\n";
        freeaddrinfo(addr_result);
        closesocket(sock);
        WSACleanup();
        return INVALID_SOCKET;
    }

    freeaddrinfo(addr_result); // 不需再使用地址資訊
    return sock;
}

// 接收伺服器廣播的訊息的函式，使用執行緒持續接收
void receive_messages(SOCKET sock)
{
    char buffer[BUFFER_SIZE];
    int bytes_received;
    // 持續從伺服器接收訊息並顯示於終端機
    while ((bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0)) > 0)
    {
        buffer[bytes_received] = '\0';
        std::string msg(buffer);

        // 顯示來自其他客戶端的消息
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << msg << "> ";
    }

    // 若跳出迴圈表示斷線或發生錯誤
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout << "\nDisconnected from server.\n";
    closesocket(sock);
    WSACleanup();
    exit(0);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cerr << "Wrong Format" << std::endl;
        exit(1);
    }
    std::string server_ip = argv[1], port = argv[2];
    SOCKET sock = connect_to_server(server_ip, port);
    if (sock == INVALID_SOCKET)
    {
        std::cerr << "Failed to connect to server.\n";
        return 1;
    }

    std::cout << "Connected to the chat server!\n";

    // 接收伺服器的歡迎消息
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0';
        std::cout << buffer; // 顯示接收到的歡迎訊息
    }
    else
    {
        std::cerr << "Failed to receive welcome message.\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // 輸入用戶名稱並發送給伺服器
    std::string username;
    std::getline(std::cin, username);
    if (username.empty())
    {
        username = "User"; // 如果用戶未輸入名稱，使用默認名稱
    }
    int send_result = send(sock, username.c_str(), static_cast<int>(username.size()), 0);
    if (send_result == SOCKET_ERROR)
    {
        std::cerr << "Failed to send username to server: " << WSAGetLastError() << "\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // 啟動接收執行緒
    std::thread recv_thread(receive_messages, sock);
    recv_thread.detach();

    // 主執行緒從標準輸入讀取使用者輸入並發送至伺服器
    std::string message;
    while (true)
    {
        {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "> ";
            std::cout.flush();
        }
        if (!std::getline(std::cin, message))
            break; // 若無法讀入行 (EOF) 則結束
        if (message == "quit")
        {
            break;
        }
        // 將訊息發給伺服器，伺服器會廣播給其他客戶端
        std::string msg_with_newline = message + "\n";
        int send_result = send(sock, msg_with_newline.c_str(), static_cast<int>(msg_with_newline.size()), 0);
        if (send_result == SOCKET_ERROR)
        {
            std::cerr << "Failed to send message: " << WSAGetLastError() << "\n";
            break;
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}