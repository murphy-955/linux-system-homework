#include "include/network/tcp_client.h"
#include "include/communication_config.h"
#include <string>
#include <iostream>
#include <format>
#include "include/protocols/PIAP.h"
#include "include/protocols/TITP.h"
#include <print>

void clear_screen() {
    std::print("\033[2J\033[H");
}

int main() {
    tcp_client client(PORT, SERVER_TEST);
    std::cout << std::format("Client connects to server: {}:{}.\n", client.get_server_ip(), client.get_port());



    std::println("\n===== 💐 Welcome to the Bounty System Server =====");
    std::println("Server: {}:{}", client.get_server_ip(), client.get_port());
    std::println("==================================================\n");

    while(true){

        bool is_authorized = false;
        std::string acc, pwd;
        std::print("Username (or 'quit' to exit): ");
        std::getline(std::cin, acc);
        if(acc == "quit") {
            break;
        }
        std::print("Password: ");
        std::getline(std::cin, pwd);

        // 创建登录认证数据报，并且在尝试后失败就要回去重新登录。
        {
            std::unique_ptr<piap_t> auth_pkt = std::make_unique<piap_t>(piap_msg_type_t::LOGIN_REQUEST);
            auth_pkt->set_usr_info(acc.c_str(), pwd.c_str());
            bool is_send = false;

            for(auto i = 0; i < MAX_RETRIES; ++i) {
                if(client.send_ctrl_packet(auth_pkt)) {
                    is_send = true;
                    break;
                }
                std::println("Warning: Authorization retrying {}/{}...", i+1, MAX_RETRIES);
            }
            
            if(!is_send) {
                std::println("Error: Failed to create the log in request, try again!");
                continue;
            }
        }

        // 接受服务器发送的数据报，并且通过状态检测后方可进行数据交互。
        {
            auto recv_pkt = client.recv_ctrl_packet();

            if(recv_pkt == nullptr) {
                std::println("Error: Failed to receive server response, try again!");
                continue;
            }

            if(recv_pkt->get_msg_type() != piap_msg_type_t::LOGIN_RESPONSE) {
                continue;
            }
            auto format_status = recv_pkt->valid_format();
            if(format_status != piap_format_type_t::FORMAT_OK) {
                std::println("Error Code {}: Log in request broke down, try again!", static_cast<uint16_t>(format_status));
                continue;
            }
            auto auth_status = recv_pkt->get_auth_status();
            if(auth_status != piap_auth_type_t::LOGIN_SUCCESS) {
                std::println("Error Code {}: {}", static_cast<uint16_t>(auth_status), recv_pkt->get_status_msg());
                continue;
            }
        }

        is_authorized = true;
        std::println("Welcome back, user: {}", acc);

        // TODO: 进行数据层面的交互。
        while(is_authorized) {
            is_authorized = false;
        }

    }

    client.disconnect();
    std::println("Client disconnecting successfully, see you next time!");
    return 0;
}