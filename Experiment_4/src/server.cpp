#include "include/communication_config.h"
#include "include/network/tcp_server.h"
#include <string>
#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include "include/protocols/PIAP.h"
#include "include/protocols/TITP.h"

// 替代 std::print 的简单实现
template<typename... Args>
void print(const std::string &format, Args... args) {
    printf(format.c_str(), args...);
}

template<typename... Args>
void println(const std::string &format, Args... args) {
    if constexpr (sizeof...(args) == 0) {
        // 没有参数时直接输出字符串
        std::cout << format << std::endl;
    } else {
        // 有参数时使用 printf 风格格式化
        printf(format.c_str(), args...);
        printf("\n");
    }
}

// 简单的用户数据库模拟
std::unordered_map<std::string, std::string> user_database = {
        {"admin", "admin123"},
        {"user1", "password1"},
        {"user2", "password2"}
};

// 任务数据库
std::unordered_map<uint64_t, std::pair<std::string, std::string>> task_database = {
        {1, {"收集资源", "前往森林收集10个木材和5个石头"}},
        {2, {"击败怪物", "前往东边的山洞击败5只哥布林"}},
        {3, {"护送任务", "护送商人安全抵达下一个城镇"}}
};

// 处理客户端认证请求
void handle_authentication(tcp_server &server, int client_fd) {
    try {
        auto request = server.recv_ctrl_packet(client_fd);
        if (!request) {
            println("Error: Failed to receive authentication packet.");
            return;
        }

        auto format_status = request->valid_format();
        if (format_status != piap_format_type_t::FORMAT_OK) {
            auto response = std::make_unique<piap_t>(piap_msg_type_t::LOGIN_RESPONSE);
            response->set_format_status(format_status);
            server.send_ctrl_packet(client_fd, response);
            println("Authentication format error: %d", static_cast<int>(format_status));
            return;
        }

        const char *username = request->get_userID();
        const char *password = request->get_password();
        piap_auth_type_t auth_status;

        // 检查用户是否存在并验证密码
        if (user_database.find(username) != user_database.end()) {
            if (user_database[username] == password) {
                auth_status = piap_auth_type_t::LOGIN_SUCCESS;
            } else {
                auth_status = piap_auth_type_t::WRONG_PASSWORD;
            }
        } else {
            auth_status = piap_auth_type_t::USER_NOT_FOUND;
        }

        // 发送认证响应
        auto response = std::make_unique<piap_t>(piap_msg_type_t::LOGIN_RESPONSE);
        response->set_auth_status(auth_status);
        server.send_ctrl_packet(client_fd, response);

        if (auth_status == piap_auth_type_t::LOGIN_SUCCESS) {
            println("User %s logged in successfully.", username);
        } else {
            println("Authentication failed for user %s", username);
        }

    } catch (const std::exception &e) {
        println("Exception during authentication: %s", e.what());
    }
}

// 处理客户端的任务请求
void handle_task_request(tcp_server &server, int client_fd) {
    try {
        auto request = server.recv_data_packet(client_fd);
        if (!request || request->get_msg_type() != titp_msg_type_t::RESOURCE_REQUEST) {
            println("Error: Invalid task request.");
            return;
        }

        uint64_t task_id = request->get_task_id();
        auto response = std::make_unique<titp_t>(titp_msg_type_t::RESOURCE_SENT);

        // 检查任务是否存在
        if (task_database.find(task_id) != task_database.end()) {
            auto &task_info = task_database[task_id];
            response->set_task_id(task_id);
            response->set_task_name(task_info.first.c_str());
            response->set_task_description(task_info.second.c_str());
            response->set_difficulty(task_difficulty_t::MEDIUM);
            response->set_resource_status(titp_resource_status_type_t::RESOURCE_ACK);
            response->set_msg_status(titp_format_type_t::FORMAT_OK);
        } else {
            response->set_resource_status(titp_resource_status_type_t::RESOURCE_NOT_FOUND);
            response->set_msg_status(titp_format_type_t::FORMAT_OK);
        }

        server.send_data_packet(client_fd, response);
        println("Handled task request for task ID %lu", task_id);

    } catch (const std::exception &e) {
        println("Exception during task handling: %s", e.what());
    }
}

int main() {
    try {
        tcp_server server(PORT);
        std::cout << std::string("Server Starts at: 0.0.0.0: ") + std::to_string(PORT) + std::string("\n");

        while (server.is_active()) {
            int client_fd = server.accept_connection();
            if (client_fd < 0) {
                // 继续等待下一个连接
                continue;
            }

            println("Client connected with FD: %d", client_fd);

            // 处理认证
            handle_authentication(server, client_fd);

            // 认证成功后处理任务请求
            while (true) {
                // 这里应该使用select/poll/epoll来处理多客户端，但为了简单起见使用循环
                handle_task_request(server, client_fd);
            }

            server.kick_client(client_fd);
            println("Client %d disconnected.", client_fd);
        }

        server.shutdown_server();
        std::cout << std::string("Server closes successfully!\n");

    } catch (const std::exception &e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}