/*
 * Copyright (c) 2004 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "twin-fedit.h"

static Display *dpy;
static Window win;
static Visual *visual;
static int depth;
static int width = 512;
static int height = 512;
static double scale = 8;
static cairo_t *cr;
static cairo_surface_t *surface;
static int offset;

static int offsets[1024];

static int init(int argc, char **argv)
{
    int scr;
    XSetWindowAttributes wa;
    XTextProperty wm_name, icon_name;
    XSizeHints sizeHints;
    XWMHints wmHints;
    Atom wm_delete_window;

    dpy = XOpenDisplay(0);
    scr = DefaultScreen(dpy);
    visual = DefaultVisual(dpy, scr);
    depth = DefaultDepth(dpy, scr);

    wa.background_pixel = WhitePixel(dpy, scr);
    wa.event_mask =
        (KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
         PointerMotionMask | ExposureMask | StructureNotifyMask);

    wm_name.value = (unsigned char *) argv[0];
    wm_name.encoding = XA_STRING;
    wm_name.format = 8;
    wm_name.nitems = strlen((char *) wm_name.value) + 1;
    icon_name = wm_name;

    win =
        XCreateWindow(dpy, RootWindow(dpy, scr), 0, 0, width, height, 0, depth,
                      InputOutput, visual, CWBackPixel | CWEventMask, &wa);
    sizeHints.flags = 0;
    wmHints.flags = InputHint;
    wmHints.input = True;
    XSetWMProperties(dpy, win, &wm_name, &icon_name, argv, argc, &sizeHints,
                     &wmHints, 0);
    XSetWMProtocols(dpy, win, &wm_delete_window, 1);

    XMapWindow(dpy, win);

    surface = cairo_xlib_surface_create(dpy, win, visual, width, height);

    cr = cairo_create(surface);

    cairo_translate(cr, 150, 420);
    cairo_scale(cr, scale, scale);

    cairo_set_font_size(cr, 2);
    return 1;
}

static cmd_t *copy_cmd(cmd_t *cmd)
{
    cmd_t *n = malloc(sizeof(cmd_t));
    if (!cmd)
        return 0;
    *n = *cmd;
    n->next = copy_cmd(cmd->next);
    return n;
}

static void free_cmd(cmd_t *cmd)
{
    if (cmd) {
        free_cmd(cmd->next);
        free(cmd);
    }
}

static cmd_t *insert_cmd(cmd_t **prev)
{
    cmd_t *n = malloc(sizeof(cmd_t));

    n->op = op_noop;
    n->next = *prev;
    *prev = n;
    return n;
}

static void delete_cmd(cmd_t **head, cmd_t *cmd)
{
    while (*head != cmd)
        head = &(*head)->next;
    *head = cmd->next;
    free(cmd);
}

static void push(char_t *c)
{
    cmd_stack_t *s = malloc(sizeof(cmd_stack_t));

    s->cmd = copy_cmd(c->cmd);
    s->prev = c->stack;
    c->stack = s;
}

static void pop(char_t *c)
{
    cmd_stack_t *s = c->stack;
    if (!s)
        return;
    free_cmd(c->cmd);
    c->cmd = s->cmd;
    c->stack = s->prev;
    c->first = 0;
    c->last = 0;
    free(s);
}

static cmd_t *append_cmd(char_t *c)
{
    cmd_t **prev;

    for (prev = &c->cmd; *prev; prev = &(*prev)->next)
        ;
    return insert_cmd(prev);
}

static int commas(char *line)
{
    int n = 0;
    char c;
    while ((c = *line++))
        if (c == ',')
            ++n;
    return n;
}

static char_t *read_char(void)
{
    char_t *c = malloc(sizeof(char_t));
    char line[1024];
    cmd_t *cmd;

    c->cmd = 0;
    c->stack = 0;
    c->first = 0;
    c->last = 0;
    while (fgets(line, sizeof(line), stdin)) {
        if (line[0] == '/') {
            int ucs4;
            if (sscanf(line + 5, "%x", &ucs4) == 1)
                offsets[ucs4] = offset;
            line[strlen(line) - 3] = '\0';
            printf("%s offset %d */\n", line, offset);
            continue;
        }
        if (line[0] != ' ' || line[4] != '\'') {
            offset += commas(line);
            printf("%s", line);
            continue;
        }
        switch (line[5]) {
        case 'm':
            cmd = append_cmd(c);
            cmd->op = op_move;
            sscanf(line + 8, "%lf, %lf", &cmd->pt[0].x, &cmd->pt[0].y);
            break;
        case 'l':
            cmd = append_cmd(c);
            cmd->op = op_line;
            sscanf(line + 8, "%lf, %lf", &cmd->pt[0].x, &cmd->pt[0].y);
            break;
        case 'c':
            cmd = append_cmd(c);
            cmd->op = op_curve;
            sscanf(line + 8, "%lf, %lf, %lf, %lf, %lf, %lf", &cmd->pt[0].x,
                   &cmd->pt[0].y, &cmd->pt[1].x, &cmd->pt[1].y, &cmd->pt[2].x,
                   &cmd->pt[2].y);
            break;
        case 'e':
            return c;
        }
    }
    return 0;
}

