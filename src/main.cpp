#include <Arduino.h>
#include <hardware/gpio.h>
#include <pico/multicore.h>
#include <pico/critical_section.h>
#include <SPI.h>
#include <Adafruit_TinyUSB.h>
#include <define.hpp>

uint8_t const desc_hid_report[] =
{
    MY_TUD_HID_REPORT_DESC_MOUSE()
};
Adafruit_USBD_HID usb_hid(desc_hid_report, sizeof(desc_hid_report), HID_ITF_PROTOCOL_NONE, 1, false);
my_hid_mouse_report_t   mouse_report;

uint8_t lastLastSts;
uint8_t lastSts;
uint8_t sts;
uint8_t xorSts;
uint8_t dir;
bool inertial = false;
bool axisAlt = false;

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

bool btn1AutoFire = false;
bool btn2AutoFire = false;
bool btn3AutoFire = false;
bool autoFireStateBtn1 = false;
bool autoFireStateBtn2 = false;
bool autoFireStateBtn3 = false;
unsigned long lastAutoFireBtn1 = 0;
unsigned long lastAutoFireBtn2 = 0;
unsigned long lastAutoFireBtn3 = 0;

unsigned long lastTime = micros();
unsigned long lastTimeBtn1 = micros();
unsigned long lastTimeBtn2 = micros();
unsigned long lastTimeBtn3 = micros();
unsigned long lastTimeBtnAxis = micros();
unsigned long lastTimeBtnSpd = micros();

int16_t cnt;
int16_t mouseStepIdx = 0;
int16_t mouseStep = mouseStepTable[mouseStepIdx];

critical_section_t reportStateCs;
volatile uint8_t sharedButtons = 0;
volatile bool sharedAxisAlt = false;

const unsigned long REPORT_INTERVAL_US = 1000u;

int8_t toMouseDelta(int16_t value) {
  if (value > 127) {
    return (int8_t)127;
  }
  if (value < -127) {
    return (int8_t)-127;
  }
  return (int8_t)value;
}

void publishReportState(uint8_t buttons, bool axisIsAlt) {
  critical_section_enter_blocking(&reportStateCs);
  sharedButtons = buttons;
  sharedAxisAlt = axisIsAlt;
  critical_section_exit(&reportStateCs);
}

void paddleIr() {
  sts = gpio_get(PIN_S1) << 1 | gpio_get(PIN_S2);

  unsigned long now = micros();
  if (now < lastTime) lastTime = 0;

  if (inertial && lastTime + INTERVAL < now) {
    dir = DIR_N;
    inertial = false;
  }

  xorSts = lastSts xor sts;
  if (xorSts == 3) {
    // 左右切り返したときに誤検知することがあるが気にしないことにする
    // ここをやめると動きがカクつくことがある
    if (lastLastSts == lastSts) {
      dir = DIR_N;
    }
    if (dir == DIR_RIGHT) {
      cnt += mouseStep;
      inertial = true;
    } else if (dir == DIR_LEFT) {
      cnt -= mouseStep;
      inertial = true;
    }
  } else if (xorSts == 1) {
    if (dir == DIR_LEFT) {
      cnt = 0;
    }
    dir = DIR_RIGHT;
    inertial = false;
    cnt += mouseStep;
  } else {
    if (dir == DIR_RIGHT) {
      cnt = 0;
    }
    dir = DIR_LEFT;
    inertial = false;
    cnt -= mouseStep;
  }

  lastLastSts = lastSts;
  lastSts = sts;
  lastTime = now;
}

void core1Task() {
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
          if (!speedComboConsumed) {
            mouseStepIdx = (mouseStepIdx + 1) % sizeof(mouseStepTable);
            mouseStep = mouseStepTable[mouseStepIdx];
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
        if (btnAxisSts) {
          axisAlt = !axisAlt;
        }
      }
    }

    if (!speedComboActive && btnSpdSts && (btn1Sts || btn2Sts || btn3Sts)) {
      speedComboActive = true;
      speedComboConsumed = true;
      if (btn1Sts) {
        btn1AutoFire = !btn1AutoFire;
      } else if (btn2Sts) {
        btn2AutoFire = !btn2AutoFire;
      } else if (btn3Sts) {
        btn3AutoFire = !btn3AutoFire;
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

  TinyUSB_Device_Init(0);
  usb_hid.begin();

  critical_section_init(&reportStateCs);
  publishReportState(0, false);

  attachInterrupt(PIN_S1, paddleIr, FALLING);
  attachInterrupt(PIN_S2, paddleIr, FALLING);

  multicore_launch_core1(core1Task);
}

void loop() {
  static unsigned long nextReportUs = 0;
  static bool reportPending = false;
  static bool hasLastSentReport = false;
  static int8_t lastSentStepValue = 0;
  static uint8_t lastSentButtons = 0;
  static bool lastSentAxisAlt = false;
  unsigned long now = micros();

  if (nextReportUs == 0) {
    nextReportUs = now + REPORT_INTERVAL_US;
  }

  if ((long)(now - nextReportUs) >= 0) {
    do {
      nextReportUs += REPORT_INTERVAL_US;
    } while ((long)(now - nextReportUs) >= 0);
    reportPending = true;
  }

  int16_t pendingCnt;
  noInterrupts();
  pendingCnt = cnt;
  interrupts();
  int8_t stepValue = toMouseDelta(pendingCnt);

  uint8_t reportButtons;
  bool reportAxisAlt;
  critical_section_enter_blocking(&reportStateCs);
  reportButtons = sharedButtons;
  reportAxisAlt = sharedAxisAlt;
  critical_section_exit(&reportStateCs);

  mouse_report.buttons = reportButtons;
  if (reportAxisAlt) {
    mouse_report.x = 0;
    mouse_report.y = stepValue;
  } else {
    mouse_report.x = stepValue;
    mouse_report.y = 0;
  }

  bool reportChanged = !hasLastSentReport ||
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
    usb_hid.sendReport(0, &mouse_report, sizeof(mouse_report));
    noInterrupts();
    cnt -= stepValue;
    interrupts();
    hasLastSentReport = true;
    lastSentStepValue = stepValue;
    lastSentButtons = reportButtons;
    lastSentAxisAlt = reportAxisAlt;
    nextReportUs = now + REPORT_INTERVAL_US;
    reportPending = false;
  }
}

