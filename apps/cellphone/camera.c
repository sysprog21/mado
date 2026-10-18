/*
 * Twin - A Tiny Window System
 * Copyright (c) 2026 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/*
 * Camera: a faux viewfinder over a procedural landscape.
 *
 * The scene is static, so nothing repaints while the camera idles: the
 * shutter flash is the only animation, and it invalidates the viewfinder
 * only while it fades.  The scene is drawn from paths in permille
 * coordinates of its rectangle, so the last-shot thumbnail is the same
 * painter at a smaller size (and the selfie camera is the same painter
 * mirrored).  Everything is filled shapes; the coverage cache keeps
 * repaints cheap.
 */

#include <stdio.h>

#include "phone.h"

#define CTRL_H 70 /* bottom control bar */
#define VF_MARGIN 4
#define VF_RADIUS 8
#define SHUTTER_R 23
#define THUMB_SZ 36
#define FLASH_MS 220

#define BAR UI_RGB(0x121212)
#define MODE_ON UI_RGB(0xffd54f)
#define MODE_OFF UI_RGB(0x9e9e9e)
#define REC_RED UI_RGB(0xe53935)

typedef struct {
    ui_view_t view;
    int shots;       /* photos taken */
    twin_a8_t flash; /* white overlay strength, fades to 0 */
    bool video;      /* video mode instead of photo */
    bool recording;
    bool selfie;      /* front camera: mirrored scene */
    bool shot_selfie; /* camera used for the last shot */
    int pressed;      /* control under the finger, CTL_NONE when none */
} camera_t;

enum { CTL_NONE, CTL_SHUTTER, CTL_PHOTO, CTL_VIDEO, CTL_FLIP };


static ui_rect_t viewfinder(ui_view_t *v)
{
    return ui_rect(VF_MARGIN, VF_MARGIN, PHONE_W - 2 * VF_MARGIN,
                   ui_view_height(v) - CTRL_H - 2 * VF_MARGIN);
}

static ui_rect_t ctrl_bar(ui_view_t *v)
{
    return ui_rect(0, ui_view_height(v) - CTRL_H, PHONE_W, CTRL_H);
}

/* Center line of the shutter row, and the top of the mode labels */
static twin_coord_t ctrl_cy(ui_view_t *v)
{
    return ui_view_height(v) - CTRL_H + 44;
}

static twin_coord_t mode_y(ui_view_t *v)
{
    return ui_view_height(v) - CTRL_H + 3;
}

static ui_rect_t control_rect(ui_view_t *v, int ctl)
{
    switch (ctl) {
    case CTL_SHUTTER:
        return ui_rect(PHONE_W / 2 - SHUTTER_R - 2, ctrl_cy(v) - SHUTTER_R - 2,
                       2 * SHUTTER_R + 4, 2 * SHUTTER_R + 4);
    case CTL_PHOTO:
        return ui_rect(PHONE_W / 2 - 64, mode_y(v), 60, 16);
    case CTL_VIDEO:
        return ui_rect(PHONE_W / 2 + 4, mode_y(v), 60, 16);
    case CTL_FLIP:
        return ui_rect(PHONE_W - 58, ctrl_cy(v) - 20, 40, 40);
    default:
        return ui_rect(0, 0, 0, 0);
    }
}

static ui_rect_t thumb_rect(ui_view_t *v)
{
    return ui_rect(20, ctrl_cy(v) - THUMB_SZ / 2, THUMB_SZ, THUMB_SZ);
}

/*
 * Scene
 */
typedef struct {
    short x, y; /* permille of the scene rectangle */
} pt_t;

static const pt_t far_ridge[] = {
    {0, 560},   {110, 430}, {200, 480}, {330, 330},  {450, 460},  {540, 400},
    {690, 290}, {820, 420}, {920, 370}, {1000, 410}, {1000, 760}, {0, 760},
};

static const pt_t near_ridge[] = {
    {0, 640},   {140, 540}, {280, 610},  {400, 560},  {560, 650},
    {740, 530}, {880, 600}, {1000, 560}, {1000, 800}, {0, 800},
};

static const pt_t road[] = {
    {470, 760},
    {530, 760},
    {760, 1000},
    {240, 1000},
};

static const pt_t trees[] = {
    {90, 790}, {150, 820}, {820, 800}, {900, 830}, {960, 790},
};

