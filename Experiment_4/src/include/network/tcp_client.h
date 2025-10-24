#pragma once

#include <cstdint>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include "../protocols/PIAP.h"
#include "../protocols/TITP.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

/**
 * @brief 基于 TCP 的类 HTTP 理念客户端类，用于控制 TCP 管道的连接与终止以及二类数据包的发送。
 * 
 */
class tcp_client{

        private:
        int client_fd;
        int port;
        std::string server_ip;
        struct sockaddr_in server_addr;
        bool is_connected;

        public:
        explicit tcp_client(int port, const std::string& server_ip)
        : client_fd(-1), port(port), server_ip(server_ip), is_connected(false) {
            std::memset(&server_addr, 0, sizeof(sockaddr_in));
            connect_server();
        }
        ~tcp_client() noexcept {
            disconnect();
        }

        tcp_client(const tcp_client&) = delete;
        tcp_client& operator=(const tcp_client&) = delete;

        bool connect_server() {

            if (is_connected) {
                throw std::logic_error(
                std::string("Error: Client fd ") + std::to_string(client_fd) + std::string(" already connected to ") +
                server_ip + std::string(":") + std::to_string(port) + std::string(". Call disconnect() first.")
                );
            }

            client_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (client_fd < 0) {
                throw std::runtime_error(
                std::string("Error: Failed to create socket: ") + strerror(errno)
                );
            }

            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(port);

            if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
                close(client_fd);
                client_fd = -1;
                throw std::runtime_error(
                std::string("Error: Invalid server address: ") + server_ip
                );
            }

            if (connect(client_fd, (sockaddr * ) & server_addr, sizeof(sockaddr_in)) < 0) {
                int saved_errno = errno;
                close(client_fd);
                client_fd = -1;
                throw std::runtime_error(
                std::string("Error: Failed to connect to ") + server_ip + std::string(":") + std::to_string(port) +
                std::string(" - ") + strerror(saved_errno)
                );
            }

            is_connected = true;
            return true;
        }

        bool send_ctrl_packet(const std::unique_ptr<piap_t>& packet) const {
            if (!is_connected) {
                throw
                std::runtime_error(std::string("Error: Client ") + std::to_string(client_fd) +
                                   std::string(" have not connected server yet."));
            }

            auto buffer = packet->serialize();
            size_t packet_size = buffer.size();
            const void *buf = buffer.data();

            ssize_t sent_size;
            do {
                sent_size = send(client_fd, buf, packet_size, 0);
            } while (sent_size < 0 && errno == EINTR);

            // TODO: 如果后续需要引入日志系统来管理，这边需要拆分并修改。
            if (sent_size < 0 || sent_size != static_cast<ssize_t>(packet_size)) return false;

            return true;
        }

        std::unique_ptr<piap_t> recv_ctrl_packet() const {
            if (!is_connected) {
                throw
                std::runtime_error(std::string("Error: Client ") + std::to_string(client_fd) +
                                   std::string(" have not connected server yet."));
            }

            std::vector <std::byte> buf(PIAP_TOTAL_SIZE);
            ssize_t recv_size = recv(client_fd, buf.data(), PIAP_TOTAL_SIZE, MSG_WAITALL);

            // TODO: 如果后续引入日志系统，则需要拆分并修改。
            if (recv_size <= 0 || recv_size != static_cast<ssize_t>(PIAP_TOTAL_SIZE)) return nullptr;

            auto packet = piap_t::deserialize(buf.data(), buf.size());
            if (!packet) {
                throw
                std::runtime_error(std::string("Error: Failed to deserialize packet!"));
            }
            return packet;
        }

        bool send_data_packet(const std::unique_ptr<titp_t>& packet) const {
            if (!is_connected) {
                throw
                std::runtime_error(std::string("Error: Client ") + std::to_string(client_fd) +
                                   std::string(" have not connected server yet."));
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

        std::unique_ptr<titp_t> recv_data_packet() const {
            if (!is_connected) {
                throw
                std::runtime_error(std::string("Error: Client ") + std::to_string(client_fd) +
                                   std::string(" have not connected server yet."));
            }

            // 先接收消息头以确定消息大小
            char header_buffer[sizeof(titp_header_t)];
            ssize_t recv_size = recv(client_fd, header_buffer, sizeof(titp_header_t), MSG_WAITALL);

            if (recv_size <= 0 || recv_size != static_cast<ssize_t>(sizeof(titp_header_t))) return nullptr;

            // 将缓冲区转换为 titp_header_t 指针
            titp_header_t *header = reinterpret_cast<titp_header_t * >(header_buffer);

            // 验证魔数和版本 (convert from network byte order)
            uint32_t magic = ntohl(header->magic);
            uint16_t version = ntohs(header->version);
            if (magic != TITP_MAGIC || version != TITP_VERSION) return nullptr;

            // 分配完整消息的缓冲区
            uint32_t payload_length = ntohl(header->payload_length);
            size_t total_size = sizeof(titp_header_t) + payload_length;
            std::vector <std::byte> buffer(total_size);
            std::memcpy(buffer.data(), header_buffer, sizeof(titp_header_t));

            // 接收剩余的负载数据
            recv_size = recv(client_fd, buffer.data() + sizeof(titp_header_t), payload_length, MSG_WAITALL);
            if (recv_size <= 0 || recv_size != static_cast<ssize_t>(payload_length)) return nullptr;

            // 反序列化消息
            auto packet = titp_t::deserialize(buffer.data(), buffer.size());
            return packet;
        }

        bool disconnect() noexcept {
            if (client_fd >= 0) {
                close(client_fd);
                client_fd = -1;
                is_connected = false;
                return true;
            }
            return false;
        }

        int get_client_fd() const noexcept {
            return client_fd;
        }

        int get_port() const noexcept {
            return port;
        }

        const std::string& get_server_ip() const noexcept {
            return server_ip;
        }

        bool is_active() const noexcept {
            return is_connected && client_fd >= 0;
        }
};
