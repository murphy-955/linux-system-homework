#include <stdlib.h>         // 提供标准库函数（如exit、atoi）
#include <stdio.h>          // 提供输入输出函数（如printf、fprintf）
#include <errno.h>          // 提供错误码定义（如errno）
#include <string.h>         // 提供字符串操作函数（如memset、strlen）
#include <netdb.h>          // 提供网络主机信息函数（如gethostbyname）
#include <sys/types.h>      // 提供基本系统数据类型（如pid_t、size_t）
#include <netinet/in.h>     // 提供网络地址结构（如struct sockaddr_in）
#include <sys/socket.h>     // 提供套接字操作函数（如socket、bind、listen）
#include <unistd.h>         // 提供POSIX系统调用（如close、read、write）
#include <arpa/inet.h>      // 提供网络地址转换函数（如inet_ntoa、ntohs）
#include <fcntl.h>          // 提供文件控制操作（如open函数及标志位）
#include <stdarg.h>         // 提供可变参数支持（如va_list、vfprintf）
// 函数声明：写入客户端日志（详细定义见文件末尾）
void write_client_log(const char *format, ...);

/*
 * 主函数：TCP客户端主逻辑
 * @param argc 命令行参数数量（应等于3：程序名、服务器IP、服务器端口）
 * @param argv 命令行参数数组（argv[1]=服务器IP，argv[2]=服务器端口）
 * @return 0表示正常退出，非0表示异常退出
 */
int main(int argc, char *argv[])
{
    int sockfd;                  // 套接字文件描述符（用于标识客户端TCP连接）
    char buffer[1024];           // 数据缓冲区（存储发送/接收的消息）
    struct sockaddr_in server_addr; // 服务器地址结构（存储服务器IP、端口等信息）
    int portnumber, nbytes;      // portnumber: 端口号（整数）；nbytes: 接收数据字节数

    // 1. 校验命令行参数
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <IP> <port>\n", argv[0]);  // 向标准错误输出用法提示
        write_client_log("Usage: %s <IP> <port>", argv[0]);    // 记录参数错误日志
        exit(1);  // 参数错误，异常退出
    }

    // 2. 创建TCP套接字
    // AF_INET: IPv4协议族；SOCK_STREAM: 字节流套接字（TCP）；0: 默认协议（TCP）
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");  // 输出系统错误信息（如"socket: No such file or directory"）
        write_client_log("socket error: %s", strerror(errno));  // 记录套接字创建失败日志
        exit(1);  // 套接字创建失败，异常退出
    }

    // 3. 初始化服务器地址结构
    memset(&server_addr, 0x00, sizeof(server_addr));  // 清零结构体（避免随机数据干扰）
    server_addr.sin_family = AF_INET;                 // 设置地址族为IPv4
    // 将字符串IP（如"192.168.1.1"）转换为网络字节序二进制IP
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    // 将字符串端口（如"8080"）转换为整数后，再转为网络字节序（大端序）
    server_addr.sin_port = htons(atoi(argv[2]));

    // 4. 连接服务器
    // 参数：套接字描述符、服务器地址结构体指针、结构体长度
    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
        perror("connect");  // 输出连接错误信息
        write_client_log("connect error: %s", strerror(errno));  // 记录连接失败日志
        close(sockfd);      // 关闭已创建的套接字（释放资源）
        exit(1);            // 连接失败，异常退出
    }

    // 连接成功，输出提示并记录日志
    printf("Connected to server at %s:%s\n", argv[1], argv[2]);
    write_client_log("Connected to server at %s:%s", argv[1], argv[2]);
    write_client_log("===============================================================================================");

    // 5. 循环发送/接收消息（与服务器交互）
    for (;;) {  // 无限循环，直到服务器断开连接或发生错误
        // 5.1 读取用户输入
        printf("Enter message to send: ");  // 提示用户输入消息
        fgets(buffer, sizeof(buffer), stdin);  // 从标准输入（键盘）读取一行数据到缓冲区
        // 移除字符串末尾的换行符（fgets会将回车符'\n'也读入缓冲区）
        buffer[strcspn(buffer, "\n")] = 0;

        // 5.2 记录发送消息日志
        write_client_log("Client: %s", buffer);

        // 5.3 发送消息到服务器
        // 参数：套接字描述符、数据缓冲区、数据长度（不包含终止符）、0（默认标志）
        if (send(sockfd, buffer, strlen(buffer), 0) == -1) {
            perror("send");  // 输出发送错误信息
            close(sockfd);   // 关闭套接字
            exit(1);         // 发送失败，异常退出
        }

        // 5.4 接收服务器回复
        // 接收数据到缓冲区，最多接收sizeof(buffer)-1字节（留1字节给终止符'\0'）
        nbytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (nbytes == -1) {  // 接收错误
            perror("recv");
            write_client_log("recv error: %s", strerror(errno));
            close(sockfd);
            exit(1);
        } else if (nbytes == 0) {  // 服务器主动断开连接（TCP四次挥手完成）
            printf("Server disconnected\n");
            write_client_log("Server disconnected");
            break;  // 退出循环
        }
        buffer[nbytes] = '\0';  // 添加字符串终止符（recv不会自动添加）
        printf("Server: %s\n", buffer);  // 输出服务器回复
        write_client_log("Server: %s", buffer);  // 记录服务器回复日志
    }
    write_client_log("===============================================================================================");

    // 6. 关闭套接字，释放资源
    close(sockfd);
    return 0;  // 正常退出
}

/**
 * 写入客户端日志到文件（log/client_log）
 *
 * @param format 格式化字符串（如 "client message: %s"）
 * @param ... 可变参数（与格式化字符串中的占位符对应，如字符串、整数等）
 */
void write_client_log(const char *format, ...) {
    va_list args;  // 可变参数列表（用于处理不确定数量的参数）
    FILE *fp;      // 文件指针（用于日志文件操作）
    int fd;        // 文件描述符（由open返回，用于fdopen转换）

    // 打开日志文件：O_WRONLY（只写）、O_CREAT（文件不存在则创建）、O_APPEND（追加模式）
    // 权限0644：所有者可读写，组用户和其他用户可读
    if ((fd = open("log/client_log", O_WRONLY | O_CREAT | O_APPEND, 0644)) == -1) {
        perror("open");  // 输出文件打开错误
        exit(1);         // 日志文件打开失败，程序退出（实际应用中可优化为非致命错误处理）
    }
    // 将文件描述符转换为文件指针（便于使用stdio库函数，如vfprintf）
    if ((fp = fdopen(fd, "a")) == NULL) {
        perror("fdopen");
        exit(1);
    }

    // 处理可变参数并写入日志
    va_start(args, format);  // 初始化可变参数列表（format是最后一个固定参数）
    vfprintf(fp, format, args);  // 向日志文件写入格式化数据（支持可变参数）
    va_end(args);  // 清理可变参数列表
    fprintf(fp, "\n");  // 每条日志后追加换行符（保持日志可读性）

    fclose(fp);  // 关闭文件指针（会自动关闭关联的文件描述符fd）
}