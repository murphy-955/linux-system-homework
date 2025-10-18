#pragma once

#include <cstdint>
#include <cstring>
#include <ctime>
#include <memory>

// 此文件是自定义 BSTP 的连接控制部分，协议叫做：玩家身份认证协议 (Player Identity Authorization Protocol, 简称 PIAP)

// PIAP 协议认证信息类型枚举类
enum class piap_msg_type_t : uint16_t {

    // ----- 客户端认证信息类型 ------
    SIGNUP_REQUEST = 0x0001,        // 客户端向服务器发送注册其请求
    LOGIN_REQUEST = 0x0002,         // 客户端向服务器发送认证请求。
    LOGOUT_REQUEST = 0x0003,        // 客户端请求断开连接

    // ----- 服务器认证消息类型
    SIGNUP_RESPONSE = 0x0004,       // 服务器成功注册用户。
    LOGIN_RESPONSE = 0x0005,        // 服务器同意客户端登录
    FORCE_LOGOUT = 0x0006           // 服务器发送信息使得客户端强制退出。
};

enum class piap_format_type_t : uint16_t {

    FORMAT_OK = 50,              // 格式正确，报文正常阅读

    // Otherwise:
    MAGIC_MISMATCH = 300,        // 魔数与 PIAP 协议不匹配。
    BAD_VERSION,                 // 错误的版本号。
    MSG_TYPE_NOT_FOUND,          // 报文类型不存在。
    TIMESTAMP_ERR,               // 时间戳异常。
};

// PIAP 协议错误信息类型枚举类
enum class piap_auth_type_t : uint16_t {

    // ----- PIAP 协议只有该状态才算作控制通路连接 -----
    SIGNUP_SUCCESS = 100,        // 注册成功，按照本处逻辑注册即可直接登录。
    LOGIN_SUCCESS = 200,         // 验证与登录均通过，客户端成功连接上服务器并允许传输信息。

    // Otherwise:

    // ----- 客户端请求错误 -----
    BAD_REQUEST = 400,           // 客户端请求错误，即非注册也非登录。
    USER_ALREADY_EXISTS,         // 注册的账户已经存在。
    USER_NOT_FOUND,              // 服务器内并不存在该玩家账号。
    WRONG_PASSWORD,              // 账户登录信息错误。
    USER_BANNED,                 // 账户被服务器直接拒绝，目前只有被封号的原因。

    // ----- 服务器错误 -----
    SERVER_ERR_RESPONSE = 500,   // 服务器响应异常。
    SERVER_UNAVAILABLE = 503     // 服务器不可用。
};

constexpr uint32_t PIAP_MAGIC = 0x50494150;
constexpr uint16_t PIAP_VERSION = 0x0100;
// TTL 应当由客户端与服务器约定，目前设置 TTL 为一分钟，保证连接不会被拦截并盗取信息。
constexpr unsigned int PIAP_TTL = 60;

constexpr size_t PIAP_TOTAL_SIZE = 532;

struct piap_header_t {
    uint32_t magic;
    uint16_t version;
    uint16_t msg_type;
    uint32_t payload_length;
    uint32_t timestamp;
    uint32_t reserved;

    explicit piap_header_t(uint16_t msg, uint32_t pylength, uint32_t time = 0)
        : magic(PIAP_MAGIC), version(PIAP_VERSION),
          msg_type(msg), payload_length(pylength),
          timestamp(time), reserved(0) {}
};

struct piap_payload_t {
    char userID[64];
    char password[128];
    char session[64];         // TODO: 用于生成唯一令牌
    char status_msg[256];
    
    uint16_t msg_status;
    uint16_t auth_status;

    piap_payload_t() : msg_status(0), auth_status(0) {
        std::memset(userID, 0, sizeof(userID));
        std::memset(password, 0, sizeof(password));
        std::memset(session, 0, sizeof(session));
        std::memset(status_msg, 0, sizeof(status_msg));
    }
};

/**
 * @brief 用户身份认证协议的具体定义，具有首部与载荷部分。
 * 
 */
class piap_t {

private:
    piap_header_t header;
    piap_payload_t payload;
    
public:

    explicit piap_t(piap_msg_type_t msg_t)
        : header(static_cast<uint16_t>(msg_t), sizeof(piap_payload_t)),
          payload() {}

    // PIAP 报文不允许被拷贝
    piap_t(const piap_t&) = delete;
    piap_t& operator=(const piap_t&) = delete;

    // TODO: PIAP 允许移动当且仅当请求方为原客户端。

    /**
     * @brief PIAP 协议的序列化函数，手动进行分配。
     * 
     */
    const void *data() const {
        return &header;
    }

