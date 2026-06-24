#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <set>
#include <vector>

#include "itmo_notification/due_notification.hpp"
#include "itmo_notification/notification.hpp"

namespace itmo_notification {

class NotificationLimitator {
public:

    NotificationLimitator();

    void limit(size_t& limit);
    void setLimit(size_t limit);
    void setPeriod(size_t period);

private:
    size_t latest_timestamp_;
    size_t sent_counter_;
    size_t sent_limit_;
    size_t period_;

    size_t unixNow();

};


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

    // POST /v1/due/claim?now=...&limit=...
    std::vector<DueNotification> claim(std::int64_t now, std::size_t limit);

    // POST /v1/limits/config?period=...&limit=...
    void configLimit(size_t period, size_t limit);

private:
    std::unordered_map<std::string, Notification> notifications_;
    std::set<ScheduleEntry, ScheduleEntryLess>    schedule_;
    mutable std::shared_mutex mu_;
    mutable NotificationLimitator limitator_;
};

}  // namespace itmo_notification
