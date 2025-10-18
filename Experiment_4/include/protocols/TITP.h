#pragma once

// 此文件是自定义 BSTP 的数据连接部分，协议叫做 任务信息传输协议 (Task Information Transfer Protocol)

#include <cstdint>
#include <memory>
#include <cstring>
#include <ctime>

constexpr uint32_t TITP_MAGIC = 0x54495450;
constexpr uint16_t TITP_VERSION = 0X0100;
constexpr unsigned int TITP_TTL = 30;
constexpr uint32_t MAX_TASK_DESCRIPTION_SIZE = 2048;

enum class titp_msg_type_t : uint16_t {
    // ----- 客户端数据响应类型 -----
    RESOURCE_REQUEST = 0x0001,          // 客户端请求得到需求资源
    
    // ----- 服务器数据响应类型 -----
    RESOURCE_SENT = 0x0004,             // 服务器发送资源，但成不成功未知。
};

enum class titp_format_type_t : uint16_t {

    FORMAT_OK = 50,              // 格式正确，报文正常阅读

    // Otherwise:
    MAGIC_MISMATCH = 300,        // 魔数与 PIAP 协议不匹配。
    BAD_VERSION,                 // 错误的版本号。
    MSG_TYPE_NOT_FOUND,          // 报文类型不存在。
    TIMESTAMP_ERR,               // 时间戳异常。
};

enum class titp_resource_status_type_t : uint16_t {
    // 当且仅当客户端处于这个状态，才可以正常分析文件。
    RESOURCE_ACK = 1000,                // 客户端成功从服务器得到资源。

    // Otherwise:

    // 资源本身问题，用户不应该在短时间内再次请求。
    RESOURCE_NOT_FOUND = 2000,          // 请求的服务器资源不存在。 TODO: 对于目前设计这只是一个占位错误码，以后才会实装。
    RESOURCE_INFO_MISMATCH,             // 请求的服务器资源与服务器内置信息不匹配。
    RESOURCE_EXPIRED,                   // 请求的服务器资源超出权限，禁止请求 (VIP资源)
    RESOURCE_UNAVAILABLE,               // 请求的服务器资源暂时无法使用。

    // 服务器传输资源问题，用户可以再次尝试请求。
    SERVER_TRANSFER_TIMEOUT = 3000,     // 服务器延迟导致资源请求超时。 TODO: 与时间相关的机制在后续设计中引入。
    SERVER_TRANSFER_BREAKDOWN,          // 服务器自身问题导致资源传输不过去。
};

enum class task_difficulty_t : uint8_t {
    UNKNOWN = 0,
    QUIET_EASY = 1,
    EASY,
    MEDIUM,
    HARD,
    EXTREMELY_HARD
};

struct titp_header_t {

    uint32_t magic;
    uint16_t version;
    uint16_t msg_type;
    uint32_t payload_length;
    uint32_t timestamp;
    uint32_t reserved;                  // 保留字段，后续看情况拓展。

    explicit titp_header_t(titp_msg_type_t msg_t, uint32_t pylength)
    : magic(TITP_MAGIC), version(TITP_VERSION),
      msg_type(static_cast<uint16_t>(msg_t)),
      payload_length(pylength), timestamp(0),
      reserved(0) {}
};

struct titp_task_metadata_t {
    uint64_t task_id;                       // 8 字节
    char task_name[64];                     // 64 字节
    uint8_t difficulty;                     // 1 字节
    uint8_t reserved[3];                    // 3 字节（对齐到 4 字节边界）
    uint16_t msg_status;                    // 2 字节（格式验证状态）
    uint16_t resource_status;               // 2 字节（资源业务状态）
};

struct titp_request_payload_t {
    uint64_t task_id;
};

struct titp_response_payload_t {
    titp_task_metadata_t metadata;
    char task_description[MAX_TASK_DESCRIPTION_SIZE];
};

struct titp_t {

private:
    titp_header_t header;

