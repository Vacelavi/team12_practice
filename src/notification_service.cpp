#include "itmo_notification/notification_service.hpp"

#include <mutex>
#include <shared_mutex>
#include <utility>

namespace itmo_notification {

namespace {

DueNotification toDue(const Notification &n) {
  return {
      n.id,      n.user_id, n.channel,  n.recipient,  n.template_name,
      n.payload, n.send_at, n.priority, n.created_at,
  };
}

} // namespace

NotificationService::NotificationService() = default;
NotificationService::~NotificationService() = default;

void NotificationService::add(Notification notification) {

  std::unique_lock<std::shared_mutex> lk(mu_);

  if (notifications_.contains(notification.id)) {
    return;
  }

  schedule_.insert(ScheduleEntry(notification));
  notifications_[notification.id] = std::move(notification);
}

bool NotificationService::cancel(std::string_view id) {
  const auto key = std::string(id);

  std::unique_lock<std::shared_mutex> lk(mu_);
  auto it = notifications_.find(key);
  if (it == notifications_.end()) {
    return false;
  }

  schedule_.erase(ScheduleEntry{(*it).second});
  notifications_.erase(it);
  return true;
}

bool NotificationService::markSent(std::string_view id) {
  const auto key = std::string(id);

  std::unique_lock<std::shared_mutex> lk(mu_);
  auto it = notifications_.find(key);
  if (it == notifications_.end()) {
    return false;
  }

  schedule_.erase(ScheduleEntry{(*it).second});
  notifications_.erase(it);
  return true;
}

std::optional<Notification>
NotificationService::get(std::string_view id) const {

  std::shared_lock<std::shared_mutex> lk(mu_);
  auto it = notifications_.find(std::string(id));
  if (it == notifications_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::vector<DueNotification> NotificationService::due(std::int64_t now,
                                                      std::size_t limit) const {
  std::shared_lock<std::shared_mutex> lk(mu_);

  size_t result_size = std::min(schedule_.size(), limit);
  std::vector<DueNotification> result;
  result.reserve(result_size);

  for (const auto &notification : schedule_) {
    if (notification.send_at > now || result.size() >= result_size) {
      break;
    }

    result.push_back(toDue(notifications_.at(notification.id)));
  }

  return result;
}

} // namespace itmo_notification
