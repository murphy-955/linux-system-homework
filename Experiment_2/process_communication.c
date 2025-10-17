#include <stdio.h>      // 标准输入输出库
#include <stdlib.h>     // 标准库，包含exit等函数
#include <string.h>     // 字符串处理函数库
#include <unistd.h>     // Unix标准函数库，包含fork等系统调用
#include <sys/types.h>  // 定义系统数据类型
#include <sys/stat.h>   // 文件状态相关函数
#include <fcntl.h>      // 文件控制相关函数
#include <sys/wait.h>   // 等待子进程结束的函数

/**
 * 编写程序自选某种本地进程间通信机制实现客户端进程与服务器端进程之间信息的发送接收。
 */


/**
 * 程序功能：使用文件作为本地进程间通信机制，实现客户端进程与服务器端进程之间的信息发送与接收
 * 实现方式：通过fork创建父子进程，子进程作为客户端向文件写入消息，父进程作为服务器从文件读取消息
 */

int main() {
    // 创建子进程，实现客户端-服务器通信模型
    int pid = fork();

    if (pid == 0) {
        // 子进程 - 作为客户端角色
        printf("客户端进程启动，准备发送消息...\n");

        // 打开文件用于写入消息，文件路径为"./log/log"
        int fd = open("./log/log", O_WRONLY);
        if (fd == -1) {
            // 文件打开失败时输出错误信息并退出
            perror("open");
            exit(1);
        }

        // 定义要发送的消息内容
        char *msg = "hello server";

        // 向文件写入消息
        if (write(fd, msg, strlen(msg)) == -1) {
            // 写入失败时输出错误信息并退出
            perror("write");
            exit(1);
        }

        printf("客户端已发送消息: %s\n", msg);

        // 关闭文件描述符
        close(fd);
        printf("客户端进程结束\n");
    } else if (pid > 0) {
        // 父进程 - 作为服务器角色
        printf("服务器进程启动，等待接收消息...\n");

        // 打开文件用于读取消息，文件路径为"./log/log"
        int fd = open("./log/log", O_RDONLY);
        if (fd == -1) {
            // 文件打开失败时输出错误信息并退出
            perror("open");
            exit(1);
        }

        // 创建缓冲区用于存储读取的消息
        char buf[128];

        // 从文件读取消息
        if (read(fd, buf, sizeof(buf)) == -1) {
            // 读取失败时输出错误信息并退出
            perror("read");
            exit(1);
        }

        // 输出接收到的消息
        printf("server say: %s\n", buf);

        // 关闭文件描述符
        close(fd);

        // 等待子进程结束，避免产生僵尸进程
        wait(NULL);
        printf("服务器进程结束\n");
    } else {
        // fork创建子进程失败
        perror("fork");
        exit(1);
    }
    return 0;
}