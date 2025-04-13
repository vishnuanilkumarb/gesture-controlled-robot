#include <Wire.h>
#include <BluetoothSerial.h>  // Include BluetoothSerial library

#define MPU_ADDR 0x68

int16_t gyroX_raw, gyroY_raw, gyroZ_raw;
float gyroX, gyroY, gyroZ;
float gyroY_center = 0, gyroZ_center = 0;

int16_t accY_init, accZ_init;
int16_t accY_raw, accZ_raw;

unsigned long lastOutputTime = 0;
const unsigned long debounceDelay = 300;

String lastOutput = "";

const unsigned long comboWindow = 400;
String firstDirection = "";
unsigned long firstDirectionTime = 0;
bool comboInProgress = false;

bool comboLockActive = false;
String comboMainDirection = "";

// BUTTON MODE VARIABLES
const int FORWARD_BTN = 4;
const int BACKWARD_BTN = 5;
const int LEFT_BTN = 23;
const int RIGHT_BTN = 19;
const int MODE_BTN = 18;

bool buttonMode = true;
String prevBtnDir = "";
bool allowNewBtnInput = true;

// LONG PRESS TRACKING
unsigned long modeBtnPressTime = 0;
bool recalibrationDone = false;
const unsigned long longPressDuration = 2000;  // 2 seconds for long press
bool longPressTriggered = false;

// BUTTON COMBO TRACKING
unsigned long btnPressStartTime = 0;
bool waitingForCombo = false;
String tempSingleDir = "";
const unsigned long comboDelay = 100;

// Bluetooth setup
BluetoothSerial BT;  // BluetoothSerial object for Bluetooth Classic communication
bool connected = false;  // Track if the ESP32 is connected to the HC-05

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // Start Bluetooth communication
  BT.begin("ESP32_Master",true);  // Replace with your device name

  pinMode(FORWARD_BTN, INPUT_PULLUP);
  pinMode(BACKWARD_BTN, INPUT_PULLUP);
  pinMode(LEFT_BTN, INPUT_PULLUP);
  pinMode(RIGHT_BTN, INPUT_PULLUP);
  pinMode(MODE_BTN, INPUT_PULLUP);

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  delay(1000);
  calibrateGyro();
  captureInitialAccel();

  Serial.println("Gesture + Button Mode Ready (Bluetooth Enabled)");
  BT.println("Bluetooth Initialized");

  // Attempt to connect to HC-05
  connectToHC05();
}

void loop() {
  // If not connected, try to connect
  if (!connected) {
    connectToHC05();
  }

  // Handle mode switching with toggle button
  if (digitalRead(MODE_BTN) == HIGH) {
    if (modeBtnPressTime == 0) {
      modeBtnPressTime = millis(); // Record the time when the button is pressed
    } else if (millis() - modeBtnPressTime > debounceDelay) {
      // Switch mode after debounce delay
      buttonMode = !buttonMode; // Toggle buttonMode
      modeBtnPressTime = 0;  // Reset the press time
      longPressTriggered = false; // Reset the long press trigger flag
    }
  } else {
    // Check if the button is being held for a long press
    if (modeBtnPressTime > 0 && millis() - modeBtnPressTime >= longPressDuration && !longPressTriggered) {
      // Long press detected, reset the stop position
      resetStopPosition();
      longPressTriggered = true;
    }
  }

  // Print the current mode whenever it changes
  static bool prevMode = false;
  if (buttonMode != prevMode) {
    if (buttonMode) {
      Serial.println("🟢 BUTTON MODE ENABLED");
      BT.println("🟢 BUTTON MODE ENABLED");
    } else {
      Serial.println("🌀 GYRO MODE ENABLED");
      BT.println("🌀 GYRO MODE ENABLED");
    }
    prevMode = buttonMode;  // Update prevMode to the current mode
  }

  // Handle the corresponding mode
  if (buttonMode) {
    handleButtonControl();  // Call button control function when in button mode
  } else {
    handleGyroControl();  // Call gyro control function when in gyro mode
  }
}

