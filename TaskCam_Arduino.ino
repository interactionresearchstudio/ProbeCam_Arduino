/* Taskcam v2 software for IRS Taskcam v2 Camera Shield + IRS Taskcam Camera Module */
/* Written by Andy Sheen 2017/2018 */

/*~~~~~~~~~~TO DO~~~~~~~~~~*/
/*
   - Add photo save bitmap
   - Add sleep mode bitmap
   - Add Contrast/Light/FX menu (bonus)
*/
#include <SoftwareSerial.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "bitmaps.h"
#include "font.h"

// Pins
#define OLED_RESET 4
#define PWR_PIN 2
#define CAM_PWR 10
#define RIGHT_BUTTON 8
#define LEFT_BUTTON 9
#define CAM_RX  12
#define CAM_TX  13
#define SHUTTER 3
#define LED 11

Adafruit_SSD1306 display(OLED_RESET);

boolean leftDebounce = false;
boolean rightDebounce = false;
bool dir;
char prevQuestion[64];
long buttonCheck;
static int buttonInterval = 20;

long sleepMillis = 0;
#define SLEEPTIME 40000
boolean newQuestion = false;

boolean buttonHeld = false;
boolean pwrdwn = false;
long buttonHeldCount;
#define OFF_HOLD_DURATION 2000

bool buttonPressed = false;

int currentQuestionIndex = 0;
int numOfQuestions = 16;
byte questionTicks = 0;
long prevMillis;
long currentMillis;
#define SPEED_SCROLLING 50
int scrollingPos;
char inputBuffer[64];
bool flag = false;

SoftwareSerial camSerial(CAM_RX, CAM_TX); // RX, TX

#define DEBUG

void setup() {

  // Power Pin
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, HIGH);

  // Camera power pin
  pinMode(CAM_PWR, OUTPUT);

  // Buttons
  pinMode(SHUTTER, INPUT_PULLUP);
  pinMode(RIGHT_BUTTON, INPUT_PULLUP);
  pinMode(LEFT_BUTTON, INPUT_PULLUP);

  // LED
  pinMode(LED, OUTPUT);

  // Camera Module Interface
  camSerial.begin(38400);

#ifdef DEBUG
  Serial.begin(9600);
#endif

  // Display
  display.setFont(&Monospaced_plain_12);
  Wire.begin();
  Wire.beginTransmission(60);
  if (Wire.endTransmission() == 0) {
    display.begin(SSD1306_SWITCHCAPVCC, 60);
  } else {
    display.begin(SSD1306_SWITCHCAPVCC, 61);
  }
  display.clearDisplay();

  // Get first question
  digitalWrite(CAM_PWR, HIGH);
  startUpScreen();
  initialiseCamera();
  indexQuestions();
  getQuestion(currentQuestionIndex);

  delay(500);

  digitalWrite(CAM_PWR, 0);

  // Display "Select a Task"
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setTextWrap(true);
  display.setCursor(0, 26);
  display.setTextWrap(true);
  display.print(F("Select a Task"));
#ifdef DEBUG
  Serial.println(F("Select"));
#endif
  display.display();
  delay(2000);

  // Display first question
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setTextWrap(true);
  display.setCursor(0, 26);
  display.print(inputBuffer);
  drawTicks(questionTicks);
  display.display();
}

void loop() { // run over and over

  updateSleep();
  updateButtons();
  updateQuestion();
  updatePowerOff();

  if (digitalRead(SHUTTER) == 0 && flag == false) {
    flag = true;
  }
  if (digitalRead(SHUTTER) == 1 && flag == true) {
    flag = false;
    digitalWrite(10, 1);
    display.clearDisplay();
    display.setCursor(12, 38);
    display.println(F("Task Selected"));
    display.display();
    delay(400);
    initialiseCamera();
    delay(200);
    indexQuestions();
    delay(800);
    for (int i = 0; i <= 64; i += 4) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(12, 38 - i);
      display.println(F("Task Selected"));
      display.setCursor(8, 38 + (64 - i));
      display.println(F("Take Photo Now"));
      display.display();
      delay(10);
    }
    //    display.setTextSize(1);
    //    display.setCursor(8, 42);
    //    display.println(F("Take Photo Now"));
    //    display.display();
    boolean waiting = true;
    while (waiting) {
      if (digitalRead(SHUTTER) == 0) {
        waiting = false;
      }
      if (digitalRead(LEFT_BUTTON) == 0 || digitalRead(RIGHT_BUTTON) == 0) {
        digitalWrite(CAM_PWR, LOW);
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setTextWrap(true);
        display.setCursor(0, 26);
        display.print(inputBuffer);
        display.display();
        sleepMillis = millis();
        buttonHeldCount = millis();
        delay(800);
        return;
      }
    }
    delay(200);
    analogWrite(LED, 255);
    drawCamera();
    capturePicture();
    delay(1500);
    getQuestion(currentQuestionIndex);
    display.setCursor(0, 25);
    display.clearDisplay();
    display.println(inputBuffer);
    // animate tick
    if (questionTicks) {
      for (int i = 0; i < questionTicks; i++) {
        if (i == questionTicks - 1) {
          display.display();
          delay(500);
        }
        tick(5 + (i * 12), 8);
        display.display();
      }
    }
    digitalWrite(CAM_PWR, 0);
    sleepMillis = millis();
    buttonHeldCount = millis();
  }

}