#define DOT_SIZE 1

static void dot(cairo_t *cr,
                double x,
                double y,
                double red,
                double blue,
                double green,
                double alpha)
{
    cairo_set_source_rgba(cr, red, blue, green, alpha);
    cairo_set_line_width(cr, 0.7);
    cairo_move_to(cr, x + DOT_SIZE, y);
    cairo_arc(cr, x, y, DOT_SIZE, 0, M_PI * 2);
    cairo_stroke(cr);
}

static void spot(cairo_t *cr,
                 double x,
                 double y,
                 double red,
                 double blue,
                 double green,
                 double alpha)
{
    cairo_set_source_rgba(cr, red, blue, green, alpha);
    cairo_move_to(cr, x - DOT_SIZE, y);
    cairo_arc(cr, x, y, DOT_SIZE, 0, M_PI * 2);
    cairo_fill(cr);
}

static void draw_char(char_t *c)
{
    cmd_t *cmd;
    cmd_stack_t *s;
    int i;

    XClearArea(dpy, win, 0, 0, 0, 0, False);

    for (cmd = c->cmd; cmd; cmd = cmd->next) {
        double alpha;

        if (cmd == c->first || cmd == c->last)
            alpha = 1;
        else
            alpha = 0.5;

        switch (cmd->op) {
        case op_move:
            dot(cr, cmd->pt[0].x, cmd->pt[0].y, 1, 1, 0, alpha);
            break;
        case op_line:
            dot(cr, cmd->pt[0].x, cmd->pt[0].y, 1, 0, 0, alpha);
            break;
        case op_curve:
            dot(cr, cmd->pt[0].x, cmd->pt[0].y, 0, 0, 1, alpha);
            dot(cr, cmd->pt[1].x, cmd->pt[1].y, 0, 0, 1, alpha);
            dot(cr, cmd->pt[2].x, cmd->pt[2].y, 0, 1, 0, alpha);
            break;
        default:
            break;
        }
    }
    for (s = c->stack; s; s = s->prev)
        if (!s->prev)
            break;
    if (s) {
        for (cmd = s->cmd; cmd; cmd = cmd->next) {
            double alpha = 1;

            switch (cmd->op) {
            case op_move:
                spot(cr, cmd->pt[0].x, cmd->pt[0].y, 1, 1, 0, alpha);
                break;
            case op_line:
                spot(cr, cmd->pt[0].x, cmd->pt[0].y, 1, 0, 0, alpha);
                break;
            case op_curve:
                spot(cr, cmd->pt[0].x, cmd->pt[0].y, 0, 0, 1, alpha);
                spot(cr, cmd->pt[1].x, cmd->pt[1].y, 0, 0, 1, alpha);
                spot(cr, cmd->pt[2].x, cmd->pt[2].y, 0, 1, 0, alpha);
                break;
            default:
                break;
            }
        }
    }
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_line_width(cr, 0.5);

    for (cmd = c->cmd; cmd; cmd = cmd->next) {
        switch (cmd->op) {
        case op_move:
            cairo_move_to(cr, cmd->pt[0].x, cmd->pt[0].y);
            break;
        case op_line:
            cairo_line_to(cr, cmd->pt[0].x, cmd->pt[0].y);
            break;
        case op_curve:
            cairo_curve_to(cr, cmd->pt[0].x, cmd->pt[0].y, cmd->pt[1].x,
                           cmd->pt[1].y, cmd->pt[2].x, cmd->pt[2].y);
            break;
        default:
            abort();
        }
    }
    cairo_stroke(cr);

    for (cmd = c->cmd, i = 0; cmd; cmd = cmd->next, i++) {
        double tx, ty;
        char buf[11];

        if (cmd->op == op_curve) {
            tx = cmd->pt[2].x;
            ty = cmd->pt[2].y;
        } else {
            tx = cmd->pt[0].x;
            ty = cmd->pt[0].y;
        }
        {
            cairo_save(cr);
            if (cmd == c->first)
                cairo_set_source_rgb(cr, 0, .5, 0);
            else if (cmd == c->last)
                cairo_set_source_rgb(cr, 0, 0, .5);
            else
                cairo_set_source_rgb(cr, 0, .5, .5);

            cairo_move_to(cr, tx - 2, ty + 3);
            snprintf(buf, sizeof(buf), "%d", i);
            cairo_show_text(cr, buf);
            cairo_restore(cr);
        }
    }
}

