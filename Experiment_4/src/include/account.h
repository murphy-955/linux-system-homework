#pragma once

#include <string>
#include <cstdint>
#include <stdexcept>

constexpr const uint8_t MAX_LEVEL = 100;
constexpr const uint8_t MIN_LEVEL = 1;

class account {

private:
    std::string usr_ID;
    std::string password;
    std::string usr_name;
    
    uint8_t level;

public:
    account(const std::string& uid, const std::string& pwd, const std::string& name,
           uint8_t lvl, uint16_t str, uint16_t intel, uint16_t agi)
        : usr_ID(uid),
          password(pwd),
          usr_name(name),
          level(lvl) 
        {
        if (usr_ID.empty()) {
            throw std::invalid_argument("Error: UserID cannot be empty.");
        }
        if (password.empty()) {
            throw std::invalid_argument("Error: Password cannot be empty.");
        }
        if (usr_name.empty()) {
            throw std::invalid_argument("Error: Character name cannot be empty.");
        }
        if (level < MIN_LEVEL || level > MAX_LEVEL) {
            throw std::invalid_argument("Error: Level must be between 1 and 100.");
        }
    }

    const std::string& get_usr_ID() const { return usr_ID; }
    const std::string& get_character_name() const { return usr_name; }
    uint8_t get_level() const { return level; }

    void chg_password(const std::string& old_pwd, const std::string& new_pwd) {
        if (old_pwd != password) {
            throw std::invalid_argument("Error: Invalid password.");
        }
        if (new_pwd.empty()) {
            throw std::invalid_argument("Error: New password cannot be empty.");
        }
        password = new_pwd;
    }

    void set_usr_name(const std::string& name) {
        if (name.empty()) {
            throw std::invalid_argument("Error: Character name cannot be empty.");
        }
        usr_name = name;
    }
};