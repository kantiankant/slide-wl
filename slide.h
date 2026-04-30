#ifndef SLIDE_H
#define SLIDE_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/util/log.h>

#ifdef __FreeBSD__
#include <dev/evdev/input-event-codes.h>
#else
#include <linux/input-event-codes.h>
#endif

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <math.h>
#include <time.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define LENGTH(x) (sizeof(x) / sizeof(x[0]))

typedef struct {
    int    active;
    int    closing;       
    float  t;            
    struct timespec start;
} slide_anim;

// translation because porting is hard and I'm lazy
#define Mod4Mask    WLR_MODIFIER_LOGO
#define ShiftMask   WLR_MODIFIER_SHIFT
#define ControlMask WLR_MODIFIER_CTRL

typedef union {
    const char **com;
    const int    i;
    const float  f;
} Arg;

typedef struct {
    uint32_t     mod;
    xkb_keysym_t keysym;
    void       (*function)(const Arg);
    const Arg    arg;
} key;

typedef enum {
    SLIDE_GRAB_NONE,
    SLIDE_GRAB_MOVE,
    SLIDE_GRAB_RESIZE,
} slide_grab_mode;

struct slide_server {
    struct wl_display              *wl_display;
    struct wlr_backend             *backend;
    struct wlr_viewporter *viewporter;
    struct wlr_renderer            *renderer;
    struct wlr_allocator           *allocator;
    struct wlr_scene               *scene;
    struct wlr_scene_output_layout *scene_layout;

    struct wlr_xdg_shell           *xdg_shell;
    struct wl_listener              new_xdg_toplevel;
    struct wl_listener              new_xdg_popup;
    struct wl_list                  toplevels;
    struct wl_list                  dying_toplevels; 

    // xdg-decoration: we tell clients to remove their CSDs and we don't negotiate
    struct wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
    struct wl_listener                    new_xdg_decoration;

    // layer shell: the thing that bars and backgrounds need to not hate us
    struct wlr_layer_shell_v1      *layer_shell;
    struct wl_listener              new_layer_surface;
    struct wl_list                  layer_surfaces;

    // xdg-output because bars can figure out where the screen actually is
    struct wlr_xdg_output_manager_v1 *xdg_output_manager;

    // wlr-foreign-toplevel-management: lets bars/taskbars see window titles; doing otherwise is cringe and boring
    struct wlr_foreign_toplevel_manager_v1 *foreign_toplevel_manager;

    struct wlr_cursor              *cursor;
    struct wlr_xcursor_manager     *cursor_mgr;
    struct wl_listener              cursor_motion;
    struct wl_listener              cursor_motion_absolute;
    struct wl_listener              cursor_button;
    struct wl_listener              cursor_axis;
    struct wl_listener              cursor_frame;

    struct wlr_seat                *seat;
    struct wl_listener              new_input;
    struct wl_listener              request_cursor;
    struct wl_listener              pointer_focus_change;
    struct wl_listener              request_set_selection;
    struct wl_list                  keyboards;

    // canvas viewport offset
    int                             vx, vy;

    // canvas zoom: 1.0 native and <1.0 when zoomed out (duh)
    float                           zoom;

    // Super + Shift + Right Mouse Drag state
    int                             panning;
    double                          pan_start_x, pan_start_y;
    int                             pan_origin_vx, pan_origin_vy;

    // Interactive move grab state
    slide_grab_mode                 grab_mode;
    struct slide_toplevel          *grabbed;
    double                          grab_x, grab_y;        // cursor offset within window

    // resize grab state
    double                          grab_orig_cursor_x, grab_orig_cursor_y;
    unsigned int                    grab_orig_w, grab_orig_h;

    // Currently focused toplevel
    struct slide_toplevel          *focused;

    /* Non-NULL when a layer surface (e.g. wofi/rofi) owns keyboard focus.
     Points to the slide_layer_surface that stole it. */
    void                           *exclusive_focus;

    struct wlr_output_layout       *output_layout;
    struct wl_list                  outputs;
    struct wl_listener              new_output;

    // primary output dimensions
    int                             sw, sh;

    struct wlr_scene_tree          *layer_tree[4];
    struct wlr_scene_tree          *toplevel_tree;
};

struct slide_output {
    struct wl_list          link;
    struct slide_server    *server;
    struct wlr_output      *wlr_output;
    struct wl_listener      frame;
    struct wl_listener      request_state;
    struct wl_listener      destroy;
};

struct slide_toplevel {
    struct wl_list            link;
    struct slide_server      *server;
    struct wlr_xdg_toplevel  *xdg_toplevel;
    struct wlr_scene_tree    *scene_tree;

    // foreign toplevel handle — lets waybar et al. know we exist
    struct wlr_foreign_toplevel_handle_v1 *foreign_handle;

    struct wlr_xdg_toplevel_decoration_v1 *decoration;
    struct slide_decoration               *slide_deco; // our wrapper, so destroy can null d->toplevel

    int          cx, cy;      // canvas-space position
    int          wx, wy;      // saved canvas pos for fullscreen restore
    unsigned int ww, wh;      // saved size for fullscreen restore
    int          fullscreen;

    slide_anim   anim;        // open/close animation state

    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener commit;
    struct wl_listener destroy;
    struct wl_listener request_maximize;
    struct wl_listener request_fullscreen;
};

struct slide_popup {
    struct wlr_xdg_popup  *xdg_popup;
    struct wl_listener     commit;
    struct wl_listener     destroy;
};

struct slide_keyboard {
    struct wl_list       link;
    struct slide_server *server;
    struct wlr_keyboard *wlr_keyboard;
    struct wl_listener   modifiers;
    struct wl_listener   key;
    struct wl_listener   destroy;
};

// layer surface lives here
struct slide_layer_surface {
    struct wl_list                    link;
    struct slide_server              *server;
    struct slide_output              *output;
    struct wlr_layer_surface_v1      *wlr_layer_surface;
    struct wlr_scene_layer_surface_v1 *scene_layer;

    struct wl_listener                map;
    struct wl_listener                unmap;
    struct wl_listener                commit;
    struct wl_listener                destroy;
};

// Functions called from config.h keybinds
void run(const Arg arg);
void win_kill(const Arg arg);
void win_center(const Arg arg);
void win_fs(const Arg arg);
void win_cycle(const Arg arg);
void win_move(const Arg arg);
void pan_by_key(const Arg arg);
void slide_quit(const Arg arg);
void canvas_zoom(const Arg arg);

#endif /* SLIDE_H */
