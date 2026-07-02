#include <Arduino.h>
#include <hardware/gpio.h>
#include <hardware/pio.h>
#include <hardware/watchdog.h>
#include <pico/multicore.h>
#include <pico/critical_section.h>
#include <SPI.h>
#include <EEPROM.h>
#include <Adafruit_TinyUSB.h>
#if defined(BOARD_RP2040_ZERO)
#include <Adafruit_NeoPixel.h>
#endif
#include <define.hpp>
#include "s1s2_encoder.pio.h"

uint8_t const desc_hid_report_mouse[] =
{
    MY_TUD_HID_REPORT_DESC_MOUSE()
};
uint8_t const desc_hid_report_gamepad[] =
{
  MY_TUD_HID_REPORT_DESC_GAMEPAD()
};

enum DeviceMode : uint8_t {
  DEVICE_MODE_MOUSE = 0,
  DEVICE_MODE_GAMEPAD,
  DEVICE_MODE_MAX
};

Adafruit_USBD_HID usb_hid;
my_hid_mouse_report_t   mouse_report;
joykey_hid_gamepad_report_t gamepad_report;

bool axisAlt = false;
DeviceMode currentMode = DEVICE_MODE_MOUSE;

uint8_t speedIndexByMode[DEVICE_MODE_MAX] = {0};
uint8_t autoFireFlagsByMode[DEVICE_MODE_MAX] = {0};

uint8_t lastBtn1Sts;
uint8_t btn1Sts;
uint8_t lastBtn2Sts;
uint8_t btn2Sts;
uint8_t lastBtn3Sts;
uint8_t btn3Sts;
uint8_t lastBtnAxisSts;
uint8_t btnAxisSts;
uint8_t lastBtnSpdSts;
uint8_t btnSpdSts;
bool speedComboActive = false;
bool speedComboConsumed = false;
bool modeSwitchComboActive = false;

bool btn1AutoFire = false;
bool btn2AutoFire = false;
bool btn3AutoFire = false;
bool autoFireStateBtn1 = false;
bool autoFireStateBtn2 = false;
bool autoFireStateBtn3 = false;
unsigned long lastAutoFireBtn1 = 0;
unsigned long lastAutoFireBtn2 = 0;
unsigned long lastAutoFireBtn3 = 0;

unsigned long lastTimeBtn1 = micros();
unsigned long lastTimeBtn2 = micros();
unsigned long lastTimeBtn3 = micros();
unsigned long lastTimeBtnAxis = micros();
unsigned long lastTimeBtnSpd = micros();

int16_t cnt;
int16_t mouseStepIdx = 0;
int16_t mouseStep = mouseStepTable[mouseStepIdx];

PIO s1s2Pio = pio1;
uint s1s2Sm = 0;
uint s1s2ProgramOffset = 0;
uint8_t s1s2LastSts = 0;

critical_section_t reportStateCs;
critical_section_t settingsSaveCs;
critical_section_t modeSwitchCs;
critical_section_t modeStateCs;
critical_section_t ledBlinkCs;
critical_section_t speedBlinkCs;
volatile uint8_t sharedButtons = 0;
volatile bool sharedAxisAlt = false;
volatile bool settingsSaveRequested = false;
volatile bool modeSwitchRequested = false;
volatile uint8_t sharedMode = DEVICE_MODE_MOUSE;
volatile bool activityLedBlinkRequested = false;
volatile uint8_t speedLedBlinkRequestedCount = 0;

bool startupLedActive = false;
uint8_t startupLedMode = DEVICE_MODE_MOUSE;
unsigned long startupLedStartMs = 0;
bool startupLedLastOn = false;
bool activityLedBlinkActive = false;
unsigned long activityLedBlinkUntilMs = 0;
unsigned long activityLedNextAllowedMs = 0;
bool speedLedBlinkActive = false;
bool speedLedBlinkOn = false;
uint8_t speedLedBlinkRemaining = 0;
unsigned long speedLedBlinkNextToggleMs = 0;

#if defined(BOARD_RP2040_ZERO)
Adafruit_NeoPixel statusLed(1, PIN_LED, NEO_GRB + NEO_KHZ800);
#endif

struct DeviceSettings {
  uint8_t magic;
  uint8_t version;
  uint8_t currentMode;
  uint8_t speedIndexByMode[DEVICE_MODE_MAX];
  uint8_t autoFireFlagsByMode[DEVICE_MODE_MAX];
  uint8_t mode;
  uint8_t reserved;
  uint8_t checksum;
};

static uint8_t const featureReport[8] = {0x21, 0x26, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00};
static uint8_t lastOutputReport[8] = {0};