    size_t size() const {
        return sizeof(piap_header_t) + sizeof(piap_payload_t);
    }

    static std::unique_ptr<piap_t> deserialize(const void *data, size_t len) {
        if(len < sizeof(piap_header_t) + sizeof(piap_payload_t)) return nullptr;

        const piap_header_t *h = static_cast<const piap_header_t*>(data);
        if(h->magic != PIAP_MAGIC || h->version != PIAP_VERSION) return nullptr;

        auto packet = std::make_unique<piap_t>(static_cast<piap_msg_type_t>(h->msg_type));

        std::memcpy(&packet->header, h, sizeof(piap_header_t));

        // 因为内存连续，因此如果要得到载荷的部分就应该加上对应的长度才可以访问到实际的内存。
        const std::byte *payload_bytes = static_cast<const std::byte*>(data) + sizeof(piap_header_t);
        std::memcpy(&packet->payload, payload_bytes, sizeof(piap_payload_t));

        return packet;
    }

    /**
     * @brief 验证数据包格式是否合法（魔数、版本、TTL、字段完整性）
     * 
     * @return piap_format_type_t 格式验证结果
     */
    piap_format_type_t valid_format() noexcept {

        if (header.magic != PIAP_MAGIC) {
            return piap_format_type_t::MAGIC_MISMATCH;
        }
        
        if (header.version != PIAP_VERSION) {
            return piap_format_type_t::BAD_VERSION;
        }
        
        uint16_t msg = header.msg_type;
        if (msg != static_cast<uint16_t>(piap_msg_type_t::SIGNUP_REQUEST) &&
            msg != static_cast<uint16_t>(piap_msg_type_t::LOGIN_REQUEST) &&
            msg != static_cast<uint16_t>(piap_msg_type_t::LOGOUT_REQUEST) &&
            msg != static_cast<uint16_t>(piap_msg_type_t::SIGNUP_RESPONSE) &&
            msg != static_cast<uint16_t>(piap_msg_type_t::LOGIN_RESPONSE) &&
            msg != static_cast<uint16_t>(piap_msg_type_t::FORCE_LOGOUT)) {
            return piap_format_type_t::MSG_TYPE_NOT_FOUND;
        }
        
        if (header.timestamp != 0) {
            uint32_t current_time = static_cast<uint32_t>(std::time(nullptr));
            if (current_time > header.timestamp + PIAP_TTL || 
                current_time < header.timestamp) {
                return piap_format_type_t::TIMESTAMP_ERR;
            }
        }
        
        if (header.msg_type == static_cast<uint16_t>(piap_msg_type_t::SIGNUP_REQUEST) ||
            header.msg_type == static_cast<uint16_t>(piap_msg_type_t::LOGIN_REQUEST)) {
            
            if (payload.userID[0] == '\0' || payload.password[0] == '\0') {
                return piap_format_type_t::MSG_TYPE_NOT_FOUND;
            }
        }
        
        return piap_format_type_t::FORMAT_OK;
    }
    
    
    /**
     * @brief 验证用户凭证（用户名和密码），服务器端使用
     * 
     * @param expected_userID 期望的用户名（从数据库获取）
     * @param expected_password 期望的密码（从数据库获取）
     * @return piap_auth_type_t 验证结果
     */
    piap_auth_type_t valid_auth(const char* expected_userID = nullptr, 
                                        const char* expected_password = nullptr) noexcept {
        
        // 管理注册逻辑，只需要注册的话就不用考虑用户名和密码的问题了。
        if (header.msg_type == static_cast<uint16_t>(piap_msg_type_t::SIGNUP_REQUEST)) {
            if (expected_userID != nullptr) {
                return piap_auth_type_t::USER_ALREADY_EXISTS;
            }
            
            return piap_auth_type_t::SIGNUP_SUCCESS;
        }
        
        // 管理登录逻辑，登录需要进行身份认证。
        if (header.msg_type == static_cast<uint16_t>(piap_msg_type_t::LOGIN_REQUEST)) {
            if (expected_userID == nullptr || 
                std::strncmp(payload.userID, 
                    expected_userID, 
                    sizeof(payload.userID)) != 0) {

                return piap_auth_type_t::USER_NOT_FOUND;
            }
            
            // TODO: 后续可以检查用户是否被封禁
            
            if (expected_password == nullptr || 
                std::strncmp(payload.password, 
                    expected_password, 
                    sizeof(payload.password)) != 0) {

                return piap_auth_type_t::WRONG_PASSWORD;
            }
            
            return piap_auth_type_t::LOGIN_SUCCESS;
        }

        return piap_auth_type_t::BAD_REQUEST;
    }

