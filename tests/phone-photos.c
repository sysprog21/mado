/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Photos and Camera scenarios.  Both apps behave the same with or without
 * a JPEG loader (procedural art stands in for missing pictures), so these
 * checks hold in every build configuration.
 */

#include "phone-test.h"

int app_photos_test_index(void);
bool app_photos_test_hires(void);
int app_photos_test_hires_count(void);
bool app_photos_test_grid_open(void);
int app_camera_test_shots(void);
int app_camera_test_flash(void);

/* Thumbnail centers, content coordinates (grid at x 5, y 56; 74 + 4 gap) */
#define THUMB_X(i) (5 + ((i) % 3) * 78 + 37)
#define THUMB_Y(i) (56 + ((i) / 3) * 78 + 30)

void test_photos(void)
{
    ph_open("Photos");
    CHECK(app_photos_test_grid_open(), "photo grid on top");
    ph_shot("photos-grid");

    /* Open the third photo; the full-size picture waits for the push */
    th_tap(phone_th, THUMB_X(2), CY(THUMB_Y(2)));
    th_advance(phone_th, 120);
    CHECK_EQ(phone_depth(), 2, "viewer pushed over the grid");
    CHECK_EQ(app_photos_test_index(), 2, "viewer opens on the tapped photo");
    CHECK(!app_photos_test_hires(), "full-size picture deferred");
    CHECK_EQ(app_photos_test_hires_count(), 0, "nothing decoded yet");
    ph_wait(400);
    CHECK(app_photos_test_hires(), "full-size picture loaded after settling");
    CHECK_EQ(app_photos_test_hires_count(), 1, "one full-size picture");
    ph_shot("photos-viewer");

    /* Swipe left: next photo, scaled thumbnail first, then full size */
    th_drag(phone_th, 200, CY(130), 40, CY(130), 8);
    th_advance(phone_th, 240);
    CHECK_EQ(app_photos_test_index(), 3, "swipe left shows the next photo");
    CHECK(!app_photos_test_hires(), "swipe re-arms the deferred load");
    CHECK(app_photos_test_hires_count() <= 1, "at most one full-size picture");
    ph_wait(400);
    CHECK(app_photos_test_hires(), "next photo loaded at full size");
    CHECK_EQ(app_photos_test_hires_count(), 1, "previous picture freed");

    /* Mid-drag frames must be consistent: blitted photo, repainted bars */
    th_press(phone_th, 200, CY(130));
    for (int x = 180; x >= 100; x -= 20) {
        th_advance(phone_th, 16);
        th_move(phone_th, x, CY(130));
    }
    CHECK_EQ(th_damage_check(phone_th), 0, "mid-swipe frame consistent");
    th_release(phone_th, 100, CY(130));
    ph_wait(2000);
    CHECK_EQ(app_photos_test_index(), 4, "slow long drag flips the photo");
    ph_drag(40, 130, 200, 130);
    CHECK_EQ(app_photos_test_index(), 3, "and back");

    /* Swipe right twice, then past the first photo (rubber band) */
    ph_drag(40, 130, 200, 130);
    ph_drag(40, 130, 200, 130);
    CHECK_EQ(app_photos_test_index(), 1, "swipe right goes back");
    ph_drag(40, 130, 200, 130);
    CHECK_EQ(app_photos_test_index(), 0, "first photo");
    ph_drag(40, 130, 200, 130);
    CHECK_EQ(app_photos_test_index(), 0, "no photo before the first");
    /* A short drag springs back */
    ph_drag(120, 130, 90, 130);
    CHECK_EQ(app_photos_test_index(), 0, "short drag springs back");
    CHECK(app_photos_test_hires(), "settled photo at full size");
    CHECK_EQ(app_photos_test_hires_count(), 1, "still one picture");
    ph_shot("photos-viewer-first");

    /* Tap hides the chrome, tap again restores it */
    ph_tap(120, 130);
    ph_shot("photos-viewer-bare");
    ph_tap(120, 130);

    phone_back();
    ph_wait(2000);
    CHECK_EQ(phone_depth(), 1, "back returns to the grid");
    CHECK(app_photos_test_grid_open(), "grid on top again");
    CHECK_EQ(app_photos_test_hires_count(), 0, "viewer freed its picture");
    CHECK_EQ(th_damage_check(phone_th), 0, "grid repainted after the pop");

    ph_home();

    /* Second cycle, closing the viewer before its deferred load: the first
     * run warmed shared scratch buffers, so the heap must not grow now */
    size_t heap0 = th_heap_net(phone_th);
    ph_open("Photos");
    th_tap(phone_th, THUMB_X(4), CY(THUMB_Y(4)));
    th_advance(phone_th, 100);
    CHECK_EQ(app_photos_test_index(), 4, "fifth photo opened");
    phone_back();
    ph_wait(2000);
    CHECK_EQ(app_photos_test_hires_count(), 0, "deferred load cancelled");
    CHECK(app_photos_test_grid_open(), "grid after early back");
    ph_tap(THUMB_X(5), THUMB_Y(5));
    CHECK(app_photos_test_hires(), "last photo loaded");
    ph_drag(200, 130, 40, 130);
    CHECK_EQ(app_photos_test_index(), 5, "no photo after the last");
    ph_home();
    CHECK(th_heap_net(phone_th) <= heap0, "photos freed everything (%zu > %zu)",
          th_heap_net(phone_th), heap0);
}