    union {
        titp_request_payload_t request;
        titp_response_payload_t response;
    } payload;

public:

    explicit titp_t(titp_msg_type_t msg_t) 
    : header(msg_t, 0) {
        std::memset(&payload, 0, sizeof(payload));
        if(msg_t == titp_msg_type_t::RESOURCE_REQUEST) {
            header.payload_length = sizeof(titp_request_payload_t);
        }
        else if (msg_t == titp_msg_type_t::RESOURCE_SENT) {
            header.payload_length = sizeof(titp_response_payload_t);
        }
    }

    titp_t(const titp_t&) = delete;
    titp_t& operator=(const titp_t&) = delete;

    size_t data(void *buf) const noexcept {
        if(!buf) return 0;

        std::byte *ptr = static_cast<std::byte*>(buf);
        
        std::memcpy(ptr, &header, sizeof(titp_header_t));
        ptr += sizeof(titp_header_t);

        if (header.msg_type == static_cast<uint16_t>(titp_msg_type_t::RESOURCE_REQUEST)) {
            std::memcpy(ptr, &payload.request, sizeof(titp_request_payload_t));
        } 
        else if(header.msg_type == static_cast<uint16_t>(titp_msg_type_t::RESOURCE_SENT)) {
            std::memcpy(ptr, &payload.response, sizeof(titp_response_payload_t));
        }

        return size();
    }

    size_t size() const noexcept {
        return sizeof(titp_header_t) + header.payload_length;
    }

    static std::unique_ptr<titp_t> deserialize(const void *data, size_t len) {

        if (len < sizeof(titp_header_t)) return nullptr;
        
        const titp_header_t *h = static_cast<const titp_header_t*>(data);
        
        if (h->magic != TITP_MAGIC || h->version != TITP_VERSION) return nullptr;
        
        auto msg_type = static_cast<titp_msg_type_t>(h->msg_type);
        
        if (len < sizeof(titp_header_t) + h->payload_length) {
            return nullptr;
        }

        auto packet = std::make_unique<titp_t>(msg_type);
        std::memcpy(&packet->header, h, sizeof(titp_header_t));
        const std::byte *payload_ptr = static_cast<const std::byte*>(data) + sizeof(titp_header_t);
        
        if (msg_type == titp_msg_type_t::RESOURCE_REQUEST) {
            std::memcpy(&packet->payload.request, payload_ptr, sizeof(titp_request_payload_t));
        } 
        else {
            std::memcpy(&packet->payload.response, payload_ptr, sizeof(titp_response_payload_t));
        }
        
        return packet;
    }

    titp_format_type_t valid_format() noexcept {

        if (header.magic != TITP_MAGIC) {
            return titp_format_type_t::MAGIC_MISMATCH;
        }
        
        if (header.version != TITP_VERSION) {
            return titp_format_type_t::BAD_VERSION;
        }
        
        uint16_t msg = header.msg_type;
        if (msg != static_cast<uint16_t>(titp_msg_type_t::RESOURCE_REQUEST) &&
            msg != static_cast<uint16_t>(titp_msg_type_t::RESOURCE_SENT)) {
            return titp_format_type_t::MSG_TYPE_NOT_FOUND;
        }
        
        if (header.timestamp != 0) {
            uint32_t current_time = static_cast<uint32_t>(std::time(nullptr));
            if (current_time > header.timestamp + TITP_TTL || 
                current_time < header.timestamp) {
                return titp_format_type_t::TIMESTAMP_ERR;
            }
        }
        
        return titp_format_type_t::FORMAT_OK;
    }
    
    titp_resource_status_type_t valid_resource(bool resource_exists, bool resource_valid = true) noexcept {
        if (header.msg_type != static_cast<uint16_t>(titp_msg_type_t::RESOURCE_SENT)) {
            return titp_resource_status_type_t::RESOURCE_NOT_FOUND;
        }
        
        if (!resource_exists) {
            return titp_resource_status_type_t::RESOURCE_NOT_FOUND;
        }
        
        if (!resource_valid) {
            return titp_resource_status_type_t::RESOURCE_INFO_MISMATCH;
        }
        
        return titp_resource_status_type_t::RESOURCE_ACK;
    }
    
