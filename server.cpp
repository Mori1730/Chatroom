#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <memory>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 1024 // 緩衝區大小

// 客戶端資訊
struct ClientInfo
{
    SOCKET socket;
    int id;
    std::string ip_address;
    std::string username;
};

// 用於記錄已連接的客戶端
std::vector<ClientInfo> clients;
// 用於保護 clients 陣列的互斥鎖
std::mutex clients_mutex;
// 用於生成唯一的客戶端 ID
int clientIdCounter = 1;

// 將訊息廣播給所有已連線的客戶端，exclude_sock 為不需接收該訊息的 socket（通常為發送者本身）
void broadcast_message(const std::string &message, SOCKET exclude_sock)
{
    std::lock_guard<std::mutex> lock(clients_mutex); // 鎖定 clients 列表
    for (const auto &client : clients)
    {
        if (client.socket != exclude_sock)
        {
            int send_result = send(client.socket, message.c_str(), static_cast<int>(message.size()), 0); // 將訊息發給客戶端
            if (send_result == SOCKET_ERROR)
            {
                std::cerr << "send failed to Client " << client.id << ": " << WSAGetLastError() << "\n";
            }
            else
            {
                std::cout << "Sent message to Client " << client.id << "\n";
            }
        }
    }
}

// 處理客戶端連線的函式，獨立用執行緒執行
void handle_client(ClientInfo client)
{
    char buffer[BUFFER_SIZE];
    int bytes_received;

    // 接收客戶端的 username
    bytes_received = recv(client.socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0)
    {
        // 接收 username 失敗，斷開連接
        std::cerr << "Failed to receive username from Client " << client.id << ". Disconnecting.\n";
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.erase(
                std::remove_if(clients.begin(), clients.end(),
                               [&](const ClientInfo &c)
                               { return c.id == client.id; }),
                clients.end());
        }
        closesocket(client.socket);
        return;
    }

    buffer[bytes_received] = '\0';
    client.username = std::string(buffer);
    std::cout << "Client " << client.id << " (" << client.username << ") connected from " << client.ip_address << ".\n";

    // Broadcast that a new user has joined
    std::string join_msg = client.username + " has joined the chat.\n";
    broadcast_message(join_msg, client.socket);

    // 持續接收此客戶端的訊息
    while ((bytes_received = recv(client.socket, buffer, BUFFER_SIZE - 1, 0)) > 0)
    {
        buffer[bytes_received] = '\0'; // 將接收到的資料字串終止
        std::string msg(buffer);
        // 將該客戶端的訊息廣播給其他客戶端
        std::string formatted_message = client.username + ": " + msg;
        broadcast_message(formatted_message, client.socket);
    }

    // 客戶端斷線或發生錯誤
    std::cout << "Client " << client.id << " (" << client.username << ") disconnected.\n";
    std::string disconnect_msg = client.username + " has left the chat.\n";
    broadcast_message(disconnect_msg, client.socket);

    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        // 從 clients 中移除該客戶端
        clients.erase(
            std::remove_if(clients.begin(), clients.end(),
                           [&](const ClientInfo &c)
                           { return c.id == client.id; }),
            clients.end());
    }

    closesocket(client.socket); // 關閉客戶端 socket
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Wrong Format" << std::endl;
        exit(1);
    }
    std::string port = argv[1];
    WSADATA wsaData;
    int iResult;

    // 初始化 WinSock 2.2
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
        std::cerr << "WSAStartup failed: " << iResult << "\n";
        return 1;
    }

    std::unique_ptr<addrinfo> hints(new addrinfo());
    ZeroMemory(hints.get(), sizeof(addrinfo));
    hints->ai_family = AF_INET;       // 使用 IPv4
    hints->ai_socktype = SOCK_STREAM; // TCP 串流
    hints->ai_protocol = IPPROTO_TCP;
    hints->ai_flags = AI_PASSIVE; // 用於被動式 socket（用於 accept）

    addrinfo *addr_result = NULL;
    // 取得本機可用的 socket 位址資訊
    iResult = getaddrinfo(NULL, port.c_str(), hints.get(), &addr_result);
    if (iResult != 0)
    {
        std::cerr << "getaddrinfo failed: " << iResult << "\n";
        WSACleanup();
        return 1;
    }

    // 使用取得的資訊建立監聽 socket
    SOCKET listen_socket = socket(addr_result->ai_family, addr_result->ai_socktype, addr_result->ai_protocol);
    if (listen_socket == INVALID_SOCKET)
    {
        std::cerr << "socket failed: " << WSAGetLastError() << "\n";
        freeaddrinfo(addr_result);
        WSACleanup();
        return 1;
    }

    // 綁定地址與埠號
    iResult = bind(listen_socket, addr_result->ai_addr, (int)addr_result->ai_addrlen);
    if (iResult == SOCKET_ERROR)
    {
        std::cerr << "bind failed: " << WSAGetLastError() << "\n";
        freeaddrinfo(addr_result);
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(addr_result); // 不再需要 addr_result

    // 開始監聽連線
    iResult = listen(listen_socket, SOMAXCONN);
    if (iResult == SOCKET_ERROR)
    {
        std::cerr << "listen failed: " << WSAGetLastError() << "\n";
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server listening on port " << port << "\n";

    // 伺服器主迴圈，不斷 accept 新的客戶端連線
    while (true)
    {
        SOCKET client_socket = accept(listen_socket, NULL, NULL);
        if (client_socket == INVALID_SOCKET)
        {
            std::cerr << "accept failed: " << WSAGetLastError() << "\n";
            break;
        }

        // 取得客戶端的 IP 地址
        sockaddr_in client_addr;
        int addr_size = sizeof(client_addr);
        getpeername(client_socket, (sockaddr *)&client_addr, &addr_size);
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);

        // 分配Client ID
        int client_id = clientIdCounter++;

        // 創建並初始化 ClientInfo
        ClientInfo newClient;
        newClient.socket = client_socket;
        newClient.id = client_id;
        newClient.ip_address = std::string(client_ip);
        newClient.username = "User" + std::to_string(client_id); // 預設用戶名稱

        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.push_back(newClient);
        }

        std::cout << "Client " << newClient.id << " connected from " << newClient.ip_address << ".\n";

        // 發送歡迎消息並提示輸入用戶名稱
        std::string welcome_message = "Welcome! Please enter your username:";
        int send_result = send(newClient.socket, welcome_message.c_str(), static_cast<int>(welcome_message.size()), 0);
        if (send_result == SOCKET_ERROR)
        {
            std::cerr << "send failed to Client " << newClient.id << ": " << WSAGetLastError() << "\n";
            // 如果無法發送歡迎消息，則斷開連接
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                clients.erase(
                    std::remove_if(clients.begin(), clients.end(),
                                   [&](const ClientInfo &c)
                                   { return c.id == newClient.id; }),
                    clients.end());
            }
            closesocket(newClient.socket);
            continue;
        }
        else
        {
            std::cout << "Sent welcome message to Client " << newClient.id << "\n";
        }

        // 啟動新執行緒處理該客戶端
        std::thread t(handle_client, newClient);
        t.detach(); // 分離執行緒，讓它自行運作
    }

    // 若主迴圈跳出，關閉監聽 socket 並清理 WSA
    closesocket(listen_socket);
    WSACleanup();
    return 0;
}
