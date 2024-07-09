/*
 * Twin - A Tiny Window System
 * Copyright (c) 2004 Keith Packard <keithp@keithp.com>
 * All rights reserved.
 */

#include "apps_text.h"

#define D(x) twin_double_to_fixed(x)

void apps_text_start(twin_screen_t *screen,
                     const char *name,
                     int x,
                     int y,
                     int w,
                     int h)
{
    twin_window_t *text = twin_window_create(screen, TWIN_ARGB32,
                                             TwinWindowApplication, x, y, w, h);
    twin_fixed_t fx, fy;
    static const char *lines[] = {
        "Fourscore and seven years ago our fathers brought forth on",
        "this continent a new nation, conceived in liberty and",
        "dedicated to the proposition that all men are created equal.",
        "",
        "Now we are engaged in a great civil war, testing whether that",
        "nation or any nation so conceived and so dedicated can long",
        "endure. We are met on a great battlefield of that war. We",
        "have come to dedicate a portion of it as a final resting",
        "place for those who died here that the nation might live.",
        "This we may, in all propriety do. But in a larger sense, we",
        "cannot dedicate, we cannot consecrate, we cannot hallow this",
        "ground. The brave men, living and dead who struggled here",
        "have hallowed it far above our poor power to add or detract.",
        "The world will little note nor long remember what we say here,",
        "but it can never forget what they did here.",
        "",
        "It is rather for us the living, we here be dedicated to the",
        "great task remaining before us--that from these honored",
        "dead we take increased devotion to that cause for which they",
        "here gave the last full measure of devotion--that we here",
        "highly resolve that these dead shall not have died in vain, that",
        "this nation shall have a new birth of freedom, and that",
        "government of the people, by the people, for the people shall",
        "not perish from the earth.",
        0};
    const char **l;
    twin_path_t *path;

    twin_window_set_name(text, name);
    path = twin_path_create();
#define TEXT_SIZE 9
    twin_path_set_font_size(path, D(TEXT_SIZE));
    fx = D(3);
    fy = D(10);
    twin_fill(text->pixmap, 0xc0c0c0c0, TWIN_SOURCE, 0, 0,
              text->client.right - text->client.left,
              text->client.bottom - text->client.top);
    for (l = lines; *l; l++) {
        twin_path_move(path, fx, fy);
        twin_path_utf8(path, *l);
        twin_paint_path(text->pixmap, 0xff000000, path);
        twin_path_empty(path);
        fy += D(TEXT_SIZE);
    }
    twin_window_show(text);
}
