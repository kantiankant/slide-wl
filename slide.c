#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "slide.h"

static struct slide_server *G = NULL;


// forward delcarations
static void focus_toplevel(struct slide_toplevel *toplevel);
static void reproject_all(struct slide_server *server);
static void viewport_follow(struct slide_toplevel *c);

// include config.h thingy
#include "config.h"

// helpers
static inline int to_screen_x(struct slide_server *s, int cx) { return cx - s->vx; }
static inline int to_screen_y(struct slide_server *s, int cy) { return cy - s->vy; }

static void toplevel_get_size(struct slide_toplevel *t,
                               unsigned int *w, unsigned int *h)
{
    struct wlr_box geo;
    geo = t->xdg_toplevel->base->geometry;
    *w = geo.width;
    *h = geo.height;
}

static void win_reposition(struct slide_toplevel *t) {
    wlr_scene_node_set_position(&t->scene_tree->node,
        to_screen_x(t->server, t->cx),
        to_screen_y(t->server, t->cy));
}

static void reproject_all(struct slide_server *server) {
    struct slide_toplevel *t;
    wl_list_for_each(t, &server->toplevels, link) {
        if (!t->fullscreen)
            win_reposition(t);
    }
}


// Panning here

void pan_by(struct slide_server *s, int dx, int dy) {
    s->vx += dx;
    s->vy += dy;
    reproject_all(s);
}

void pan_by_key(const Arg arg) {
    switch (arg.i) {
        case 0: pan_by(G, -PAN_STEP, 0); break;
        case 1: pan_by(G,  PAN_STEP, 0); break;
        case 2: pan_by(G, 0, -PAN_STEP); break;
        case 3: pan_by(G, 0,  PAN_STEP); break;
    }
}

static void viewport_follow(struct slide_toplevel *c) {
    struct slide_server *s = c->server;
    unsigned int cw, ch;
    toplevel_get_size(c, &cw, &ch);

    int sx = to_screen_x(s, c->cx);
    int sy = to_screen_y(s, c->cy);
    int margin = WIN_MOVE_STEP;

    if (sx < margin)                    s->vx += sx - margin;
    if (sy < margin)                    s->vy += sy - margin;
    if (sx + (int)cw > s->sw - margin)  s->vx += sx + (int)cw - s->sw + margin;
    if (sy + (int)ch > s->sh - margin)  s->vy += sy + (int)ch - s->sh + margin;

    reproject_all(s);
}

static void viewport_center_on(struct slide_toplevel *c) {
    struct slide_server *s = c->server;
    unsigned int cw, ch;
    toplevel_get_size(c, &cw, &ch);
    s->vx = c->cx - (s->sw - (int)cw) / 2;
    s->vy = c->cy - (s->sh - (int)ch) / 2;
    reproject_all(s);
}



// hocus Focus

static void focus_toplevel(struct slide_toplevel *toplevel) {
    if (!toplevel) return;

    struct slide_server *server = toplevel->server;
    struct wlr_seat     *seat   = server->seat;
    struct wlr_surface  *prev   = seat->keyboard_state.focused_surface;
    struct wlr_surface  *surf   = toplevel->xdg_toplevel->base->surface;

    if (prev == surf) return;

    if (prev) {
        struct wlr_xdg_toplevel *pt =
            wlr_xdg_toplevel_try_from_wlr_surface(prev);
        if (pt) wlr_xdg_toplevel_set_activated(pt, false);
    }

    wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);
    wl_list_remove(&toplevel->link);
    wl_list_insert(&server->toplevels, &toplevel->link);
    wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, true);

    struct wlr_keyboard *kb = wlr_seat_get_keyboard(seat);
    if (kb)
        wlr_seat_keyboard_notify_enter(seat, surf,
            kb->keycodes, kb->num_keycodes, &kb->modifiers);

    server->focused = toplevel;
}


// Window actions

void win_kill(const Arg arg) {
    (void)arg;
    if (G->focused) wlr_xdg_toplevel_send_close(G->focused->xdg_toplevel);
}

void win_center(const Arg arg) {
    (void)arg;
    struct slide_toplevel *t = G->focused;
    if (!t) return;
    unsigned int w, h;
    toplevel_get_size(t, &w, &h);
    t->cx = G->vx + (G->sw - (int)w) / 2;
    t->cy = G->vy + (G->sh - (int)h) / 2;
    win_reposition(t);
}