typedef struct {
    ui_rect_t r;
    bool mirror;
} frame_t;

static twin_fixed_t sx(const frame_t *f, int x)
{
    if (f->mirror)
        x = 1000 - x;
    return ui_fx(f->r.left) + ui_fx(ui_w(f->r)) / 1000 * x;
}

static twin_fixed_t sy(const frame_t *f, int y)
{
    return ui_fx(f->r.top) + ui_fx(ui_h(f->r)) / 1000 * y;
}

static void poly(twin_path_t *p, const frame_t *f, const pt_t *pts, int n)
{
    twin_path_move(p, sx(f, pts[0].x), sy(f, pts[0].y));
    for (int i = 1; i < n; i++)
        twin_path_draw(p, sx(f, pts[i].x), sy(f, pts[i].y));
    twin_path_close(p);
}

static void scene_poly(twin_pixmap_t *px,
                       const frame_t *f,
                       const pt_t *pts,
                       int n,
                       int y0,
                       twin_argb32_t c0,
                       int y1,
                       twin_argb32_t c1)
{
    twin_path_t *p = ui_path();
    poly(p, f, pts, n);
    twin_paint_path_linear(px, p, 0, sy(f, y0), c0, 0, sy(f, y1), c1);
    ui_path_done(p);
}

static void scene_ellipse(twin_pixmap_t *px,
                          const frame_t *f,
                          int x,
                          int y,
                          int rx,
                          int ry,
                          twin_argb32_t c)
{
    twin_path_t *p = ui_path();
    twin_fixed_t unit =
        ui_fx(ui_w(f->r) < ui_h(f->r) ? ui_w(f->r) : ui_h(f->r));
    twin_path_ellipse(p, sx(f, x), sy(f, y), unit / 1000 * rx,
                      unit / 1000 * ry);
    twin_paint_path(px, c, p);
    ui_path_done(p);
}

#define N(a) ((int) (sizeof(a) / sizeof((a)[0])))

static void paint_scene(twin_pixmap_t *px, ui_rect_t r, bool mirror)
{
    frame_t f = {r, mirror};
    int h = ui_h(r);

    /* Sky: deep blue fading to a warm horizon */
    ui_vgrad(px, ui_rect(r.left, r.top, ui_w(r), h * 62 / 100),
             UI_RGB(0x5a9fe0), UI_RGB(0xffdcb0));
    ui_fill(px,
            ui_rect(r.left, r.top + h * 62 / 100, ui_w(r), h - h * 62 / 100),
            UI_RGB(0xffdcb0));

    /* Sun with a soft halo, and a couple of clouds */
    scene_ellipse(px, &f, 720, 270, 150, 150, ui_alpha(UI_RGB(0xfff3c4), 70));
    scene_ellipse(px, &f, 720, 270, 105, 105, ui_alpha(UI_RGB(0xfff3c4), 110));
    scene_ellipse(px, &f, 720, 270, 70, 70, UI_RGB(0xfff8e1));
    scene_ellipse(px, &f, 230, 170, 120, 26, ui_alpha(UI_WHITE, 180));
    scene_ellipse(px, &f, 300, 150, 70, 30, ui_alpha(UI_WHITE, 200));

    scene_poly(px, &f, far_ridge, N(far_ridge), 290, UI_RGB(0x8ea2c0), 760,
               UI_RGB(0x6c7f9b));
    scene_poly(px, &f, near_ridge, N(near_ridge), 530, UI_RGB(0x557a5c), 800,
               UI_RGB(0x3b5a43));

    /* Meadow, a lake, a road to the horizon, trees */
    ui_vgrad(px,
             ui_rect(r.left, r.top + h * 76 / 100, ui_w(r), h - h * 76 / 100),
             UI_RGB(0x7aa64f), UI_RGB(0x2f5a25));
    scene_ellipse(px, &f, 250, 860, 190, 36, UI_RGB(0x8cc4e6));
    scene_ellipse(px, &f, 230, 852, 120, 12, ui_alpha(UI_WHITE, 90));
    scene_poly(px, &f, road, N(road), 760, UI_RGB(0xa08c72), 1000,
               UI_RGB(0x6d5c48));

    twin_path_t *p = ui_path();
    for (int i = 0; i < N(trees); i++) {
        pt_t tri[] = {{trees[i].x, (short) (trees[i].y - 110)},
                      {(short) (trees[i].x + 30), trees[i].y},
                      {(short) (trees[i].x - 30), trees[i].y}};
        poly(p, &f, tri, 3);
    }
    twin_paint_path(px, UI_RGB(0x1f3d1c), p);
    ui_path_done(p);
}

