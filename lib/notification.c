#include "notification.h"
#include "util.h"

void send_notification(uint8_t *author, uint8_t *content)
{
    NotifyNotification *notification = notify_notification_new((char *) author,
            (char *) content, "dialog-information");
    if (notification == NULL) {
        error(0, "Cannot create notification");
    }
    if (!notify_notification_show(notification, NULL)) {
        error(0, "Cannot show notifcation");
    }
    g_object_unref(G_OBJECT(notification));
}
