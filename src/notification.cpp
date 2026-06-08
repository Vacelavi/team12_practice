#include "itmo_notification/notification.hpp"

namespace itmo_notification {

static_assert(sizeof(Notification) > 0);

bool ScheduleEntryLess::operator()(const ScheduleEntry& lhs, const ScheduleEntry& rhs) const {
    if (lhs.send_at != rhs.send_at) return lhs.send_at < rhs.send_at;
    if (lhs.priority != rhs.priority) return lhs.priority > rhs.priority;
    if (lhs.created_at != rhs.created_at) return lhs.created_at < rhs.created_at;
    return lhs.id < rhs.id;
}

}  // namespace itmo_notification
