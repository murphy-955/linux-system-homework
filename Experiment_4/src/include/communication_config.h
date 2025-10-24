#pragma once
#include <string>

constexpr unsigned int PORT = 4396;

inline const std::string SERVER_TEST = "127.0.0.1";
inline const std::string SERVER_IP_ADDR = "192.168.0.103";

constexpr int MAX_RETRIES = 5;

// 添加一些常用的错误消息和状态码
constexpr const char* SOCKET_ERROR = "Socket creation failed";
constexpr const char* CONNECTION_ERROR = "Connection to server failed";
constexpr const char* AUTHENTICATION_ERROR = "Authentication failed";
constexpr const char* TASK_ERROR = "Task request failed";

// 添加一些超时设置
constexpr int CONNECTION_TIMEOUT = 10; // 秒
constexpr int RECEIVE_TIMEOUT = 5;    // 秒