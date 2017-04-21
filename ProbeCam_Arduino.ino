/*
   TODO

   - Dynamic speed change based on length of question
*/
#include <SoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

SoftwareSerial cam(2, 3);

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

unsigned long prevMillis;
unsigned long currentMillis;

uint8_t savingAnimationCounter = 0;

// Animation speeds in milliseconds.
#define SPEED_SAVING 50
#define SPEED_SCROLLING 10

int numOfQuestions = 0;

int currentQuestion = 0;
String currentQuestionString;
int currentQuestionTicks;

int scrollingPos = 128;
int slidingPos = 0;

#define LEFT 8
#define RIGHT 9
#define SHUTTER 6
#define SLEEP 7

boolean saving;
boolean sleeping;

unsigned long prevSecond;
int secondsElapsed;

void setup() {
  cam.begin(9600);
  Serial.begin(9600);

  pinMode(10, OUTPUT);
  digitalWrite(10, LOW);
  digitalWrite(10, HIGH);

  Serial.println("hello");
  // Start and clear display.
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 16);
  display.setTextWrap(false);
  display.print("ProbeCam");
  display.display();

  pinMode(LEFT, INPUT_PULLUP);
  pinMode(RIGHT, INPUT_PULLUP);
  pinMode(SHUTTER, INPUT_PULLUP);
  pinMode(SLEEP, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(SLEEP), wake, LOW);
  
  // Get number of questions
  //cam.println("$");
  String incomingCommand;
  while (numOfQuestions == 0) {
    Serial.println("Waiting...");
    if (cam.available() > 0) {
      Serial.println("Received from camera...");
      char inByte = cam.read();
      if (inByte == 0x0A) numOfQuestions = incomingCommand.toInt();
      else if(inByte == 'R') {
        Serial.println("Cam online");
        cam.println("$");
      }
      else incomingCommand += (char)inByte;
    }
  }

  Serial.print(numOfQuestions);
  Serial.println(" questions.");

  currentQuestionString = getQuestion(currentQuestion);
  //Serial.println(getQuestion(1));
}

void loop() {
  if (digitalRead(LEFT) == LOW) {
    secondsElapsed = 0;
    if (currentQuestion != 0) moveUp();
  }
  if (digitalRead(RIGHT) == LOW) {
    secondsElapsed = 0;
    if (currentQuestion != numOfQuestions - 1) moveDown();
  }
  if (digitalRead(SHUTTER) == LOW) {
    if (!sleeping) {
      secondsElapsed = 0;
      takePicture();
    }
  }
  if (digitalRead(SLEEP) == LOW) {
    if (sleeping) {
      sleeping = false;
      delay(500);
    }
    else {
      if (!saving) {
        sleep();
      }
    }
  }

  //  savingScreen();
  if (!sleeping) {
    if (millis() % 1000 == 0) secondsElapsed++;
    if (secondsElapsed > 300) sleep();
    if (saving) {
      savingScreen();
      while (cam.available() > 0) {
        char inByte = cam.read();
        if (inByte == 0x0A) {
          saving = false;
          currentQuestionTicks++;
        }
      }
    }
    else scrollText();
  }
}

void savingScreen() {
  currentMillis = millis();
  if (currentMillis - prevMillis >= SPEED_SAVING) {
    display.clearDisplay();
    if (savingAnimationCounter > 6) savingAnimationCounter = 0;
    // Draw diagonal lines.
    for (int i = -32; i < 26; i++) display.drawLine(i * 7 + 32 + savingAnimationCounter, 0, i * 7 + savingAnimationCounter, 32, WHITE);
    display.display();

    savingAnimationCounter++;
    Serial.println(savingAnimationCounter);
    prevMillis = currentMillis;
  }
}

void scrollText() {
  currentMillis = millis();
  if (currentMillis - prevMillis >= SPEED_SCROLLING) {
    //Serial.println();
    if (scrollingPos < (long)currentQuestionString.length() * 12 * -1) scrollingPos = 128;
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(scrollingPos, 16);
    display.setTextWrap(false);
    display.print(currentQuestionString);
    //tick(5, 8);
    applyTicks();
    display.display();

    scrollingPos -= 3;

    prevMillis = currentMillis;
  }
}

void moveUp() {
  Serial.println("UP!");
  int currentScrollingPos = scrollingPos;
  scrollingPos = 10;

  String nextQuestion = getQuestion(currentQuestion - 1);

  for (int i = 0; i < 32; i += 3) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setTextWrap(false);

    // Current question
    display.setCursor(currentScrollingPos, 16 + i);
    display.print(currentQuestionString);

    // Previous question
    display.setCursor(scrollingPos, (long)16 * -1 + i);
    display.print(nextQuestion);

    display.display();

    delay(10);
  }
  currentQuestion--;
  currentQuestionString = nextQuestion;
}

void moveDown() {
  Serial.println("DOWN!");
  int currentScrollingPos = scrollingPos;
  scrollingPos = 10;

  String nextQuestion = getQuestion(currentQuestion + 1);

  for (int i = 0; i < 32; i += 3) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setTextWrap(false);

    // Current question
    display.setCursor(currentScrollingPos, 16 + i * -1);
    display.print(currentQuestionString);

    // Next question
    display.setCursor(scrollingPos, (long)48 + i * -1);
    display.print(nextQuestion);

    display.display();

    delay(10);
  }
  currentQuestion++;
  currentQuestionString = nextQuestion;
}

void takePicture() {
  cam.println("!" + String(currentQuestion));
  for (int i = 0; i < 128 / 2 + 20; i += 10) {
    display.fillCircle(128 / 2, 32 / 2, i, WHITE);
    display.display();
    delay(10);
  }
  delay(500);
  saving = true;
}

String getQuestion(int q) {
  String incoming;
  String cmd = "?" + String(q);
  cam.println(cmd);
  boolean endFlag = false;

  while (!endFlag) {
    if (cam.available() > 0) {
      char inByte = cam.read();
      if (inByte == 0x0A) {
        endFlag = true;
      }
      else incoming += (char)inByte;
    }
  }

  // Take out ticks.
  currentQuestionTicks = 0;
  while (incoming.charAt(0) == '#') {
    currentQuestionTicks++;
    incoming = incoming.substring(1);
  }

  return incoming;
}

void sleep() {
  display.clearDisplay();
  display.display();
  sleeping = true;
  cam.println('S');
  delay(5000);
  //disableADC();
  //enableSleepMode();
  //disableBOD();
  //__asm__ __volatile__("sleep");
}

void wake() {
  sleeping = false;
}

void applyTicks() {
  if (currentQuestionTicks) {
    for (int i = 0; i < currentQuestionTicks; i++) {
      tick(5 + (i * 12), 8);
    }
  }
}

void tick(int x, int y) {
  display.drawLine(x, y - 3, x + 3, y, WHITE);
  display.drawLine(x + 3, y, x + 3 + 6, y - 6, WHITE);
}

// Power saving

void disableADC() {
  ADCSRA &= ~(1 << 7);
}

void enableADC() {
  ADCSRA |= (1 << 7);
}

void enableSleepMode() {
  SMCR |= (1 << 2);
  SMCR |= 1;
}

void disableBOD() {
  MCUCR |= (3 << 5);
  MCUCR = (MCUCR & ~(1 << 5)) | (1 << 6);
}