uint16_t hidPadGetReportCallback(uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
  (void)report_id;

  if (report_type == HID_REPORT_TYPE_FEATURE)
  {
    uint16_t len = (reqlen < sizeof(featureReport)) ? reqlen : (uint16_t)sizeof(featureReport);
    memcpy(buffer, featureReport, len);
    return len;
  }

  return 0;
}

void hidPadSetReportCallback(uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
  (void)report_id;

  if ((report_type == HID_REPORT_TYPE_OUTPUT || report_type == HID_REPORT_TYPE_FEATURE) && bufsize)
  {
    uint16_t len = (bufsize < sizeof(lastOutputReport)) ? bufsize : (uint16_t)sizeof(lastOutputReport);
    memcpy(lastOutputReport, buffer, len);
  }
}

uint8_t calcSettingsChecksum(const DeviceSettings& settings) {
  uint8_t checksum = settings.magic ^ settings.version ^ settings.currentMode ^ settings.mode ^ settings.reserved;
  for (uint8_t i = 0; i < DEVICE_MODE_MAX; ++i) {
    checksum ^= settings.speedIndexByMode[i];
    checksum ^= settings.autoFireFlagsByMode[i];
  }
  return checksum;
}

uint8_t getRuntimeAutoFireFlags() {
  return (uint8_t)((btn1AutoFire ? 0x01 : 0x00) |
                   (btn2AutoFire ? 0x02 : 0x00) |
                   (btn3AutoFire ? 0x04 : 0x00));
}

void applyRuntimeAutoFireFlags(uint8_t flags) {
  btn1AutoFire = (flags & 0x01) != 0;
  btn2AutoFire = (flags & 0x02) != 0;
  btn3AutoFire = (flags & 0x04) != 0;
}

void syncRuntimeSettingsToModeCache(DeviceMode mode) {
  if (mode >= DEVICE_MODE_MAX) {
    return;
  }

  uint8_t speedIndex = (mouseStepIdx >= 0) ? (uint8_t)mouseStepIdx : 0;
  if (speedIndex >= sizeof(mouseStepTable)) {
    speedIndex = 0;
  }

  speedIndexByMode[mode] = speedIndex;
  autoFireFlagsByMode[mode] = getRuntimeAutoFireFlags();
}

void applyModeCacheToRuntimeSettings(DeviceMode mode) {
  if (mode >= DEVICE_MODE_MAX) {
    return;
  }

  uint8_t speedIndex = speedIndexByMode[mode];
  if (speedIndex >= sizeof(mouseStepTable)) {
    speedIndex = 0;
  }

  mouseStepIdx = speedIndex;
  mouseStep = (currentMode == DEVICE_MODE_MOUSE) ? mouseStepTable[mouseStepIdx] : mouseStepTable[0] * 4;
  applyRuntimeAutoFireFlags(autoFireFlagsByMode[mode]);
}

void saveSettingsToNand() {
  syncRuntimeSettingsToModeCache(currentMode);

  DeviceSettings settings = {
    SETTINGS_MAGIC,
    SETTINGS_VERSION,
    (uint8_t)currentMode,
    {0},
    {0},
    (uint8_t)currentMode,
    0,
    0
  };
  for (uint8_t i = 0; i < DEVICE_MODE_MAX; ++i) {
    settings.speedIndexByMode[i] = speedIndexByMode[i];
    settings.autoFireFlagsByMode[i] = autoFireFlagsByMode[i];
  }
  settings.checksum = calcSettingsChecksum(settings);

  // RP2040はフラッシュ書き込み中に他コア実行が干渉すると停止することがあるため、
  // 保存中だけもう一方のコアをロックアウトする。
  multicore_lockout_start_blocking();
  EEPROM.put(0, settings);
  EEPROM.commit();
  multicore_lockout_end_blocking();
}

void requestSettingsSave() {
  critical_section_enter_blocking(&settingsSaveCs);
  settingsSaveRequested = true;
  critical_section_exit(&settingsSaveCs);
}

bool consumeSettingsSaveRequest() {
  bool requested;
  critical_section_enter_blocking(&settingsSaveCs);
  requested = settingsSaveRequested;
  settingsSaveRequested = false;
  critical_section_exit(&settingsSaveCs);
  return requested;
}

void requestModeSwitch() {
  critical_section_enter_blocking(&modeSwitchCs);
  modeSwitchRequested = true;
  critical_section_exit(&modeSwitchCs);
}

void requestActivityLedBlink() {
  critical_section_enter_blocking(&ledBlinkCs);
  activityLedBlinkRequested = true;
  critical_section_exit(&ledBlinkCs);
}

void requestSpeedLedBlink(uint8_t count) {
  critical_section_enter_blocking(&speedBlinkCs);
  speedLedBlinkRequestedCount = count;
  critical_section_exit(&speedBlinkCs);
}

