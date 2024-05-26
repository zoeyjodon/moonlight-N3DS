#ifndef KEYCODE_MAP_HPP
#define KEYCODE_MAP_HPP

#include <3ds.h>
#include <map>
#include <vector>

struct key_boundary {
    u16 max_x;
    int key_idx;
};

struct row_boundary {
    u16 max_y;
    std::vector<key_boundary> keys;
};

static std::vector<row_boundary> key_boundaries{
    {37,
     {
         {64, 0},
         {128, 1},
         {192, 2},
         {256, 3},
         {GSP_SCREEN_HEIGHT_BOTTOM, 4},
     }},
    {70,
     {
         {32, 5},
         {64, 6},
         {96, 7},
         {128, 8},
         {160, 9},
         {192, 10},
         {224, 11},
         {256, 12},
         {288, 13},
         {GSP_SCREEN_HEIGHT_BOTTOM, 14},
     }},
    {103,
     {
         {32, 15},
         {64, 16},
         {96, 17},
         {128, 18},
         {160, 19},
         {192, 20},
         {224, 21},
         {256, 22},
         {288, 23},
         {GSP_SCREEN_HEIGHT_BOTTOM, 24},
     }},
    {136,
     {
         {16, -1},
         {48, 25},
         {80, 26},
         {112, 27},
         {144, 28},
         {176, 29},
         {208, 30},
         {240, 31},
         {272, 32},
         {304, 33},
         {GSP_SCREEN_HEIGHT_BOTTOM, -1},
     }},
    {169,
     {
         {48, 34},
         {80, 35},
         {112, 36},
         {144, 37},
         {176, 38},
         {208, 39},
         {240, 40},
         {272, 41},
         {GSP_SCREEN_HEIGHT_BOTTOM, 42},
     }},
    {202,
     {
         {64, 43},
         {96, 44},
         {224, 45},
         {256, 46},
         {GSP_SCREEN_HEIGHT_BOTTOM, 47},
     }},
};

struct keycode_info {
    short code;
    bool require_shift = false;
};

const short KEYBOARD_SWITCH_KC = 0;
const short SHIFT_KC = 0xA0;
const short CTRL_KC = 0xA2;
const short ALT_KC = 0xA4;
static std::map<int, keycode_info> default_keycodes{
    {0, {0x1B, false}},                // VK_ESCAPE
    {1, {CTRL_KC, false}},             // VK_CONTROL Left control
    {2, {ALT_KC, false}},              // VK_ALT Left alt
    {3, {0x09, false}},                // VK_TAB
    {4, {0x2E, false}},                // VK_DELETE
    {5, {0x31, false}},                // VK_1
    {6, {0x32, false}},                // VK_2
    {7, {0x33, false}},                // VK_3
    {8, {0x34, false}},                // VK_4
    {9, {0x35, false}},                // VK_5
    {10, {0x36, false}},               // VK_6
    {11, {0x37, false}},               // VK_7
    {12, {0x38, false}},               // VK_8
    {13, {0x39, false}},               // VK_9
    {14, {0x30, false}},               // VK_0
    {15, {0x51, false}},               // VK_Q
    {16, {0x57, false}},               // VK_W
    {17, {0x45, false}},               // VK_E
    {18, {0x52, false}},               // VK_R
    {19, {0x54, false}},               // VK_T
    {20, {0x59, false}},               // VK_Y
    {21, {0x55, false}},               // VK_U
    {22, {0x49, false}},               // VK_I
    {23, {0x4F, false}},               // VK_O
    {24, {0x50, false}},               // VK_P
    {25, {0x41, false}},               // VK_A
    {26, {0x53, false}},               // VK_S
    {27, {0x44, false}},               // VK_D
    {28, {0x46, false}},               // VK_F
    {29, {0x47, false}},               // VK_G
    {30, {0x48, false}},               // VK_H
    {31, {0x4A, false}},               // VK_J
    {32, {0x4B, false}},               // VK_K
    {33, {0x4C, false}},               // VK_L
    {34, {SHIFT_KC, false}},           // VK_SHIFT Left shift
    {35, {0x5A, false}},               // VK_Z
    {36, {0x58, false}},               // VK_X
    {37, {0x43, false}},               // VK_C
    {38, {0x56, false}},               // VK_V
    {39, {0x42, false}},               // VK_B
    {40, {0x4E, false}},               // VK_N
    {41, {0x4D, false}},               // VK_M
    {42, {0x08, false}},               // VK_BACK_SPACE
    {43, {KEYBOARD_SWITCH_KC, false}}, // Special key -- keyboard switch
    {44, {0x5B, false}},               // KEY_LEFTMETA
    {45, {0x20, false}},               // VK_SPACE
    {46, {0xBE, false}},               // VK_DOT
    {47, {0x0D, false}},               // VK_ENTER
};

