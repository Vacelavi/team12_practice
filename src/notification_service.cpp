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

NotificationLimitator::NotificationLimitator()
    : latest_timestamp_(unixNow())
    , sent_counter_(0)
    , sent_limit_(100)
    , period_(3600)
{}

void NotificationLimitator::limit(size_t& limit) {
    if (unixNow() - latest_timestamp_ >= period_) {
        sent_counter_ = 0;
    }
    size_t available = sent_limit_ - sent_counter_;
    limit = std::min(limit, available);
    sent_counter_ += limit;
}

void NotificationLimitator::setLimit(size_t limit) {
    sent_limit_ = limit;
}

void NotificationLimitator::setPeriod(size_t period) {
    period_ = period;
}

size_t NotificationLimitator::unixNow() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

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
    limitator_.limit(limit);

    size_t result_size = std::min(schedule_.size(), limit);
    std::vector<DueNotification> result;
    result.reserve(result_size);

    for (const auto &notification : schedule_) {
        if (notification.send_at > now || result.size() >= result_size
             || notifications_.at(notification.id).status != NotificationStatus::Pending) {
            break;
        }

        result.push_back(toDue(notifications_.at(notification.id)));
    }

    return result;
}

std::vector<DueNotification> NotificationService::claim(std::int64_t now, 
                                                        std::size_t limit) {
    std::unique_lock<std::shared_mutex> lk(mu_);
    limitator_.limit(limit);

    std::vector<DueNotification> result;
    for (auto& [id, notif] : notifications_) {
        if (notif.status != NotificationStatus::Pending) {
            continue;
        }
        if (notif.send_at > now) {
            continue;
        }
        if (result.size() >= limit) {
            break;
        }
        
        notif.status = NotificationStatus::Processing;
        result.push_back(toDue(notif));
    }
    
    return result;
}

} // namespace itmo_notification