// Attempt to connect to HC-05
void connectToHC05() {
  if (!BT.connected()) {
    Serial.println("Attempting to connect to HC-05...");
    BT.connect("HC-05");  // Try connecting to HC-05 by its Bluetooth name
    if (BT.connected()) {
      connected = true;
      Serial.println("Connected to HC-05!");
      BT.println("Connected to HC-05!");
    } else {
      Serial.println("Connection failed. Retrying...");
      delay(1000);  // Retry after 1 second
    }
  }
}

// Reset the stop position by recalibrating the accelerometer and gyro
void resetStopPosition() {
  captureInitialAccel();  // Re-capture initial accelerometer values
  calibrateGyro();        // Re-calibrate the gyroscope
  Serial.println("Stop position reset!");
  BT.println("Stop position reset!");
}

// Handle Button Control Mode
void handleButtonControl() {
  bool fwd = digitalRead(FORWARD_BTN) == HIGH;
  bool bwd = digitalRead(BACKWARD_BTN) == HIGH;
  bool lft = digitalRead(LEFT_BTN) == HIGH;
  bool rgt = digitalRead(RIGHT_BTN) == HIGH;

  String btnDir = "";

  // Check for no buttons pressed -> send STOP
  if (!fwd && !bwd && !lft && !rgt) {
    if (prevBtnDir != "STOP") {
      sendDirection("STOP");
      prevBtnDir = "STOP";
    }
    allowNewBtnInput = true;
    waitingForCombo = false;
    tempSingleDir = "";
    btnPressStartTime = 0;
    return;
  }

  // Combo buttons first
  if (fwd && lft) btnDir = "FORWARD LEFT";
  else if (fwd && rgt) btnDir = "FORWARD RIGHT";
  else if (bwd && lft) btnDir = "BACKWARD LEFT";
  else if (bwd && rgt) btnDir = "BACKWARD RIGHT";

  if (btnDir != "") {
    sendIfNewDirection(btnDir);
    allowNewBtnInput = false;
    waitingForCombo = false;
    return;
  }

  // Handle single button press
  if (fwd) btnDir = "FORWARD";
  else if (bwd) btnDir = "BACKWARD";
  else if (lft) btnDir = "LEFT";
  else if (rgt) btnDir = "RIGHT";

  unsigned long now = millis();

  if (btnDir != tempSingleDir) {
    tempSingleDir = btnDir;
    btnPressStartTime = now;
    waitingForCombo = true;
  } else if (waitingForCombo && now - btnPressStartTime > comboDelay) {
    sendIfNewDirection(tempSingleDir);
    waitingForCombo = false;
    allowNewBtnInput = false;
  }
}

// Send new direction if it's different from the last one
void sendIfNewDirection(String dir) {
  if (dir != prevBtnDir && allowNewBtnInput) {
    sendDirection(dir);
    prevBtnDir = dir;
    if (dir.indexOf(" ") > 0) {
      allowNewBtnInput = false;  // For combos, wait until all buttons are released
    }
  }
}

