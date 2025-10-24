#include "include/communication_config.h"
#include "include/network/tcp_client.h"
#include <string>
#include <iostream>
#include <string>
#include "include/protocols/PIAP.h"
#include "include/protocols/TITP.h"

// 替代 std::print 的简单实现
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

void clear_screen() {
    print("\033[2J\033[H");
}

// 显示主菜单
void show_main_menu() {
    println("\n===== 主菜单 =====");
    println("1. 查看任务列表");
    println("2. 请求任务详情");
    println("3. 退出登录");
    println("=================");
    print("请选择操作: ");
}

// 请求任务详情
void request_task_detail(tcp_client &client) {
    uint64_t task_id;
    print("请输入任务ID (1-3): ");
    std::cin >> task_id;
    std::cin.ignore(); // 忽略换行符

    // 创建任务请求包
    std::unique_ptr <titp_t> request = std::make_unique<titp_t>(titp_msg_type_t::RESOURCE_REQUEST);
    request->set_task_id(task_id);

    // 发送请求
    bool is_send = false;
    for (auto i = 0; i < MAX_RETRIES; ++i) {
        if (client.send_data_packet(request)) {
            is_send = true;
            break;
        }
        println("警告: 请求重试 {}/{}...", i + 1, MAX_RETRIES);
    }

    if (!is_send) {
        println("错误: 发送任务请求失败，请重试！");
        return;
    }

    // 接收响应
    auto response = client.recv_data_packet();
    if (!response) {
        println("错误: 接收任务响应失败，请重试！");
        return;
    }

    // 处理响应
    if (response->get_resource_status() == titp_resource_status_type_t::RESOURCE_ACK) {
        println("\n任务详情:");
        println("ID: {}", response->get_task_id());
        println("名称: {}", response->get_task_name());
        println("难度: {}", static_cast<int>(response->get_difficulty()));
        println("描述: {}", response->get_task_description());
    } else {
        println("错误: 任务不存在或无法获取！");
    }
}

int main() {
    try {
        tcp_client client(PORT, SERVER_TEST);
        std::cout << std::string("Client connects to server: ") + client.get_server_ip() + std::string(":") +
                     std::to_string(client.get_port()) + std::string("\n");

        println("\n===== 💐 Welcome to the Bounty System Server =====");
        println("Server: %s:%d", client.get_server_ip(), client.get_port());
        println("==================================================\n");

        while (true) {
            bool is_authorized = false;
            std::string acc, pwd;
            print("Username (or 'quit' to exit): ");
            std::getline(std::cin, acc);
            if (acc == "quit") {
                break;
            }
            print("Password: ");
            std::getline(std::cin, pwd);

            // 创建登录认证数据报
            {
                std::unique_ptr <piap_t> auth_pkt = std::make_unique<piap_t>(piap_msg_type_t::LOGIN_REQUEST);
                auth_pkt->set_usr_info(acc.c_str(), pwd.c_str());
                bool is_send = false;

                for (auto i = 0; i < MAX_RETRIES; ++i) {
                    if (client.send_ctrl_packet(auth_pkt)) {
                        is_send = true;
                        break;
                    }
                    println("Warning: Authorization retrying %d/%s...", i + 1, MAX_RETRIES);
                }

                if (!is_send) {
                    println("Error: Failed to create the log in request, try again!");
                    continue;
                }
            }

            // 接受服务器发送的数据报
            {
                auto recv_pkt = client.recv_ctrl_packet();

                if (recv_pkt == nullptr) {
                    println("Error: Failed to receive server response, try again!");
                    continue;
                }

                if (recv_pkt->get_msg_type() != piap_msg_type_t::LOGIN_RESPONSE) {
                    continue;
                }
                auto format_status = recv_pkt->valid_format();
                if (format_status != piap_format_type_t::FORMAT_OK) {
                    println("Error Code %s: Log in request broke down, try again!",
                            static_cast<uint16_t>(format_status));
                    continue;
                }
                auto auth_status = recv_pkt->get_auth_status();
                if (auth_status != piap_auth_type_t::LOGIN_SUCCESS) {
                    println("Error Code {}: {}", static_cast<uint16_t>(auth_status), recv_pkt->get_status_msg());
                    continue;
                }
            }


            is_authorized = true;
            println("Welcome back, user: {}", acc);

            // 进行数据层面的交互
            while (is_authorized) {
                println("\n===== Available Operations =====");
                println("1. Request Task");
                println("2. Logout");
                print("Please choose an option: ");

                std::string choice;
                std::getline(std::cin, choice);

                if (choice == "1") {
                    uint64_t task_id;
                    print("Enter task ID (1-3): ");
                    std::string task_id_str;
                    std::getline(std::cin, task_id_str);
                    try {
                        task_id = std::stoull(task_id_str);
                    } catch (...) {
                        println("Invalid task ID. Please enter a number.");
                        continue;
                    }

                    // 创建任务请求数据包
                    auto task_request = std::make_unique<titp_t>(titp_msg_type_t::RESOURCE_REQUEST);
                    task_request->set_task_id(task_id);

                    // 发送任务请求
                    bool sent = false;
                    for (int i = 0; i < MAX_RETRIES; ++i) {
                        if (client.send_data_packet(task_request)) {
                            sent = true;
                            break;
                        }
                        println("Warning: Task request retrying {}/{}", i + 1, MAX_RETRIES);
                    }

                    if (!sent) {
                        println("Error: Failed to send task request.");
                        continue;
                    }

                    // 接收任务响应
                    auto task_response = client.recv_data_packet();
                    if (!task_response) {
                        println("Error: Failed to receive task response.");
                        continue;
                    }

                    // 处理任务响应
                    auto format_status = task_response->get_format_status();
                    auto resource_status = task_response->get_resource_status();

                    if (format_status != titp_format_type_t::FORMAT_OK) {
                        println("Error: Invalid task response format.");
                        continue;
                    }

                    if (resource_status != titp_resource_status_type_t::RESOURCE_ACK) {
                        println("Error: Failed to get task. Status: %s", static_cast<int>(resource_status));
                        continue;
                    }

                    // 显示任务信息
                    println("\n===== Task Details =====");
                    println("ID: %s", task_response->get_task_id());
                    println("Name: %s", task_response->get_task_name());
                    println("Description: %s", task_response->get_task_description());
                    println("Difficulty: %s", static_cast<int>(task_response->get_difficulty()));
                    println("========================");

                } else if (choice == "2") {
                    // 退出登录
                    auto logout_packet = std::make_unique<piap_t>(piap_msg_type_t::LOGOUT_REQUEST);
                    client.send_ctrl_packet(logout_packet);
                    is_authorized = false;
                    println("You have been logged out.");
                } else {
                    println("Invalid option. Please try again.");
                }
            }

        }
    } catch (const std::exception &e) {
        println("Client error: %s", e.what());
    }

    return 0;
}