bool consumeActivityLedBlinkRequest() {
  bool requested;
  critical_section_enter_blocking(&ledBlinkCs);
  requested = activityLedBlinkRequested;
  activityLedBlinkRequested = false;
  critical_section_exit(&ledBlinkCs);
  return requested;
}

uint8_t consumeSpeedLedBlinkRequest() {
  uint8_t count;
  critical_section_enter_blocking(&speedBlinkCs);
  count = speedLedBlinkRequestedCount;
  speedLedBlinkRequestedCount = 0;
  critical_section_exit(&speedBlinkCs);
  return count;
}

bool consumeModeSwitchRequest() {
  bool requested;
  critical_section_enter_blocking(&modeSwitchCs);
  requested = modeSwitchRequested;
  modeSwitchRequested = false;
  critical_section_exit(&modeSwitchCs);
  return requested;
}

void startupLedSet(bool on, DeviceMode mode) {
#if defined(BOARD_RP2040_ZERO)
  statusLed.setBrightness(STARTUP_LED_BRIGHTNESS);
  if (on) {
    if (mode == DEVICE_MODE_GAMEPAD) {
      statusLed.setPixelColor(0, statusLed.Color(0, 255, 0));
    } else {
      statusLed.setPixelColor(0, statusLed.Color(255, 128, 0));
    }
  } else {
    statusLed.setPixelColor(0, statusLed.Color(0, 0, 0));
  }
  statusLed.show();
#else
  digitalWrite(PIN_LED, on ? HIGH : LOW);
#endif
}

void activityLedSet(bool on) {
#if defined(BOARD_RP2040_ZERO)
  statusLed.setBrightness(ACTIVITY_LED_BRIGHTNESS);
  if (on) {
    statusLed.setPixelColor(0, statusLed.Color(255, 255, 0));
  } else {
    statusLed.setPixelColor(0, statusLed.Color(0, 0, 0));
  }
  statusLed.show();
#else
  digitalWrite(PIN_LED, on ? HIGH : LOW);
#endif
}

void speedLedSet(bool on) {
#if defined(BOARD_RP2040_ZERO)
  statusLed.setBrightness(ACTIVITY_LED_BRIGHTNESS);
  if (on) {
    statusLed.setPixelColor(0, statusLed.Color(128, 200, 255));
  } else {
    statusLed.setPixelColor(0, statusLed.Color(0, 0, 0));
  }
  statusLed.show();
#else
  digitalWrite(PIN_LED, on ? HIGH : LOW);
#endif
}

void restoreLedBaseState() {
  if (activityLedBlinkActive) {
    activityLedSet(true);
    return;
  }

  bool startupOn = startupLedActive && startupLedLastOn;
  if (startupOn) {
    startupLedSet(true, (DeviceMode)startupLedMode);
  } else {
    startupLedSet(false, (DeviceMode)startupLedMode);
  }
}

void startStartupLedPattern(DeviceMode mode) {
  startupLedMode = (uint8_t)mode;
  startupLedStartMs = millis();
  startupLedActive = true;
  startupLedLastOn = true;
  startupLedSet(true, mode);
}

void updateStartupLedPattern(unsigned long nowMs) {
  if (!startupLedActive) {
    return;
  }

  unsigned long elapsedMs = nowMs - startupLedStartMs;
  if (elapsedMs >= STARTUP_LED_TOTAL_MS) {
    if (startupLedLastOn) {
      if (!activityLedBlinkActive) {
        startupLedSet(false, (DeviceMode)startupLedMode);
      }
      startupLedLastOn = false;
    }
    startupLedActive = false;
    return;
  }

  bool nextOn = true;
  if (startupLedMode == DEVICE_MODE_GAMEPAD) {
    nextOn = ((elapsedMs / STARTUP_LED_BLINK_HALF_MS) % 2u) == 0u;
  }

  if (nextOn != startupLedLastOn) {
    if (!activityLedBlinkActive) {
      startupLedSet(nextOn, (DeviceMode)startupLedMode);
    }
    startupLedLastOn = nextOn;
  }
}

void updateActivityLedBlink(unsigned long nowMs) {
  if (consumeActivityLedBlinkRequest()) {
    if ((long)(nowMs - activityLedNextAllowedMs) >= 0) {
      activityLedBlinkActive = true;
      activityLedBlinkUntilMs = nowMs + ACTIVITY_LED_ON_MS;
      activityLedNextAllowedMs = nowMs + ACTIVITY_LED_ON_MS + ACTIVITY_LED_OFF_MS;
      activityLedSet(true);
    }
  }

  if (activityLedBlinkActive && (long)(nowMs - activityLedBlinkUntilMs) >= 0) {
    activityLedBlinkActive = false;

    if (!speedLedBlinkActive) {
      restoreLedBaseState();
    }
  }
}