void win_move(const Arg arg) {
    struct slide_toplevel *t = G->focused;
    if (!t || t->fullscreen) return;
    switch (arg.i) {
        case 0: t->cx -= WIN_MOVE_STEP; break;
        case 1: t->cx += WIN_MOVE_STEP; break;
        case 2: t->cy -= WIN_MOVE_STEP; break;
        case 3: t->cy += WIN_MOVE_STEP; break;
    }
    win_reposition(t);
    viewport_follow(t);
}

void win_fs(const Arg arg) {
    (void)arg;
    struct slide_toplevel *t = G->focused;
    if (!t) return;

    if ((t->fullscreen = !t->fullscreen)) {
//         Save current geometry and go fullscreen 
        unsigned int w, h;
        toplevel_get_size(t, &w, &h);
        t->wx = to_screen_x(G, t->cx);
        t->wy = to_screen_y(G, t->cy);
        t->ww = w; t->wh = h;
        wlr_xdg_toplevel_set_size(t->xdg_toplevel, G->sw, G->sh);
        wlr_scene_node_set_position(&t->scene_tree->node, 0, 0);
    } else {
        wlr_xdg_toplevel_set_size(t->xdg_toplevel, t->ww, t->wh);
        win_reposition(t);
    }
}

void win_cycle(const Arg arg) {
    if (!G->focused || wl_list_length(&G->toplevels) < 2) return;

    struct slide_toplevel *next;
    if (arg.i) {
        next = wl_container_of(G->focused->link.prev, next, link);
        if (&next->link == &G->toplevels)
            next = wl_container_of(G->toplevels.prev, next, link);
    } else {
        next = wl_container_of(G->focused->link.next, next, link);
        if (&next->link == &G->toplevels)
            next = wl_container_of(G->toplevels.next, next, link);
    }
    focus_toplevel(next);
    viewport_center_on(next);
}

void slide_quit(const Arg arg) {
    (void)arg;
    wl_display_terminate(G->wl_display);
}

void run(const Arg arg) {
    if (fork()) return;
    setsid();
    execvp(arg.com[0], (char *const *)arg.com);
    exit(1);
}


// Keyboard stuff here

static void keyboard_handle_modifiers(struct wl_listener *listener, void *data) {
    struct slide_keyboard *kb = wl_container_of(listener, kb, modifiers);
    wlr_seat_set_keyboard(kb->server->seat, kb->wlr_keyboard);
    wlr_seat_keyboard_notify_modifiers(kb->server->seat,
        &kb->wlr_keyboard->modifiers);
}

static void keyboard_handle_key(struct wl_listener *listener, void *data) {
    struct slide_keyboard          *kb     = wl_container_of(listener, kb, key);
    struct slide_server            *server = kb->server;
    struct wlr_keyboard_key_event  *event  = data;
    struct wlr_seat                *seat   = server->seat;

    uint32_t keycode = event->keycode + 8;
    const xkb_keysym_t *syms;
    int nsyms = xkb_state_key_get_syms(kb->wlr_keyboard->xkb_state,
                                        keycode, &syms);

    bool handled = false;
    uint32_t mods = wlr_keyboard_get_modifiers(kb->wlr_keyboard);

    if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        for (int s = 0; s < nsyms; s++) {
            for (unsigned int i = 0; i < LENGTH(keys); i++) {
                if (syms[s] == keys[i].keysym && mods == keys[i].mod) {
                    keys[i].function(keys[i].arg);
                    handled = true;
                }
            }
        }
    }

    if (!handled) {
        wlr_seat_set_keyboard(seat, kb->wlr_keyboard);
        wlr_seat_keyboard_notify_key(seat, event->time_msec,
            event->keycode, event->state);
    }
}

static void keyboard_handle_destroy(struct wl_listener *listener, void *data) {
    struct slide_keyboard *kb = wl_container_of(listener, kb, destroy);
    wl_list_remove(&kb->modifiers.link);
    wl_list_remove(&kb->key.link);
    wl_list_remove(&kb->destroy.link);
    wl_list_remove(&kb->link);
    free(kb);
}

