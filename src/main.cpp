#include <Arduino.h>
#include <hardware/gpio.h>
#include <hardware/pio.h>
#include <hardware/watchdog.h>
#include <pico/multicore.h>
#include <pico/critical_section.h>
#include <SPI.h>
#include <EEPROM.h>
#include <Adafruit_TinyUSB.h>
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

PIO s1s2Pio = pio0;
uint s1s2Sm = 0;
uint s1s2ProgramOffset = 0;
uint8_t s1s2LastSts = 0;

critical_section_t reportStateCs;
critical_section_t settingsSaveCs;
critical_section_t modeSwitchCs;
volatile uint8_t sharedButtons = 0;
volatile bool sharedAxisAlt = false;
volatile bool settingsSaveRequested = false;
volatile bool modeSwitchRequested = false;

const unsigned long REPORT_INTERVAL_US = 1000u;
const int EEPROM_SIZE_BYTES = 64;
const uint8_t GAMEPAD_HAT_CENTER = 8;

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
  mouseStep = mouseStepTable[mouseStepIdx] * ((currentMode == DEVICE_MODE_GAMEPAD) ? 4 : 1);
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

bool consumeModeSwitchRequest() {
  bool requested;
  critical_section_enter_blocking(&modeSwitchCs);
  requested = modeSwitchRequested;
  modeSwitchRequested = false;
  critical_section_exit(&modeSwitchCs);
  return requested;
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

  return gamepadButtons;
}

void publishReportState(uint8_t buttons, bool axisIsAlt) {
  critical_section_enter_blocking(&reportStateCs);
  sharedButtons = buttons;
  sharedAxisAlt = axisIsAlt;
  critical_section_exit(&reportStateCs);
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

  while (!pio_sm_is_rx_fifo_empty(s1s2Pio, s1s2Sm)) {
    uint8_t newSts = (uint8_t)(pio_sm_get(s1s2Pio, s1s2Sm) & 0x03);
    uint8_t transition = (uint8_t)((s1s2LastSts << 2) | newSts);
    int8_t delta = transitionTable[transition];

    if (delta != 0) {
      cnt += delta;
    }
    s1s2LastSts = newSts;
  }
}

void core1Task() {
  // multicore_lockout_start_blocking() を使うため、
  // ロックアウト対象コア側の初期化を行う。
  multicore_lockout_victim_init();

  while (true) {
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
            mouseStepIdx = (mouseStepIdx + 1) % sizeof(mouseStepTable);
            mouseStep = mouseStepTable[mouseStepIdx];
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

    if (btnSpdSts && btnAxisSts) {
      if (!modeSwitchComboActive) {
        modeSwitchComboActive = true;
        speedComboConsumed = true;
        requestModeSwitch();
      }
    } else if (!btnSpdSts && !btnAxisSts) {
      modeSwitchComboActive = false;
    }

    if (!modeSwitchComboActive && !speedComboActive && btnSpdSts && (btn1Sts || btn2Sts || btn3Sts)) {
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

    if (btn1AutoFire && outBtn1) {
      if (now - lastAutoFireBtn1 >= AUTO_FIRE_INTERVAL) {
        autoFireStateBtn1 = !autoFireStateBtn1;
        lastAutoFireBtn1 = now;
      }
      btn1AutoFireOutput = autoFireStateBtn1;
    } else {
      lastAutoFireBtn1 = now;
      autoFireStateBtn1 = false;
    }

    if (btn2AutoFire && outBtn2) {
      if (now - lastAutoFireBtn2 >= AUTO_FIRE_INTERVAL) {
        autoFireStateBtn2 = !autoFireStateBtn2;
        lastAutoFireBtn2 = now;
      }
      btn2AutoFireOutput = autoFireStateBtn2;
    } else {
      lastAutoFireBtn2 = now;
      autoFireStateBtn2 = false;
    }

    if (btn3AutoFire && outBtn3) {
      if (now - lastAutoFireBtn3 >= AUTO_FIRE_INTERVAL) {
        autoFireStateBtn3 = !autoFireStateBtn3;
        lastAutoFireBtn3 = now;
      }
      btn3AutoFireOutput = autoFireStateBtn3;
    } else {
      lastAutoFireBtn3 = now;
      autoFireStateBtn3 = false;
    }

    uint8_t finalBtn1 = btn1AutoFire ? btn1AutoFireOutput : outBtn1;
    uint8_t finalBtn2 = btn2AutoFire ? btn2AutoFireOutput : outBtn2;
    uint8_t finalBtn3 = btn3AutoFire ? btn3AutoFireOutput : outBtn3;
    uint8_t finalButtons = finalBtn1 | finalBtn2 << 1 | finalBtn3 << 2;

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
  TinyUSBDevice.setProductDescriptor(usbIdentity.product);
  usb_hid.begin();

  critical_section_init(&reportStateCs);
  critical_section_init(&settingsSaveCs);
  critical_section_init(&modeSwitchCs);
  publishReportState(0, false);

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
  static int16_t gamepadAccumulatedCnt = 0;
  unsigned long now = micros();

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
    axisAlt = false;
    publishReportState(0, false);
    saveSettingsToNand();
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
    int16_t deltaCnt = pendingCnt - lastGamepadCnt;
    lastGamepadCnt = pendingCnt;
    if (deltaCnt != 0) {
      int16_t nextAccumulated = gamepadAccumulatedCnt + deltaCnt * mouseStep;
      if (nextAccumulated > 127) {
        nextAccumulated = 127;
      } else if (nextAccumulated < -127) {
        nextAccumulated = -127;
      }
      gamepadAccumulatedCnt = nextAccumulated;
    }
    if (gamepadAccumulatedCnt > 127) {
      gamepadAccumulatedCnt = 127;
    } else if (gamepadAccumulatedCnt < -127) {
      gamepadAccumulatedCnt = -127;
    } else if (gamepadAccumulatedCnt < mouseStep && gamepadAccumulatedCnt > -mouseStep) {
      gamepadAccumulatedCnt = 0;
    }

    stepValue = (int8_t)gamepadAccumulatedCnt;
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

  mouse_report.buttons = reportButtons;

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

  if (!reportChanged) {
    reportPending = false;
    return;
  }

  if (TinyUSBDevice.mounted() && usb_hid.ready()) {
    if (currentMode == DEVICE_MODE_MOUSE) {
      usb_hid.sendReport(0, &mouse_report, sizeof(mouse_report));
    } else {
      usb_hid.sendReport(0, &gamepad_report, sizeof(gamepad_report));
    }

    int16_t consumedTicks = (mouseStep != 0) ? ((int16_t)stepValue / mouseStep) : stepValue;
    noInterrupts();
    cnt -= consumedTicks;
    interrupts();

    hasLastSentReport = true;
    lastSentStepValue = stepValue;
    lastSentButtons = reportButtons;
    lastSentAxisAlt = reportAxisAlt;
    nextReportUs = now + REPORT_INTERVAL_US;
    reportPending = false;
  }
}