void updateSpeedLedBlink(unsigned long nowMs) {
  uint8_t requestedCount = consumeSpeedLedBlinkRequest();
  if (requestedCount > 0) {
    speedLedBlinkActive = true;
    speedLedBlinkRemaining = requestedCount;
    speedLedBlinkOn = true;
    speedLedBlinkNextToggleMs = nowMs + SPEED_LED_ON_MS;
    speedLedSet(true);
  }

  if (!speedLedBlinkActive) {
    return;
  }

  if ((long)(nowMs - speedLedBlinkNextToggleMs) < 0) {
    return;
  }

  if (speedLedBlinkOn) {
    speedLedSet(false);
    speedLedBlinkOn = false;
    if (speedLedBlinkRemaining > 0) {
      --speedLedBlinkRemaining;
    }

    if (speedLedBlinkRemaining == 0) {
      speedLedBlinkActive = false;
      restoreLedBaseState();
      return;
    }

    speedLedBlinkNextToggleMs = nowMs + SPEED_LED_OFF_MS;
    return;
  }

  speedLedSet(true);
  speedLedBlinkOn = true;
  speedLedBlinkNextToggleMs = nowMs + SPEED_LED_ON_MS;
}

void loadSettingsFromNand() {
  DeviceSettings settings;
  EEPROM.get(0, settings);

  if (settings.magic != SETTINGS_MAGIC ||
      settings.version != SETTINGS_VERSION ||
      settings.checksum != calcSettingsChecksum(settings)) {
    speedIndexByMode[DEVICE_MODE_MOUSE] = 0;
    speedIndexByMode[DEVICE_MODE_GAMEPAD] = 0;
    autoFireFlagsByMode[DEVICE_MODE_MOUSE] = 0;
    autoFireFlagsByMode[DEVICE_MODE_GAMEPAD] = 0;
    applyModeCacheToRuntimeSettings(currentMode);
    return;
  }

  for (uint8_t i = 0; i < DEVICE_MODE_MAX; ++i) {
    speedIndexByMode[i] = settings.speedIndexByMode[i];
    autoFireFlagsByMode[i] = settings.autoFireFlagsByMode[i];
  }

  if (settings.currentMode < DEVICE_MODE_MAX) {
    currentMode = (DeviceMode)settings.currentMode;
  } else if (settings.mode < DEVICE_MODE_MAX) {
    // 互換性: 旧フィールドが有効ならそれを使う。
    currentMode = (DeviceMode)settings.mode;
  }

  applyModeCacheToRuntimeSettings(currentMode);
}

int8_t toMouseDelta(int16_t value) {
  if (value > 127) {
    return (int8_t)127;
  }
  if (value < -127) {
    return (int8_t)-127;
  }
  return (int8_t)value;
}

uint32_t mapMouseButtonsToGamepadButtons(uint8_t mouseButtons) {
  uint16_t gamepadButtons = 0;
  static uint8_t PpShift = OUT_PAD_SQUARE;
  static bool LastPpShiftTrig = false;
  static bool wasR2Pressed = false;
  static unsigned long r2PressStartUs = 0;
  static bool l2BurstActive = false;
  static unsigned long l2BurstStartUs = 0;
  static unsigned long lastL2PulseUs = 0;
  static bool l2PulseActive = false;
  static unsigned long l2PulseStartUs = 0;
  unsigned long now = micros();

  // micros() がオーバーフローした場合に備えて開始時刻を補正する。
  if (r2PressStartUs > now) {
    r2PressStartUs = now;
  }
  if (l2BurstStartUs > now) {
    l2BurstStartUs = now;
  }
  if (lastL2PulseUs > now) {
    lastL2PulseUs = now;
  }
  if (l2PulseStartUs > now) {
    l2PulseStartUs = now;
  }

  if ((mouseButtons & 0x01) != 0) {
    gamepadButtons |= OUT_PAD_R2;
  }
  if ((mouseButtons & 0x02) != 0) {
    if (!LastPpShiftTrig) {
      PpShift = (PpShift == OUT_PAD_SQUARE) ? OUT_PAD_CROSS : OUT_PAD_SQUARE;
    }
    LastPpShiftTrig = true;
    gamepadButtons |= PpShift;
  } else {
    LastPpShiftTrig = false;
  }
  if ((mouseButtons & 0x04) != 0) {
    gamepadButtons |= OUT_PAD_TRIANGLE;
  }
  if ((mouseButtons & 0x08) != 0) {
    gamepadButtons |= OUT_PAD_PS;
  }

  bool r2Pressed = (gamepadButtons & OUT_PAD_R2) != 0;
  if (r2Pressed) {
    if (!wasR2Pressed) {
      r2PressStartUs = now;
    }
    wasR2Pressed = true;
    l2BurstActive = false;
    l2PulseActive = false;
  } else {
    if (wasR2Pressed) {
      unsigned long heldUs = now - r2PressStartUs;
      if (heldUs >= GAMEPAD_R2_HOLD_TRIGGER_US) {
        l2BurstActive = true;
        l2BurstStartUs = now;
        lastL2PulseUs = now - GAMEPAD_L2_PULSE_INTERVAL_US;
        l2PulseActive = false;
      }
    }
    wasR2Pressed = false;
  }

  if (l2BurstActive) {
    unsigned long elapsedUs = now - l2BurstStartUs;
    if (elapsedUs < GAMEPAD_L2_BURST_DURATION_US) {
      if (!l2PulseActive && (now - lastL2PulseUs) >= GAMEPAD_L2_PULSE_INTERVAL_US) {
        l2PulseActive = true;
        l2PulseStartUs = now;
        lastL2PulseUs = now;
        requestActivityLedBlink();
      }

      if (l2PulseActive && (now - l2PulseStartUs) < GAMEPAD_L2_PULSE_WIDTH_US) {
        gamepadButtons |= OUT_PAD_L2;
      } else {
        l2PulseActive = false;
      }
    } else {
      l2BurstActive = false;
      l2PulseActive = false;
    }
  }

  return gamepadButtons;
}