static cmd_t *pos_to_cmd(char_t *c, cmd_t *start, int ix, int iy)
{
    double x = ix, y = iy;
    double best_err = 1;
    cmd_t *cmd, *best_cmd = 0;

    cairo_device_to_user(cr, &x, &y);
    if (start)
        start = start->next;
    if (!start)
        start = c->cmd;

    cmd = start;
    while (cmd) {
        int i = cmd->op == op_curve ? 2 : 0;
        double dx = cmd->pt[i].x - x;
        double dy = cmd->pt[i].y - y;
        double err = sqrt(dx * dx + dy * dy);

        if (err < best_err) {
            best_err = err;
            best_cmd = cmd;
        }
        if (cmd->next)
            cmd = cmd->next;
        else
            cmd = c->cmd;
        if (cmd == start)
            cmd = 0;
    }
    return best_cmd;
}

static int is_before(cmd_t *before, cmd_t *after)
{
    if (!before)
        return 0;
    if (before->next == after)
        return 1;
    return is_before(before->next, after);
}

static void order(cmd_t **first_p, cmd_t **last_p)
{
    if (!is_before(*first_p, *last_p)) {
        cmd_t *t = *first_p;
        *first_p = *last_p;
        *last_p = t;
    }
}

static void replace_with_spline(char_t *c, cmd_t *first, cmd_t *last)
{
    pts_t *pts = new_pts();
    spline_t s;
    cmd_t *cmd, *next, *save;

    order(&first, &last);
    for (cmd = first; cmd != last->next; cmd = cmd->next) {
        int i = cmd->op == op_curve ? 2 : 0;
        add_pt(pts, &cmd->pt[i]);
    }

    s = fit(pts->pt, pts->n);

    push(c);

    save = last->next;

    for (cmd = first->next; cmd != save; cmd = next) {
        next = cmd->next;
        delete_cmd(&c->cmd, cmd);
    }

    cmd = insert_cmd(&first->next);

    cmd->op = op_curve;
    cmd->pt[0] = s.b;
    cmd->pt[1] = s.c;
    cmd->pt[2] = s.d;

    dispose_pts(pts);
    c->first = c->last = 0;
}

static void split(char_t *c, cmd_t *first, cmd_t *last)
{
    cmd_t *cmd;

    push(c);
    cmd = insert_cmd(&first->next);
    cmd->op = op_line;
    cmd->pt[0] = lerp(&first->pt[0], &last->pt[0]);
    if (last->op == op_move) {
        cmd_t *extra = insert_cmd(&last->next);

        extra->op = op_line;
        extra->pt[0] = last->pt[0];
        last->pt[0] = cmd->pt[0];
    }
    c->first = c->last = 0;
}

static void delete(char_t *c, cmd_t *first)
{
    push(c);
    delete_cmd(&c->cmd, first);
    c->first = c->last = 0;
}

static void tweak_spline(char_t *c, cmd_t *first, int p2, double dx, double dy)
{
    int i = p2 ? 1 : 0;

    push(c);
    first->pt[i].x += dx;
    first->pt[i].y += dy;
}

