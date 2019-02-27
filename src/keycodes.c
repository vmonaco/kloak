#include <string.h>
#include <linux/input.h>
#include "keycodes.h"

struct name_value {
    const char *name;
    const int value;
};

static struct name_value key_table[] = {
        {"KEY_ESC", KEY_ESC},
        {"KEY_1", KEY_1},
        {"KEY_2", KEY_2},
        {"KEY_3", KEY_3},
        {"KEY_4", KEY_4},
        {"KEY_5", KEY_5},
        {"KEY_6", KEY_6},
        {"KEY_7", KEY_7},
        {"KEY_8", KEY_8},
        {"KEY_9", KEY_9},
        {"KEY_0", KEY_0},
        {"KEY_MINUS", KEY_MINUS},
        {"KEY_EQUAL", KEY_EQUAL},
        {"KEY_BACKSPACE", KEY_BACKSPACE},
        {"KEY_TAB", KEY_TAB},
        {"KEY_Q", KEY_Q},
        {"KEY_W", KEY_W},
        {"KEY_E", KEY_E},
        {"KEY_R", KEY_R},
        {"KEY_T", KEY_T},
        {"KEY_Y", KEY_Y},
        {"KEY_U", KEY_U},
        {"KEY_I", KEY_I},
        {"KEY_O", KEY_O},
        {"KEY_P", KEY_P},
        {"KEY_LEFTBRACE", KEY_LEFTBRACE},
        {"KEY_RIGHTBRACE", KEY_RIGHTBRACE},
        {"KEY_ENTER", KEY_ENTER},
        {"KEY_LEFTCTRL", KEY_LEFTCTRL},
        {"KEY_A", KEY_A},
        {"KEY_S", KEY_S},
        {"KEY_D", KEY_D},
        {"KEY_F", KEY_F},
        {"KEY_G", KEY_G},
        {"KEY_H", KEY_H},
        {"KEY_J", KEY_J},
        {"KEY_K", KEY_K},
        {"KEY_L", KEY_L},
        {"KEY_SEMICOLON", KEY_SEMICOLON},
        {"KEY_APOSTROPHE", KEY_APOSTROPHE},
        {"KEY_GRAVE", KEY_GRAVE},
        {"KEY_LEFTSHIFT", KEY_LEFTSHIFT},
        {"KEY_BACKSLASH", KEY_BACKSLASH},
        {"KEY_Z", KEY_Z},
        {"KEY_X", KEY_X},
        {"KEY_C", KEY_C},
        {"KEY_V", KEY_V},
        {"KEY_B", KEY_B},
        {"KEY_N", KEY_N},
        {"KEY_M", KEY_M},
        {"KEY_COMMA", KEY_COMMA},
        {"KEY_DOT", KEY_DOT},
        {"KEY_SLASH", KEY_SLASH},
        {"KEY_RIGHTSHIFT", KEY_RIGHTSHIFT},
        {"KEY_KPASTERISK", KEY_KPASTERISK},
        {"KEY_LEFTALT", KEY_LEFTALT},
        {"KEY_SPACE", KEY_SPACE},
        {"KEY_CAPSLOCK", KEY_CAPSLOCK},
        {"KEY_F1", KEY_F1},
        {"KEY_F2", KEY_F2},
        {"KEY_F3", KEY_F3},
        {"KEY_F4", KEY_F4},
        {"KEY_F5", KEY_F5},
        {"KEY_F6", KEY_F6},
        {"KEY_F7", KEY_F7},
        {"KEY_F8", KEY_F8},
        {"KEY_F9", KEY_F9},
        {"KEY_F10", KEY_F10},
        {"KEY_NUMLOCK", KEY_NUMLOCK},
        {"KEY_SCROLLLOCK", KEY_SCROLLLOCK},
        {"KEY_KP7", KEY_KP7},
        {"KEY_KP8", KEY_KP8},
        {"KEY_KP9", KEY_KP9},
        {"KEY_KPMINUS", KEY_KPMINUS},
        {"KEY_KP4", KEY_KP4},
        {"KEY_KP5", KEY_KP5},
        {"KEY_KP6", KEY_KP6},
        {"KEY_KPPLUS", KEY_KPPLUS},
        {"KEY_KP1", KEY_KP1},
        {"KEY_KP2", KEY_KP2},
        {"KEY_KP3", KEY_KP3},
        {"KEY_KP0", KEY_KP0},
        {"KEY_KPDOT", KEY_KPDOT},

