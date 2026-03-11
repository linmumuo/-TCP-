#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <conio.h>

#pragma comment(lib, "ws2_32.lib")

#define BUF_SIZE 1000
#define RECV_BUF_SIZE 2000

// 函数声明
void clear_stdin();
void graceful_disconnect(SOCKET s);
int connect_to_server(const char* server_ip, int server_port);

int main() {
    WSADATA wsa;
    SOCKET s = INVALID_SOCKET;
    struct sockaddr_in server;
    char message[BUF_SIZE] = { 0 };
    char server_reply[RECV_BUF_SIZE] = { 0 };
    int recv_size;
    int ret = 1;

    // 初始化Winsock
    printf("初始化Winsock...\n");
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup失败，错误代码: %d\n", WSAGetLastError());
        return 1;
    }
    printf("Winsock初始化成功\n");

    // 连接服务器
    s = connect_to_server("127.0.0.1", 8888);
    if (s == INVALID_SOCKET) {
        goto cleanup;
    }

    // 接收欢迎消息
    recv_size = recv(s, server_reply, RECV_BUF_SIZE - 1, 0);
    if (recv_size == SOCKET_ERROR) {
        printf("接收欢迎消息失败: %d\n", WSAGetLastError());
        goto cleanup;
    }
    else if (recv_size == 0) {
        printf("服务器关闭了连接\n");
        goto cleanup;
    }
    server_reply[recv_size] = '\0';
    printf("服务器: %s\n", server_reply);
    memset(server_reply, 0, RECV_BUF_SIZE);

    // 通信循环
    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(s, &readfds);

        struct timeval timeout;
        timeout.tv_sec = 1;  // 1秒超时
        timeout.tv_usec = 0;

        int activity = select(0, &readfds, NULL, NULL, &timeout);

        if (activity == SOCKET_ERROR) {
            printf("select错误: %d\n", WSAGetLastError());
            break;
        }

        // 检查服务器消息
        if (FD_ISSET(s, &readfds)) {
            memset(server_reply, 0, RECV_BUF_SIZE);
            recv_size = recv(s, server_reply, RECV_BUF_SIZE - 1, 0);

            if (recv_size > 0) {
                server_reply[recv_size] = '\0';
                printf("服务器: %s", server_reply);

                // 检查是否是断开连接确认
                if (strstr(server_reply, "断开连接确认") != NULL ||
                    strstr(server_reply, "DISCONNECT_ACK") != NULL) {
                    printf("收到断开连接确认，正在关闭连接...\n");
                    graceful_disconnect(s);
                    ret = 0;
                    break;
                }
            }
            else if (recv_size == 0) {
                printf("服务器正常断开连接\n");
                ret = 0;
                break;
            }
            else if (recv_size == SOCKET_ERROR) {
                int error = WSAGetLastError();
                if (error == WSAECONNRESET) {
                    printf("连接被服务器重置\n");
                }
                else if (error == WSAECONNABORTED) {
                    printf("连接被中止\n");
                }
                else {
                    printf("接收失败: %d\n", error);
                }
                break;
            }
        }

        // 检查用户输入
        if (_kbhit()) {
            printf("\n请输入命令 (CHAT <消息> 或 QUIT): ");
            if (fgets(message, BUF_SIZE, stdin) == NULL) {
                break;
            }

            // 移除换行符
            message[strcspn(message, "\n")] = '\0';

            // 处理空输入
            if (strlen(message) == 0) {
                continue;
            }

            // 退出命令
            if (strcmp(message, "QUIT") == 0) {
                printf("正在断开连接...\n");

                // 发送QUIT命令
                send(s, "QUIT\n", 5, 0);

                // 等待服务器响应
                Sleep(100);

                // 优雅关闭连接
                graceful_disconnect(s);
                ret = 0;
                break;
            }
            // CHAT命令或其他消息
            else {
                char send_msg[BUF_SIZE];
                if (strncmp(message, "CHAT", 4) == 0) {
                    strcpy(send_msg, message);
                    strcat(send_msg, "\n");
                }
                else {
                    snprintf(send_msg, sizeof(send_msg), "CHAT %s\n", message);
                }

                if (send(s, send_msg, strlen(send_msg), 0) == SOCKET_ERROR) {
                    printf("发送失败: %d\n", WSAGetLastError());
                    break;
                }
                printf("已发送: %s\n", send_msg);
            }

            memset(message, 0, BUF_SIZE);
        }
    }

cleanup:
    // 确保socket已关闭
    if (s != INVALID_SOCKET) {
        closesocket(s);
        s = INVALID_SOCKET;
    }

    // 清理Winsock
    WSACleanup();

    printf("按回车键退出...");
    clear_stdin();
    getchar();

    return ret;
}

// 清空输入缓冲区
void clear_stdin() {
    while (_kbhit()) {
        _getch();
    }
}

// 优雅断开连接
void graceful_disconnect(SOCKET s) {
    if (s == INVALID_SOCKET) {
        return;
    }

    // 关闭发送端
    shutdown(s, SD_SEND);

    // 接收剩余数据（如果有）
    char buffer[1024];
    int recv_size;
    struct timeval tv;
    fd_set readfds;

    tv.tv_sec = 1;  // 1秒超时
    tv.tv_usec = 0;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(s, &readfds);

        int activity = select(0, &readfds, NULL, NULL, &tv);

        if (activity > 0 && FD_ISSET(s, &readfds)) {
            recv_size = recv(s, buffer, sizeof(buffer) - 1, 0);
            if (recv_size <= 0) {
                break;
            }
            buffer[recv_size] = '\0';
            printf("接收剩余数据: %s\n", buffer);
        }
        else {
            break;
        }
    }

    // 完全关闭socket
    closesocket(s);

    printf("已优雅断开连接\n");
}

// 连接到服务器
int connect_to_server(const char* server_ip, int server_port) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) {
        printf("创建socket失败: %d\n", WSAGetLastError());
        return INVALID_SOCKET;
    }

    // 设置TCP_NODELAY禁用Nagle算法
    int flag = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(server_port);
    server.sin_addr.s_addr = inet_addr(server_ip);

    printf("正在连接到服务器 %s:%d...\n", server_ip, server_port);

    if (connect(s, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        printf("连接失败: %d\n", WSAGetLastError());
        closesocket(s);
        return INVALID_SOCKET;
    }

    printf("成功连接到服务器\n");
    return s;
}