void test_camera(void)
{
    ph_open("Camera");
    CHECK_EQ(app_camera_test_shots(), 0, "no shots yet");
    ph_shot("camera");

    /* Idle viewfinder: nothing repaints */
    uint32_t frames = phone_th->screen->stats.frames;
    ph_wait(500);
    CHECK_EQ(phone_th->screen->stats.frames, frames, "idle camera is static");

    /* Shutter: flash fades out, counter goes up */
    th_tap(phone_th, PHONE_W / 2, CY(PHONE_CONTENT_H - 70 + 44));
    th_advance(phone_th, 48);
    CHECK_EQ(app_camera_test_shots(), 1, "shutter takes a photo");
    int flash = app_camera_test_flash();
    CHECK(flash > 0 && flash < 255, "flash fading (%d)", flash);
    ph_wait(500);
    CHECK_EQ(app_camera_test_flash(), 0, "flash finished");
    frames = phone_th->screen->stats.frames;
    ph_wait(500);
    CHECK_EQ(phone_th->screen->stats.frames, frames, "static after the flash");

    ph_tap(PHONE_W / 2, PHONE_CONTENT_H - 70 + 44);
    CHECK_EQ(app_camera_test_shots(), 2, "second shot");
    ph_shot("camera-shot");

    /* Video mode: shutter records instead of shooting */
    ph_tap(PHONE_W / 2 + 30, PHONE_CONTENT_H - 70 + 11);
    ph_tap(PHONE_W / 2, PHONE_CONTENT_H - 70 + 44);
    CHECK_EQ(app_camera_test_shots(), 2, "video mode does not count shots");
    ph_shot("camera-video");

    /* Front camera, back to photo mode by swiping */
    ph_tap(PHONE_W - 38, PHONE_CONTENT_H - 70 + 44);
    ph_drag(60, 100, 160, 100);
    ph_tap(PHONE_W / 2, PHONE_CONTENT_H - 70 + 44);
    CHECK_EQ(app_camera_test_shots(), 3, "photo mode again after swipe");
    ph_shot("camera-selfie");

    ph_home();

    /* The first run warms shared scratch paths; a second cycle must not
     * grow the heap */
    size_t heap0 = th_heap_net(phone_th);
    ph_open("Camera");
    ph_tap(PHONE_W - 38, PHONE_CONTENT_H - 70 + 44);
    ph_tap(PHONE_W / 2, PHONE_CONTENT_H - 70 + 44);
    ph_home();
    CHECK(th_heap_net(phone_th) <= heap0, "camera freed everything (%zu > %zu)",
          th_heap_net(phone_th), heap0);
}