static std::map<int, keycode_info> alt_keycodes{
    {0, {0x1B, false}},    // VK_ESCAPE
    {1, {CTRL_KC, false}}, // VK_CONTROL Left control
    {2, {ALT_KC, false}},  // VK_ALT Left alt
    {3, {0x09, false}},    // VK_TAB
    {4, {0x2E, false}},    // VK_DELETE
    {5, {0x70, false}},    // VK_F1
    {6, {0x71, false}},    // VK_F2
    {7, {0x72, false}},    // VK_F3
    {8, {0x73, false}},    // VK_F4
    {9, {0x74, false}},    // VK_F5
    {10, {0x75, false}},   // VK_F6
    {11, {0x76, false}},   // VK_F7
    {12, {0x77, false}},   // VK_F8
    {13, {0x78, false}},   // VK_F9
    {14, {0x79, false}},   // VK_F10
    {15, {0xBB, true}},    // VK_EQUALS Note: can be = OR +
    {16, {0xBD, false}},   // VK_MINUS Note: can be - OR _
    {17, {0xBB, false}},   // VK_EQUALS Note: can be = OR +
    {18, {0xBD, true}},    // VK_MINUS Note: can be - OR _
    {19, {0xC0, false}},   // VK_GRAVE Note: can be ` OR ~
    {20, {0xDE, false}},   // VK_APOSTROPHE Note: can be ' OR "
    {21, {0xDE, true}},    // VK_APOSTROPHE Note: can be ' OR "
    {22, {0xBF, false}},   // VK_SLASH Note: can be / or ?
    {23, {0xDC, false}},   // VK_BACK_SLASH Note: can be \ or |
    {24, {0xDC, true}},    // VK_BACK_SLASH Note: can be \ or |
    {25, {0xDB, false}},   // VK_BRACELEFT Note: can be [ or {
    {26, {0xDD, false}},   // VK_BRACERIGHT Note: can be ] or }
    {27, {0xDB, true}},    // VK_BRACELEFT Note: can be [ or {
    {28, {0xDD, true}},    // VK_BRACERIGHT Note: can be ] or }
    {29, {0xBC, true}},    // VK_OEM_102 Note: can be , or <
    {30, {-1, false}},
    {31, {0x26, false}}, // VK_UP
    {32, {-1, false}},
    {33, {0xBE, true}},                // VK_OEM_102 Note: can be . or >
    {34, {SHIFT_KC, false}},           // VK_SHIFT Left shift
    {35, {0xBF, true}},                // VK_SLASH Note: can be / or ?
    {36, {0xBA, true}},                // VK_SEMICOLON Note: can be ; or :
    {37, {0xBA, false}},               // VK_SEMICOLON Note: can be ; or :
    {38, {0xC0, true}},                // VK_GRAVE Note: can be ` OR ~
    {39, {0x25, false}},               // VK_LEFT
    {40, {0x28, false}},               // VK_DOWN
    {41, {0x27, false}},               // VK_RIGHT
    {42, {0x08, false}},               // VK_BACK_SPACE
    {43, {KEYBOARD_SWITCH_KC, false}}, // Special key -- keyboard switch
    {44, {0x5B, false}},               // KEY_LEFTMETA
    {45, {0x20, false}},               // VK_SPACE
    {46, {0xBC, false}},               // VK_COMMA
    {47, {0x0D, false}},               // VK_ENTER
};

#endif
