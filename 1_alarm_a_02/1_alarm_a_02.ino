#include "RTC.h"
#include "Arduino_LED_Matrix.h"

ArduinoLEDMatrix matrix;

// ========================================
// ALARM SETTINGS
// ========================================

int alarmHour = 13;
int alarmMinute = 41;

bool alarmActive = false;
bool alarmTriggered = false;

// Prevent sending ALARM repeatedly
bool alarmSent = false;

// Button control
const int buttonPin = 2;
bool lastButtonState = HIGH;

// Potentiometer mode control
const int potPin = A0;
String currentMode = "";


// ========================================
// 3 x 5 PIXEL FONT
// ========================================

const byte digits[10][5] = {

  {0b111, 0b101, 0b101, 0b101, 0b111}, // 0
  {0b010, 0b110, 0b010, 0b010, 0b111}, // 1
  {0b111, 0b001, 0b111, 0b100, 0b111}, // 2
  {0b111, 0b001, 0b111, 0b001, 0b111}, // 3
  {0b101, 0b101, 0b111, 0b001, 0b001}, // 4
  {0b111, 0b100, 0b111, 0b001, 0b111}, // 5
  {0b111, 0b100, 0b111, 0b101, 0b111}, // 6
  {0b111, 0b001, 0b001, 0b001, 0b001}, // 7
  {0b111, 0b101, 0b111, 0b101, 0b111}, // 8
  {0b111, 0b101, 0b111, 0b001, 0b111}  // 9
};


// ========================================
// MATRIX FRAME
// ========================================

byte frame[8][12];


// ========================================
// CLEAR MATRIX
// ========================================

void clearFrame() {

  for (int row = 0; row < 8; row++) {

    for (int col = 0; col < 12; col++) {

      frame[row][col] = 0;

    }
  }
}


// ========================================
// DRAW DIGIT
// ========================================

void drawDigit(int number, int startColumn) {

  for (int row = 0; row < 5; row++) {

    for (int col = 0; col < 3; col++) {

      if (digits[number][row] & (1 << (2 - col))) {

        frame[row + 1][startColumn + col] = 1;

      }
    }
  }
}


// ========================================
// DISPLAY TWO DIGITS
// ========================================

void displayTwoDigits(int number) {

  clearFrame();

  int firstDigit = number / 10;
  int secondDigit = number % 10;

  drawDigit(firstDigit, 2);
  drawDigit(secondDigit, 7);

  matrix.renderBitmap(frame, 8, 12);
}


// ========================================
// SETUP
// ========================================

void setup() {

  Serial.begin(9600);

  RTC.begin();
 
  /*
  RTCTime startTime(
  25,
  Month::SEPTEMBER,
  2026,
  13,
  34,
  00,
  DayOfWeek::FRIDAY,
  SaveLight::SAVING_TIME_ACTIVE
  );

  RTC.setTime(startTime);
  */

  matrix.begin();

  pinMode(buttonPin, INPUT_PULLUP);

}


// ========================================
// LOOP
// ========================================

void loop() {

  // ----------------------------------------
  // READ CURRENT TIME
  // ----------------------------------------

  RTCTime currentTime;

  RTC.getTime(currentTime);

  int hour = currentTime.getHour();
  int minute = currentTime.getMinutes();

  // ----------------------------------------
  // READ DISMISS BUTTON
  // ----------------------------------------

  bool buttonState = digitalRead(buttonPin);

  if (lastButtonState == HIGH && buttonState == LOW) {
    Serial.println("DISMISS");
  }

  lastButtonState = buttonState;

  // ----------------------------------------
  // READ MODE POTENTIOMETER
  // ----------------------------------------

  int potValue = analogRead(potPin);

  String newMode;

  if (potValue <= 80) {
    newMode = "EASY";
  }
  else if (potValue >= 943) {
    newMode = "HARD";
  }
  else {
    newMode = "MEDIUM";
  }

  if (newMode != currentMode) {
    currentMode = newMode;
    Serial.println("MODE:" + currentMode);
  }

  // ----------------------------------------
  // CHECK ALARM TIME
  // ----------------------------------------

  bool isAlarmMinute =
    (hour == alarmHour && minute == alarmMinute);


  if (isAlarmMinute && !alarmTriggered) {

    alarmActive = true;

    alarmTriggered = true;

  }


  if (!isAlarmMinute) {

    alarmTriggered = false;
    alarmSent = false;
    alarmActive = false;

  }


  // ----------------------------------------
  // SEND ALARM TO PROCESSING
  // ----------------------------------------

  if (alarmActive && !alarmSent) {

    Serial.println("ALARM");

    alarmSent = true;

  }


  // ----------------------------------------
  // DISPLAY TIME
  // ----------------------------------------
  
  // if ((millis() / 2000) % 2 == 0) {
  // displayTwoDigits(hour);
  // }
  // else {
  displayTwoDigits(minute);
  // }

  // ----------------------------------------
  // SERIAL MONITOR
  // ----------------------------------------

  Serial.print("TIME,");

  if (hour < 10) {
    Serial.print("0");
  }

  Serial.print(hour);

  Serial.print(":");

  if (minute < 10) {
    Serial.print("0");
  }

  Serial.print(minute);


  if (alarmActive) {

    Serial.println(",ACTIVE");

  }

  else {

    Serial.println(",WAITING");

  }


  delay(200);

}