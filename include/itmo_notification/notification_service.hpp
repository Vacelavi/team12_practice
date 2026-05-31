#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "itmo_notification/due_notification.hpp"
#include "itmo_notification/notification.hpp"

namespace itmo_notification {

// In-memory планировщик уведомлений. API менять нельзя: он соответствует HTTP-ручкам.
class NotificationService {
public:
    NotificationService();
    ~NotificationService();

    NotificationService(const NotificationService&)            = delete;
    NotificationService& operator=(const NotificationService&) = delete;

    // POST /v1/notifications
    void add(Notification notification);

    // DELETE /v1/notifications/{id}
    bool cancel(std::string_view id);

    // POST /v1/notifications/{id}/sent
    bool markSent(std::string_view id);

    // GET /v1/notifications/{id}
    std::optional<Notification> get(std::string_view id) const;

    // GET /v1/due?now=...&limit=...
    std::vector<DueNotification> due(std::int64_t now, std::size_t limit) const;

private:
    std::unordered_map<std::string, Notification> notifications_;
    std::vector<std::string>                      schedule_;
    std::unordered_set<std::string>               cancelled_;
    std::unordered_set<std::string>               sent_;

    mutable std::mutex mu_;
};

}  // namespace itmo_notification
