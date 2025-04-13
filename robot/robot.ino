#include <SoftwareSerial.h>

SoftwareSerial BTSerial(10, 11); // RX, TX

// L298N Motor Pins
const int IN1 = 2;  // Left motor direction
const int IN2 = 3;
const int IN3 = 4;  // Right motor direction
const int IN4 = 5;

const int ENA = 6;  // PWM for left motors
const int ENB = 9;  // PWM for right motors

const int SPEED_LEFT = 200;   // You can adjust this (0–255)
const int SPEED_RIGHT = 200;

void setup() {
  Serial.begin(9600);
  BTSerial.begin(9600);
  Serial.println("HC-05 ready");

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  stopMotors();
}

String command = "";

void loop() {
  while (BTSerial.available()) {
    char c = BTSerial.read();

    if (c == '\n' || c == '\r') {
      if (command.length() > 0) {
        command.trim();  // Clean up spaces
        Serial.println("Received: " + command);
        handleCommand(command);
        command = "";  // Clear for next command
      }
    } else {
      command += c;
    }
  }
}


void handleCommand(String cmd) {
  if (cmd == "FORWARD") {
    moveForward();
  } else if (cmd == "BACKWARD") {
    moveBackward();
  } else if (cmd == "LEFT") {
    turnLeft();
  } else if (cmd == "RIGHT") {
    turnRight();
  } else if (cmd == "FORWARD LEFT") {
    moveForwardLeft();
  } else if (cmd == "FORWARD RIGHT") {
    moveForwardRight();
  } else if (cmd == "BACKWARD LEFT") {
    moveBackwardLeft();
  } else if (cmd == "BACKWARD RIGHT") {
    moveBackwardRight();
  } else if (cmd == "STOP") {
    stopMotors();
  }
}

// Motion Functions
void moveForward() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); // Left forward
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); // Right forward
  analogWrite(ENA, SPEED_LEFT);
  analogWrite(ENB, SPEED_RIGHT);
}

void moveBackward() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); // Left backward
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); // Right backward
  analogWrite(ENA, SPEED_LEFT);
  analogWrite(ENB, SPEED_RIGHT);
}

void turnLeft() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);  // Left stop
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); // Right forward
  analogWrite(ENA, 0);
  analogWrite(ENB, SPEED_RIGHT);
}

void turnRight() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); // Left forward
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);  // Right stop
  analogWrite(ENA, SPEED_LEFT);
  analogWrite(ENB, 0);
}

void moveForwardLeft() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  analogWrite(ENA, SPEED_LEFT * 0.6);  // slower left
  analogWrite(ENB, SPEED_RIGHT);
}

void moveForwardRight() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  analogWrite(ENA, SPEED_LEFT);
  analogWrite(ENB, SPEED_RIGHT * 0.6);  // slower right
}

void moveBackwardLeft() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
  analogWrite(ENA, SPEED_LEFT * 0.6);
  analogWrite(ENB, SPEED_RIGHT);
}

void moveBackwardRight() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
  analogWrite(ENA, SPEED_LEFT);
  analogWrite(ENB, SPEED_RIGHT * 0.6);
}

void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}
