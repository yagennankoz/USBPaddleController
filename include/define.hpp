#pragma once

#include <Arduino.h>

#ifdef BOARD_PICO
  #define PIN_S1      D27
  #define PIN_S2      D26

  #define PIN_BTN1    D19
  #define PIN_BTN2    D18
  #define PIN_BTN3    D15
  #define PIN_BTNSPD  D17
  #define PIN_BTNAXIS D16

  #ifndef PIN_LED
    #define PIN_LED   D25
  #endif
#endif

#ifdef BOARD_RP2040_ZERO
  #define PIN_S1      D27
  #define PIN_S2      D26

  #define PIN_BTN1    D13
  #define PIN_BTN2    D12
  #define PIN_BTN3    D11
  #define PIN_BTNSPD  D10
  #define PIN_BTNAXIS D9

  #ifdef PIN_LED
    #undef PIN_LED
  #endif
  #define PIN_LED     D16
#endif

#define DIR_N       (0u)
#define DIR_RIGHT   (1u)
#define DIR_LEFT    (2u)

#define INTERVAL      (1000u)
#define INTERVAL_BTN  (10000u)

#define OUT_PAD_SQUARE (1u << 0)
#define OUT_PAD_CROSS (1u << 1)
#define OUT_PAD_CIRCLE (1u << 2)
#define OUT_PAD_TRIANGLE (1u << 3)
#define OUT_PAD_L2 (1u << 6)
#define OUT_PAD_R2 (1u << 7)
#define OUT_PAD_PS (1u << 12)

typedef struct
{
  uint16_t vid;
  uint16_t pid;
  const char *manufacturer;
  const char *product;
} UsbIdentityProfile;

// USB VID/PID profile IDs used by keyAssign[].usbProfile.
enum
{
  USB_ID_PROFILE_MOUSE = 0,
  USB_ID_PROFILE_GAMEPAD,
  USB_ID_PROFILE_MAX
};

// USB identity table indexed by USB_ID_PROFILE_*.
const UsbIdentityProfile usbIdentityProfiles[USB_ID_PROFILE_MAX] = {
    {0x2E8A, 0x0005, "USB PADDLE CONTROLLER", "PADDLE"},
    {0x2E8A, 0x0004, "USB PADDLE CONTROLLER", "POLE POSITION PADDLE"},
};