void getQuestion(uint8_t question) {
  // Flush serial
  while (camSerial.available() > 0) {
    camSerial.read();
  }

  // q + question num return Qs
  camSerial.write(0x71);
  camSerial.write((byte)question);
  camSerial.write(0x0D);
  camSerial.write(0x0A);

  // Empty input buffer and question ticks.
  for (int i = 0; i < 64 ; i++) {
    inputBuffer[i] = 0;
  }
  questionTicks = 0;

  // Wait for 64 characters.
  while (camSerial.available() < 63) {
    delay(1);
  }

  // Throw away ACKs?
  while ((char)camSerial.peek() == 0x06) {
    camSerial.read();
  }

  // Count question ticks.
  while ((char)camSerial.peek() == '#') {
    questionTicks++;
    camSerial.read();
  }

  // Read question.
  for (uint8_t i = 0; i < 64 - questionTicks; i++) {
    inputBuffer[i] = (char)camSerial.read();
#ifdef DEBUG
    Serial.print(inputBuffer[i]);
#endif
  }
#ifdef DEBUG
  Serial.println();
#endif

  // Flush serial.
  while (camSerial.available() > 0) {
    camSerial.read();
  }
}

// Initialise camera module (blocking)
void initialiseCamera() {
  camSerial.write('~');
  camSerial.write('i');
  camSerial.write(0x0D);
  camSerial.write(0x0A);

  delay(100);

  while (camSerial.available() < 2) {
    // Wait for "INI"
  }

#ifdef DEBUG
  Serial.println(F("Cam init'd"));
#endif

  // Flush serial
  while (camSerial.available() > 0) {
    camSerial.read();
  }
}

// Index questions within SD card
void indexQuestions() {
  // Flush serial
  while (camSerial.available() > 0) {
    camSerial.read();
  }

  //Index Questions command
  camSerial.write(0x7E);
  camSerial.write(0x2B);
  camSerial.write(0x0D);
  camSerial.write(0x0A);

  while (camSerial.available() < 2) {
    // Wait for reply.
  }

  numOfQuestions = (int)camSerial.read();

  // Flush serial
  while (camSerial.available() > 0) {
    camSerial.read();
  }
}

// Capture picture
void capturePicture() {
  // Flush serial
  while (camSerial.available() > 0) {
    camSerial.read();
  }

  camSerial.write(0x21);
  camSerial.write((uint8_t)currentQuestionIndex);
  camSerial.write(0x0D);
  camSerial.write(0x0A);
  delay(1400);

  while (camSerial.available() < 0) {
    delay(1);
  }

  //Ack for when picture finished saving... needs fixing as hang over from oversized Q in buffer
  byte in = camSerial.read();
  if (in == 0x06) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(10, 0);
    display.clearDisplay();
    display.println(F("       ERROR"));
    display.display();
    delay(600);
    display.display();
  }
  else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(35, 40);
    analogWrite(LED, 0);
    drawSaving();
    display.display();
    delay(800);
  }
  while (camSerial.available() > 0) {
    camSerial.read();
  }

  // Wait for saving to complete
  delay(1500);
}

void updatePowerOff() {
  int butRead = digitalRead(SHUTTER);
  if (butRead == 0 && buttonHeld == false) {
    sleepMillis = millis();
    buttonHeldCount = millis();
    buttonHeld = true;
  }
  if (buttonHeld == true) {
    if (millis() - buttonHeldCount > OFF_HOLD_DURATION) {
#ifdef DEBUG
      Serial.println(F("Powering down"));
#endif
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 38);
      drawPowerdown();
      delay(300);
      pwrdwn = true;
      while (1) {
        if (pwrdwn == true) {
          display.clearDisplay();
          display.display();
          digitalWrite(PWR_PIN, 0);
        }
      }
    }
  }
  if (buttonHeld == true && butRead == 1) {
    buttonHeld = false;
  }
}

void updateButtons() {

  if (millis() - buttonCheck > buttonInterval) {
    buttonCheck = millis();

    if (digitalRead(LEFT_BUTTON) == LOW && leftDebounce == false) {
      sleepMillis = millis();
      leftDebounce = true;
      currentQuestionIndex--;
      newQuestion = true;
      dir = false;
      analogWrite(LED, 10);
    }
    if (digitalRead(LEFT_BUTTON) == HIGH && leftDebounce == true) {
      leftDebounce = false;
      analogWrite(LED, 0);
    }

    if (digitalRead(RIGHT_BUTTON) == LOW && rightDebounce == false) {
      sleepMillis = millis();
      rightDebounce = true;
      currentQuestionIndex++;
      dir = true;
      newQuestion = true;
      analogWrite(LED, 10);
    }
    if (digitalRead(RIGHT_BUTTON) == HIGH && rightDebounce == true) {
      rightDebounce = false;
      analogWrite(LED, 0);
    }

    if (currentQuestionIndex >= numOfQuestions) {
      currentQuestionIndex = 0;
    }

    if (currentQuestionIndex < 0) {
      currentQuestionIndex = numOfQuestions - 1;
    }

  }
}