static void server_new_keyboard(struct slide_server *server,
                                 struct wlr_input_device *device)
{
    struct wlr_keyboard   *wlr_kb = wlr_keyboard_from_input_device(device);
    struct slide_keyboard *kb     = calloc(1, sizeof(*kb));
    kb->server      = server;
    kb->wlr_keyboard = wlr_kb;

    struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap  *km  = xkb_keymap_new_from_names(ctx, NULL,
                                  XKB_KEYMAP_COMPILE_NO_FLAGS);
    wlr_keyboard_set_keymap(wlr_kb, km);
    xkb_keymap_unref(km);
    xkb_context_unref(ctx);
    wlr_keyboard_set_repeat_info(wlr_kb, 25, 600);

    kb->modifiers.notify = keyboard_handle_modifiers;
    wl_signal_add(&wlr_kb->events.modifiers, &kb->modifiers);
    kb->key.notify = keyboard_handle_key;
    wl_signal_add(&wlr_kb->events.key, &kb->key);
    kb->destroy.notify = keyboard_handle_destroy;
    wl_signal_add(&device->events.destroy, &kb->destroy);

    wlr_seat_set_keyboard(server->seat, wlr_kb);
    wl_list_insert(&server->keyboards, &kb->link);
}

// Pointer / cursor                                                     

static struct slide_toplevel *toplevel_at(struct slide_server *server,
    double lx, double ly, struct wlr_surface **surface, double *sx, double *sy)
{
    struct wlr_scene_node *node =
        wlr_scene_node_at(&server->scene->tree.node, lx, ly, sx, sy);
    if (!node || node->type != WLR_SCENE_NODE_BUFFER) return NULL;

    struct wlr_scene_buffer  *sb = wlr_scene_buffer_from_node(node);
    struct wlr_scene_surface *ss = wlr_scene_surface_try_from_buffer(sb);
    if (!ss) return NULL;

    *surface = ss->surface;
    struct wlr_scene_tree *tree = node->parent;
    while (tree && !tree->node.data) tree = tree->node.parent;
    return tree ? tree->node.data : NULL;
}

static void process_cursor_motion(struct slide_server *server, uint32_t time) {
    /* Panning takes absolute priority */
    if (server->panning) {
        double dx = server->cursor->x - server->pan_start_x;
        double dy = server->cursor->y - server->pan_start_y;
        server->vx = server->pan_origin_vx - (int)dx;
        server->vy = server->pan_origin_vy - (int)dy;
        reproject_all(server);
        return;
    }

    double sx, sy;
    struct wlr_surface    *surface = NULL;
    struct slide_toplevel *toplevel = toplevel_at(server,
        server->cursor->x, server->cursor->y, &surface, &sx, &sy);

    if (!toplevel)
        wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "default");

    if (surface) {
        wlr_seat_pointer_notify_enter(server->seat, surface, sx, sy);
        wlr_seat_pointer_notify_motion(server->seat, time, sx, sy);
    } else {
        wlr_seat_pointer_clear_focus(server->seat);
    }
}

static void server_cursor_motion(struct wl_listener *listener, void *data) {
    struct slide_server           *server = wl_container_of(listener, server, cursor_motion);
    struct wlr_pointer_motion_event *event = data;
    wlr_cursor_move(server->cursor, &event->pointer->base,
        event->delta_x, event->delta_y);
    process_cursor_motion(server, event->time_msec);
}

static void server_cursor_motion_absolute(struct wl_listener *listener, void *data) {
    struct slide_server                    *server = wl_container_of(listener, server, cursor_motion_absolute);
    struct wlr_pointer_motion_absolute_event *event = data;
    wlr_cursor_warp_absolute(server->cursor, &event->pointer->base,
        event->x, event->y);
    process_cursor_motion(server, event->time_msec);
}

static void server_cursor_button(struct wl_listener *listener, void *data) {
    struct slide_server             *server = wl_container_of(listener, server, cursor_button);
    struct wlr_pointer_button_event *event  = data;

    wlr_seat_pointer_notify_button(server->seat,
        event->time_msec, event->button, event->state);

    if (event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
        if (server->panning) {
            server->panning = 0;
            wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "default");
        }
        return;
    }

    // Mod + shift + Right Mouse drag = pan lmao
    uint32_t mods = 0;
    struct wlr_keyboard *kb = wlr_seat_get_keyboard(server->seat);
    if (kb) mods = wlr_keyboard_get_modifiers(kb);

    if (event->button == BTN_RIGHT /* 0x111 */ &&
        (mods & (WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT)) ==
                 (WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT))
    {
        server->panning       = 1;
        server->pan_start_x   = server->cursor->x;
        server->pan_start_y   = server->cursor->y;
        server->pan_origin_vx = server->vx;
        server->pan_origin_vy = server->vy;
        wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "fleur");
        return;
    }

    double sx, sy;
    struct wlr_surface    *surface  = NULL;
    struct slide_toplevel *toplevel = toplevel_at(server,
        server->cursor->x, server->cursor->y, &surface, &sx, &sy);
    focus_toplevel(toplevel);
}

