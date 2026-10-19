/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Contacts scenario: scrolling and flinging the A-Z list, opening a contact
 * card, and the new-contact form (soft keyboard, validation, capacity).
 */

#include <string.h>

#include "data.h"
#include "phone-test.h"

int app_contacts_test_offset(void);
const char *app_contacts_test_detail(void);
const char *app_contacts_test_form_error(void);
bool app_contacts_test_keyboard(void);

/* Layout mirrored from contacts.c */
#define LIST_TOP PHONE_HEADER_H

/* Center y of stock contact @i (every stock initial is distinct) */
static twin_coord_t row_y(int i)
{
    return LIST_TOP + (i + 1) * PHONE_SECTION_H + i * PHONE_LIST_ROW_H +
           PHONE_LIST_ROW_H / 2 - app_contacts_test_offset();
}

static void type_hw(const char *s)
{
    for (; *s; s++)
        th_key(phone_th, (twin_ucs4_t) *s);
    ph_wait(100);
}

/* Form geometry with the keyboard hidden */
#define FIELD_BOX_Y(i) (PHONE_HEADER_H + 6 + (i) * 50 + 15 + 15)
#define SAVE_X 178
#define SAVE_Y (PHONE_CONTENT_H - 44 + 20)
#define CANCEL_X 62

static void test_list_scroll(void)
{
    CHECK_EQ(app_contacts_test_offset(), 0, "list starts at the top");
    ph_shot("contacts-list");

    /* Slow drag: pausing before lifting the finger means no fling */
    th_press(phone_th, 120, CY(200));
    for (int i = 1; i <= 10; i++) {
        th_advance(phone_th, 16);
        th_move(phone_th, 120, CY(200 - i * 10));
    }
    th_advance(phone_th, 150);
    th_release(phone_th, 120, CY(100));
    ph_wait(500);
    CHECK_EQ(app_contacts_test_offset(), 100, "drag scrolls by its distance");
    CHECK_EQ(phone_depth(), 1, "a scroll is not a tap");
    ph_shot("contacts-list-scrolled");

    /* Rows map to the right contact under a scroll offset */
    ph_tap(120, row_y(3));
    const char *name = app_contacts_test_detail();
    CHECK_STR(name, "Daniel Brown", "detail shows the contact");
    CHECK_EQ(phone_depth(), 2, "detail pushed");
    ph_shot("contacts-detail");

    /* Call and Message hand off to other apps */
    ph_tap(42, 132);
    CHECK_EQ(phone_depth(), 3, "call opens the dialer");
    phone_back();
    ph_wait(2000);
    ph_tap(120, 132);
    CHECK_EQ(phone_depth(), 3, "message opens messages");
    phone_back();
    ph_wait(2000);
    CHECK(app_contacts_test_detail() != NULL, "back on the card");
    phone_back();
    ph_wait(2000);
    CHECK_EQ(app_contacts_test_offset(), 100, "list keeps its scroll offset");

    /* Fling: keeps moving after the finger lifts, then comes to rest */
    th_drag(phone_th, 120, CY(200), 120, CY(140), 4);
    int o1 = app_contacts_test_offset();
    th_advance(phone_th, 64);
    int o2 = app_contacts_test_offset();
    CHECK(o2 > o1, "fling continues: %d -> %d", o1, o2);
    ph_wait(3000);
    int o3 = app_contacts_test_offset();
    ph_wait(500);
    CHECK(o3 > o2, "fling travelled: %d -> %d", o2, o3);
    CHECK_EQ(app_contacts_test_offset(), o3, "fling stopped");
    CHECK_EQ(phone_depth(), 1, "fling is not a tap");
    CHECK_EQ(th_damage_check(phone_th), 0, "no stale pixels after fling");

    /* Fling back to the top */
    for (int i = 0; i < 3 && app_contacts_test_offset(); i++)
        ph_drag(120, 60, 120, 240);
    CHECK_EQ(app_contacts_test_offset(), 0, "back at the top");
}

static void open_form(void)
{
    ph_tap(20, PHONE_HEADER_H / 2); /* add, at the header start */
    CHECK(app_contacts_test_form_error() != NULL, "form opened");
}

