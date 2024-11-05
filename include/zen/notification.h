#ifndef NOTIFICATION_H
#define NOTIFICATION_H 

#include <stdint.h>

#include "config.h"

#ifdef USE_LUFT
#include "luft.h"
#else
#include <libnotify/notify.h>
#endif

void send_notification(uint8_t *author, uint8_t *content);

#endif