static void server_cursor_axis(struct wl_listener *listener, void *data) {
    struct slide_server           *server = wl_container_of(listener, server, cursor_axis);
    struct wlr_pointer_axis_event *event  = data;
    wlr_seat_pointer_notify_axis(server->seat,
        event->time_msec, event->orientation, event->delta,
        event->delta_discrete, event->source, event->relative_direction);
}

static void server_cursor_frame(struct wl_listener *listener, void *data) {
    struct slide_server *server = wl_container_of(listener, server, cursor_frame);
    wlr_seat_pointer_notify_frame(server->seat);
}


// Input device routing here

static void server_new_input(struct wl_listener *listener, void *data) {
    struct slide_server     *server = wl_container_of(listener, server, new_input);
    struct wlr_input_device *device = data;

    switch (device->type) {
    case WLR_INPUT_DEVICE_KEYBOARD:
        server_new_keyboard(server, device);
        break;
    case WLR_INPUT_DEVICE_POINTER:
        wlr_cursor_attach_input_device(server->cursor, device);
        break;
    default:
        break;
    }

    uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
    if (!wl_list_empty(&server->keyboards))
        caps |= WL_SEAT_CAPABILITY_KEYBOARD;
    wlr_seat_set_capabilities(server->seat, caps);
}

static void seat_request_cursor(struct wl_listener *listener, void *data) {
    struct slide_server *server = wl_container_of(listener, server, request_cursor);
    struct wlr_seat_pointer_request_set_cursor_event *event = data;
    if (server->seat->pointer_state.focused_client == event->seat_client)
        wlr_cursor_set_surface(server->cursor, event->surface,
            event->hotspot_x, event->hotspot_y);
}

static void seat_pointer_focus_change(struct wl_listener *listener, void *data) {
    struct slide_server *server = wl_container_of(listener, server, pointer_focus_change);
    struct wlr_seat_pointer_focus_change_event *event = data;
    if (!event->new_surface)
        wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "default");
}

static void seat_request_set_selection(struct wl_listener *listener, void *data) {
    struct slide_server *server = wl_container_of(listener, server, request_set_selection);
    struct wlr_seat_request_set_selection_event *event = data;
    wlr_seat_set_selection(server->seat, event->source, event->serial);
}


// Output ig

static void output_frame(struct wl_listener *listener, void *data) {
    struct slide_output      *output = wl_container_of(listener, output, frame);
    struct wlr_scene         *scene  = output->server->scene;
    struct wlr_scene_output  *so     = wlr_scene_get_scene_output(scene, output->wlr_output);

    wlr_scene_output_commit(so, NULL);

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(so, &now);
}

static void output_request_state(struct wl_listener *listener, void *data) {
    struct slide_output *output = wl_container_of(listener, output, request_state);
    const struct wlr_output_event_request_state *event = data;
    wlr_output_commit_state(output->wlr_output, event->state);
}

static void output_destroy(struct wl_listener *listener, void *data) {
    struct slide_output *output = wl_container_of(listener, output, destroy);
    wl_list_remove(&output->frame.link);
    wl_list_remove(&output->request_state.link);
    wl_list_remove(&output->destroy.link);
    wl_list_remove(&output->link);
    free(output);
}

static void server_new_output(struct wl_listener *listener, void *data) {
    struct slide_server *server     = wl_container_of(listener, server, new_output);
    struct wlr_output   *wlr_output = data;

    wlr_output_init_render(wlr_output, server->allocator, server->renderer);

    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);

    struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
    if (mode) wlr_output_state_set_mode(&state, mode);
    wlr_output_commit_state(wlr_output, &state);
    wlr_output_state_finish(&state);