void publishReportState(uint8_t buttons, bool axisIsAlt) {
  critical_section_enter_blocking(&reportStateCs);
  sharedButtons = buttons;
  sharedAxisAlt = axisIsAlt;
  critical_section_exit(&reportStateCs);
}

void publishModeState(DeviceMode mode) {
  critical_section_enter_blocking(&modeStateCs);
  sharedMode = (uint8_t)mode;
  critical_section_exit(&modeStateCs);
}

DeviceMode consumeModeState() {
  uint8_t mode;
  critical_section_enter_blocking(&modeStateCs);
  mode = sharedMode;
  critical_section_exit(&modeStateCs);

  if (mode >= DEVICE_MODE_MAX) {
    return DEVICE_MODE_MOUSE;
  }
  return (DeviceMode)mode;
}

void initS1S2Pio() {
  s1s2ProgramOffset = pio_add_program(s1s2Pio, &s1s2_encoder_program);
  pio_sm_config config = s1s2_encoder_program_get_default_config(s1s2ProgramOffset);
  sm_config_set_in_pins(&config, PIN_S2);
  sm_config_set_in_shift(&config, false, false, 32);

  pio_gpio_init(s1s2Pio, PIN_S2);
  pio_gpio_init(s1s2Pio, PIN_S1);
  gpio_pull_up(PIN_S2);
  gpio_pull_up(PIN_S1);
  pio_sm_set_consecutive_pindirs(s1s2Pio, s1s2Sm, PIN_S2, 2, false);

  pio_sm_init(s1s2Pio, s1s2Sm, s1s2ProgramOffset, &config);
  pio_sm_set_enabled(s1s2Pio, s1s2Sm, true);

  s1s2LastSts = (gpio_get(PIN_S1) << 1) | gpio_get(PIN_S2);
}

void consumeS1S2Pio() {
  static const int8_t transitionTable[16] = {
    0, -1, 1, 0,
    1, 0, 0, -1,
    -1, 0, 0, 1,
    0, 1, -1, 0,
  };

  // RX FIFOが埋まり続けるとloop全体が飢餓するため、
  // 1回の呼び出しで処理するサンプル数に上限を設ける。
  const uint8_t maxSamplesPerLoop = 64;
  uint8_t samples = 0;

  while (!pio_sm_is_rx_fifo_empty(s1s2Pio, s1s2Sm) && samples < maxSamplesPerLoop) {
    uint8_t newSts = (uint8_t)(pio_sm_get(s1s2Pio, s1s2Sm) & 0x03);
    uint8_t transition = (uint8_t)((s1s2LastSts << 2) | newSts);
    int8_t delta = transitionTable[transition];

    if (delta != 0) {
      cnt += delta;
    }
    s1s2LastSts = newSts;
    ++samples;
  }
}