    void set_msg_status(titp_format_type_t status) noexcept {
        if (header.msg_type == static_cast<uint16_t>(titp_msg_type_t::RESOURCE_SENT)) {
            payload.response.metadata.msg_status = static_cast<uint16_t>(status);
        }
    }
    
    void set_resource_status(titp_resource_status_type_t status) noexcept {
        if (header.msg_type == static_cast<uint16_t>(titp_msg_type_t::RESOURCE_SENT)) {
            payload.response.metadata.resource_status = static_cast<uint16_t>(status);
        }
    }
    
    void set_task_id(uint64_t id) noexcept {
        if (header.msg_type == static_cast<uint16_t>(titp_msg_type_t::RESOURCE_REQUEST)) {
            payload.request.task_id = id;
        } else {
            payload.response.metadata.task_id = id;
        }
    }
    
    void set_task_name(const char* name) noexcept {
        if (header.msg_type == static_cast<uint16_t>(titp_msg_type_t::RESOURCE_SENT)) {
            std::strncpy(payload.response.metadata.task_name, name, sizeof(payload.response.metadata.task_name) - 1);
            payload.response.metadata.task_name[sizeof(payload.response.metadata.task_name) - 1] = '\0';
        }
    }
    
    void set_task_description(const char* desc) noexcept {
        if (header.msg_type == static_cast<uint16_t>(titp_msg_type_t::RESOURCE_SENT)) {
            std::strncpy(payload.response.task_description, desc, sizeof(payload.response.task_description) - 1);
            payload.response.task_description[sizeof(payload.response.task_description) - 1] = '\0';
        }
    }
    
    void set_difficulty(task_difficulty_t diff) noexcept {
        if (header.msg_type == static_cast<uint16_t>(titp_msg_type_t::RESOURCE_SENT)) {
            payload.response.metadata.difficulty = static_cast<uint8_t>(diff);
        }
    }
    
    titp_msg_type_t get_msg_type() const noexcept {
        return static_cast<titp_msg_type_t>(header.msg_type);
    }
    
    titp_format_type_t get_format_status() const noexcept {
        if (header.msg_type == static_cast<uint16_t>(titp_msg_type_t::RESOURCE_SENT)) {
            return static_cast<titp_format_type_t>(payload.response.metadata.msg_status);
        }
        return titp_format_type_t::FORMAT_OK;
    }
    
    titp_resource_status_type_t get_resource_status() const noexcept {
        if (header.msg_type == static_cast<uint16_t>(titp_msg_type_t::RESOURCE_SENT)) {
            return static_cast<titp_resource_status_type_t>(payload.response.metadata.resource_status);
        }
        return titp_resource_status_type_t::RESOURCE_NOT_FOUND;
    }
    
    uint64_t get_task_id() const noexcept {
        if (header.msg_type == static_cast<uint16_t>(titp_msg_type_t::RESOURCE_REQUEST)) {
            return payload.request.task_id;
        }
        return payload.response.metadata.task_id;
    }
    
    const char* get_task_name() const noexcept {
        if (header.msg_type == static_cast<uint16_t>(titp_msg_type_t::RESOURCE_SENT)) {
            return payload.response.metadata.task_name;
        }
        return "";
    }
    
    const char* get_task_description() const noexcept {
        if (header.msg_type == static_cast<uint16_t>(titp_msg_type_t::RESOURCE_SENT)) {
            return payload.response.task_description;
        }
        return "";
    }
    
    task_difficulty_t get_difficulty() const noexcept {
        if (header.msg_type == static_cast<uint16_t>(titp_msg_type_t::RESOURCE_SENT)) {
            return static_cast<task_difficulty_t>(payload.response.metadata.difficulty);
        }
        return task_difficulty_t::UNKNOWN;
    }

};