//     Grab screen dimensions for first output 
    if (server->sw == 0) {
        server->sw = wlr_output->width;
        server->sh = wlr_output->height;
    }

    struct slide_output *output = calloc(1, sizeof(*output));
    output->wlr_output = wlr_output;
    output->server     = server;

    output->frame.notify = output_frame;
    wl_signal_add(&wlr_output->events.frame, &output->frame);
    output->request_state.notify = output_request_state;
    wl_signal_add(&wlr_output->events.request_state, &output->request_state);
    output->destroy.notify = output_destroy;
    wl_signal_add(&wlr_output->events.destroy, &output->destroy);

    wl_list_insert(&server->outputs, &output->link);

    struct wlr_output_layout_output *l_output =
        wlr_output_layout_add_auto(server->output_layout, wlr_output);
    struct wlr_scene_output *scene_output =
        wlr_scene_output_create(server->scene, wlr_output);
    wlr_scene_output_layout_add_output(server->scene_layout, l_output, scene_output);
}

// XDG shell toplevels (prob works idk) 

static void xdg_toplevel_map(struct wl_listener *listener, void *data) {
    struct slide_toplevel *t      = wl_container_of(listener, t, map);
    struct slide_server   *server = t->server;

    wl_list_insert(&server->toplevels, &t->link);

    // spawn near cursor
    unsigned int w, h;
    toplevel_get_size(t, &w, &h);

    int ax = 0, ay = 0, aw = server->sw, ah = server->sh;

    int sx = (int)server->cursor->x - (int)w / 2;
    int sy = (int)server->cursor->y - (int)h / 2;
    if (sx < ax)              sx = ax;
    if (sy < ay)              sy = ay;
    if (sx + (int)w > ax+aw)  sx = ax + aw - (int)w;
    if (sy + (int)h > ay+ah)  sy = ay + ah - (int)h;

    t->cx = sx + server->vx;
    t->cy = sy + server->vy;
    win_reposition(t);

    focus_toplevel(t);
}

static void xdg_toplevel_unmap(struct wl_listener *listener, void *data) {
    struct slide_toplevel *t = wl_container_of(listener, t, unmap);

    if (t == t->server->focused) t->server->focused = NULL;
    wl_list_remove(&t->link);

    if (!wl_list_empty(&t->server->toplevels)) {
        struct slide_toplevel *next =
            wl_container_of(t->server->toplevels.next, next, link);
        focus_toplevel(next);
    }
}

static void xdg_toplevel_commit(struct wl_listener *listener, void *data) {
    struct slide_toplevel *t = wl_container_of(listener, t, commit);
    if (t->xdg_toplevel->base->initial_commit)
        wlr_xdg_toplevel_set_size(t->xdg_toplevel, 0, 0);
}

static void xdg_toplevel_destroy(struct wl_listener *listener, void *data) {
    struct slide_toplevel *t = wl_container_of(listener, t, destroy);
    wl_list_remove(&t->map.link);
    wl_list_remove(&t->unmap.link);
    wl_list_remove(&t->commit.link);
    wl_list_remove(&t->destroy.link);
    wl_list_remove(&t->request_move.link);
    wl_list_remove(&t->request_resize.link);
    wl_list_remove(&t->request_maximize.link);
    wl_list_remove(&t->request_fullscreen.link);
    free(t);
}

static void xdg_toplevel_request_move(struct wl_listener *listener, void *data) {
}

static void xdg_toplevel_request_resize(struct wl_listener *listener, void *data) {
}

static void xdg_toplevel_request_maximize(struct wl_listener *listener, void *data) {
    struct slide_toplevel *t = wl_container_of(listener, t, request_maximize);
    if (t->xdg_toplevel->base->initialized)
        wlr_xdg_surface_schedule_configure(t->xdg_toplevel->base);
}

static void xdg_toplevel_request_fullscreen(struct wl_listener *listener, void *data) {
    struct slide_toplevel *t = wl_container_of(listener, t, request_fullscreen);
    if (t->xdg_toplevel->base->initialized)
        wlr_xdg_surface_schedule_configure(t->xdg_toplevel->base);
}

