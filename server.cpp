#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <conio.h>

#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

typedef struct {
    SOCKET client_socket;
    struct sockaddr_in client_addr;
} client_info_t;

// 全局变量
SOCKET client_sockets[MAX_CLIENTS] = { INVALID_SOCKET };
int client_count = 0;

// 函数声明
int initialize_winsock();
SOCKET create_server_socket(int port);
int handle_client(SOCKET client_socket, struct sockaddr_in client_addr);
DWORD WINAPI client_handler(LPVOID param);
void process_client_command(SOCKET client_socket, const char* buffer);
void start_server_console();
void graceful_disconnect(SOCKET client_socket);
void broadcast_message(const char* message);

// 添加客户端到列表
void add_client(SOCKET client_socket) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] == INVALID_SOCKET) {
            client_sockets[i] = client_socket;
            client_count++;
            printf("客户端 [%d] 已添加到列表\n", i);
            return;
        }
    }
}

// 从列表移除客户端
void remove_client(SOCKET client_socket) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] == client_socket) {
            client_sockets[i] = INVALID_SOCKET;
            client_count--;
            printf("客户端 [%d] 已从列表移除\n", i);
            return;
        }
    }
}

// 优雅断开连接
void graceful_disconnect(SOCKET client_socket) {
    // 发送断开连接确认
    const char* disconnect_msg = "DISCONNECT_ACK\n";
    send(client_socket, disconnect_msg, strlen(disconnect_msg), 0);

    // 关闭发送端
    shutdown(client_socket, SD_SEND);

    // 等待一段时间确保客户端收到确认
    Sleep(100);

    // 从客户端列表移除
    remove_client(client_socket);

    // 完全关闭socket
    closesocket(client_socket);

    printf("已优雅断开客户端连接\n");
}

// 广播消息
void broadcast_message(const char* message) {
    char full_msg[BUFFER_SIZE];
    snprintf(full_msg, sizeof(full_msg), "广播消息: %s\n", message);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != INVALID_SOCKET) {
            send(client_sockets[i], full_msg, strlen(full_msg), 0);
        }
    }
}

// 简单的服务器控制台
void start_server_console() {
    printf("\n服务器控制台已启动!\n");
    printf("输入 'list' 查看客户端列表\n");
    printf("输入 'send <编号> <消息>' 向客户端发送消息\n");
    printf("输入 'broadcast <消息>' 广播消息\n");
    printf("输入 'disconnect <编号>' 断开指定客户端连接\n");
    printf("输入 'quit' 关闭服务器\n\n");

    char input[256];

    while (1) {
        printf("服务器> ");
        if (fgets(input, sizeof(input), stdin)) {
            input[strcspn(input, "\n")] = '\0';  // 移除换行符

            // 列出客户端
            if (strcmp(input, "list") == 0) {
                printf("当前连接的客户端 (%d 个):\n", client_count);
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (client_sockets[i] != INVALID_SOCKET) {
                        printf("客户端 [%d]\n", i);
                    }
                }
            }
            // 发送消息给特定客户端
            else if (strncmp(input, "send ", 5) == 0) {
                int client_id;
                char message[200];
                if (sscanf(input + 5, "%d %199[^\n]", &client_id, message) == 2) {
                    if (client_id >= 0 && client_id < MAX_CLIENTS &&
                        client_sockets[client_id] != INVALID_SOCKET) {

                        char full_msg[256];
                        snprintf(full_msg, sizeof(full_msg), "服务器消息: %s\n", message);
                        send(client_sockets[client_id], full_msg, strlen(full_msg), 0);
                        printf("已向客户端 [%d] 发送: %s\n", client_id, message);
                    }
                    else {
                        printf("客户端 [%d] 不存在\n", client_id);
                    }
                }
                else {
                    printf("用法: send <客户端编号> <消息>\n");
                }
            }
            // 广播消息
            else if (strncmp(input, "broadcast ", 10) == 0) {
                char message[200];
                strcpy(message, input + 10);
                broadcast_message(message);
            }
            // 断开指定客户端连接
            else if (strncmp(input, "disconnect ", 11) == 0) {
                int client_id;
                if (sscanf(input + 11, "%d", &client_id) == 1) {
                    if (client_id >= 0 && client_id < MAX_CLIENTS &&
                        client_sockets[client_id] != INVALID_SOCKET) {

                        printf("正在断开客户端 [%d] 的连接...\n", client_id);
                        graceful_disconnect(client_sockets[client_id]);
                    }
                    else {
                        printf("客户端 [%d] 不存在\n", client_id);
                    }
                }
                else {
                    printf("用法: disconnect <客户端编号>\n");
                }
            }
            // 退出服务器
            else if (strcmp(input, "quit") == 0) {
                printf("正在关闭服务器...\n");

                // 断开所有客户端连接
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (client_sockets[i] != INVALID_SOCKET) {
                        graceful_disconnect(client_sockets[i]);
                    }
                }

                exit(0);
            }
            // 未知命令
            else if (strlen(input) > 0) {
                printf("未知命令。可用命令: list, send, broadcast, disconnect, quit\n");
            }
        }
    }
}

