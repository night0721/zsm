#ifndef NOTIFICATION_H
#define NOTIFICATION_H 

#include <stdint.h>
#include <libnotify/notify.h>

void send_notification(uint8_t *content);

#endif