static void server_new_xdg_toplevel(struct wl_listener *listener, void *data) {
    struct slide_server     *server      = wl_container_of(listener, server, new_xdg_toplevel);
    struct wlr_xdg_toplevel *xdg_toplevel = data;

    struct slide_toplevel *t = calloc(1, sizeof(*t));
    t->server       = server;
    t->xdg_toplevel = xdg_toplevel;
    t->scene_tree   = wlr_scene_xdg_surface_create(&server->scene->tree,
                          xdg_toplevel->base);
    t->scene_tree->node.data    = t;
    xdg_toplevel->base->data    = t->scene_tree;

    t->map.notify = xdg_toplevel_map;
    wl_signal_add(&xdg_toplevel->base->surface->events.map, &t->map);
    t->unmap.notify = xdg_toplevel_unmap;
    wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &t->unmap);
    t->commit.notify = xdg_toplevel_commit;
    wl_signal_add(&xdg_toplevel->base->surface->events.commit, &t->commit);
    t->destroy.notify = xdg_toplevel_destroy;
    wl_signal_add(&xdg_toplevel->events.destroy, &t->destroy);
    t->request_move.notify = xdg_toplevel_request_move;
    wl_signal_add(&xdg_toplevel->events.request_move, &t->request_move);
    t->request_resize.notify = xdg_toplevel_request_resize;
    wl_signal_add(&xdg_toplevel->events.request_resize, &t->request_resize);
    t->request_maximize.notify = xdg_toplevel_request_maximize;
    wl_signal_add(&xdg_toplevel->events.request_maximize, &t->request_maximize);
    t->request_fullscreen.notify = xdg_toplevel_request_fullscreen;
    wl_signal_add(&xdg_toplevel->events.request_fullscreen, &t->request_fullscreen);
}


// xdg popup thing here

static void xdg_popup_commit(struct wl_listener *listener, void *data) {
    struct slide_popup *p = wl_container_of(listener, p, commit);
    if (p->xdg_popup->base->initial_commit)
        wlr_xdg_surface_schedule_configure(p->xdg_popup->base);
}

static void xdg_popup_destroy(struct wl_listener *listener, void *data) {
    struct slide_popup *p = wl_container_of(listener, p, destroy);
    wl_list_remove(&p->commit.link);
    wl_list_remove(&p->destroy.link);
    free(p);
}

static void server_new_xdg_popup(struct wl_listener *listener, void *data) {
    struct wlr_xdg_popup *xdg_popup = data;
    struct slide_popup   *p         = calloc(1, sizeof(*p));
    p->xdg_popup = xdg_popup;

    struct wlr_xdg_surface *parent =
        wlr_xdg_surface_try_from_wlr_surface(xdg_popup->parent);
    assert(parent);
    struct wlr_scene_tree *parent_tree = parent->data;
    xdg_popup->base->data = wlr_scene_xdg_surface_create(parent_tree, xdg_popup->base);

    p->commit.notify = xdg_popup_commit;
    wl_signal_add(&xdg_popup->base->surface->events.commit, &p->commit);
    p->destroy.notify = xdg_popup_destroy;
    wl_signal_add(&xdg_popup->events.destroy, &p->destroy);
}

// main

