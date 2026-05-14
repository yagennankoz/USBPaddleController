#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <hardware/gpio.h>
#include <SPI.h>
#include <Adafruit_TinyUSB.h>
#include <define.hpp>

uint8_t const desc_hid_report[] =
{
    MY_TUD_HID_REPORT_DESC_MOUSE()
};
Adafruit_USBD_HID usb_hid(desc_hid_report, sizeof(desc_hid_report), HID_ITF_PROTOCOL_NONE, 2, false);
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
unsigned long now;

int8_t cnt;
int8_t mouseStepIdx = 0;
int8_t mouseStep = mouseStepTable[mouseStepIdx];

// チャタリング対策用
unsigned long lastPaddleIrTime = 0;
float smoothedCnt = 0.0f;

int8_t toMouseDelta(float value) {
  return (int8_t)(value >= 0.0f ? value + 0.5f : value - 0.5f);
}

void paddleIr() {
  // デバウンス: 前回の割り込みから一定時間経過していない場合はスキップ
  unsigned long now_ir = micros();
  if (now_ir - lastPaddleIrTime < DEBOUNCE_TIME_US) {
    return;
  }
  lastPaddleIrTime = now_ir;
  
  sts = digitalRead(PIN_S1) << 1 | digitalRead(PIN_S2);

  now = micros();
  if (now < lastTime) lastTime = 0;

  if (inertial && lastTime + INTERVAL < now) {
    dir = DIR_N;
  }

  xorSts = lastSts xor sts;
  if (xorSts == 3) {
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
    dir = DIR_RIGHT;
    inertial = false;
    cnt += mouseStep;
  } else {
    dir = DIR_LEFT;
    inertial = false;
    cnt -= mouseStep;
  }

  lastLastSts = lastSts;
  lastSts = sts;
  lastTime = now;

  // 停止直後の立ち上がりと逆方向への切り返しは減衰させず、連続回転中だけ平滑化する。
  if (smoothedCnt == 0.0f || (smoothedCnt > 0.0f && cnt < 0) || (smoothedCnt < 0.0f && cnt > 0)) {
    smoothedCnt = (float)cnt;
  } else {
    smoothedCnt = smoothedCnt * (1.0f - EMA_ALPHA) + cnt * EMA_ALPHA;
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

  // 起動時にボタンが同時に押されている場合は、マウスの移動量を遅くする
  if (!digitalRead(PIN_BTN1) && !digitalRead(PIN_BTN2)) {
    mouseStepIdx = sizeof(mouseStepTable) - 1;
    mouseStep = mouseStepTable[mouseStepIdx];
  }

  TinyUSB_Device_Init(0);
  usb_hid.begin();

  attachInterrupt(PIN_S1, paddleIr, FALLING);
  attachInterrupt(PIN_S2, paddleIr, FALLING);
}

void loop() {
  now = micros();
  if (lastTime > now) {
    lastTime = 0;
  }
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

  btn1Sts = !digitalRead(PIN_BTN1);
  if (lastBtn1Sts != btn1Sts) {
    if (lastTimeBtn1 + INTERVAL_BTN > now) {
      btn1Sts = lastBtn1Sts;
    } else {
      lastTimeBtn1 = now;
    }
  }

  btn2Sts = !digitalRead(PIN_BTN2);
  if (lastBtn2Sts != btn2Sts) {
    if (lastTimeBtn2 + INTERVAL_BTN > now) {
      btn2Sts = lastBtn2Sts;
    } else {
      lastTimeBtn2 = now;
    }
  }

  btn3Sts = !digitalRead(PIN_BTN3);
  if (lastBtn3Sts != btn3Sts) {
    if (lastTimeBtn3 + INTERVAL_BTN > now) {
      btn3Sts = lastBtn3Sts;
    } else {
      lastTimeBtn3 = now;
    }
  }

  btnSpdSts = !digitalRead(PIN_BTNSPD);
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

  btnAxisSts = !digitalRead(PIN_BTNAXIS);
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

  // 平滑化された値をマウスレポートに使用
  int8_t smoothedValue = toMouseDelta(smoothedCnt);
  
  if (axisAlt) {
    mouse_report.x = 0;
    mouse_report.y = smoothedValue;
  } else {
    mouse_report.x = smoothedValue;
    mouse_report.y = 0;
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

  // オートファイア処理
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

  mouse_report.buttons = finalBtn1 | finalBtn2 << 1 | finalBtn3 << 2;

  if (TinyUSBDevice.mounted() && usb_hid.ready()) {
    usb_hid.sendReport(0, &mouse_report, sizeof(mouse_report));
    cnt -= (float)smoothedValue;
    smoothedCnt -= (float)smoothedValue;
  }

  lastBtn1Sts = btn1Sts;
  lastBtn2Sts = btn2Sts;
  lastBtn3Sts = btn3Sts;
  lastBtnAxisSts = btnAxisSts;
  lastBtnSpdSts = btnSpdSts;
}

