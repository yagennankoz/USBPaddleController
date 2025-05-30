#pragma once

#include <Arduino.h>

#define PIN_S1      D27
#define PIN_S2      D26

#define PIN_BTN1    D19
#define PIN_BTN2    D18

#define DIR_N       (0u)
#define DIR_RIGHT   (1u)
#define DIR_LEFT    (2u)

#define MOUSE_STEP      (6u)
#define MOUSE_STEP_SLOW (1u)

#define INTERVAL      (200000u)
#define INTERVAL_BTN  (10000u)

#define MY_TUD_HID_REPORT_DESC_MOUSE(...) \
    HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP     )                 ,\
    HID_USAGE      ( HID_USAGE_DESKTOP_MOUSE    )                 ,\
    HID_COLLECTION ( HID_COLLECTION_APPLICATION )                 ,\
        /* Report ID if any */\
        __VA_ARGS__ \
        HID_USAGE          ( HID_USAGE_DESKTOP_POINTER              ) ,\
        HID_COLLECTION     ( HID_COLLECTION_PHYSICAL                ) ,\
        /* Button Map */ \
        HID_USAGE_PAGE     ( HID_USAGE_PAGE_BUTTON                  ) ,\
        HID_USAGE_MIN      ( 1                                      ) ,\
        HID_USAGE_MAX      ( 2                                      ) ,\
        HID_LOGICAL_MIN    ( 0x00                                   ) ,\
        HID_LOGICAL_MAX    ( 0x01                                   ) ,\
        HID_REPORT_COUNT   ( 2                                      ) ,\
        HID_REPORT_SIZE    ( 1                                      ) ,\
        HID_INPUT          ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
        HID_REPORT_COUNT   ( 1                                      ) ,\
        HID_REPORT_SIZE    ( 6                                      ) ,\
        HID_INPUT          ( HID_CONSTANT | HID_ARRAY | HID_ABSOLUTE ) ,\
        /* X, Y */ \
        HID_USAGE_PAGE     ( HID_USAGE_PAGE_DESKTOP                 ) ,\
        HID_USAGE          ( HID_USAGE_DESKTOP_X                    ) ,\
        HID_USAGE          ( HID_USAGE_DESKTOP_Y                    ) ,\
        HID_LOGICAL_MIN    ( 0x81                                   ) ,\
        HID_LOGICAL_MAX    ( 0x7f                                   ) ,\
        HID_REPORT_SIZE    ( 8                                      ) ,\
        HID_REPORT_COUNT   ( 2                                      ) ,\
        HID_INPUT          ( HID_DATA | HID_VARIABLE | HID_RELATIVE ) ,\
    HID_COLLECTION_END ,\
HID_COLLECTION_END

typedef struct MY_TU_ATTR_PACKED
{
  uint8_t   buttons;      ///< Buttons mask for currently pressed buttons
  int8_t    x;            ///< Delta x  movement of left analog-stick
  int8_t    y;            ///< Delta y  movement of left analog-stick
}my_hid_mouse_report_t;
