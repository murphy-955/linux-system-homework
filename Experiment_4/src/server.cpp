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
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

template<typename... Args>
void print(const std::string &format, Args... args) {
    if (sizeof...(args) > 0) {
        printf(format.c_str(), args...);
    } else {
        printf("%s", format.c_str());
    }
}

template<typename... Args>
void println(const std::string &format, Args... args) {
    if (sizeof...(args) > 0) {
        printf(format.c_str(), args...);
    } else {
        printf("%s", format.c_str());
    }
    printf("\n");
}

// 封装发送控制包函数
bool send_ctrl_packet(tcp_server &server, int client_fd, std::unique_ptr<piap_t> packet) {
    return server.send_ctrl_packet(client_fd, packet);
}

// 封装发送数据包函数
bool send_data_packet(tcp_server &server, int client_fd, std::unique_ptr<titp_t> packet) {
    return server.send_data_packet(client_fd, packet);
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
bool handle_authentication(tcp_server &server, int client_fd) {
    try {
        auto request = server.recv_ctrl_packet(client_fd);
        if (!request) {
            println("Error: Failed to receive authentication packet.");
            return false;
        }

        auto format_status = request->valid_format();
        if (format_status != piap_format_type_t::FORMAT_OK) {
            auto response = std::make_unique<piap_t>(piap_msg_type_t::LOGIN_RESPONSE);
            response->set_format_status(format_status);
            send_ctrl_packet(server, client_fd, std::move(response));
            println("Authentication format error: %d", static_cast<int>(format_status));
            return false;
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
        send_ctrl_packet(server, client_fd, std::move(response));

        if (auth_status == piap_auth_type_t::LOGIN_SUCCESS) {
            println("User %s logged in successfully.", username);
            return true;
        } else {
            println("Authentication failed for user %s", username);
            return false;
        }

    } catch (const std::exception &e) {
        println("Exception during authentication: %s", e.what());
        return false;
    }
}

// 认证后处理客户端会话
void handle_client_session(tcp_server &server, int client_fd);

int main() {
    try {
        tcp_server server(PORT);
        std::cout << std::string("Server Starts at:") +  SERVER_TEST + std::to_string(PORT) + std::string("\n");
        println("Type 'exit' or 'quit' to shutdown the server.");

        fd_set readfds;
        int max_fd = std::max(server.get_server_fd(), 0) + 1;

        while (server.is_active()) {
            FD_ZERO(&readfds);
            FD_SET(server.get_server_fd(), &readfds);
            FD_SET(0, &readfds);

            int activity = select(max_fd, &readfds, nullptr, nullptr, nullptr);

            if (activity < 0) {
                println("Select error.");
                break;
            }

            if (FD_ISSET(0, &readfds)) {
                std::string input;
                std::getline(std::cin, input);
                if (input == "exit" || input == "quit") {
                    println("Shutting down server...");
                    break;
                }
            }

            if (FD_ISSET(server.get_server_fd(), &readfds)) {
                int client_fd = server.accept_connection();
                if (client_fd < 0) {
                    continue;
                }

                println("Client connected with FD: %d", client_fd);

                if (handle_authentication(server, client_fd)) {
                    handle_client_session(server, client_fd);
                }

                server.kick_client(client_fd);
                println("Client %d disconnected.", client_fd);
            }
        }

        server.shutdown_server();
        std::cout << std::string("Server closes successfully!\n");

    } catch (const std::exception &e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

void handle_client_session(tcp_server &server, int client_fd) {
    while (true) {
        // 先尝试读控制包
        auto ctrl_packet = server.recv_ctrl_packet(client_fd);
        if (ctrl_packet) {
            if (ctrl_packet->get_msg_type() == piap_msg_type_t::LOGOUT_REQUEST) {
                println("Client %d logged out.", client_fd);
                break;
            }
            continue;
        }
        
        // 再尝试读数据包
        auto data_packet = server.recv_data_packet(client_fd);
        if (data_packet) {
            if (data_packet->get_msg_type() == titp_msg_type_t::RESOURCE_REQUEST) {
                // 处理任务请求（同之前的代码）
                uint64_t task_id = data_packet->get_task_id();
                auto response = std::make_unique<titp_t>(titp_msg_type_t::RESOURCE_SENT);
                
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
                
                send_data_packet(server, client_fd, std::move(response));
                println("Handled task request for task ID %lu", task_id);
            }
            continue;
        }
        
        // 两种包都没收到，连接断开
        if (!ctrl_packet && !data_packet) {
            println("Connection closed for client %d", client_fd);
            break;
        }
    }
}