/*
 * Round the corners of @r by painting the outside of each corner arc in
 * @bg (cheaper than a mask, and the background is a flat color here).
 */
static void cut_corners(twin_pixmap_t *px,
                        ui_rect_t r,
                        int rad,
                        twin_argb32_t bg)
{
    twin_path_t *p = ui_path();
    struct {
        twin_coord_t x, y, cx, cy;
        twin_angle_t start;
    } c[4] = {
        {r.left, r.top, r.left + rad, r.top + rad, TWIN_ANGLE_180},
        {r.right, r.top, r.right - rad, r.top + rad, TWIN_ANGLE_270},
        {r.right, r.bottom, r.right - rad, r.bottom - rad, TWIN_ANGLE_0},
        {r.left, r.bottom, r.left + rad, r.bottom - rad, TWIN_ANGLE_90},
    };
    for (int i = 0; i < 4; i++) {
        twin_path_move(p, ui_fx(c[i].x), ui_fx(c[i].y));
        twin_path_arc(p, ui_fx(c[i].cx), ui_fx(c[i].cy), ui_fx(rad), ui_fx(rad),
                      c[i].start, TWIN_ANGLE_90);
        twin_path_close(p);
    }
    twin_paint_path(px, bg, p);
    ui_path_done(p);
}

static void chip(twin_pixmap_t *px,
                 twin_coord_t x,
                 twin_coord_t y,
                 ui_align_t align,
                 twin_argb32_t bg,
                 const char *label)
{
    twin_coord_t w = ui_text_width(FONT_SM, label) + 14;
    if (align == UI_RIGHT)
        x -= w;
    else if (align == UI_CENTER)
        x -= w / 2;
    ui_rect_t r = ui_rect(x, y, w, 20);
    ui_rrect(px, r, 10, bg);
    ui_text_in(px, FONT_SM, r, UI_CENTER, UI_WHITE, label);
}

/* Four L-shaped corner brackets as one path, painted once */
static void focus_brackets(twin_pixmap_t *px,
                           twin_coord_t cx,
                           twin_coord_t cy,
                           int half,
                           int leg,
                           twin_argb32_t c)
{
    twin_path_t *p = ui_path();
    for (int i = 0; i < 4; i++) {
        int sxn = i & 1 ? 1 : -1, syn = i & 2 ? 1 : -1;
        twin_coord_t x = cx + sxn * half, y = cy + syn * half;
        /* Horizontal and vertical legs extending inward from the corner */
        twin_coord_t hx = sxn > 0 ? x - leg : x;
        twin_coord_t vy = syn > 0 ? y - leg : y;
        twin_coord_t ty = syn > 0 ? y - 2 : y;
        twin_coord_t tx = sxn > 0 ? x - 2 : x;
        twin_path_rectangle(p, ui_fx(hx), ui_fx(ty), ui_fx(leg), ui_fx(2));
        twin_path_rectangle(p, ui_fx(tx), ui_fx(vy), ui_fx(2), ui_fx(leg));
    }
    twin_paint_path(px, c, p);
    ui_path_done(p);
}

static void paint_viewfinder(camera_t *c, twin_pixmap_t *px)
{
    ui_rect_t vf = viewfinder(&c->view);
    twin_coord_t cx = (vf.left + vf.right) / 2, cy = (vf.top + vf.bottom) / 2;

    paint_scene(px, vf, c->selfie);

    /* Rule-of-thirds grid */
    twin_argb32_t grid = ui_alpha(UI_WHITE, 70);
    for (int i = 1; i < 3; i++) {
        ui_fill(px, ui_rect(vf.left + ui_w(vf) * i / 3, vf.top, 1, ui_h(vf)),
                grid);
        ui_fill(px, ui_rect(vf.left, vf.top + ui_h(vf) * i / 3, ui_w(vf), 1),
                grid);
    }

    focus_brackets(px, cx, cy, 24, 12, ui_alpha(UI_WHITE, 220));
    ui_circle(px, cx, cy, 3, UI_RGB(0xfff176));
    ui_text(px, FONT_SM, cx, cy + 24 + 16, UI_CENTER, ui_alpha(UI_WHITE, 210),
            "EV -0.3");

    twin_argb32_t chip_bg = ui_alpha(UI_RGB(0x263238), 150);
    chip(px, vf.left + 8, vf.top + 8, UI_LEFT, chip_bg, "HDR");
    chip(px, vf.right - 8, vf.top + 8, UI_RIGHT, chip_bg,
         c->video ? "4K 30" : "12 MP");
    if (c->recording)
        chip(px, cx, vf.top + 8, UI_CENTER, REC_RED, "REC");

    if (c->flash)
        ui_fill(px, vf, ui_alpha(UI_WHITE, c->flash));
    cut_corners(px, vf, VF_RADIUS, UI_BLACK);
    ui_rrect_stroke(px, vf, VF_RADIUS, 1, UI_RGB(0x455a64));
}

