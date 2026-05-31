#pragma once

#include <cstdint>
#include <string>

namespace itmo_notification {

struct DueNotification {
    std::string  id;
    std::string  user_id;
    std::string  channel;
    std::string  recipient;
    std::string  template_name;
    std::string  payload;
    std::int64_t send_at{};
    int          priority{};
    std::int64_t created_at{};
};

}  // namespace itmo_notification
