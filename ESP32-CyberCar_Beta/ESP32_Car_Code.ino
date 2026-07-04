#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

// TB6612FNG Pins
#define AIN1 27
#define AIN2 26
#define PWMA 14
#define BIN1 33
#define BIN2 32
#define PWMB 25
#define STBY 4

int speedVal = 200; // Speed 0–255

void setup() {
  Serial.begin(9600);
  SerialBT.begin("ESP32_Car");  // Bluetooth device name

  pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT); pinMode(PWMA, OUTPUT);
  pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT); pinMode(PWMB, OUTPUT);
  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, HIGH);  // Enable motors
}

void loop() {
  if (SerialBT.available()) {
    char cmd = SerialBT.read();
    handleCommand(cmd);
  }
}

void handleCommand(char cmd) {
  switch (cmd) {
    case 'F': forward(); break;
    case 'B': backward(); break;
    case 'L': turnLeft(); break;
    case 'R': turnRight(); break;
    case 'S': stopMotors(); break;
    case '1': speedVal = 100; break;
    case '2': speedVal = 150; break;
    case '3': speedVal = 200; break;
    case '4': speedVal = 255; break;
  }
}

// Movement functions
void forward() {
  digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW);
  analogWrite(PWMA, speedVal);
  analogWrite(PWMB, speedVal);
}

void backward() {
  digitalWrite(AIN1, LOW); digitalWrite(AIN2, HIGH);
  digitalWrite(BIN1, LOW); digitalWrite(BIN2, HIGH);
  analogWrite(PWMA, speedVal);
  analogWrite(PWMB, speedVal);
}

void turnLeft() {
  digitalWrite(AIN1, LOW); digitalWrite(AIN2, LOW);  // Left motor stop
  digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW); // Right motor forward
  analogWrite(PWMA, 0);
  analogWrite(PWMB, speedVal);
}

void turnRight() {
  digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW); // Left motor forward
  digitalWrite(BIN1, LOW); digitalWrite(BIN2, LOW);  // Right motor stop
  analogWrite(PWMA, speedVal);
  analogWrite(PWMB, 0);
}

void stopMotors() {
  analogWrite(PWMA, 0);
  analogWrite(PWMB, 0);
}