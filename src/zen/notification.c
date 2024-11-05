#include "util.h"
#include "zen/notification.h"
#include "config.h"

void send_notification(uint8_t *author, uint8_t *content)
{
#ifdef USE_LUFT
	char *texts[3];
	texts[0] = author;
	texts[1] = content;
	render_notification(texts, 2);
#else
	NotifyNotification *notification = notify_notification_new((char *) author,
            (char *) content, "dialog-information");
    if (notification == NULL) {
        write_log(LOG_ERROR, "Cannot create notification");
    }
    if (!notify_notification_show(notification, NULL)) {
        write_log(LOG_ERROR, "Cannot show notifcation");
    }
    g_object_unref(G_OBJECT(notification));
#endif
}
