#include <Arduino.h>

#include <Dabble.h> // Dabble library
#include <SoftwareSerial.h>

// --- HM-10 AT Rename (automatic) ---
// On startup we optionally attempt to send AT commands to set a desired name BEFORE starting Dabble.
// If the module is already connected to a phone, AT commands will usually fail (that's fine; we continue).
// Set BT_DESIRED_NAME to desired non-empty name (max ~12 chars for HM-10) or leave blank "" to skip rename.
#define BT_DESIRED_NAME "ROFL_COPTER"  // e.g. "CARBLE"; empty string disables rename attempt

static const uint8_t BT_RX_PIN = 2; // HM-10 TX -> Arduino D2 (Arduino RX)
static const uint8_t BT_TX_PIN = 3; // HM-10 RX -> Arduino D3 (Arduino TX) (voltage divider recommended)
static const unsigned long AT_COMMAND_TIMEOUT_MS = 1500;

SoftwareSerial btSerial(BT_RX_PIN, BT_TX_PIN); // RX, TX (only used before Dabble.begin)

// ---------------- Motor Pin Configuration ----------------
// Adjust according to your motor driver (e.g., L298N / L293D)
// Two DC motors: Left (A), Right (B)
const uint8_t IN1 = 5;   // Left motor forward (PWM capable)
const uint8_t IN2 = 6;   // Left motor backward (PWM capable)
const uint8_t IN3 = 9;   // Right motor forward (PWM capable)
const uint8_t IN4 = 10;   // Right motor backward (PWM capable)

uint8_t speedPct = 70;   // Default speed percentage (0-100)

// Map percentage to 0-255 PWM
uint8_t pwmFromPct(uint8_t pct) { return (uint16_t)pct * 255 / 100; }

void stopMotors() {
  analogWrite(IN1, 0); analogWrite(IN2, 0);
  analogWrite(IN3, 0); analogWrite(IN4, 0);
}

void driveForward() {
  analogWrite(IN1, pwmFromPct(speedPct)); analogWrite(IN2, 0);
  analogWrite(IN3, pwmFromPct(speedPct)); analogWrite(IN4, 0);
}

void driveBackward() {
  analogWrite(IN1, 0); analogWrite(IN2, pwmFromPct(speedPct));
  analogWrite(IN3, 0); analogWrite(IN4, pwmFromPct(speedPct));
}

void turnLeft() {
  // Left motor slower / reverse, right forward
  analogWrite(IN1, 0); analogWrite(IN2, pwmFromPct(speedPct/2));
  analogWrite(IN3, pwmFromPct(speedPct)); analogWrite(IN4, 0);
}

void turnRight() {
  analogWrite(IN1, pwmFromPct(speedPct)); analogWrite(IN2, 0);
  analogWrite(IN3, 0); analogWrite(IN4, pwmFromPct(speedPct/2));
}

void setupPins() {
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  stopMotors();
}

// Forward declarations for AT helpers
void attemptAutoRename();
void sendAtCommand(const String& cmd);
bool waitForBtResponse(const char* expect, unsigned long timeoutMs);
void flushBtInput(unsigned long waitMs = 30);

void setupSerial() {
  Serial.begin(9600);
  while(!Serial) { /* wait for native USB (not needed on Uno, but safe) */ }
  Serial.println(F("Startup: attempting optional HM-10 rename (if enabled)"));
  attemptAutoRename();
  Serial.println(F("Remote Car Starting (HM-10 BLE / Dabble)"));
  Dabble.begin(9600); // After AT attempts so Dabble can own the SoftwareSerial
}

void setup() {
  setupPins();
  setupSerial();
}

unsigned long lastStatusMs = 0;

void loopBLE() {
  Dabble.processInput();
  // Using GamePad module: Up, Down, Left, Right, Start (Stop), Triangle/Square etc
  bool moved = false;
  if (GamePad.isUpPressed())        { driveForward(); moved = true; }
  else if (GamePad.isDownPressed()) { driveBackward(); moved = true; }
  else if (GamePad.isLeftPressed()) { turnLeft(); moved = true; }
  else if (GamePad.isRightPressed()){ turnRight(); moved = true; }
  else if (GamePad.isStartPressed() || GamePad.isSelectPressed()) { stopMotors(); moved = true; }

  // Use analog joystick (if available) for proportional speed
  int joyX = GamePad.getXaxisData(); // -7 .. +7
  int joyY = GamePad.getYaxisData(); // -7 .. +7 (forward positive?)
  if (!moved && (joyX != 0 || joyY != 0)) {
    // Simple differential drive from joystick
    int forward = joyY; // -7..7
    int turn = joyX;    // -7..7
    int left = forward - turn / 2;
    int right = forward + turn / 2;
    auto clamp = [](int v){ if (v>7) v=7; if (v<-7) v=-7; return v; };
    left = clamp(left); right = clamp(right);
    int base = 255; // max PWM
    int lPWM = map(left, -7, 7, -base, base);
    int rPWM = map(right, -7, 7, -base, base);
    // Apply to motors
    if (lPWM >=0) { analogWrite(IN1, lPWM); analogWrite(IN2,0);} else { analogWrite(IN1,0); analogWrite(IN2,-lPWM);} 
    if (rPWM >=0) { analogWrite(IN3, rPWM); analogWrite(IN4,0);} else { analogWrite(IN3,0); analogWrite(IN4,-rPWM);} 
  } else if (!moved && joyX == 0 && joyY == 0) {
    // Joystick released: ensure we stop (dead-man style)
    stopMotors();
  }
  if (millis() - lastStatusMs > 3000) {
    lastStatusMs = millis();
    Serial.println(F("BLE Loop alive"));
  }
}

void loop() { loopBLE(); }

// --------------- AT RENAME (AUTO) ---------------
void flushBtInput(unsigned long waitMs) {
  unsigned long start = millis();
  while (millis() - start < waitMs) {
    while (btSerial.available()) { btSerial.read(); start = millis(); }
  }
}

bool waitForBtResponse(const char* expect, unsigned long timeoutMs) {
  size_t idx = 0, len = strlen(expect);
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (btSerial.available()) {
      char c = (char)btSerial.read();
      if (c == expect[idx]) {
        idx++; if (idx == len) return true;
      } else idx = (c == expect[0]) ? 1 : 0;
    }
  }
  return false;
}

void sendAtCommand(const String& cmd) {
  btSerial.print(cmd); btSerial.print("\r\n");
  Serial.print(F("AT> ")); Serial.println(cmd);
}

void attemptAutoRename() {
  if (strlen(BT_DESIRED_NAME) == 0) { Serial.println(F("BT rename disabled (empty BT_DESIRED_NAME).")); return; }
  btSerial.begin(9600);
  delay(120);
  flushBtInput(40);
  sendAtCommand("AT");
  bool ok = waitForBtResponse("OK", AT_COMMAND_TIMEOUT_MS);
  if (!ok) { Serial.println(F("HM-10 no AT response (maybe already connected) - skipping rename.")); return; }
  Serial.println(F("HM-10 responded OK."));
  String at = String("AT+NAME") + BT_DESIRED_NAME;
  sendAtCommand(at);
  waitForBtResponse("OK", AT_COMMAND_TIMEOUT_MS);
  sendAtCommand("AT+RESET");
  waitForBtResponse("OK", AT_COMMAND_TIMEOUT_MS);
  Serial.print(F("Rename attempt done -> ")); Serial.println(BT_DESIRED_NAME);
}