void core1Task() {
  // multicore_lockout_start_blocking() を使うため、
  // ロックアウト対象コア側の初期化を行う。
  multicore_lockout_victim_init();
  bool modeSwitchArmed = false;

  while (true) {
    DeviceMode deviceMode = consumeModeState();
    unsigned long now = micros();
    if (lastTimeBtn1 > now) {
      lastTimeBtn1 = 0;
    }
    if (lastTimeBtn2 > now) {
      lastTimeBtn2 = 0;
    }
    if (lastTimeBtn3 > now) {
      lastTimeBtn3 = 0;
    }
    if (lastTimeBtnSpd > now) {
      lastTimeBtnSpd = 0;
    }
    if (lastTimeBtnAxis > now) {
      lastTimeBtnAxis = 0;
    }

    btn1Sts = !gpio_get(PIN_BTN1);
    if (lastBtn1Sts != btn1Sts) {
      if (lastTimeBtn1 + INTERVAL_BTN > now) {
        btn1Sts = lastBtn1Sts;
      } else {
        lastTimeBtn1 = now;
      }
    }

    btn2Sts = !gpio_get(PIN_BTN2);
    if (lastBtn2Sts != btn2Sts) {
      if (lastTimeBtn2 + INTERVAL_BTN > now) {
        btn2Sts = lastBtn2Sts;
      } else {
        lastTimeBtn2 = now;
      }
    }

    btn3Sts = !gpio_get(PIN_BTN3);
    if (lastBtn3Sts != btn3Sts) {
      if (lastTimeBtn3 + INTERVAL_BTN > now) {
        btn3Sts = lastBtn3Sts;
      } else {
        lastTimeBtn3 = now;
      }
    }

    btnSpdSts = !gpio_get(PIN_BTNSPD);
    if (lastBtnSpdSts != btnSpdSts) {
      if (lastTimeBtnSpd + INTERVAL_BTN > now) {
        btnSpdSts = lastBtnSpdSts;
      } else {
        lastTimeBtnSpd = now;
        if (!btnSpdSts) {
          if (!speedComboConsumed && !modeSwitchComboActive) {
            if (deviceMode == DEVICE_MODE_MOUSE) {
              mouseStepIdx = (mouseStepIdx + 1) % sizeof(mouseStepTable);
              mouseStep = mouseStepTable[mouseStepIdx];
              requestSpeedLedBlink((uint8_t)(mouseStepIdx + 1));
            }
            requestSettingsSave();
          }
          speedComboActive = false;
          speedComboConsumed = false;
        }
      }
    }

    btnAxisSts = !gpio_get(PIN_BTNAXIS);
    if (lastBtnAxisSts != btnAxisSts) {
      if (lastTimeBtnAxis + INTERVAL_BTN > now) {
        btnAxisSts = lastBtnAxisSts;
      } else {
        lastTimeBtnAxis = now;
        if (btnAxisSts && !btnSpdSts) {
          axisAlt = !axisAlt;
        }
      }
    }

    if (!btnSpdSts || !btnAxisSts) {
      modeSwitchComboActive = false;
      modeSwitchArmed = true;
    }

    if (modeSwitchArmed && btnSpdSts && btnAxisSts) {
      if (!modeSwitchComboActive) {
        modeSwitchComboActive = true;
        modeSwitchArmed = false;
        speedComboConsumed = true;
        requestModeSwitch();
      }
    }

    if (deviceMode == DEVICE_MODE_MOUSE &&
        !modeSwitchComboActive &&
        !speedComboActive &&
        btnSpdSts &&
        (btn1Sts || btn2Sts || btn3Sts)) {
      speedComboActive = true;
      speedComboConsumed = true;
      if (btn1Sts) {
        btn1AutoFire = !btn1AutoFire;
        requestSettingsSave();
      } else if (btn2Sts) {
        btn2AutoFire = !btn2AutoFire;
        requestSettingsSave();
      } else if (btn3Sts) {
        btn3AutoFire = !btn3AutoFire;
        requestSettingsSave();
      }
    }

    uint8_t outBtn1 = btn1Sts;
    uint8_t outBtn2 = btn2Sts;
    uint8_t outBtn3 = btn3Sts;

    if (speedComboActive) {
      outBtn1 = 0;
      outBtn2 = 0;
      outBtn3 = 0;
    }

    uint8_t btn1AutoFireOutput = 0;
    uint8_t btn2AutoFireOutput = 0;
    uint8_t btn3AutoFireOutput = 0;

    if (deviceMode == DEVICE_MODE_MOUSE && btn1AutoFire && outBtn1) {
      if (now - lastAutoFireBtn1 >= AUTO_FIRE_INTERVAL) {
        autoFireStateBtn1 = !autoFireStateBtn1;
        lastAutoFireBtn1 = now;
        if (autoFireStateBtn1) {
          requestActivityLedBlink();
        }
      }
      btn1AutoFireOutput = autoFireStateBtn1;
    } else {
      lastAutoFireBtn1 = now;
      autoFireStateBtn1 = false;
    }

    if (deviceMode == DEVICE_MODE_MOUSE && btn2AutoFire && outBtn2) {
      if (now - lastAutoFireBtn2 >= AUTO_FIRE_INTERVAL) {
        autoFireStateBtn2 = !autoFireStateBtn2;
        lastAutoFireBtn2 = now;
        if (autoFireStateBtn2) {
          requestActivityLedBlink();
        }
      }
      btn2AutoFireOutput = autoFireStateBtn2;
    } else {
      lastAutoFireBtn2 = now;
      autoFireStateBtn2 = false;
    }

    if (deviceMode == DEVICE_MODE_MOUSE && btn3AutoFire && outBtn3) {
      if (now - lastAutoFireBtn3 >= AUTO_FIRE_INTERVAL) {
        autoFireStateBtn3 = !autoFireStateBtn3;
        lastAutoFireBtn3 = now;
        if (autoFireStateBtn3) {
          requestActivityLedBlink();
        }
      }
      btn3AutoFireOutput = autoFireStateBtn3;
    } else {
      lastAutoFireBtn3 = now;
      autoFireStateBtn3 = false;
    }

    uint8_t finalBtn1 = (deviceMode == DEVICE_MODE_MOUSE && btn1AutoFire) ? btn1AutoFireOutput : outBtn1;
    uint8_t finalBtn2 = (deviceMode == DEVICE_MODE_MOUSE && btn2AutoFire) ? btn2AutoFireOutput : outBtn2;
    uint8_t finalBtn3 = (deviceMode == DEVICE_MODE_MOUSE && btn3AutoFire) ? btn3AutoFireOutput : outBtn3;
    uint8_t finalButtons = finalBtn1 | finalBtn2 << 1 | finalBtn3 << 2 | btnSpdSts << 3 | btnAxisSts << 4;

    publishReportState(finalButtons, axisAlt);

    lastBtn1Sts = btn1Sts;
    lastBtn2Sts = btn2Sts;
    lastBtn3Sts = btn3Sts;
    lastBtnAxisSts = btnAxisSts;
    lastBtnSpdSts = btnSpdSts;
  }
}

