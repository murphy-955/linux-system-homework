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


// 函数声明：写入服务器日志（支持格式化字符串）
void write_server_log(const char *format, ...);

int main(int argc, char *argv[]) {
    int sockfd;               // 监听套接字描述符（用于接受客户端连接）
    int newsockfd;            // 客户端连接套接字描述符（用于与客户端通信）
    struct sockaddr_in server_addr;  // 服务器地址结构（存储IP、端口等信息）
    struct sockaddr_in client_addr;  // 客户端地址结构（存储连接客户端的信息）
    socklen_t sin_size;       // 客户端地址结构长度（用于accept函数）
    int portnumber, nbytes;   // portnumber：端口号；nbytes：实际接收的字节数
    // 回复客户端
    const char *response = "Message received";  // 服务器默认回复消息

    // 校验命令行参数：必须提供端口号（程序名+端口号共2个参数）
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);  // 输出用法提示到标准错误流
        write_server_log("Usage: %s <port>\n", argv[0]);  // 记录参数错误日志
        exit(1);  // 异常退出（非0退出码表示错误）
    }

    // 1. 创建TCP套接字
    // AF_INET：IPv4地址族；SOCK_STREAM：TCP流式套接字；0：默认协议（TCP）
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");  // 输出错误信息（结合errno）
        write_server_log("socket error");  // 记录套接字创建失败日志
        exit(1);
    }

    // 2. 初始化服务器地址结构
    memset(&server_addr, 0x00, sizeof(server_addr));  // 清零地址结构
    server_addr.sin_family = AF_INET;                 // IPv4地址族
    server_addr.sin_addr.s_addr = INADDR_ANY;         // 绑定到所有本地网络接口（0.0.0.0）
    server_addr.sin_port = htons(atoi(argv[1]));       // 端口号转换为网络字节序（大端序）
    write_server_log("server port: %s", argv[1]);      // 记录服务器端口日志
    write_server_log("server addr: %s", inet_ntoa(server_addr.sin_addr));  // 记录服务器IP日志

    // 3. 绑定套接字到指定地址和端口
    // 将监听套接字sockfd与server_addr绑定，使客户端可通过该地址连接
    if (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
        perror("bind");  // 输出绑定错误信息（如端口被占用）
        exit(1);
    }

    // 4. 监听客户端连接
    // 使套接字进入被动监听状态，backlog=5表示最多允许5个未处理连接排队
    if (listen(sockfd, 5) == -1) {
        perror("listen");  // 输出监听错误信息
        write_server_log("listen error");  // 记录监听失败日志
        exit(1);
    }

    // 打印监听状态（控制台输出+日志记录）
    printf("Server is listening on port %s...\n", argv[1]);
    write_server_log("server is listening on port %s...", argv[1]);
    write_server_log("===============================================================================================");  // 日志分隔线

    char buffer[1024];  // 接收客户端消息的缓冲区（大小1024字节）

    // 5. 接受客户端连接（阻塞等待）
    sin_size = sizeof(client_addr);  // 初始化客户端地址结构长度
    // accept返回新的连接套接字newsockfd，用于与该客户端通信
    if ((newsockfd = accept(sockfd, (struct sockaddr *) &client_addr, &sin_size)) == -1) {
        perror("accept");  // 输出接受连接错误信息
        write_server_log("accept error");  // 记录连接接受失败日志
        exit(1);
    }

    // 打印客户端连接信息（IP:端口）
    printf("Client connected from %s:%d\n",
           inet_ntoa(client_addr.sin_addr),  // 将网络字节序IP转换为字符串
           ntohs(client_addr.sin_port));     // 将网络字节序端口转换为主机字节序
    write_server_log("client connected from %s:%d",
                     inet_ntoa(client_addr.sin_addr),
                     ntohs(client_addr.sin_port));  // 记录客户端连接日志
    write_server_log("===============================================================================================");  // 日志分隔线

    // 6. 循环接收客户端消息
    for(;;) {
        // 接收客户端数据：通过连接套接字newsockfd接收，最多接收缓冲区大小-1字节（留1字节给终止符）
        nbytes = recv(newsockfd, buffer, sizeof(buffer) - 1, 0);
        if (nbytes == -1) {  // 接收错误（如网络异常）
            perror("recv");
            write_server_log("recv error");  // 记录接收错误日志
            write_server_log("===============================================================================================");
            close(newsockfd);  // 关闭当前客户端连接
            close(sockfd);     // 关闭监听套接字
            exit(1);
        } else if (nbytes == 0) {  // 客户端断开连接（recv返回0表示连接关闭）
            printf("Client disconnected\n");
            write_server_log("client disconnected");  // 记录客户端断开日志
            write_server_log("===============================================================================================");
            break;  // 退出接收循环
        }
        buffer[nbytes] = '\0';  // 手动添加字符串终止符（recv不会自动添加）
        printf("Client: %s\n", buffer);  // 打印客户端消息到控制台
        write_server_log("client message: %s", buffer);  // 记录客户端消息日志
        write_server_log("server message: %s", response);  // 记录服务器回复日志

        // 回复客户端：发送response字符串，长度为strlen(response)（不含终止符）
        if (send(newsockfd, response, strlen(response), 0) == -1) {
            perror("send");  // 输出发送错误信息
            write_server_log("send error");  // 记录发送失败日志
            close(newsockfd);
            close(sockfd);
            exit(1);
        }
    }

    // 7. 关闭套接字（释放资源）
    close(newsockfd);  // 关闭客户端连接套接字
    close(sockfd);     // 关闭监听套接字
    write_server_log("server closed");  // 记录服务器关闭日志
    write_server_log("===============================================================================================");
    return 0;
}

/**
 * 写入服务器日志
 *
 * @param format 格式化字符串（如 "client message: %s"）
 * @param ... 可变参数（与格式化字符串中的占位符对应）
 */
void write_server_log(const char *format, ...) {  // 修正参数为格式化字符串+可变参数

    va_list args;  // 可变参数列表（用于接收不定数量的参数）
    FILE *fp;      // 文件指针（用于日志文件操作）
    int fd;        // 文件描述符（用于打开日志文件）

    // 打开日志文件：若不存在则创建，存在则追加内容，权限0644（所有者可读写，其他只读）
    if ((fd = open("log/server_log", O_WRONLY | O_CREAT | O_APPEND, 0644)) == -1) {
        perror("open");  // 输出文件打开错误信息
        exit(1);
    }
    // 将文件描述符转换为文件指针（便于使用stdio库函数进行格式化输出）
    if ((fp = fdopen(fd, "a")) == NULL) {
        perror("fdopen");  // 输出文件指针转换错误信息
        exit(1);
    }

    // 处理格式化参数并写入日志
    va_start(args, format);  // 初始化可变参数列表（format为最后一个固定参数）
    vfprintf(fp, format, args);  // 格式化输出到日志文件（支持可变参数）
    va_end(args);  // 清理可变参数列表
    fprintf(fp, "\n");  // 追加换行符（每条日志占一行）

    fclose(fp);  // 关闭文件指针（自动关闭关联的文件描述符）
}