        {"KEY_ZENKAKUHANKAKU", KEY_ZENKAKUHANKAKU},
        {"KEY_102ND", KEY_102ND},
        {"KEY_F11", KEY_F11},
        {"KEY_F12", KEY_F12},
        {"KEY_RO", KEY_RO},
        {"KEY_KATAKANA", KEY_KATAKANA},
        {"KEY_HIRAGANA", KEY_HIRAGANA},
        {"KEY_HENKAN", KEY_HENKAN},
        {"KEY_KATAKANAHIRAGANA", KEY_KATAKANAHIRAGANA},
        {"KEY_MUHENKAN", KEY_MUHENKAN},
        {"KEY_KPJPCOMMA", KEY_KPJPCOMMA},
        {"KEY_KPENTER", KEY_KPENTER},
        {"KEY_RIGHTCTRL", KEY_RIGHTCTRL},
        {"KEY_KPSLASH", KEY_KPSLASH},
        {"KEY_SYSRQ", KEY_SYSRQ},
        {"KEY_RIGHTALT", KEY_RIGHTALT},
        {"KEY_LINEFEED", KEY_LINEFEED},
        {"KEY_HOME", KEY_HOME},
        {"KEY_UP", KEY_UP},
        {"KEY_PAGEUP", KEY_PAGEUP},
        {"KEY_LEFT", KEY_LEFT},
        {"KEY_RIGHT", KEY_RIGHT},
        {"KEY_END", KEY_END},
        {"KEY_DOWN", KEY_DOWN},
        {"KEY_PAGEDOWN", KEY_PAGEDOWN},
        {"KEY_INSERT", KEY_INSERT},
        {"KEY_DELETE", KEY_DELETE},
        {"KEY_MACRO", KEY_MACRO},
        {"KEY_MUTE", KEY_MUTE},
        {"KEY_VOLUMEDOWN", KEY_VOLUMEDOWN},
        {"KEY_VOLUMEUP", KEY_VOLUMEUP},
        {"KEY_POWER", KEY_POWER},
        {"KEY_KPEQUAL", KEY_KPEQUAL},
        {"KEY_KPPLUSMINUS", KEY_KPPLUSMINUS},
        {"KEY_PAUSE", KEY_PAUSE},
        {"KEY_SCALE", KEY_SCALE},

        {"KEY_KPCOMMA", KEY_KPCOMMA},
        {"KEY_HANGEUL", KEY_HANGEUL},
        {"KEY_HANGUEL", KEY_HANGUEL},
        {"KEY_HANJA", KEY_HANJA},
        {"KEY_YEN", KEY_YEN},
        {"KEY_LEFTMETA", KEY_LEFTMETA},
        {"KEY_RIGHTMETA", KEY_RIGHTMETA},
        {"KEY_COMPOSE", KEY_COMPOSE},

        {"KEY_F13", KEY_F13},
        {"KEY_F14", KEY_F14},
        {"KEY_F15", KEY_F15},
        {"KEY_F16", KEY_F16},
        {"KEY_F17", KEY_F17},
        {"KEY_F18", KEY_F18},
        {"KEY_F19", KEY_F19},
        {"KEY_F20", KEY_F20},
        {"KEY_F21", KEY_F21},
        {"KEY_F22", KEY_F22},
        {"KEY_F23", KEY_F23},
        {"KEY_F24", KEY_F24},

        {"KEY_UNKNOWN", KEY_UNKNOWN},

        {NULL, 0}
};

int lookup_keycode(const char *name) {
    struct name_value *p;
    for (p = key_table; p->name != NULL; ++p) {
        if (strcmp(p->name, name) == 0) {
            return p->value;
        }
    }
    return -1;
}

const char *lookup_keyname(const int code) {
    struct name_value *p;
    for (p = key_table; p->name != NULL; ++p) {
        if (code == p->value) {
            return p->name;
        }
    }
    return "KEY_UNKNOWN";
}
