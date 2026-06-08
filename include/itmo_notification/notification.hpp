#pragma once

#include <cstdint>
#include <string>

namespace itmo_notification {

enum class NotificationStatus {
    Pending,
    Sent,
    Cancelled,
};

struct Notification {
    std::string  id;
    std::string  user_id;
    std::string  channel;
    std::string  recipient;
    std::string  template_name;
    std::string  payload;
    std::int64_t send_at{};
    int          priority{};
    std::int64_t created_at{};
    NotificationStatus status{NotificationStatus::Pending};
};

struct ScheduleEntry {
    std::string id;
    std::int64_t send_at{};
    int          priority{};
    std::int64_t created_at{};

    ScheduleEntry(const Notification& notification)
        : id(notification.id)
        , send_at(notification.send_at)
        , priority(notification.priority)
        , created_at(notification.created_at)
    {}
};

struct ScheduleEntryLess {
    bool operator()(const ScheduleEntry& lhs, const ScheduleEntry& rhs) const;
};

}  // namespace itmo_notification