void updateQuestion() {
  if (newQuestion) {
    newQuestion = false;
    digitalWrite(CAM_PWR, 1);
    for (byte j = 0; j < 9; j++) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setTextWrap(true);
      display.setCursor(0, 26);
      display.print(inputBuffer);
      for (byte i = 0; i < 3; i++) {
        if (i == j % 3) display.fillCircle(105 + (7 * i), 58, 2, WHITE);
        else display.drawCircle(105 + (7 * i), 58, 2, WHITE);
      }
      display.display();
      delay(100);
    }
    //initialiseCamera();
    indexQuestions();
    delay(300);
    strncpy(prevQuestion, inputBuffer, 64);
    getQuestion(currentQuestionIndex);
    if (dir == 1) {
      moveDown();
    } else {
      moveUp();
    }
    drawTicks(questionTicks);
    display.display();
    scrollingPos = 0;
    delay(50);
    digitalWrite(CAM_PWR, 0);
  }
}

// Checks if enough time has passed to put the camera to sleep.
void updateSleep() {
  if (millis() - sleepMillis > SLEEPTIME) {
#ifdef DEBUG
    Serial.println("SLEEP");
    Serial.println(sleepMillis);
#endif
    display.clearDisplay();
    display.drawBitmap(32, 0, powering, 64, 64, 1);
    display.display();
    delay(1000);
    digitalWrite(CAM_PWR, LOW);
    digitalWrite(PWR_PIN, LOW);
  }
}

// Displays the ProbeTools logo.
void startUpScreen() {
#ifdef DEBUG
  Serial.println(F("startup"));
#endif
  display.clearDisplay();
  display.drawBitmap(0, 0, logo16_glcd_bmp, 128, 64, 1);
  display.display();
  delay(1700);
}

// Draw a single tick.
void tick(int x, int y) {
  display.drawLine(x, y - 3, x + 3, y, WHITE);
  display.drawLine(x + 3, y, x + 3 + 6, y - 6, WHITE);
}

// Draw a number of ticks
void drawTicks(byte tickNum) {
  //byte tickNum = questionTicks;
  if (tickNum) {
    for (int i = 0; i < tickNum; i++) {
      tick(5 + (i * 12), 8);
    }
  }
}

// Draw power down icon
void drawPowerdown() {
  display.clearDisplay();
  display.drawBitmap(32, 0, powering, 64, 64, 1);
  display.display();
  delay(1000);
}

// Draw saving icon
void drawSaving() {
  display.clearDisplay();
  display.drawBitmap(32, 0, saved, 64, 64, 1);
  display.display();
  delay(800);
}

// Draw camera icon
void drawCamera() {
  display.clearDisplay();
  display.drawBitmap(32, 8, cam_logo, 64, 49, 1);
  display.display();
}

// Move question upwards
void moveUp() {
  scrollingPos = 26;
  int currentScrollingPos = scrollingPos;

  for (int i = 0; i < 54; i += 3) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setTextWrap(true);

    // Current question
    display.setCursor(0, 26 + i);
    display.print(prevQuestion);

    // Previous question
    display.setCursor(0, (long)26 * -1 + i);
    display.print(inputBuffer);

    display.display();

    delay(10);
  }

  // Horizontal
  //  for (int i = 0; i < 128; i+= 4) {
  //    display.clearDisplay();
  //    display.setTextSize(1);
  //    display.setTextColor(WHITE);
  //    display.setTextWrap(true);
  //
  //    display.setCursor(i * -1, 26);
  //    display.print(prevQuestion);
  //
  //    display.setCursor(128 - i+1, 26);
  //    display.print(inputBuffer);
  //
  //    display.display();
  //  }
}

// Move question downwards
void moveDown() {
  int currentScrollingPos = scrollingPos;
  scrollingPos = 26;

  for (int i = 0; i < 54; i += 3) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setTextWrap(true);

    // Current question
    display.setCursor(0, 26 + i * -1);
    display.print(prevQuestion);

    // Next question
    display.setCursor(0, (long)78 + i * -1);
    display.print(inputBuffer);

    display.display();

    delay(10);
  }

  // Horizontal
  //  for (int i = 0; i < 128; i+= 4) {
  //    display.clearDisplay();
  //    display.setTextSize(1);
  //    display.setTextColor(WHITE);
  //    display.setTextWrap(true);
  //
  //    display.setCursor(i, 26);
  //    display.print(prevQuestion);
  //
  //    display.setCursor((128 - i+1) * -1, 26);
  //    display.print(inputBuffer);
  //
  //    display.display();
  //  }
}