int main(int argc, char* argv[]) {
    WSADATA wsa;
    SOCKET server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    int client_addr_len = sizeof(client_addr);

    if (argc != 2) {
        printf("使用方法: %s <端口号>\n", argv[0]);
        return 1;
    }

    int server_port = atoi(argv[1]);
    if (server_port <= 0 || server_port > 65535) {
        printf("错误: 端口号必须在1-65535之间\n");
        return 1;
    }

    printf("=== 聊天服务器启动 ===\n");
    printf("注意: 文件传输功能已被移除\n\n");

    // 初始化Winsock
    if (initialize_winsock() != 0) {
        return 1;
    }

    // 创建服务器socket
    server_socket = create_server_socket(server_port);
    if (server_socket == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }

    // 创建服务器控制台线程
    HANDLE console_thread = CreateThread(NULL, 0,
        (LPTHREAD_START_ROUTINE)start_server_console, NULL, 0, NULL);

    printf("服务器正在监听端口 %d...\n", server_port);
    printf("按Ctrl+C或输入'quit'命令关闭服务器\n");

    // 主循环：接受客户端连接
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_socket == INVALID_SOCKET) {
            printf("接受客户端连接失败: %d\n", WSAGetLastError());
            continue;
        }

        printf("新的客户端连接: %s:%d\n",
            inet_ntoa(client_addr.sin_addr),
            ntohs(client_addr.sin_port));

        // 创建客户端信息结构
        client_info_t* client_info = (client_info_t*)malloc(sizeof(client_info_t));
        client_info->client_socket = client_socket;
        client_info->client_addr = client_addr;

        // 创建线程处理客户端
        CreateThread(NULL, 0, client_handler, client_info, 0, NULL);
    }

    // 清理资源
    closesocket(server_socket);
    WSACleanup();
    return 0;
}

// 初始化Winsock
int initialize_winsock() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Winsock初始化失败: %d\n", WSAGetLastError());
        return 1;
    }
    printf("Winsock初始化成功\n");
    return 0;
}

// 创建服务器socket
SOCKET create_server_socket(int port) {
    SOCKET server_socket;
    struct sockaddr_in server_addr;

    // 创建socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        printf("创建socket失败: %d\n", WSAGetLastError());
        return INVALID_SOCKET;
    }

    // 设置SO_REUSEADDR选项，允许地址重用
    int reuse = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR,
        (char*)&reuse, sizeof(reuse)) == SOCKET_ERROR) {
        printf("设置SO_REUSEADDR失败: %d\n", WSAGetLastError());
    }

    // 设置服务器地址
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // 绑定socket
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("绑定失败: %d\n", WSAGetLastError());
        closesocket(server_socket);
        return INVALID_SOCKET;
    }

    // 开始监听
    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        printf("监听失败: %d\n", WSAGetLastError());
        closesocket(server_socket);
        return INVALID_SOCKET;
    }

    return server_socket;
}

// 客户端处理线程
DWORD WINAPI client_handler(LPVOID param) {
    client_info_t* client_info = (client_info_t*)param;
    SOCKET client_socket = client_info->client_socket;

    // 添加到客户端列表
    add_client(client_socket);

    printf("线程开始处理客户端: %s:%d\n",
        inet_ntoa(client_info->client_addr.sin_addr),
        ntohs(client_info->client_addr.sin_port));

    // 处理客户端请求
    handle_client(client_socket, client_info->client_addr);

    printf("客户端处理线程结束\n");
    free(client_info);
    return 0;
}

// 处理客户端请求
int handle_client(SOCKET client_socket, struct sockaddr_in client_addr) {
    char buffer[BUFFER_SIZE];
    int bytes_received;

    // 发送欢迎消息
    const char* welcome_msg = "欢迎使用聊天服务器！\n可用命令:\n1. CHAT <消息> - 发送消息\n2. QUIT - 退出\n";
    send(client_socket, welcome_msg, strlen(welcome_msg), 0);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

        if (bytes_received == SOCKET_ERROR) {
            int error = WSAGetLastError();
            printf("接收数据错误 (客户端 %s:%d): %d\n",
                inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), error);
            break;
        }
        else if (bytes_received == 0) {
            printf("客户端主动断开连接: %s:%d\n",
                inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            break;
        }

        buffer[bytes_received] = '\0';
        printf("收到客户端 [%s:%d]: %s",
            inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buffer);

        // 处理客户端命令
        process_client_command(client_socket, buffer);
    }

    // 从客户端列表移除
    remove_client(client_socket);
    closesocket(client_socket);

    return 0;
}

// 处理客户端命令
void process_client_command(SOCKET client_socket, const char* buffer) {
    char message[BUFFER_SIZE];

    // 处理CHAT命令
    if (strncmp(buffer, "CHAT", 4) == 0) {
        strncpy(message, buffer + 5, BUFFER_SIZE - 1);
        message[BUFFER_SIZE - 1] = '\0';

        // 回复确认
        char reply_msg[BUFFER_SIZE];
        snprintf(reply_msg, sizeof(reply_msg), "已收到您的消息: %s\n", message);
        send(client_socket, reply_msg, strlen(reply_msg), 0);
    }
    // 处理QUIT命令 - 优雅断开连接
    else if (strncmp(buffer, "QUIT", 4) == 0) {
        printf("客户端请求断开连接\n");

        // 发送断开确认
        const char* goodbye_msg = "断开连接确认。再见！\n";
        send(client_socket, goodbye_msg, strlen(goodbye_msg), 0);

        // 等待客户端确认接收
        Sleep(50);

        // 优雅关闭连接
        graceful_disconnect(client_socket);
    }
    else {
        const char* error_msg = "未知命令，请使用CHAT或QUIT\n";
        send(client_socket, error_msg, strlen(error_msg), 0);
    }
}