// Handle Gyro Control Mode
void handleGyroControl() {
  readMPUGyro();
  readMPUAccel();

  float deltaY = gyroY - gyroY_center;
  float deltaZ = gyroZ - gyroZ_center;

  float thresholdZ = 40.0;
  float thresholdY = 40.0;

  unsigned long now = millis();

  if (isInStopPosition()) {
    if (lastOutput != "STOP") {
      sendDirection("STOP");
      resetCombo();
      comboLockActive = false;
    }
    return;
  }

  String newDirection = "";
  if (deltaZ > thresholdZ) newDirection = "FORWARD";
  else if (deltaZ < -thresholdZ) newDirection = "BACKWARD";
  else if (deltaY > thresholdY) newDirection = "RIGHT";
  else if (deltaY < -thresholdY) newDirection = "LEFT";

  if (comboLockActive) {
    if (newDirection == comboMainDirection || newDirection == "STOP") {
      comboLockActive = false;
      resetCombo();
      if (newDirection != "STOP") sendDirection(newDirection);
    }
    return;
  }

  if (newDirection != "" && newDirection != lastOutput) {
    if (!comboInProgress) {
      firstDirection = newDirection;
      firstDirectionTime = now;
      comboInProgress = true;
    } else if (now - firstDirectionTime <= comboWindow) {
      if (newDirection != firstDirection) {
        String combined = combineDirections(firstDirection, newDirection);
        if (combined != "") {
          sendDirection(combined);
          comboMainDirection = getMainDirection(combined);
          comboLockActive = true;
          resetCombo();
          return;
        } else {
          resetCombo();
          return;
        }
      }
    }
  }

  if (comboInProgress && now - firstDirectionTime > comboWindow) {
    sendDirection(firstDirection);
    resetCombo();
  }
}

// Combine two directions into a combo direction
String combineDirections(String first, String second) {
  if ((first == "FORWARD" && second == "LEFT") || (first == "LEFT" && second == "FORWARD"))
    return "FORWARD LEFT";
  if ((first == "FORWARD" && second == "RIGHT") || (first == "RIGHT" && second == "FORWARD"))
    return "FORWARD RIGHT";
  if ((first == "BACKWARD" && second == "LEFT") || (first == "LEFT" && second == "BACKWARD"))
    return "BACKWARD LEFT";
  if ((first == "BACKWARD" && second == "RIGHT") || (first == "RIGHT" && second == "BACKWARD"))
    return "BACKWARD RIGHT";
  return "";
}

String getMainDirection(String combo) {
  if (combo.startsWith("FORWARD")) return "FORWARD";
  if (combo.startsWith("BACKWARD")) return "BACKWARD";
  return "";
}

// Reset combo tracking
void resetCombo() {
  comboInProgress = false;
  firstDirection = "";
  firstDirectionTime = 0;
}

// Send direction to Bluetooth serial
void sendDirection(String dir) {
  unsigned long now = millis();
  if (dir != lastOutput && (now - lastOutputTime >= debounceDelay)) {
    Serial.println("Direction: " + dir);
    BT.println(dir);  // Send direction over Bluetooth to slave
    lastOutput = dir;
    lastOutputTime = now;
  }
}

// Read gyroscope data from the MPU6050
void readMPUGyro() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x43);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);

  gyroX_raw = Wire.read() << 8 | Wire.read();
  gyroY_raw = Wire.read() << 8 | Wire.read();
  gyroZ_raw = Wire.read() << 8 | Wire.read();

  gyroX = gyroX_raw / 131.0;
  gyroY = gyroY_raw / 131.0;
  gyroZ = gyroZ_raw / 131.0;
}

// Read accelerometer data from the MPU6050
void readMPUAccel() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3D);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 4, true);

  accY_raw = Wire.read() << 8 | Wire.read();
  accZ_raw = Wire.read() << 8 | Wire.read();
}

// Capture initial accelerometer values
void captureInitialAccel() {
  readMPUAccel();
  accY_init = accY_raw;
  accZ_init = accZ_raw;
}

// Check if the device is in the stop position based on accelerometer data
bool isInStopPosition() {
  const int accelTolerance = 2000;
  return abs(accY_raw - accY_init) < accelTolerance &&
         abs(accZ_raw - accZ_init) < accelTolerance;
}

// Calibrate the gyroscope by averaging samples
void calibrateGyro() {
  long sumY = 0, sumZ = 0;
  int samples = 100;
  for (int i = 0; i < samples; i++) {
    readMPUGyro();
    sumY += gyroY;
    sumZ += gyroZ;
    delay(5);
  }
  gyroY_center = sumY / samples;
  gyroZ_center = sumZ / samples;
}