static void undo(char_t *c)
{
    pop(c);
}

static void button(char_t *c, XButtonEvent *bev)
{
    cmd_t *first = bev->button == 1 ? c->first : c->last;
    cmd_t *where = pos_to_cmd(c, first, bev->x, bev->y);

    if (!where) {
        XBell(dpy, 50);
        return;
    }
    switch (bev->button) {
    case 1:
        c->first = where;
        break;
    case 2:
    case 3:
        c->last = where;
        break;
    }
    draw_char(c);
}

static void play(char_t *c)
{
    XEvent ev;
    char key_string[10];

    XClearArea(dpy, win, 0, 0, 0, 0, True);
    for (;;) {
        XNextEvent(dpy, &ev);
        switch (ev.type) {
        case KeyPress:
            if (XLookupString((XKeyEvent *) &ev, key_string, sizeof(key_string),
                              0, 0) == 1) {
                switch (key_string[0]) {
                case 'q':
                    return;
                case 'c':
                    XClearArea(dpy, ev.xkey.window, 0, 0, 0, 0, True);
                    break;
                case 's':
                    if (c->first && c->last) {
                        split(c, c->first, c->last);
                        draw_char(c);
                    }
                    break;
                case 'u':
                    undo(c);
                    draw_char(c);
                    break;
                case 'f':
                    if (c->first && c->last) {
                        replace_with_spline(c, c->first, c->last);
                        draw_char(c);
                    }
                    break;
                case 'd':
                    if (c->first) {
                        delete (c, c->first);
                        draw_char(c);
                    }
                    break;
                }
            } else {
                cmd_t *spline;
                if (c->first && c->first->op == op_curve)
                    spline = c->first;
                else if (c->last && c->last->op == op_curve)
                    spline = c->last;
                else
                    spline = 0;
                if (spline) {
                    int keysyms_keycode;
                    KeySym *keysym = XGetKeyboardMapping(dpy, ev.xkey.keycode,
                                                         1, &keysyms_keycode);
                    switch (keysyms_keycode) {
                    case XK_Left:
                        tweak_spline(c, spline, ev.xkey.state & ShiftMask, -1,
                                     0);
                        draw_char(c);
                        break;
                    case XK_Right:
                        tweak_spline(c, spline, ev.xkey.state & ShiftMask, 1,
                                     0);
                        draw_char(c);
                        break;
                    case XK_Up:
                        tweak_spline(c, spline, ev.xkey.state & ShiftMask, 0,
                                     -1);
                        draw_char(c);
                        break;
                    case XK_Down:
                        tweak_spline(c, spline, ev.xkey.state & ShiftMask, 0,
                                     1);
                        draw_char(c);
                        break;
                    }
                    XFree(keysym);
                }
            }
            break;
        case Expose:
            if (ev.xexpose.count == 0)
                draw_char(c);
            break;
        case ButtonPress:
            button(c, &ev.xbutton);
            break;
        }
    }
}

static void write_char(char_t *c)
{
    cmd_t *cmd;

    for (cmd = c->cmd; cmd; cmd = cmd->next) {
        switch (cmd->op) {
        case op_move:
            printf("    'm', %g, %g,\n", cmd->pt[0].x, cmd->pt[0].y);
            offset += 3;
            break;
        case op_line:
            printf("    'l', %g, %g,\n", cmd->pt[0].x, cmd->pt[0].y);
            offset += 3;
            break;
        case op_curve:
            printf("    'c', %g, %g, %g, %g, %g, %g,\n", cmd->pt[0].x,
                   cmd->pt[0].y, cmd->pt[1].x, cmd->pt[1].y, cmd->pt[2].x,
                   cmd->pt[2].y);
            offset += 7;
            break;
        default:
            break;
        }
    }
    printf("    'e',\n");
    offset += 1;
}

int main(int argc, char **argv)
{
    char_t *c;
    int ucs4;

    if (!init(argc, argv))
        exit(1);
    while ((c = read_char())) {
        play(c);
        write_char(c);
    }
    for (ucs4 = 0; ucs4 < 0x80; ucs4++) {
        if ((ucs4 & 7) == 0)
            printf("\n   ");
        printf(" %4d,", offsets[ucs4]);
    }
    printf("\n");

    return 0;
}