int main(int argc, char *argv[]) {
    wlr_log_init(WLR_ERROR, NULL);

    char *startup_cmd = NULL;
    int c;
    while ((c = getopt(argc, argv, "s:h")) != -1) {
        switch (c) {
        case 's': startup_cmd = optarg; break;
        default:
            printf("Usage: %s [-s startup_command]\n", argv[0]);
            return 0;
        }
    }

    struct slide_server server = {0};
    G = &server;

    signal(SIGCHLD, SIG_IGN);

    server.wl_display = wl_display_create();
    server.backend    = wlr_backend_autocreate(
        wl_display_get_event_loop(server.wl_display), NULL);
    if (!server.backend) { wlr_log(WLR_ERROR, "failed to create backend"); return 1; }

    server.renderer = wlr_renderer_autocreate(server.backend);
    if (!server.renderer) { wlr_log(WLR_ERROR, "failed to create renderer"); return 1; }
    wlr_renderer_init_wl_display(server.renderer, server.wl_display);

    server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);
    if (!server.allocator) { wlr_log(WLR_ERROR, "failed to create allocator"); return 1; }

    wlr_compositor_create(server.wl_display, 5, server.renderer);
    wlr_subcompositor_create(server.wl_display);
    wlr_data_device_manager_create(server.wl_display);

    server.output_layout = wlr_output_layout_create(server.wl_display);
    wl_list_init(&server.outputs);
    server.new_output.notify = server_new_output;
    wl_signal_add(&server.backend->events.new_output, &server.new_output);

    server.scene        = wlr_scene_create();
    server.scene_layout = wlr_scene_attach_output_layout(server.scene, server.output_layout);

    // xdg shell
    wl_list_init(&server.toplevels);
    server.xdg_shell = wlr_xdg_shell_create(server.wl_display, 3);
    server.new_xdg_toplevel.notify = server_new_xdg_toplevel;
    wl_signal_add(&server.xdg_shell->events.new_toplevel, &server.new_xdg_toplevel);
    server.new_xdg_popup.notify = server_new_xdg_popup;
    wl_signal_add(&server.xdg_shell->events.new_popup, &server.new_xdg_popup);

    // cursor
    server.cursor     = wlr_cursor_create();
    wlr_cursor_attach_output_layout(server.cursor, server.output_layout);
    server.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);

    server.cursor_motion.notify = server_cursor_motion;
    wl_signal_add(&server.cursor->events.motion, &server.cursor_motion);
    server.cursor_motion_absolute.notify = server_cursor_motion_absolute;
    wl_signal_add(&server.cursor->events.motion_absolute, &server.cursor_motion_absolute);
    server.cursor_button.notify = server_cursor_button;
    wl_signal_add(&server.cursor->events.button, &server.cursor_button);
    server.cursor_axis.notify = server_cursor_axis;
    wl_signal_add(&server.cursor->events.axis, &server.cursor_axis);
    server.cursor_frame.notify = server_cursor_frame;
    wl_signal_add(&server.cursor->events.frame, &server.cursor_frame);

    // seat
    wl_list_init(&server.keyboards);
    server.new_input.notify = server_new_input;
    wl_signal_add(&server.backend->events.new_input, &server.new_input);
    server.seat = wlr_seat_create(server.wl_display, "seat0");
    server.request_cursor.notify = seat_request_cursor;
    wl_signal_add(&server.seat->events.request_set_cursor, &server.request_cursor);
    server.pointer_focus_change.notify = seat_pointer_focus_change;
    wl_signal_add(&server.seat->pointer_state.events.focus_change,
        &server.pointer_focus_change);
    server.request_set_selection.notify = seat_request_set_selection;
    wl_signal_add(&server.seat->events.request_set_selection,
        &server.request_set_selection);

    const char *socket = wl_display_add_socket_auto(server.wl_display);
    if (!socket) { wlr_backend_destroy(server.backend); return 1; }

    if (!wlr_backend_start(server.backend)) {
        wlr_backend_destroy(server.backend);
        wl_display_destroy(server.wl_display);
        return 1;
    }

    setenv("WAYLAND_DISPLAY", socket, true);
    setenv("XDG_SESSION_TYPE", "wayland", true);

    if (startup_cmd) {
        if (fork() == 0)
            execl("/bin/sh", "/bin/sh", "-c", startup_cmd, (void *)NULL);
    }

    wlr_log(WLR_INFO, "slide running on WAYLAND_DISPLAY=%s", socket);
    wl_display_run(server.wl_display);

    // cleaning crew
    wl_display_destroy_clients(server.wl_display);
    wl_list_remove(&server.new_xdg_toplevel.link);
    wl_list_remove(&server.new_xdg_popup.link);
    wl_list_remove(&server.cursor_motion.link);
    wl_list_remove(&server.cursor_motion_absolute.link);
    wl_list_remove(&server.cursor_button.link);
    wl_list_remove(&server.cursor_axis.link);
    wl_list_remove(&server.cursor_frame.link);
    wl_list_remove(&server.new_input.link);
    wl_list_remove(&server.request_cursor.link);
    wl_list_remove(&server.pointer_focus_change.link);
    wl_list_remove(&server.request_set_selection.link);
    wl_list_remove(&server.new_output.link);
    wlr_scene_node_destroy(&server.scene->tree.node);
    wlr_xcursor_manager_destroy(server.cursor_mgr);
    wlr_cursor_destroy(server.cursor);
    wlr_allocator_destroy(server.allocator);
    wlr_renderer_destroy(server.renderer);
    wlr_backend_destroy(server.backend);
    wl_display_destroy(server.wl_display);
    return 0;
}

/* tung */
