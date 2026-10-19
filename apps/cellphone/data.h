/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#ifndef _APPS_PHONE_DATA_H_
#define _APPS_PHONE_DATA_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    const char *name;
    const char *phone;
    const char *email;
    char initial; /* first letter, for section headers */
} phone_contact_t;

#define PHONE_CONTACT_STOCK 12
#define PHONE_CONTACT_CAPACITY 16

/* Contacts sorted by name; index 0 .. phone_contact_count() - 1 */
const phone_contact_t *phone_contact_at(uint32_t idx);
uint32_t phone_contact_count(void);
/* Insert in sorted order; returns the new index or -1 when full/invalid */
int32_t phone_contact_add(const char *name,
                          const char *phone,
                          const char *email);
/* Drop contacts added at run time */
void phone_data_reset(void);

typedef struct {
    const char *text;
    uint8_t is_sent; /* 1 = sent by user, 0 = received */
    const char *time;
} phone_sms_msg_t;

typedef struct {
    const char *contact_name;
    const char *preview;
    const char *timestamp;
    const phone_sms_msg_t *messages;
    uint32_t msg_count;
} phone_sms_thread_t;

#define PHONE_SMS_THREADS 4
const phone_sms_thread_t *phone_sms_threads(void);

typedef enum {
    PHONE_CALL_IN = 0,
    PHONE_CALL_OUT,
    PHONE_CALL_MISSED,
} phone_call_dir_t;

typedef struct {
    const char *name;
    const char *phone;
    const char *time;
    phone_call_dir_t dir;
} phone_call_t;

#define PHONE_CALLLOG_COUNT 15
const phone_call_t *phone_calllog(void);

typedef struct {
    const char *title;
    const char *artist;
    const char *album;
    uint32_t duration_sec;
} phone_track_t;

#define PHONE_TRACK_COUNT 10
const phone_track_t *phone_tracks(void);

#endif /* _APPS_PHONE_DATA_H_ */