void setup() {
  cnt = 0;

  EEPROM.begin(EEPROM_SIZE_BYTES);
  loadSettingsFromNand();

  // X/Y軸は常に起動時にX軸へ戻す
  axisAlt = false;

  pinMode(PIN_S1, INPUT_PULLUP);
  pinMode(PIN_S2, INPUT_PULLUP);
  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);
  pinMode(PIN_BTN3, INPUT_PULLUP);
  pinMode(PIN_BTNSPD, INPUT_PULLUP);
  pinMode(PIN_BTNAXIS, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);

#if defined(BOARD_RP2040_ZERO)
  statusLed.begin();
  statusLed.setBrightness(STARTUP_LED_BRIGHTNESS);
  statusLed.setPixelColor(0, statusLed.Color(0, 0, 0));
  statusLed.show();
#else
  digitalWrite(PIN_LED, LOW);
#endif

  // 起動時にボタンが同時に押されている場合は、低速モードにする
  if (!gpio_get(PIN_BTN1) && !gpio_get(PIN_BTN2)) {
    mouseStepIdx = sizeof(mouseStepTable) - 1;
    mouseStep = mouseStepTable[mouseStepIdx];
  }

  const UsbIdentityProfile& usbIdentity =
      (currentMode == DEVICE_MODE_GAMEPAD)
          ? usbIdentityProfiles[USB_ID_PROFILE_GAMEPAD]
          : usbIdentityProfiles[USB_ID_PROFILE_MOUSE];

  if (currentMode == DEVICE_MODE_GAMEPAD) {
    usb_hid.setPollInterval(1);
    usb_hid.setReportCallback(hidPadGetReportCallback, hidPadSetReportCallback);
    usb_hid.setReportDescriptor(desc_hid_report_gamepad, sizeof(desc_hid_report_gamepad));
  } else {
    usb_hid.setReportDescriptor(desc_hid_report_mouse, sizeof(desc_hid_report_mouse));
  }

  TinyUSB_Device_Init(0);
  TinyUSBDevice.setID(usbIdentity.vid, usbIdentity.pid);
  TinyUSBDevice.setManufacturerDescriptor(usbIdentity.manufacturer);
  TinyUSBDevice.setProductDescriptor(usbIdentity.product);
  usb_hid.begin();

  // Enumerate USB first, then start startup LED pattern without blocking.
  startStartupLedPattern(currentMode);

  critical_section_init(&reportStateCs);
  critical_section_init(&settingsSaveCs);
  critical_section_init(&modeSwitchCs);
  critical_section_init(&modeStateCs);
  critical_section_init(&ledBlinkCs);
  critical_section_init(&speedBlinkCs);
  publishReportState(0, false);
  publishModeState(currentMode);

  initS1S2Pio();

  multicore_launch_core1(core1Task);
}

