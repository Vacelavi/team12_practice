#include "itmo_notification/notification_service.hpp"

#include <algorithm>
#include <utility>

namespace itmo_notification {

namespace {

DueNotification toDue(const Notification& n) {
    return {
        n.id,
        n.user_id,
        n.channel,
        n.recipient,
        n.template_name,
        n.payload,
        n.send_at,
        n.priority,
        n.created_at,
    };
}

}  // namespace

NotificationService::NotificationService()  = default;
NotificationService::~NotificationService() = default;

void NotificationService::add(Notification notification) {
    std::lock_guard<std::mutex> lk(mu_);
    notification.status = NotificationStatus::Pending;
    schedule_.push_back(notification.id);
    notifications_[notification.id] = std::move(notification);
}

bool NotificationService::cancel(std::string_view id) {
    std::lock_guard<std::mutex> lk(mu_);
    const auto key = std::string(id);
    auto it = notifications_.find(key);
    if (it == notifications_.end()) {
        return false;
    }
    cancelled_.insert(key);
    it->second.status = NotificationStatus::Cancelled;
    return true;
}

bool NotificationService::markSent(std::string_view id) {
    std::lock_guard<std::mutex> lk(mu_);
    const auto key = std::string(id);
    auto it = notifications_.find(key);
    if (it == notifications_.end() || cancelled_.count(key) != 0) {
        return false;
    }
    sent_.insert(key);
    it->second.status = NotificationStatus::Sent;
    return true;
}

std::optional<Notification> NotificationService::get(std::string_view id) const {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = notifications_.find(std::string(id));
    if (it == notifications_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<DueNotification> NotificationService::due(std::int64_t now,
                                                      std::size_t  limit) const {
    std::lock_guard<std::mutex> lk(mu_);

    std::vector<DueNotification> result;
    result.reserve(schedule_.size());
    for (const auto& id : schedule_) {
        auto it = notifications_.find(id);
        if (it == notifications_.end()) {
            continue;
        }
        const auto& n = it->second;
        if (n.send_at > now) {
            continue;
        }
        if (cancelled_.count(id) != 0 || sent_.count(id) != 0) {
            continue;
        }
        result.push_back(toDue(n));
    }

    std::sort(result.begin(), result.end(), [](const DueNotification& a,
                                               const DueNotification& b) {
        if (a.send_at != b.send_at) return a.send_at < b.send_at;
        return a.id < b.id;
    });

    if (result.size() > limit) {
        result.resize(limit);
    }
    return result;
}

}  // namespace itmo_notification
