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

uint8_t lastBtn1Sts;
uint8_t btn1Sts;
uint8_t lastBtn2Sts;
uint8_t btn2Sts;

unsigned long lastTime = micros();
unsigned long lastTimeBtn1 = micros();
unsigned long lastTimeBtn2 = micros();
unsigned long now;

int8_t  cnt;
int8_t mouseStep = MOUSE_STEP;

void paddleIr() {
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
}

void setup() {
  cnt = 0;

  pinMode(PIN_S1, INPUT_PULLUP);
  pinMode(PIN_S2, INPUT_PULLUP);
  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);

  // 起動時にボタンが同時に押されている場合は、マウスの移動量を遅くする
  if (!digitalRead(PIN_BTN1) && !digitalRead(PIN_BTN2)) {
    mouseStep = MOUSE_STEP_SLOW;
  }

  TinyUSB_Device_Init(0);
  usb_hid.begin();

  attachInterrupt(PIN_S1, paddleIr, FALLING);
  attachInterrupt(PIN_S2, paddleIr, FALLING);
}

void loop() {
  mouse_report.y = 0;
  mouse_report.x = cnt;
  btn1Sts = !digitalRead(PIN_BTN1);
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

  mouse_report.buttons = btn1Sts | btn2Sts <<1;

  if (TinyUSBDevice.mounted() && usb_hid.ready()) {
    usb_hid.sendReport(0, &mouse_report, sizeof(mouse_report));
    cnt = 0;
  }

  lastBtn1Sts = btn1Sts;
  lastBtn2Sts = btn2Sts;
}