    /**
     * @brief 设置格式状态码
     */
    void set_msg_status(piap_format_type_t status) noexcept {
        payload.msg_status = static_cast<uint16_t>(status);
    }
    
    /**
     * @brief 设置认证状态并自动填充对应的提示信息
     * 
     * @param status 认证状态码
     */
    void set_auth_status(piap_auth_type_t status) {
        payload.auth_status = static_cast<uint16_t>(status);
        
        const char* default_msg = nullptr;
        
        switch (status) {
            case piap_auth_type_t::SIGNUP_SUCCESS:
                default_msg = "Registration successful!";
                break;
            case piap_auth_type_t::LOGIN_SUCCESS:
                default_msg = "Authentication successful!";
                break;
                
            // 客户端请求错误
            case piap_auth_type_t::BAD_REQUEST:
                default_msg = "Error: Invalid request format or missing required fields";
                break;
            case piap_auth_type_t::USER_ALREADY_EXISTS:
                default_msg = "Error: Username already exists.";
                break;
            case piap_auth_type_t::USER_NOT_FOUND:
                default_msg = "Error: Invalid account.";
                break;
            case piap_auth_type_t::WRONG_PASSWORD:
                default_msg = "Error: Invalid password.";
                break;
            case piap_auth_type_t::USER_BANNED:
                default_msg = "Permission denied: Account has been banned!";
                break;
                
            // 服务器错误
            case piap_auth_type_t::SERVER_ERR_RESPONSE:
                default_msg = "Error: Internal server error.";
                break;
            case piap_auth_type_t::SERVER_UNAVAILABLE:
                default_msg = "Error: Service temporarily unavailable.";
                break;
                
            default:
                default_msg = "Unknown error occurred!";
                break;
        }
        
        std::strncpy(payload.status_msg, default_msg, sizeof(payload.status_msg) - 1);
        payload.status_msg[sizeof(payload.status_msg) - 1] = '\0';
    }
    
    /**
     * @brief 设置格式状态并自动填充对应的提示信息
     * 
     * @param status 格式状态码
     */
    void set_format_status(piap_format_type_t status) {
        payload.msg_status = static_cast<uint16_t>(status);
        
        const char* default_msg = nullptr;
        
        switch (status) {
            case piap_format_type_t::FORMAT_OK:
                default_msg = "Format validation passed.";
                break;
            case piap_format_type_t::MAGIC_MISMATCH:
                default_msg = "Error: Protocol magic number mismatch.";
                break;
            case piap_format_type_t::BAD_VERSION:
                default_msg = "Error: Unsupported protocol version.";
                break;
            case piap_format_type_t::MSG_TYPE_NOT_FOUND:
                default_msg = "Error: Unknown message type.";
                break;
            case piap_format_type_t::TIMESTAMP_ERR:
                default_msg = "Error: Request timestamp expired or invalid.";
                break;
            default:
                default_msg = "Unknown format error occurred!";
                break;
        }
        
        std::strncpy(payload.status_msg, default_msg, sizeof(payload.status_msg) - 1);
        payload.status_msg[sizeof(payload.status_msg) - 1] = '\0';
    }
    
    /**
     * @brief 设置用户凭据（客户端发送请求时使用）
     */
    void set_usr_info(const char* user, const char* pass) {
        std::strncpy(payload.userID, user, sizeof(payload.userID) - 1);
        payload.userID[sizeof(payload.userID) - 1] = '\0';
        
        std::strncpy(payload.password, pass, sizeof(payload.password) - 1);
        payload.password[sizeof(payload.password) - 1] = '\0';
    }

    // ========== Getters ==========
    
    /**
     * @brief 获取消息类型
     */
    piap_msg_type_t get_msg_type() const noexcept {
        return static_cast<piap_msg_type_t>(header.msg_type);
    }
    
    /**
     * @brief 获取格式验证状态
     */
    piap_format_type_t get_format_status() const noexcept {
        return static_cast<piap_format_type_t>(payload.msg_status);
    }
    
    /**
     * @brief 获取认证状态
     */
    piap_auth_type_t get_auth_status() const noexcept {
        return static_cast<piap_auth_type_t>(payload.auth_status);
    }
    
    /**
     * @brief 获取用户名
     */
    const char* get_userID() const noexcept {
        return payload.userID;
    }
    
    /**
     * @brief 获取密码
     */
    const char* get_password() const noexcept {
        return payload.password;
    }
    
    /**
     * @brief 获取错误消息
     */
    const char* get_status_msg() const noexcept {
        return payload.status_msg;
    }
};