static void test_add(void)
{
    uint32_t n = phone_contact_count();

    open_form();
    CHECK(!app_contacts_test_keyboard(), "keyboard hidden at first");
    ph_tap(120, FIELD_BOX_Y(0));
    CHECK(app_contacts_test_keyboard(), "focusing a field shows the keyboard");

    /* Soft keys: names auto-capitalize */
    ph_key('z');
    ph_key('o');
    ph_key('e');
    ph_shot("contacts-form-keyboard");
    ph_key('\r'); /* Enter: on to the phone field, digits layout */
    ph_key('5');
    ph_key('5');
    ph_key('5');
    ph_key('-');
    type_hw("0199\r");
    type_hw("zoe@example.com\r");
    CHECK(!app_contacts_test_keyboard(), "Enter on the last field hides it");

    ph_tap(SAVE_X, SAVE_Y);
    CHECK_EQ(phone_contact_count(), n + 1, "contact saved");
    CHECK(app_contacts_test_offset() >= 0, "back on the list");
    const phone_contact_t *c = phone_contact_at(n);
    CHECK(c && !strcmp(c->name, "Zoe") && !strcmp(c->phone, "555-0199") &&
              !strcmp(c->email, "zoe@example.com"),
          "sorted last: %s / %s / %s", c ? c->name : "-", c ? c->phone : "-",
          c ? c->email : "-");
    CHECK(app_contacts_test_offset() > 0, "list scrolled to the new contact");
    ph_shot("contacts-list-added");

    /* Hardware keys type verbatim; sorting ignores case */
    open_form();
    ph_tap(120, FIELD_BOX_Y(0));
    type_hw("bella Hart");
    ph_tap(120, PHONE_HEADER_H / 2);
    CHECK(!app_contacts_test_keyboard(), "tapping outside hides the keyboard");
    ph_tap(SAVE_X, SAVE_Y);
    c = phone_contact_at(1);
    CHECK_STR(c ? c->name : NULL, "bella Hart", "inserted after Ashley");

    /* Validation: an empty (or blank) name is refused */
    n = phone_contact_count();
    open_form();
    ph_tap(120, FIELD_BOX_Y(0));
    type_hw("   ");
    ph_tap(SAVE_X, PHONE_CONTENT_H - UI_KBD_H - 20);
    const char *err = app_contacts_test_form_error();
    CHECK_STR(err, "Name required", "form error");
    CHECK_EQ(phone_contact_count(), n, "nothing added");
    CHECK(!app_contacts_test_keyboard(), "keyboard hidden to show the error");
    ph_shot("contacts-form-validation");

    /* Typing clears the message; names stop at the storage limit */
    ph_tap(120, FIELD_BOX_Y(0));
    type_hw("\b\b\bMaximilian Alexander Montgomery-Smythe");
    err = app_contacts_test_form_error();
    CHECK(err && !*err, "error cleared by typing");
    ph_tap(120, PHONE_HEADER_H / 2);
    ph_tap(SAVE_X, SAVE_Y);
    CHECK_EQ(phone_contact_count(), n + 1, "long name saved");
    c = NULL;
    for (uint32_t i = 0; i < phone_contact_count(); i++)
        if (phone_contact_at(i)->name[0] == 'M')
            c = phone_contact_at(i);
    CHECK(c && strlen(c->name) == 31, "name capped at 31 chars: %zu",
          c ? strlen(c->name) : (size_t) 0);

    /* Capacity: fill the store, then the form reports it */
    while (phone_contact_count() < PHONE_CONTACT_CAPACITY)
        phone_contact_add("Yuri Filler", "", "");
    open_form();
    ph_tap(120, FIELD_BOX_Y(0));
    type_hw("Olivia\r");
    ph_tap(120, PHONE_HEADER_H / 2);
    ph_tap(SAVE_X, SAVE_Y);
    err = app_contacts_test_form_error();
    CHECK_STR(err, "Contact list full", "form error");
    CHECK_EQ(phone_contact_count(), PHONE_CONTACT_CAPACITY, "still full");
    ph_shot("contacts-form-full");

    ph_tap(CANCEL_X, SAVE_Y);
    CHECK(app_contacts_test_offset() >= 0, "cancel returns to the list");
    CHECK_EQ(phone_depth(), 1, "form closed");
}

void test_contacts(void)
{
    ph_open("Contacts");
    test_list_scroll();
    test_add();
    ph_home();
    phone_data_reset();
}
