#include "notification.h"

void send_notification(const char *content)
{
    NotifyNotification *noti = notify_notification_new("Client", content, "dialog-information");
    notify_notification_show(noti, NULL);
    g_object_unref(G_OBJECT(noti));
}
