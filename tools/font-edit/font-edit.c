/*
 * Copyright (c) 2004 Keith Packard
 * Copyright (c) 2024-2025 National Cheng Kung University, Taiwan
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

#include <SDL.h>
#include <SDL_keycode.h>
#include <cairo.h>
#include <stdbool.h>

#include "font-edit.h"

static SDL_Window *window;
static cairo_t *cr;
static cairo_surface_t *surface;

static int width = 512;
static int height = 512;
static double scale = 8;
static int offset;

static int offsets[1024];

/* exit_window - bool. Control the life of the window.
 * if the value is false, the window remains open;
 * otherwise, the window closes.
 */
static bool exit_window = false;

static void free_cmd(cmd_t *cmd);

static bool init(int argc maybe_unused, char **argv maybe_unused)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Failed to initialize SDL video. Reason: %s\n", SDL_GetError());
        return false;
    }

    window = SDL_CreateWindow("Font Editor", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, width, height,
                              SDL_WINDOW_SHOWN);
    if (!window) {
        printf("Failed to create SDL window. Reason: %s\n", SDL_GetError());
        return false;
    }

    /* Create an SDL surface linked to the window */
    SDL_Surface *sdl_surface = SDL_GetWindowSurface(window);

    /* Create Cairo surface based on the SDL surface */
    surface = cairo_image_surface_create_for_data(
        (unsigned char *) sdl_surface->pixels, CAIRO_FORMAT_ARGB32,
        sdl_surface->w, sdl_surface->h, sdl_surface->pitch);

    cr = cairo_create(surface);

    cairo_translate(cr, 150, 420);
    cairo_scale(cr, scale, scale);

    cairo_set_font_size(cr, 2);
    return true;
}

