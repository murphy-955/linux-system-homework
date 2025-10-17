#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

void handler(int sig){
    printf("捕获到信号\n");
}

int main(){
    // 进入子进程逻辑
    if (fork() == 0){
        printf("子进程\n");
        // 注册信号处理函数
        signal(SIGINT, handler);
        // 调用当前目录下的Hello.c,并调用main函数输出Hello World
        if (execl("./Hello", "Hello", NULL) == -1){
            perror("执行失败");
        }
        printf("子进程结束\n");
    }
    // 父进程等待子进程结束
    wait(NULL);
    // 父进程等待10秒
    sleep(10);
}
