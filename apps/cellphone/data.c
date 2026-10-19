/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Demo data: contacts, message threads, call log, music tracks.
 * Stock records are const (flash); only contacts added at run time use RAM.
 */

#include <stdio.h>

#include "data.h"

static const phone_contact_t s_contacts_stock[PHONE_CONTACT_STOCK] = {
    {"Ashley Johnson", "+1-555-0101", "ashley@example.com", 'A'},
    {"Brian Smith", "+1-555-0102", "brian@example.com", 'B'},
    {"Chris Williams", "+1-555-0103", "chris@example.com", 'C'},
    {"Daniel Brown", "+1-555-0104", "daniel@example.com", 'D'},
    {"Emily Davis", "+1-555-0105", "emily@example.com", 'E'},
    {"Frank Miller", "+1-555-0106", "frank@example.com", 'F'},
    {"Grace Wilson", "+1-555-0107", "grace@example.com", 'G'},
    {"Henry Thomas", "+1-555-0108", "henry@example.com", 'H'},
    {"Isaac Moore", "+1-555-0109", "isaac@example.com", 'I'},
    {"James Taylor", "+1-555-0110", "james@example.com", 'J'},
    {"Karen Harris", "+1-555-0111", "karen@example.com", 'K'},
    {"Laura Jackson", "+1-555-0112", "laura@example.com", 'L'},
};

#define CONTACT_ADDED_MAX (PHONE_CONTACT_CAPACITY - PHONE_CONTACT_STOCK)
#define CONTACT_ORDER_ADDED 0x80u /* high bit flags an added-slot index */

static phone_contact_t s_contacts_added[CONTACT_ADDED_MAX];
static char s_added_names[CONTACT_ADDED_MAX][32];
static char s_added_phones[CONTACT_ADDED_MAX][24];
static char s_added_emails[CONTACT_ADDED_MAX][40];

/* High bit of an order entry flags an added-slot index, so the visible
 * list stays sorted without copying stock rows into RAM. */
static uint8_t s_contact_order[PHONE_CONTACT_CAPACITY] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
};
_Static_assert(PHONE_CONTACT_STOCK == 12, "s_contact_order lists the stock");
static uint32_t s_contact_count = PHONE_CONTACT_STOCK;

static const phone_contact_t *contact_resolve(uint8_t ord)
{
    if (ord & CONTACT_ORDER_ADDED)
        return &s_contacts_added[ord & 0x7Fu];
    return &s_contacts_stock[ord];
}

static int32_t contact_name_cmp_folded(const char *lhs, const char *rhs)
{
    uint32_t i = 0;

    while (lhs[i] != '\0' && rhs[i] != '\0') {
        char a = lhs[i];
        char b = rhs[i];

        if (a >= 'a' && a <= 'z')
            a = (char) (a - 'a' + 'A');
        if (b >= 'a' && b <= 'z')
            b = (char) (b - 'a' + 'A');

        if (a != b)
            return (int32_t) ((unsigned char) a - (unsigned char) b);
        i++;
    }

    return (int32_t) ((unsigned char) lhs[i] - (unsigned char) rhs[i]);
}

static char contact_initial_for(const char *name)
{
    if (name == NULL || name[0] == '\0')
        return '#';

    char c = name[0];
    if (c >= 'a' && c <= 'z')
        c = (char) (c - 'a' + 'A');
    if ((c < 'A' || c > 'Z') && (c < '0' || c > '9'))
        return '#';
    return c;
}

const phone_contact_t *phone_contact_at(uint32_t idx)
{
    if (idx >= s_contact_count)
        return NULL;
    return contact_resolve(s_contact_order[idx]);
}

uint32_t phone_contact_count(void)
{
    return s_contact_count;
}

int32_t phone_contact_add(const char *name,
                          const char *phone,
                          const char *email)
{
    if (!name || name[0] == '\0')
        return -1;
    if (s_contact_count >= PHONE_CONTACT_CAPACITY)
        return -1;
    if (!phone)
        phone = "";
    if (!email)
        email = "";

    uint32_t buf_idx = s_contact_count - PHONE_CONTACT_STOCK;
    snprintf(s_added_names[buf_idx], sizeof(s_added_names[buf_idx]), "%s",
             name);
    snprintf(s_added_phones[buf_idx], sizeof(s_added_phones[buf_idx]), "%s",
             phone);
    snprintf(s_added_emails[buf_idx], sizeof(s_added_emails[buf_idx]), "%s",
             email);

    s_contacts_added[buf_idx] = (phone_contact_t) {
        .name = s_added_names[buf_idx],
        .phone = s_added_phones[buf_idx],
        .email = s_added_emails[buf_idx],
        .initial = contact_initial_for(s_added_names[buf_idx]),
    };

    uint32_t insert_at = s_contact_count;
    for (uint32_t i = 0; i < s_contact_count; i++) {
        const phone_contact_t *existing = contact_resolve(s_contact_order[i]);
        if (contact_name_cmp_folded(s_contacts_added[buf_idx].name,
                                    existing->name) < 0) {
            insert_at = i;
            break;
        }
    }

    for (uint32_t i = s_contact_count; i > insert_at; i--) {
        s_contact_order[i] = s_contact_order[i - 1];
    }

    s_contact_order[insert_at] = (uint8_t) (CONTACT_ORDER_ADDED | buf_idx);
    s_contact_count++;
    return (int32_t) insert_at;
}

void phone_data_reset(void)
{
    for (uint32_t i = 0; i < PHONE_CONTACT_STOCK; i++)
        s_contact_order[i] = (uint8_t) i;
    s_contact_count = PHONE_CONTACT_STOCK;
}