static void cleanup(void)
{
    if (cr) {
        cairo_destroy(cr);
        cr = NULL;
    }
    if (surface) {
        cairo_surface_destroy(surface);
        surface = NULL;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    SDL_Quit();
}

static cmd_t *copy_cmd(cmd_t *cmd)
{
    if (!cmd)
        return NULL;

    cmd_t *head = NULL;
    cmd_t **tail = &head;

    while (cmd) {
        cmd_t *n = malloc(sizeof(cmd_t));
        if (!n) {
            /* Cleanup on failure */
            free_cmd(head);
            return NULL;
        }
        *n = *cmd;
        n->next = NULL;
        *tail = n;
        tail = &n->next;
        cmd = cmd->next;
    }

    return head;
}

static void free_cmd(cmd_t *cmd)
{
    while (cmd) {
        cmd_t *next = cmd->next;
        free(cmd);
        cmd = next;
    }
}

static cmd_t *insert_cmd(cmd_t **prev)
{
    cmd_t *n = malloc(sizeof(cmd_t));
    if (!n)
        return NULL;

    n->op = op_noop;
    n->next = *prev;
    *prev = n;
    return n;
}

static cmd_t *append_cmd(char_t *c)
{
    cmd_t **prev;

    for (prev = &c->cmd; *prev; prev = &(*prev)->next)
        ;
    return insert_cmd(prev);
}

static void delete_cmd(cmd_t **head, cmd_t *cmd)
{
    while (*head != cmd)
        head = &(*head)->next;
    *head = cmd->next;
    free(cmd);
}

static bool push(char_t *c)
{
    cmd_stack_t *s = malloc(sizeof(cmd_stack_t));
    if (!s)
        return false;

    s->cmd = copy_cmd(c->cmd);
    if (!s->cmd && c->cmd) {
        free(s);
        return false;
    }

    s->prev = c->stack;
    c->stack = s;
    return true;
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

static void delete_first_cmd(char_t *c, cmd_t *first)
{
    if (!push(c))
        fprintf(stderr, "Warning: cannot save undo state\n");
    delete_cmd(&c->cmd, first);
    c->first = c->last = 0;
}

static void undo(char_t *c)
{
    pop(c);
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

static char_t *read_char(FILE *file)
{
    char_t *c = malloc(sizeof(char_t));
    if (!c)
        return NULL;

    char line[1024];
    cmd_t *cmd;

    c->cmd = 0;
    c->stack = 0;
    c->first = 0;
    c->last = 0;
    while (fgets(line, sizeof(line), file)) {
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
            if (!cmd)
                goto error;
            cmd->op = op_move;
            sscanf(line + 8, "%lf, %lf", &cmd->pt[0].x, &cmd->pt[0].y);
            break;
        case 'l':
            cmd = append_cmd(c);
            if (!cmd)
                goto error;
            cmd->op = op_line;
            sscanf(line + 8, "%lf, %lf", &cmd->pt[0].x, &cmd->pt[0].y);
            break;
        case 'c':
            cmd = append_cmd(c);
            if (!cmd)
                goto error;
            cmd->op = op_curve;
            sscanf(line + 8, "%lf, %lf, %lf, %lf, %lf, %lf", &cmd->pt[0].x,
                   &cmd->pt[0].y, &cmd->pt[1].x, &cmd->pt[1].y, &cmd->pt[2].x,
                   &cmd->pt[2].y);
            break;
        case 'e':
            return c;
        }
    }
    return NULL;

error:
    free_cmd(c->cmd);
    while (c->stack) {
        cmd_stack_t *s = c->stack;
        c->stack = s->prev;
        free_cmd(s->cmd);
        free(s);
    }
    free(c);
    return NULL;
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

    /* Clear the SDL surface */
    SDL_Surface *sdl_surface = SDL_GetWindowSurface(window);
    /* Fill with white color to clear */
    SDL_FillRect(sdl_surface, NULL,
                 SDL_MapRGB(sdl_surface->format, 255, 255, 255));

    /* Set up Cairo to draw on the surface */
    cairo_save(cr);

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

        /* Save state before rendering text */
        cairo_save(cr);
        if (cmd == c->first) {
            /* Green for the first command */
            cairo_set_source_rgb(cr, 0, .5, 0);
        } else if (cmd == c->last) {
            /* Blue for the last command */
            cairo_set_source_rgb(cr, 0, 0, .5);
        } else {
            /* Cyan for intermediate commands */
            cairo_set_source_rgb(cr, 0, .5, .5);
        }

        cairo_move_to(cr, tx - 2, ty + 3);
        /* Label with the index and draw */
        snprintf(buf, sizeof(buf), "%d", i);
        cairo_show_text(cr, buf);
        /* Restore after text drawing */
        cairo_restore(cr);
    }

    cairo_restore(cr);

    /* Finally, update the SDL surface with the new Cairo drawing */
    SDL_UpdateWindowSurface(window);
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

static bool is_before(cmd_t *before, cmd_t *after)
{
    if (!before)
        return false;

    while (before) {
        if (before->next == after)
            return true;
        before = before->next;
    }
    return false;
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
    if (!pts) {
        fprintf(stderr, "Error: out of memory\n");
        return;
    }

    spline_t s;
    cmd_t *cmd, *next, *save;

    order(&first, &last);
    for (cmd = first; cmd != last->next; cmd = cmd->next) {
        int i = cmd->op == op_curve ? 2 : 0;
        if (!add_pt(pts, &cmd->pt[i])) {
            dispose_pts(pts);
            fprintf(stderr, "Error: out of memory\n");
            return;
        }
    }

    s = fit(pts->pt, pts->n);

    if (!push(c))
        fprintf(stderr, "Warning: cannot save undo state\n");

    save = last->next;

    for (cmd = first->next; cmd != save; cmd = next) {
        next = cmd->next;
        delete_cmd(&c->cmd, cmd);
    }

    cmd = insert_cmd(&first->next);
    if (!cmd) {
        pop(c);
        dispose_pts(pts);
        fprintf(stderr, "Error: out of memory\n");
        return;
    }

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

    if (!push(c))
        fprintf(stderr, "Warning: cannot save undo state\n");
    cmd = insert_cmd(&first->next);
    if (!cmd) {
        pop(c);
        fprintf(stderr, "Error: out of memory\n");
        return;
    }
    cmd->op = op_line;
    cmd->pt[0] = lerp(&first->pt[0], &last->pt[0]);
    if (last->op == op_move) {
        cmd_t *extra = insert_cmd(&last->next);
        if (!extra) {
            pop(c);
            fprintf(stderr, "Error: out of memory\n");
            return;
        }
        extra->op = op_line;
        extra->pt[0] = last->pt[0];
        last->pt[0] = cmd->pt[0];
    }
    c->first = c->last = 0;
}

static void tweak_spline(char_t *c,
                         cmd_t *first,
                         int is_2nd_point,
                         double dx,
                         double dy)
{
    int i = !!is_2nd_point;

    if (!push(c))
        fprintf(stderr, "Warning: cannot save undo state\n");
    first->pt[i].x += dx;
    first->pt[i].y += dy;
}

static void button(char_t *c, SDL_MouseButtonEvent *bev)
{
    cmd_t *first = bev->button == SDL_BUTTON_LEFT ? c->first : c->last;
    cmd_t *where = pos_to_cmd(c, first, bev->x, bev->y);

    if (!where) {
        SDL_Log("Button click outside target");
        return;
    }
    switch (bev->button) {
    case SDL_BUTTON_LEFT:
        c->first = where;
        break;
    case SDL_BUTTON_RIGHT:
        c->last = where;
        break;
    }
}

static void play(char_t *c)
{
    /* Ensures that SDL only focuses on key events*/
    SDL_StopTextInput();

    SDL_Event event;
    SDL_Keycode key_event;

    /* keep track of the selected spline */
    cmd_t *spline = NULL;
    draw_char(c);

    while (!exit_window) {
        if (SDL_PollEvent(&event)) {
            switch (event.type) {
            /* If SDL event is detected */
            case SDL_QUIT:
                /* Click the "X" on the top of the screen to exit the program */
                exit_window = true;
                break;
            case SDL_KEYDOWN:
                /* If any key event is detected */
                key_event = event.key.keysym.sym;

                switch (key_event) {
                case SDLK_q:
                    /* To exit play()
                     * This allows the user to display the next character.
                     */
                    return;
                case SDLK_ESCAPE:
                    /* To quit the program */
                    exit_window = true;
                    return;
                case SDLK_s:
                    /* To split the line or curve formed by two adjacent points
                     * into two segments
                     */
                    if (c->first && c->last) {
                        split(c, c->first, c->last);
                    }
                    draw_char(c);
                    break;
                case SDLK_u:
                    /* To undo the last operation */
                    undo(c);
                    draw_char(c);
                    break;
                case SDLK_f:
                    /* To replace a line or a curve with another spline */
                    if (c->first && c->last) {
                        replace_with_spline(c, c->first, c->last);
                    }
                    draw_char(c);
                    break;
                case SDLK_d:
                    /* To delete the first pointer */
                    if (c->first) {
                        delete_first_cmd(c, c->first);
                    }
                    draw_char(c);
                    break;
                /* Move the points */
                case SDLK_LEFT:
                    if (!spline)
                        break;
                    tweak_spline(c, spline, event.key.keysym.mod & KMOD_SHIFT,
                                 -1, 0);
                    draw_char(c);
                    break;
                case SDLK_RIGHT:
                    if (!spline)
                        break;

                    tweak_spline(c, spline, event.key.keysym.mod & KMOD_SHIFT,
                                 1, 0);
                    draw_char(c);
                    break;
                case SDLK_UP:
                    if (!spline)
                        break;

                    tweak_spline(c, spline, event.key.keysym.mod & KMOD_SHIFT,
                                 0, -1);
                    draw_char(c);
                    break;
                case SDLK_DOWN:
                    if (!spline)
                        break;

                    tweak_spline(c, spline, event.key.keysym.mod & KMOD_SHIFT,
                                 0, 1);
                    draw_char(c);
                    break;
                }
                break;
                /* End if key event detected */

            case SDL_MOUSEBUTTONDOWN:
                /* Redraw the content after mouse button interaction */
                button(c, &event.button);
                spline = c->first;
                draw_char(c);
                break;
            }
            /* End if SDL event detected */
        }

        /* Ensure the SDL window surface is updated */
        SDL_UpdateWindowSurface(window);
    }
}

static void print_char(char_t *c)
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

static void generate_font_metrics(void)
{
    int ucs4;
    for (ucs4 = 0; ucs4 < 0x80; ucs4++) {
        if ((ucs4 & 7) == 0)
            printf("\n   ");
        printf(" %4d,", offsets[ucs4]);
    }
    printf("\n");
}

static void free_char(char_t *c)
{
    if (!c)
        return;

    free_cmd(c->cmd);
    while (c->stack) {
        cmd_stack_t *s = c->stack;
        c->stack = s->prev;
        free_cmd(s->cmd);
        free(s);
    }
    free(c);
}

int main(int argc, char **argv)
{
    char_t *c;

    if (argc < 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "r");
    if (!file) {
        printf("Failed to open file: %s\n", argv[1]);
        return 2;
    }

    if (!init(argc, argv)) {
        fclose(file);
        return 1;
    }

    while ((c = read_char(file)) && !exit_window) {
        play(c);
        print_char(c);
        free_char(c);
    }

    fclose(file);

    generate_font_metrics();

    cleanup();

    return 0;
}