void loop() {
  static unsigned long nextReportUs = 0;
  static bool reportPending = false;
  static bool hasLastSentReport = false;
  static int8_t lastSentStepValue = 0;
  static uint8_t lastSentButtons = 0;
  static bool lastSentAxisAlt = false;
  static int16_t lastGamepadCnt = 0;
  static float gamepadAccumulatedCnt = 0.0f;
  unsigned long now = micros();
  unsigned long nowMs = millis();

  updateStartupLedPattern(nowMs);
  updateActivityLedBlink(nowMs);
  updateSpeedLedBlink(nowMs);

  if (nextReportUs == 0) {
    nextReportUs = now + REPORT_INTERVAL_US;
  }

  if (consumeSettingsSaveRequest()) {
    saveSettingsToNand();
  }

  if (consumeModeSwitchRequest()) {
    syncRuntimeSettingsToModeCache(currentMode);
    currentMode = (currentMode == DEVICE_MODE_MOUSE) ? DEVICE_MODE_GAMEPAD : DEVICE_MODE_MOUSE;
    applyModeCacheToRuntimeSettings(currentMode);
    publishModeState(currentMode);
    axisAlt = false;
    publishReportState(0, false);
    saveSettingsToNand();
    // ホスト側で古いHID構成が残るのを避けるため、切替時はいったん明示的に切断する。
    TinyUSBDevice.detach();
    delay(250);
    watchdog_reboot(0, 0, 10);
    while (true) {
      tight_loop_contents();
    }
  }

  if ((long)(now - nextReportUs) >= 0) {
    do {
      nextReportUs += REPORT_INTERVAL_US;
    } while ((long)(now - nextReportUs) >= 0);
    reportPending = true;
  }

  consumeS1S2Pio();

  int16_t pendingCnt;
  noInterrupts();
  pendingCnt = cnt;
  interrupts();

  int8_t stepValue;
  if (currentMode == DEVICE_MODE_GAMEPAD) {
    if (pendingCnt != 0) {
      int16_t cntAbs = abs(pendingCnt);
      int16_t t = GAMEPAD_MAGNIFIER_THRESHOLD - cntAbs > GAMEPAD_MAGNIFIER_THRESHOLD ? GAMEPAD_MAGNIFIER_THRESHOLD : cntAbs;
      float magnification = 1.0f + (float)(GAMEPAD_MAGNIFIER_MAX * t * t) / (float)(GAMEPAD_MAGNIFIER_THRESHOLD * GAMEPAD_MAGNIFIER_THRESHOLD);
      gamepadAccumulatedCnt = gamepadAccumulatedCnt + ((float)pendingCnt * magnification);
    } else {
      gamepadAccumulatedCnt = gamepadAccumulatedCnt * 0.3f;
    }
    stepValue = toMouseDelta((int16_t)gamepadAccumulatedCnt);
    gamepadAccumulatedCnt = (float)(abs(stepValue) > 110 ? stepValue * 0.8f : stepValue);
  } else {
    int16_t scaledCnt = pendingCnt * mouseStep;
    stepValue = toMouseDelta(scaledCnt);
  }

  uint8_t reportButtons;
  bool reportAxisAlt;
  critical_section_enter_blocking(&reportStateCs);
  reportButtons = sharedButtons;
  reportAxisAlt = sharedAxisAlt;
  critical_section_exit(&reportStateCs);

  mouse_report.buttons = reportButtons & 0x07;

  if (currentMode == DEVICE_MODE_MOUSE) {
    if (reportAxisAlt) {
      mouse_report.x = 0;
      mouse_report.y = stepValue;
    } else {
      mouse_report.x = stepValue;
      mouse_report.y = 0;
    }
  } else {
    gamepad_report.x = reportAxisAlt ? 127 : stepValue + 127;
    gamepad_report.y = reportAxisAlt ? stepValue + 127 : 127;
    gamepad_report.z = 127;
    gamepad_report.rz = 127;
    gamepad_report.hat = GAMEPAD_HAT_CENTER;
    gamepad_report.buttons = mapMouseButtonsToGamepadButtons(reportButtons);
    memset(gamepad_report.vendorInput, 0, sizeof(gamepad_report.vendorInput));
    memset(gamepad_report.analog, 0, sizeof(gamepad_report.analog));
  }

  bool reportChanged = !hasLastSentReport ||
                       (currentMode == DEVICE_MODE_GAMEPAD) ||
                       (stepValue != lastSentStepValue) ||
                       (reportButtons != lastSentButtons) ||
                       (reportAxisAlt != lastSentAxisAlt);

  if (reportChanged) {
    reportPending = true;
  }

  if (!reportPending) {
    return;
  }

  if (TinyUSBDevice.mounted() && usb_hid.ready()) {
    if (currentMode == DEVICE_MODE_MOUSE) {
      usb_hid.sendReport(0, &mouse_report, sizeof(mouse_report));
    } else {
      usb_hid.sendReport(0, &gamepad_report, sizeof(gamepad_report));
    }

    noInterrupts();
    cnt -= pendingCnt;
    interrupts();

    hasLastSentReport = true;
    lastSentStepValue = stepValue;
    lastSentButtons = reportButtons;
    lastSentAxisAlt = reportAxisAlt;
    nextReportUs = now + REPORT_INTERVAL_US;
    reportPending = false;
  }
}