#define MY_TUD_HID_REPORT_DESC_GAMEPAD(...)                           \
    HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                                 \
    HID_USAGE(HID_USAGE_DESKTOP_GAMEPAD),                               \
    HID_COLLECTION(HID_COLLECTION_APPLICATION), /* Report ID if any */  \
      __VA_ARGS__                                                         \
          HID_LOGICAL_MIN(0),                                             \
      HID_LOGICAL_MAX(1),                                                 \
      HID_PHYSICAL_MIN(0),                                                \
      HID_PHYSICAL_MAX(1),                                                \
      HID_REPORT_SIZE(1),                                                 \
      HID_REPORT_COUNT(13),                                               \
      HID_USAGE_PAGE(HID_USAGE_PAGE_BUTTON),                              \
      HID_USAGE_MIN(1),                                                   \
      HID_USAGE_MAX(13),                                                  \
      HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                  \
      HID_REPORT_COUNT(3),                                                \
      HID_INPUT(HID_CONSTANT | HID_ARRAY | HID_ABSOLUTE),                 \
      HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                             \
      HID_LOGICAL_MAX(7),                                                 \
      HID_PHYSICAL_MAX_N(315, 2),                                         \
      HID_REPORT_SIZE(4),                                                 \
      HID_REPORT_COUNT(1),                                                \
      HID_UNIT(0x14),                                                     \
      HID_USAGE(HID_USAGE_DESKTOP_HAT_SWITCH),                            \
      HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE | HID_NULL_STATE), \
      HID_UNIT(0),                                                        \
      HID_REPORT_COUNT(1),                                                \
      HID_INPUT(HID_CONSTANT | HID_ARRAY | HID_ABSOLUTE),                 \
      HID_LOGICAL_MAX_N(0xff, 2),                                         \
      HID_PHYSICAL_MAX_N(0xff, 2),                                        \
      HID_USAGE(HID_USAGE_DESKTOP_X),                                     \
      HID_USAGE(HID_USAGE_DESKTOP_Y),                                     \
      HID_USAGE(HID_USAGE_DESKTOP_Z),                                     \
      HID_USAGE(HID_USAGE_DESKTOP_RZ),                                    \
      HID_REPORT_SIZE(8),                                                 \
      HID_REPORT_COUNT(4),                                                \
      HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                  \
      HID_USAGE_PAGE_N(0xFF00, 2),                                        \
      HID_USAGE(0x20),                                                    \
      HID_USAGE(0x21),                                                    \
      HID_USAGE(0x22),                                                    \
      HID_USAGE(0x23),                                                    \
      HID_USAGE(0x24),                                                    \
      HID_USAGE(0x25),                                                    \
      HID_USAGE(0x26),                                                    \
      HID_USAGE(0x27),                                                    \
      HID_USAGE(0x28),                                                    \
      HID_USAGE(0x29),                                                    \
      HID_USAGE(0x2A),                                                    \
      HID_USAGE(0x2B),                                                    \
      HID_REPORT_COUNT(12),                                               \
      HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                  \
      HID_USAGE_N(0x2126, 2),                                             \
      HID_REPORT_COUNT(8),                                                \
      HID_FEATURE(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                \
      HID_USAGE_N(0x2126, 2),                                             \
      HID_OUTPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                 \
      HID_LOGICAL_MAX_N(1023, 2),                                         \
      HID_PHYSICAL_MAX_N(1023, 2),                                        \
      HID_USAGE(0x2C),                                                    \
      HID_USAGE(0x2D),                                                    \
      HID_USAGE(0x2E),                                                    \
      HID_USAGE(0x2F),                                                    \
      HID_REPORT_SIZE(16),                                                \
      HID_REPORT_COUNT(4),                                                \
      HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                  \
    HID_COLLECTION_END


typedef struct __attribute__((packed))
{
  uint16_t buttons;       ///< 13 buttons + 3 padding bits
  uint8_t hat;            ///< Hat switch + 4 padding bits
  uint8_t x;              ///< Gamepad X axis (0-255)
  uint8_t y;              ///< Gamepad Y axis (0-255)
  uint8_t z;              ///< Gamepad Z axis (0-255)
  uint8_t rz;             ///< Gamepad RZ axis (0-255)
  uint8_t vendorInput[12];///< Vendor-defined input bytes
  uint16_t analog[4];     ///< Additional 16-bit analog inputs
} joykey_hid_gamepad_report_t;

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
            HID_USAGE_MAX      ( 3                                      ) ,\
            HID_LOGICAL_MIN    ( 0x00                                   ) ,\
            HID_LOGICAL_MAX    ( 0x01                                   ) ,\
            HID_REPORT_COUNT   ( 3                                      ) ,\
            HID_REPORT_SIZE    ( 1                                      ) ,\
            HID_INPUT          ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
            HID_REPORT_COUNT   ( 1                                      ) ,\
            HID_REPORT_SIZE    ( 5                                      ) ,\
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

typedef struct __attribute__((packed))
{
  uint8_t   buttons;      ///< Buttons mask for currently pressed buttons
  int8_t    x;            ///< Delta x  movement of left analog-stick
  int8_t    y;            ///< Delta y  movement of left analog-stick
}my_hid_mouse_report_t;

const int8_t mouseStepTable[] = {8, 6, 4, 2};

const unsigned long AUTO_FIRE_INTERVAL = 50000u; // 連射インターバル (マイクロ秒)
const unsigned long GAMEPAD_R2_HOLD_TRIGGER_US = 3000000u; // R2長押し判定時間 (マイクロ秒)
const unsigned long GAMEPAD_L2_PULSE_INTERVAL_US = 50000u; // L2押下間隔 (マイクロ秒)
const unsigned long GAMEPAD_L2_PULSE_WIDTH_US = 15000u; // L2の1回押下幅 (マイクロ秒)
const unsigned long GAMEPAD_L2_BURST_DURATION_US = 900000u; // L2自動押下継続時間 (マイクロ秒)

const float GAMEPAD_MAGNIFIER_MAX = 0.8f;
const uint16_t GAMEPAD_MAGNIFIER_THRESHOLD = 16;

const unsigned long REPORT_INTERVAL_US = 1000u;
const int EEPROM_SIZE_BYTES = 64;
const uint8_t GAMEPAD_HAT_CENTER = 8;

const unsigned long STARTUP_LED_TOTAL_MS = 5000u;
const unsigned long STARTUP_LED_BLINK_HALF_MS = 500u;
const unsigned long ACTIVITY_LED_ON_MS = 70u;
const unsigned long ACTIVITY_LED_OFF_MS = 140u;
const unsigned long SPEED_LED_ON_MS = 90u;
const unsigned long SPEED_LED_OFF_MS = 180u;
#if defined(BOARD_RP2040_ZERO)
const uint8_t STARTUP_LED_BRIGHTNESS = 24;
const uint8_t ACTIVITY_LED_BRIGHTNESS = 40;
#endif

const uint8_t SETTINGS_MAGIC = 0xA5;
const uint8_t SETTINGS_VERSION = 2;