static void paint_controls(camera_t *c, twin_pixmap_t *px, ui_rect_t clip)
{
    twin_coord_t cy = ctrl_cy(&c->view);
    ui_fill(px, ctrl_bar(&c->view), BAR);

    ui_rect_t pr = control_rect(&c->view, CTL_PHOTO),
              vr = control_rect(&c->view, CTL_VIDEO);
    ui_text_in(px, FONT_SM, pr, UI_RIGHT, c->video ? MODE_OFF : MODE_ON,
               "PHOTO");
    ui_text_in(px, FONT_SM, vr, UI_LEFT, c->video ? MODE_ON : MODE_OFF,
               "VIDEO");

    /* Last shot: a miniature of the scene, with the shot count */
    ui_rect_t t = thumb_rect(&c->view);
    if (ui_intersects(clip, ui_inset(t, -10))) {
        if (c->shots) {
            paint_scene(px, t, c->shot_selfie);
            cut_corners(px, t, 6, BAR);
            ui_rrect_stroke(px, t, 6, 2, UI_WHITE);
            char n[12];
            snprintf(n, sizeof(n), "%d", c->shots > 99 ? 99 : c->shots);
            twin_coord_t bx = t.right - 2, by = t.top + 2;
            ui_circle(px, bx, by, 9, phone_theme()->primary);
            ui_text_in(px, FONT_SM, ui_rect(bx - 9, by - 9, 18, 18), UI_CENTER,
                       UI_WHITE, n);
        } else {
            ui_rrect(px, t, 6, UI_RGB(0x2a2a2a));
            ui_icon(px, TWIN_SYM_IMAGE, FONT_NORMAL, (t.left + t.right) / 2,
                    (t.top + t.bottom) / 2, MODE_OFF);
        }
    }

    /* Shutter: white ring around a white (photo) or red (video) center */
    twin_coord_t cx = PHONE_W / 2;
    bool down = c->pressed == CTL_SHUTTER;
    ui_ring(px, cx, cy, SHUTTER_R, 3, UI_WHITE);
    if (!c->video) {
        ui_circle(px, cx, cy, down ? 16 : 18,
                  down ? UI_RGB(0xcfcfcf) : UI_WHITE);
    } else if (c->recording) {
        ui_rrect(px, ui_rect(cx - 9, cy - 9, 18, 18), 4,
                 down ? ui_darken(REC_RED, 20) : REC_RED);
    } else {
        ui_circle(px, cx, cy, down ? 16 : 18,
                  down ? ui_darken(REC_RED, 20) : REC_RED);
    }

    /* Front/back camera switch */
    ui_rect_t fr = control_rect(&c->view, CTL_FLIP);
    ui_circle(px, (fr.left + fr.right) / 2, cy, 18,
              c->pressed == CTL_FLIP ? UI_RGB(0x444444) : UI_RGB(0x2a2a2a));
    ui_icon(px, TWIN_SYM_REFRESH, FONT_NORMAL, (fr.left + fr.right) / 2, cy,
            UI_WHITE);
}

static void camera_paint(ui_view_t *v, twin_pixmap_t *px)
{
    camera_t *c = (camera_t *) v;
    ui_rect_t clip = ui_view_clip(v);
    ui_rect_t vf = viewfinder(v);

    if (ui_intersects(clip, ui_inset(vf, -VF_MARGIN))) {
        ui_fill(px, ui_rect(0, 0, PHONE_W, ctrl_bar(v).top), UI_BLACK);
        paint_viewfinder(c, px);
    }
    if (ui_intersects(clip, ctrl_bar(v)))
        paint_controls(c, px, clip);
}

