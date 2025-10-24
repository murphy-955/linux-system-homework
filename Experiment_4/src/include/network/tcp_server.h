#pragma once

#include <cstdint>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <memory>
#include <cstring>
#include <stdexcept>
#include "../protocols/PIAP.h"
#include "../protocols/TITP.h"

/**
 * @brief 基于 TCP 的类 HTTP 理念服务器类，用于管理服务器 TCP 管道的连接与终止以及二类数据包的发送。
 * 允许强制客户端直接关闭 TCP 连接，但所有的数据报检测与标签交互逻辑应当由应用层管理。
 *
 */
class tcp_server{

        private:
        int server_fd;
        int port;
        sockaddr_in server_addr;
        bool is_running;

        public:
        /**
         * @brief 初始化 TCP 服务器的同时开放服务器连接通道
         *
         * @param port 端口号，应当由客户端与服务器共同协商达成。
         */
        explicit tcp_server(int port)
        : server_fd(-1), port(port), is_running(false) {

            server_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (server_fd < 0) {
                throw std::runtime_error(
                std::string("Error: Failed to initialize server - ") + strerror(errno)
                );
            }

            int opt = 1;
            if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
                int saved_errno = errno;
                close(server_fd);
                throw std::runtime_error(
                std::string("Error: Failed to set socket options - ") + strerror(saved_errno)
                );
            }

            std::memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = INADDR_ANY;
            server_addr.sin_port = htons(port);

            if (bind(server_fd, (sockaddr * ) & server_addr, sizeof(server_addr)) < 0) {
                int saved_errno = errno;
                close(server_fd);
                server_fd = -1;
                throw std::runtime_error(
                std::string("Error: Failed to bind socket to port {} - ") + strerror(saved_errno)
                );
            }

            if (listen(server_fd, 10) < 0) {
                int saved_errno = errno;
                close(server_fd);
                server_fd = -1;
                throw std::runtime_error(
                std::string("Error: Failed to listen on port {} - ") + strerror(saved_errno)
                );
            }

            is_running = true;
        }

        ~tcp_server() noexcept {
            shutdown_server();
        }

        tcp_server(const tcp_server&) = delete;
        tcp_server& operator=(const tcp_server&) = delete;

        bool send_ctrl_packet(int client_fd, const std::unique_ptr<piap_t>& packet) const {
            if (!is_running || client_fd < 0) {
                throw
                std::runtime_error("Error: Server is not running or invalid client file descriptor.");
            }

            auto buffer = packet->serialize();
            size_t packet_size = buffer.size();
            const void *buf = buffer.data();

            ssize_t sent_size;
            do {
                sent_size = send(client_fd, buf, packet_size, 0);
            } while (sent_size < 0 && errno == EINTR);

            if (sent_size < 0 || sent_size != static_cast<ssize_t>(packet_size)) return false;

            return true;
        }

        std::unique_ptr<piap_t> recv_ctrl_packet(int client_fd) const {
            // 先 peek magic number
            uint32_t magic_buf;
            ssize_t peek_size = recv(client_fd, &magic_buf, sizeof(uint32_t), MSG_PEEK);
            
            if (peek_size != sizeof(uint32_t)) {
                return nullptr;
            }
            
            uint32_t magic = ntohl(magic_buf);
            if (magic != PIAP_MAGIC) {
                return nullptr;  // 不是 PIAP 包，不消费数据
            }
            
            // 确认是 PIAP 包后再读取
            std::vector<std::byte> buf(PIAP_TOTAL_SIZE);
            ssize_t recv_size = recv(client_fd, buf.data(), PIAP_TOTAL_SIZE, MSG_WAITALL);
            
            if (recv_size != static_cast<ssize_t>(PIAP_TOTAL_SIZE)) {
                return nullptr;
            }
            
            return piap_t::deserialize(buf.data(), buf.size());
        }

        bool send_data_packet(int client_fd, const std::unique_ptr<titp_t>& packet) const {
            if (!is_running || client_fd < 0) {
                throw
                std::runtime_error("Error: Server is not running or invalid client file descriptor.");
            }

            auto buffer = packet->serialize();
            size_t packet_size = buffer.size();
            const void *buf = buffer.data();

            ssize_t sent_size;
            do {
                sent_size = send(client_fd, buf, packet_size, 0);
            } while (sent_size < 0 && errno == EINTR);

            if (sent_size < 0 || sent_size != static_cast<ssize_t>(packet_size)) return false;

            return true;
        }

        std::unique_ptr<titp_t> recv_data_packet(int client_fd) const {
            // 先 peek magic number
            uint32_t magic_buf;
            ssize_t peek_size = recv(client_fd, &magic_buf, sizeof(uint32_t), MSG_PEEK);
            
            if (peek_size != sizeof(uint32_t)) {
                return nullptr;
            }
            
            uint32_t magic = ntohl(magic_buf);
            if (magic != TITP_MAGIC) {
                return nullptr;  // 不是 TITP 包，不消费数据
            }
            
            // 确认是 TITP 包后再读取
            char header_buffer[sizeof(titp_header_t)];
            ssize_t recv_size = recv(client_fd, header_buffer, sizeof(titp_header_t), MSG_WAITALL);
            
            if (recv_size != sizeof(titp_header_t)) {
                return nullptr;
            }
            
            titp_header_t* header = reinterpret_cast<titp_header_t*>(header_buffer);
            uint32_t payload_length = ntohl(header->payload_length);
            
            size_t total_size = sizeof(titp_header_t) + payload_length;
            std::vector<std::byte> buffer(total_size);
            std::memcpy(buffer.data(), header_buffer, sizeof(titp_header_t));
            
            recv_size = recv(client_fd, buffer.data() + sizeof(titp_header_t), 
                            payload_length, MSG_WAITALL);
            
            if (recv_size != static_cast<ssize_t>(payload_length)) {
                return nullptr;
            }
            
            return titp_t::deserialize(buffer.data(), buffer.size());
        }
        
        /**
         * @brief 从信道中监听并获取到服务器的 client_fd 以建立 TCP 连接。
         *
         * @return int 通常情况下为 `client_fd`, 如果没有监听到则返回 -1.
         */
        int accept_connection() noexcept {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);

            int fd = accept(server_fd, (struct sockaddr *) &client_addr, &addr_len);

            return fd;
        }

        bool kick_client(int client_fd) noexcept {
            if (client_fd >= 0) {
                close(client_fd);
                return true;
            }
            return false;
        }

        bool shutdown_server() noexcept {
            if (server_fd >= 0) {
                close(server_fd);
                server_fd = -1;
            }
            is_running = false;
            return true;
        }

        int get_server_fd() const noexcept {
            return server_fd;
        }

        int get_port() const noexcept {
            return port;
        }

        bool is_active() const noexcept {
            return is_running;
        }
};