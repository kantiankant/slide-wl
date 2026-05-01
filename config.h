#ifndef CONFIG_H
#define CONFIG_H

 //XF86 keysyms are the same names in xkbcommon 
#include <xkbcommon/xkbcommon-keysyms.h>

// Remap X11 keysym prefix to xkbcommon prefix 
#define XK_q          XKB_KEY_q
#define XK_e          XKB_KEY_e
#define XK_r          XKB_KEY_r
#define XK_w          XKB_KEY_w
#define XK_c          XKB_KEY_c
#define XK_f          XKB_KEY_f
#define XK_h          XKB_KEY_h
#define XK_l          XKB_KEY_l
#define XK_k          XKB_KEY_k
#define XK_j          XKB_KEY_j
#define XK_space      XKB_KEY_space
#define XK_Print      XKB_KEY_Print
#define XK_minus      XKB_KEY_minus
#define XK_equal      XKB_KEY_equal
#define XF86XK_AudioRaiseVolume  XKB_KEY_XF86AudioRaiseVolume
#define XF86XK_AudioLowerVolume  XKB_KEY_XF86AudioLowerVolume
#define XF86XK_AudioMute         XKB_KEY_XF86AudioMute
#define XF86XK_MonBrightnessUp   XKB_KEY_XF86MonBrightnessUp
#define XF86XK_MonBrightnessDown XKB_KEY_XF86MonBrightnessDown

// Animation
#define ANIM_DURATION    0.25f
#define ANIM_SCALE_FROM  0.85f
#define ANIM_BEZ_P1X     0.2f
#define ANIM_BEZ_P1Y     0.8f
#define ANIM_BEZ_P2X     0.2f
#define ANIM_BEZ_P2Y     1.0f

// Keyboard repeat
#define KB_REPEAT_RATE   25
#define KB_REPEAT_DELAY  250 

#define MOD Mod4Mask
#define WIN_MOVE_STEP 60
#define PAN_STEP 120

static const char *term[] = { "foot", NULL };
static const char *menu[] = { "wofi", "--show", "drun", NULL };


static const char *vol_up[]   = { "osd-handler", "vol-up", NULL };
static const char *vol_down[] = { "osd-handler", "vol-down", NULL };
static const char *vol_mute[] = { "osd-handler", "vol-mute", NULL };
static const char *bri_up[]   = { "osd-handler", "bri-up", NULL };
static const char *bri_down[] = { "osd-handler", "bri-down", NULL };

// lmao screenshots don't work (nvm they do now)
static const char *shot[] = { "sh", "-c", "grim -g \"$(slurp)\" ~/Screenshots/$(date +%Y-%m-%d_%H-%M-%S).png", NULL };

static const char *file[] = { "foot", "-e", "yazi",    NULL };
static const char *kew[]  = { "foot", "-e", "kew",     NULL };
static const char *ani[]  = { "foot", "-e", "ani-cli", NULL };

static key keys[] = {
    { MOD,             XK_q,                    run,        {.com = term}     },
    { MOD,             XK_e,                    run,        {.com = file}     },
    { MOD,             XK_r,                    run,        {.com = kew}      },
    { MOD|ShiftMask,   XK_r,                    run,        {.com = ani}      },
    { MOD,             XK_space,                run,        {.com = menu}     },
    { MOD,             XK_w,                    win_kill,   {0}               },
    { MOD,             XK_c,                    win_center, {0}               },
    { MOD,             XK_f,                    win_fs,     {0}               },
    { MOD,             XK_h,                    pan_by_key, {.i = 0}          },
    { MOD,             XK_l,                    pan_by_key, {.i = 1}          },
    { MOD,             XK_k,                    pan_by_key, {.i = 2}          },
    { MOD,             XK_j,                    pan_by_key, {.i = 3}          },
    { MOD|ShiftMask,   XK_h,                    win_move,   {.i = 0}          },
    { MOD|ShiftMask,   XK_l,                    win_move,   {.i = 1}          },
    { MOD|ShiftMask,   XK_k,                    win_move,   {.i = 2}          },
    { MOD|ShiftMask,   XK_j,                    win_move,   {.i = 3}          },
    { MOD|ShiftMask,   XK_e,                    slide_quit, {0}               },
    { MOD|ControlMask, XK_l,                    win_cycle,  {.i = 1}          },
    { MOD|ControlMask, XK_h,                    win_cycle,  {.i = 0}          },
    { MOD,             XK_minus,                canvas_zoom,{.f = 0.9f}       },
    { MOD,             XK_equal,                canvas_zoom,{.f = 1.0f/0.9f}  },
    { 0,               XF86XK_AudioRaiseVolume, run,        {.com = vol_up}   },
    { 0,               XF86XK_AudioLowerVolume, run,        {.com = vol_down} },
    { 0,               XF86XK_MonBrightnessUp,  run,        {.com = bri_up}   },
    { 0,               XF86XK_MonBrightnessDown,run,        {.com = bri_down} },
    { 0,               XF86XK_AudioMute,        run,        {.com = vol_mute} },
    { 0,               XK_Print,                run,        {.com = shot}     },
};

#endif /* CONFIG_H */
