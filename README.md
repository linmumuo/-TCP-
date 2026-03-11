# -TCP-
基于Windows Socket开发支持多客户端并发连接的聊天服务器，使用多线程处理每个客户端  实现客户端列表管理、广播消息、私聊、服务器主动断开等功能  设计优雅断开连接机制，通过协商关闭确保数据传输完整，避免连接重置  服务器控制台支持实时命令：查看在线客户端、发送消息、广播、强制断开等  客户端采用select模型实现非阻塞收发，同时处理用户输入和服务器消息

多线程TCP聊天服务器

基于Windows Socket开发的多客户端聊天服务器，支持广播、私聊、服务器控制台管理。

## 功能特性
- 多客户端并发连接（最大10个）
- 广播消息、私聊消息
- 服务器控制台命令（list、send、broadcast、disconnect、quit）
- 优雅断开连接机制，确保数据完整
- 客户端使用select模型实现非阻塞收发

## 技术栈
- C语言
- Winsock2
- 多线程
- TCP/IP

## 编译与运行
使用CMake
```bash
mkdir build && cd build
cmake ..
cmake --build .

手动编译（MinGW）
bash
gcc src/server.c -o server.exe -lws2_32
gcc src/client.c -o client.exe -lws2_32

运行
启动服务器：server.exe 8888

启动一个或多个客户端：client.exe

在客户端输入 CHAT hello 发送消息，输入 QUIT 退出

服务器控制台输入命令管理连接