/*********************
 * SMS
 *********************/
static const phone_sms_msg_t s_sms_ashley[] = {
    {"Hey, you coming to the meeting?", 0, "09:15"},
    {"Yeah, on my way!", 1, "09:17"},
    {"Cool, see you in 5", 0, "09:18"},
    {"Just got here", 1, "09:22"},
    {"Conference room B", 0, "09:23"},
    {"Got it, thanks!", 1, "09:24"},
};

static const phone_sms_msg_t s_sms_brian[] = {
    {"Did you see the new design specs?", 0, "14:05"},
    {"Not yet, got a link?", 1, "14:10"},
    {"Sent it to your inbox", 0, "14:11"},
    {"Thanks, looks great!", 1, "14:30"},
    {"Want to chat about it tomorrow?", 0, "14:31"},
    {"Sure, mornings work for me", 1, "14:35"},
    {"10 AM then", 0, "14:36"},
};

static const phone_sms_msg_t s_sms_chris[] = {
    {"Happy birthday!", 1, "08:00"},
    {"Thanks so much!", 0, "08:45"},
    {"Hope you have a great one", 1, "08:46"},
    {"We should grab dinner soon", 0, "08:50"},
    {"Sounds good, how about Friday?", 1, "08:52"},
};

static const phone_sms_msg_t s_sms_daniel[] = {
    {"Can you pick up groceries?", 0, "17:30"},
    {"Sure, what do we need?", 1, "17:35"},
    {"Milk, eggs, bread, and coffee", 0, "17:36"},
    {"Got it. Anything else?", 1, "17:38"},
    {"That's all, thanks!", 0, "17:39"},
    {"No problem", 1, "17:40"},
    {"Actually, grab some fruit too", 0, "17:42"},
    {"Apples and bananas?", 1, "17:43"},
};

static const phone_sms_thread_t s_sms_threads[PHONE_SMS_THREADS] = {
    {"Ashley Johnson", "Got it, thanks!", "Today", s_sms_ashley,
     sizeof(s_sms_ashley) / sizeof(s_sms_ashley[0])},
    {"Brian Smith", "10 AM then", "Today", s_sms_brian,
     sizeof(s_sms_brian) / sizeof(s_sms_brian[0])},
    {"Chris Williams", "Sounds good, how about Friday?", "Yesterday",
     s_sms_chris, sizeof(s_sms_chris) / sizeof(s_sms_chris[0])},
    {"Daniel Brown", "Apples and bananas?", "Yesterday", s_sms_daniel,
     sizeof(s_sms_daniel) / sizeof(s_sms_daniel[0])},
};

const phone_sms_thread_t *phone_sms_threads(void)
{
    return s_sms_threads;
}

/*********************
 * CALL LOG
 *********************/
static const phone_call_t s_calllog[PHONE_CALLLOG_COUNT] = {
    {"Ashley Johnson", "+1-555-0101", "Today, 14:22", PHONE_CALL_IN},
    {"Brian Smith", "+1-555-0102", "Today, 13:05", PHONE_CALL_OUT},
    {"Unknown", "+1-555-9999", "Today, 11:30", PHONE_CALL_MISSED},
    {"Chris Williams", "+1-555-0103", "Today, 10:15", PHONE_CALL_IN},
    {"Daniel Brown", "+1-555-0104", "Today, 09:45", PHONE_CALL_OUT},
    {"Emily Davis", "+1-555-0105", "Yesterday, 18:30", PHONE_CALL_MISSED},
    {"Frank Miller", "+1-555-0106", "Yesterday, 16:20", PHONE_CALL_IN},
    {"Grace Wilson", "+1-555-0107", "Yesterday, 14:10", PHONE_CALL_OUT},
    {"Ashley Johnson", "+1-555-0101", "Yesterday, 12:00", PHONE_CALL_IN},
    {"Henry Thomas", "+1-555-0108", "Yesterday, 10:30", PHONE_CALL_MISSED},
    {"Isaac Moore", "+1-555-0109", "Apr 23, 17:45", PHONE_CALL_OUT},
    {"James Taylor", "+1-555-0110", "Apr 23, 15:00", PHONE_CALL_IN},
    {"Karen Harris", "+1-555-0111", "Apr 22, 09:30", PHONE_CALL_MISSED},
    {"Laura Jackson", "+1-555-0112", "Apr 22, 08:15", PHONE_CALL_OUT},
    {"Brian Smith", "+1-555-0102", "Apr 21, 20:00", PHONE_CALL_IN},
};

const phone_call_t *phone_calllog(void)
{
    return s_calllog;
}

/*********************
 * MUSIC
 *********************/
static const phone_track_t s_tracks[PHONE_TRACK_COUNT] = {
    {"Summer in the City", "The Highway Drifters", "Open Road", 214},
    {"Heart on the Run", "Ocean Avenue", "Coastline", 187},
    {"Tomorrow's Light", "Neon Skyline", "City Lights", 245},
    {"Coming Home", "The Highway Drifters", "Open Road", 198},
    {"Out of the Blue", "Ocean Avenue", "Coastline", 220},
    {"Last Train Out", "Neon Skyline", "City Lights", 176},
    {"Catch the Wind", "Neon Skyline", "City Lights", 203},
    {"Roadside Diner", "The Highway Drifters", "Open Road", 262},
    {"Letters Unsent", "Ocean Avenue", "Coastline", 195},
    {"Goodbye for Now", "Neon Skyline", "City Lights", 240},
};

const phone_track_t *phone_tracks(void)
{
    return s_tracks;
}