static bool flash_step(ui_view_t *v, twin_fixed_t value)
{
    camera_t *c = (camera_t *) v;
    int a = 255 - twin_fixed_to_int(value * 255 + TWIN_FIXED_HALF);
    c->flash = (twin_a8_t) (a < 0 ? 0 : a);
    ui_view_invalidate(v, viewfinder(v));
    return c->flash > 0;
}

static void shutter(camera_t *c)
{
    ui_view_t *v = &c->view;
    if (c->video) {
        c->recording = !c->recording;
        ui_view_invalidate(v, viewfinder(v));
        ui_view_invalidate(v, control_rect(v, CTL_SHUTTER));
        return;
    }
    c->shots++;
    c->shot_selfie = c->selfie;
    ui_view_invalidate(v, ui_inset(thumb_rect(v), -10));
    /* A fresh flash restarts from full white; a running tween keeps going */
    bool running = c->flash > 0;
    c->flash = 255;
    ui_view_invalidate(v, viewfinder(v));
    if (!running && !ui_view_tween(v, FLASH_MS, TwinEaseOutCubic, flash_step))
        c->flash = 0;
}

static int control_at(ui_view_t *v, twin_coord_t x, twin_coord_t y)
{
    for (int i = CTL_SHUTTER; i <= CTL_FLIP; i++)
        if (ui_hit(control_rect(v, i), x, y))
            return i;
    return CTL_NONE;
}

static void set_pressed(camera_t *c, int ctl)
{
    if (c->pressed == ctl)
        return;
    if (c->pressed != CTL_NONE)
        ui_view_invalidate(&c->view, control_rect(&c->view, c->pressed));
    c->pressed = ctl;
    if (ctl != CTL_NONE)
        ui_view_invalidate(&c->view, control_rect(&c->view, ctl));
}

static void set_mode(camera_t *c, bool video)
{
    if (c->video == video)
        return;
    c->video = video;
    c->recording = false;
    ui_view_invalidate_all(&c->view);
}

static bool camera_event(ui_view_t *v, const ui_event_t *e)
{
    camera_t *c = (camera_t *) v;
    switch (e->kind) {
    case UI_PRESS:
    case UI_MOVE:
        set_pressed(c, control_at(v, e->x, e->y));
        return true;
    case UI_TAP: {
        int ctl = control_at(v, e->x, e->y);
        set_pressed(c, CTL_NONE);
        if (ctl == CTL_SHUTTER)
            shutter(c);
        else if (ctl == CTL_PHOTO || ctl == CTL_VIDEO)
            set_mode(c, ctl == CTL_VIDEO);
        else if (ctl == CTL_FLIP) {
            c->selfie = !c->selfie;
            ui_view_invalidate(v, viewfinder(v));
        }
        return true;
    }
    case UI_RELEASE:
        set_pressed(c, CTL_NONE);
        /* Horizontal swipes over the viewfinder switch modes */
        if (ui_hit(viewfinder(v), e->x0, e->y0) &&
            (e->dx < -40 || e->dx > 40) && e->dx * e->dx > e->dy * e->dy)
            set_mode(c, e->dx < 0);
        return true;
    case UI_KEY:
        if (e->ucs4 != ' ' && e->ucs4 != UI_KEY_ENTER)
            return false;
        shutter(c);
        return true;
    default:
        set_pressed(c, CTL_NONE);
        return false;
    }
}

void app_camera_open(void)
{
    camera_t *c =
        ui_view_new(sizeof(*c), "Camera", camera_paint, camera_event, NULL);
    if (!c)
        return;
    c->pressed = CTL_NONE;
    ui_view_show(&c->view);
}

/*
 * Test hooks
 */
static camera_t *top_camera(void)
{
    ui_view_t *v = phone_top();
    return v && v->paint == camera_paint ? (camera_t *) v : NULL;
}

/* Shots taken by the camera on top, -1 when it is not on top */
int app_camera_test_shots(void)
{
    camera_t *c = top_camera();
    return c ? c->shots : -1;
}

/* Current flash strength (0 when idle) */
int app_camera_test_flash(void)
{
    camera_t *c = top_camera();
    return c ? c->flash : -1;
}
