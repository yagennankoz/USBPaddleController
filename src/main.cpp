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
uint8_t lastBtnAxisSts;
uint8_t btnAxisSts;
uint8_t lastBtnSpdSts;
uint8_t btnSpdSts;

unsigned long lastTime = micros();
unsigned long lastTimeBtn1 = micros();
unsigned long lastTimeBtn2 = micros();
unsigned long lastTimeBtnAxis = micros();
unsigned long lastTimeBtnSpd = micros();
unsigned long now;

float cnt;
int8_t  mouseStepIdx = 0;
int8_t mouseStep = mouseStepTable[mouseStepIdx];

// チャタリング対策用
unsigned long lastPaddleIrTime = 0;
float smoothedCnt = 0.0f;

int8_t toMouseDelta(float value) {
  return (int8_t)(value >= 0.0f ? value + 0.5f : value - 0.5f);
}

float resolvePaddleStep(unsigned long intervalUs) {
  if (mouseStep <= 1 || intervalUs >= PADDLE_SLOW_INTERVAL_US) {
    return 1.0f;
  }

  if (intervalUs <= PADDLE_FAST_INTERVAL_US) {
    return (float)mouseStep;
  }

  float intervalRange = (float)(PADDLE_SLOW_INTERVAL_US - PADDLE_FAST_INTERVAL_US);
  float ratio = (float)(PADDLE_SLOW_INTERVAL_US - intervalUs) / intervalRange;
  return 1.0f + ratio * (float)(mouseStep - 1);
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

  unsigned long intervalUs = lastTime == 0 ? PADDLE_SLOW_INTERVAL_US : now - lastTime;
  float paddleStep = resolvePaddleStep(intervalUs);

  if (inertial && lastTime + INTERVAL < now) {
    dir = DIR_N;
  }

  xorSts = lastSts xor sts;
  if (xorSts == 3) {
    if (lastLastSts == lastSts) {
      dir = DIR_N;
    }
    if (dir == DIR_RIGHT) {
      cnt += paddleStep;
      inertial = true;
    } else if (dir == DIR_LEFT) {
      cnt -= paddleStep;
      inertial = true;
    }
  } else if (xorSts == 1) {
    dir = DIR_RIGHT;
    inertial = false;
    cnt += paddleStep;
  } else {
    dir = DIR_LEFT;
    inertial = false;
    cnt -= paddleStep;
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
  pinMode(PIN_BTNSPD, INPUT_PULLUP);
  pinMode(PIN_BTNAXIS, INPUT_PULLUP);

  // 起動時にボタンが同時に押されている場合は、マウスの移動量を遅くする
  if (!digitalRead(PIN_BTN1) && !digitalRead(PIN_BTN2)) {
    mouseStepIdx = 3;
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

  btnSpdSts = !digitalRead(PIN_BTNSPD);
  if (lastBtnSpdSts != btnSpdSts) {
    if (lastTimeBtnSpd + INTERVAL_BTN > now) {
      btnSpdSts = lastBtnSpdSts;
    } else {
      lastTimeBtnSpd = now;
      if (btnSpdSts) {
        mouseStepIdx = (mouseStepIdx + 1) % sizeof(mouseStepTable);
        mouseStep = mouseStepTable[mouseStepIdx];
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

  mouse_report.buttons = btn1Sts | btn2Sts <<1;

  if (TinyUSBDevice.mounted() && usb_hid.ready()) {
    usb_hid.sendReport(0, &mouse_report, sizeof(mouse_report));
    cnt -= (float)smoothedValue;
    smoothedCnt -= (float)smoothedValue;
  }

  lastBtn1Sts = btn1Sts;
  lastBtn2Sts = btn2Sts;
  lastBtnAxisSts = btnAxisSts;
  lastBtnSpdSts = btnSpdSts;
}

