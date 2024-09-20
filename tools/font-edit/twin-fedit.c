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

static SDL_Window *window;
static cairo_t *cr;
static cairo_surface_t *surface;

static int width = 512;
static int height = 512;
static double scale = 8;
static int offset;

static int offsets[1024];

/* Control the life of the widown,
 * if the value is 0, the windown lives,
 * otherwise the windown quit.
 */
static int exit_window = 0;

static int init(int argc, char **argv)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 0;
    }

    window = SDL_CreateWindow("Twin Fedit", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, width, height,
                              SDL_WINDOW_SHOWN);
    if (!window) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 0;
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
    draw_char(c);
}

static void play(char_t *c)
{
    /* Ensures that SDL only focuses on key events*/
    SDL_StopTextInput();

    SDL_Event event;
    int quit_state = 0;

    /* keep track of the selected spline */
    cmd_t *spline = NULL;
    while (!quit_state) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            /* If SDL event is detected */
            case SDL_QUIT:
                /* Click the "X" on the top of screen to exit the program */
                quit_state = 1;
                exit_window = 1;
                break;
            case SDL_KEYDOWN:
                /* If any key event is detected */
                SDL_Keycode key_event = event.key.keysym.sym;
                switch (key_event) {
                case SDLK_q:
                    /* Press "q" key to exit play()
                     * This allows the user to display the next character.
                     */
                    quit_state = 1;
                    break;
                case SDLK_ESCAPE:
                    /* Press "ESC" key to quit the program */
                    quit_state = 1;
                    exit_window = 1;
                    break;
                case SDLK_s:
                    /* Press "s" key to split the command between first and last
                     */
                    if (c->first && c->last) {
                        split(c, c->first, c->last);
                    }
                    break;
                case SDLK_u:
                    /* Press "u" key to undo the last operation */
                    undo(c);
                    break;
                case SDLK_f:
                    /* Press "f" key to replace with spline between first and
                     * last */
                    if (c->first && c->last) {
                        replace_with_spline(c, c->first, c->last);
                    }
                    break;
                case SDLK_d:
                    /* Press "d" key to delete the first command */
                    if (c->first) {
                        delete (c, c->first);
                    }
                    break;
                case SDLK_DOWN:
                    if (spline) {
                        int dx = 0, dy = 0;
                        if (event.key.keysym.sym == SDLK_LEFT)
                            dx = -1;
                        if (event.key.keysym.sym == SDLK_RIGHT)
                            dx = 1;
                        if (event.key.keysym.sym == SDLK_UP)
                            dy = -1;
                        if (event.key.keysym.sym == SDLK_DOWN)
                            dy = 1;
                        tweak_spline(c, spline,
                                     event.key.keysym.mod & KMOD_SHIFT, dx, dy);
                    }
                    break;
                }
                draw_char(
                    c);  // Ensure the content is redrawn after every key press
                break;
            /* End if key event detected */
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_EXPOSED)
                    draw_char(c);
                break;
            case SDL_MOUSEBUTTONDOWN:
                button(c, &event.button);
                draw_char(
                    c);  // Redraw the content after mouse button interaction
                break;
                break;
                /* End if SDL event detected */
            }
        }

        // Ensure the window surface is updated
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

static void print_offsets()
{
    int ucs4;
    for (ucs4 = 0; ucs4 < 0x80; ucs4++) {
        if ((ucs4 & 7) == 0)
            printf("\n   ");
        printf(" %4d,", offsets[ucs4]);
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    char_t *c;

    if (!init(argc, argv))
        exit(1);
    while ((c = read_char()) && !exit_window) {
        play(c);
        print_char(c);
    }

    print_offsets();

    return 0;
}
