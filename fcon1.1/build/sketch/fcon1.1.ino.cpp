#line 1 "G:\\My Drive\\esp32\\Controler html\\DefaultcatV2.6\\fcon1.1\\fcon1.1.ino"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Preferences.h>

// --------------------------------------------------
// Firmware: fcon1.1
// Board: Seeed Studio XIAO ESP32-C3
// --------------------------------------------------

static const char* FW_NAME = "fcon1.1";

// --------------------------------------------------
// Pin map (requested)
// D0  = speed input (pulse OR analog)
// D1  = ACC1
// D2  = ACC2
// D3  = ACC3
// D4  = ACC4
// D5  = ACC5
// D6  = ACC6
// D7  = Motor Forward PWM
// D8  = Motor Reverse PWM
// D9  = Motor DIR for 3-pin mode
// D10 = Buzzer audio output
// D11 = Amp audio output
// D12-D14 are board power rails (not controlled here)
// --------------------------------------------------
const int PIN_SPEED      = D0;
const int PIN_ACC[6]     = { D1, D2, D3, D4, D5, D6 };
const int PIN_MOTOR_FWD  = D7;
const int PIN_MOTOR_REV  = D8;
const int PIN_MOTOR_DIR  = D9;
const int PIN_AUDIO_BZ   = D10;
#ifdef D11
const int PIN_AUDIO_AMP  = D11;
#else
const int PIN_AUDIO_AMP  = D10;
#endif

// BLE UUIDs (kept from existing app for easy reuse)
#define SERVICE_UUID                "19b10000-e8f2-537e-4f6c-d104768a1214"
#define SENSOR_CHARACTERISTIC_UUID  "19b10001-e8f2-537e-4f6c-d104768a1214"
#define LED_CHARACTERISTIC_UUID     "19b10002-e8f2-537e-4f6c-d104768a1214"

BLEServer* pServer = nullptr;
BLECharacteristic* pSensorCharacteristic = nullptr;
BLECharacteristic* pCmdCharacteristic = nullptr;

bool deviceConnected = false;
uint32_t notifyCount = 0;

Preferences prefs;
String locoName = "fcon1.1";

// ------------------------
// Runtime state
// ------------------------
enum SpeedMode {
  SPEED_PULSE = 0,
  SPEED_ANALOG = 1
};

enum MotorMode {
  MOTOR_2PIN = 0,
  MOTOR_3PIN = 1
};

volatile uint32_t speedPulseCount = 0;
unsigned long lastSpeedSampleMs = 0;
float speedValue = 0.0f;
SpeedMode speedMode = SPEED_PULSE;
MotorMode motorMode = MOTOR_2PIN;

int throttlePercent = 0;  // 0-100
int direction = 1;        // 1=forward, 0=reverse

bool accessories[6] = { false, false, false, false, false, false };

bool bzOn = false;
bool ampOn = false;
int bzFreq = 0;
int ampFreq = 0;
unsigned long bzUntil = 0;
unsigned long ampUntil = 0;

#line 93 "G:\\My Drive\\esp32\\Controler html\\DefaultcatV2.6\\fcon1.1\\fcon1.1.ino"
void notifyChip(const String& text);
#line 99 "G:\\My Drive\\esp32\\Controler html\\DefaultcatV2.6\\fcon1.1\\fcon1.1.ino"
void loadConfig();
#line 111 "G:\\My Drive\\esp32\\Controler html\\DefaultcatV2.6\\fcon1.1\\fcon1.1.ino"
void saveName(const String& name);
#line 117 "G:\\My Drive\\esp32\\Controler html\\DefaultcatV2.6\\fcon1.1\\fcon1.1.ino"
void saveModes();
#line 124 "G:\\My Drive\\esp32\\Controler html\\DefaultcatV2.6\\fcon1.1\\fcon1.1.ino"
void setAccessory(int index1Based, bool on);
#line 131 "G:\\My Drive\\esp32\\Controler html\\DefaultcatV2.6\\fcon1.1\\fcon1.1.ino"
void setSpeedMode(SpeedMode newMode);
#line 146 "G:\\My Drive\\esp32\\Controler html\\DefaultcatV2.6\\fcon1.1\\fcon1.1.ino"
void fullStopMotor();
#line 151 "G:\\My Drive\\esp32\\Controler html\\DefaultcatV2.6\\fcon1.1\\fcon1.1.ino"
void applyMotorOutput();
#line 175 "G:\\My Drive\\esp32\\Controler html\\DefaultcatV2.6\\fcon1.1\\fcon1.1.ino"
void setMotorMode(MotorMode newMode);
#line 181 "G:\\My Drive\\esp32\\Controler html\\DefaultcatV2.6\\fcon1.1\\fcon1.1.ino"
void setThrottle(int percent);
#line 186 "G:\\My Drive\\esp32\\Controler html\\DefaultcatV2.6\\fcon1.1\\fcon1.1.ino"
void setDirection(int dir);
#line 191 "G:\\My Drive\\esp32\\Controler html\\DefaultcatV2.6\\fcon1.1\\fcon1.1.ino"
void startToneBuzzer(int freq, int durationMs);
#line 199 "G:\\My Drive\\esp32\\Controler html\\DefaultcatV2.6\\fcon1.1\\fcon1.1.ino"
void stopToneBuzzer();
#line 206 "G:\\My Drive\\esp32\\Controler html\\DefaultcatV2.6\\fcon1.1\\fcon1.1.ino"
void startToneAmp(int freq, int durationMs);
#line 214 "G:\\My Drive\\esp32\\Controler html\\DefaultcatV2.6\\fcon1.1\\fcon1.1.ino"
void stopToneAmp();
#line 221 "G:\\My Drive\\esp32\\Controler html\\DefaultcatV2.6\\fcon1.1\\fcon1.1.ino"
void stopAllAudio();
#line 226 "G:\\My Drive\\esp32\\Controler html\\DefaultcatV2.6\\fcon1.1\\fcon1.1.ino"
void sampleSpeed();
#line 243 "G:\\My Drive\\esp32\\Controler html\\DefaultcatV2.6\\fcon1.1\\fcon1.1.ino"
void emitStatus();
#line 429 "G:\\My Drive\\esp32\\Controler html\\DefaultcatV2.6\\fcon1.1\\fcon1.1.ino"
void setupPins();
#line 456 "G:\\My Drive\\esp32\\Controler html\\DefaultcatV2.6\\fcon1.1\\fcon1.1.ino"
void setupBLE();
#line 486 "G:\\My Drive\\esp32\\Controler html\\DefaultcatV2.6\\fcon1.1\\fcon1.1.ino"
void setup();
#line 501 "G:\\My Drive\\esp32\\Controler html\\DefaultcatV2.6\\fcon1.1\\fcon1.1.ino"
void loop();
#line 89 "G:\\My Drive\\esp32\\Controler html\\DefaultcatV2.6\\fcon1.1\\fcon1.1.ino"
void IRAM_ATTR speedISR() {
  speedPulseCount++;
}

void notifyChip(const String& text) {
  if (!deviceConnected || !pSensorCharacteristic) return;
  pSensorCharacteristic->setValue(text.c_str());
  pSensorCharacteristic->notify();
}

void loadConfig() {
  prefs.begin("fcon", true);
  String storedName = prefs.getString("name", "");
  int storedSpeedMode = prefs.getInt("smode", (int)SPEED_PULSE);
  int storedMotorMode = prefs.getInt("mmode", (int)MOTOR_2PIN);
  prefs.end();

  if (storedName.length() > 0) locoName = storedName;
  speedMode = (storedSpeedMode == (int)SPEED_ANALOG) ? SPEED_ANALOG : SPEED_PULSE;
  motorMode = (storedMotorMode == (int)MOTOR_3PIN) ? MOTOR_3PIN : MOTOR_2PIN;
}

void saveName(const String& name) {
  prefs.begin("fcon", false);
  prefs.putString("name", name);
  prefs.end();
}

void saveModes() {
  prefs.begin("fcon", false);
  prefs.putInt("smode", (int)speedMode);
  prefs.putInt("mmode", (int)motorMode);
  prefs.end();
}

void setAccessory(int index1Based, bool on) {
  if (index1Based < 1 || index1Based > 6) return;
  int idx = index1Based - 1;
  accessories[idx] = on;
  digitalWrite(PIN_ACC[idx], on ? HIGH : LOW);
}

void setSpeedMode(SpeedMode newMode) {
  if (speedMode == newMode) return;

  if (newMode == SPEED_PULSE) {
    pinMode(PIN_SPEED, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_SPEED), speedISR, RISING);
  } else {
    detachInterrupt(digitalPinToInterrupt(PIN_SPEED));
    pinMode(PIN_SPEED, INPUT);
  }

  speedMode = newMode;
  saveModes();
}

void fullStopMotor() {
  ledcWrite(PIN_MOTOR_FWD, 0);
  ledcWrite(PIN_MOTOR_REV, 0);
}

void applyMotorOutput() {
  int pwm = map(throttlePercent, 0, 100, 0, 255);

  if (throttlePercent <= 0) {
    ledcWrite(PIN_MOTOR_FWD, 0);
    ledcWrite(PIN_MOTOR_REV, 0);
    return;
  }

  if (motorMode == MOTOR_2PIN) {
    if (direction == 1) {
      ledcWrite(PIN_MOTOR_FWD, pwm);
      ledcWrite(PIN_MOTOR_REV, 0);
    } else {
      ledcWrite(PIN_MOTOR_FWD, 0);
      ledcWrite(PIN_MOTOR_REV, pwm);
    }
  } else {
    digitalWrite(PIN_MOTOR_DIR, direction == 1 ? HIGH : LOW);
    ledcWrite(PIN_MOTOR_FWD, pwm);
    ledcWrite(PIN_MOTOR_REV, 0);
  }
}

void setMotorMode(MotorMode newMode) {
  motorMode = newMode;
  saveModes();
  applyMotorOutput();
}

void setThrottle(int percent) {
  throttlePercent = constrain(percent, 0, 100);
  applyMotorOutput();
}

void setDirection(int dir) {
  direction = dir ? 1 : 0;
  applyMotorOutput();
}

void startToneBuzzer(int freq, int durationMs) {
  freq = constrain(freq, 50, 8000);
  ledcWriteTone(PIN_AUDIO_BZ, freq);
  bzOn = true;
  bzFreq = freq;
  bzUntil = durationMs > 0 ? millis() + (unsigned long)durationMs : 0;
}

void stopToneBuzzer() {
  ledcWriteTone(PIN_AUDIO_BZ, 0);
  bzOn = false;
  bzFreq = 0;
  bzUntil = 0;
}

void startToneAmp(int freq, int durationMs) {
  freq = constrain(freq, 50, 12000);
  ledcWriteTone(PIN_AUDIO_AMP, freq);
  ampOn = true;
  ampFreq = freq;
  ampUntil = durationMs > 0 ? millis() + (unsigned long)durationMs : 0;
}

void stopToneAmp() {
  ledcWriteTone(PIN_AUDIO_AMP, 0);
  ampOn = false;
  ampFreq = 0;
  ampUntil = 0;
}

void stopAllAudio() {
  stopToneBuzzer();
  stopToneAmp();
}

void sampleSpeed() {
  if (millis() - lastSpeedSampleMs < 500) return;

  if (speedMode == SPEED_PULSE) {
    noInterrupts();
    uint32_t pulses = speedPulseCount;
    speedPulseCount = 0;
    interrupts();
    speedValue = (float)pulses;
  } else {
    int raw = analogRead(PIN_SPEED);
    speedValue = (float)raw;
  }

  lastSpeedSampleMs = millis();
}

void emitStatus() {
  String status = String("ST FW:") + FW_NAME +
                  " SM:" + String((speedMode == SPEED_PULSE) ? "PULSE" : "ANALOG") +
                  " MM:" + String((motorMode == MOTOR_2PIN) ? "2PIN" : "3PIN") +
                  " T:" + String(throttlePercent) +
                  " D:" + String(direction) +
                  " BZ:" + String(bzOn ? "1" : "0") +
                  " BF:" + String(bzFreq) +
                  " AMP:" + String(ampOn ? "1" : "0") +
                  " AF:" + String(ampFreq) +
                  " S:" + String(speedValue, 1);
  notifyChip(status);
}

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) override {
    deviceConnected = true;
  }

  void onDisconnect(BLEServer* server) override {
    deviceConnected = false;
    server->startAdvertising();
  }
};

class MyCommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    String cmd = characteristic->getValue().c_str();
    cmd.trim();
    if (cmd.length() == 0) return;

    Serial.print("CMD: ");
    Serial.println(cmd);

    // Txx throttle
    if (cmd.startsWith("T")) {
      int t = cmd.substring(1).toInt();
      setThrottle(t);
      notifyChip("OK T");
      return;
    }

    // D0/D1 direction
    if (cmd.startsWith("D")) {
      int d = cmd.substring(1).toInt();
      setDirection(d);
      notifyChip("OK D");
      return;
    }

    if (cmd == "STOP") {
      setThrottle(0);
      notifyChip("OK STOP");
      return;
    }

    // A1 0/1 .. A6 0/1
    if (cmd.startsWith("A") && cmd.length() >= 4) {
      int accNum = cmd.substring(1, 2).toInt();
      int val = cmd.substring(3).toInt();
      if (accNum >= 1 && accNum <= 6) {
        setAccessory(accNum, val != 0);
        notifyChip("OK A" + String(accNum));
        return;
      }
    }

    // ACC n v  (explicit)
    if (cmd.startsWith("ACC ")) {
      int s1 = cmd.indexOf(' ');
      int s2 = cmd.indexOf(' ', s1 + 1);
      if (s2 > 0) {
        int n = cmd.substring(s1 + 1, s2).toInt();
        int v = cmd.substring(s2 + 1).toInt();
        if (n >= 1 && n <= 6) {
          setAccessory(n, v != 0);
          notifyChip("OK ACC");
          return;
        }
      }
      notifyChip("ERR ACC");
      return;
    }

    // Speed mode: SMODE PULSE|ANALOG
    if (cmd.startsWith("SMODE ")) {
      String mode = cmd.substring(6);
      mode.trim();
      mode.toUpperCase();
      if (mode == "PULSE") {
        setSpeedMode(SPEED_PULSE);
        notifyChip("OK SMODE");
      } else if (mode == "ANALOG") {
        setSpeedMode(SPEED_ANALOG);
        notifyChip("OK SMODE");
      } else {
        notifyChip("ERR SMODE");
      }
      return;
    }

    // Motor mode: MMODE 2|3
    if (cmd.startsWith("MMODE ")) {
      int m = cmd.substring(6).toInt();
      if (m == 2) {
        setMotorMode(MOTOR_2PIN);
        notifyChip("OK MMODE");
      } else if (m == 3) {
        setMotorMode(MOTOR_3PIN);
        notifyChip("OK MMODE");
      } else {
        notifyChip("ERR MMODE");
      }
      return;
    }

    // BZ f durMs
    if (cmd.startsWith("BZ ")) {
      int s1 = cmd.indexOf(' ');
      int s2 = cmd.indexOf(' ', s1 + 1);
      if (s2 < 0) {
        notifyChip("ERR BZ");
        return;
      }
      int f = cmd.substring(s1 + 1, s2).toInt();
      int dur = cmd.substring(s2 + 1).toInt();
      startToneBuzzer(f, dur);
      notifyChip("OK BZ");
      return;
    }

    if (cmd == "BZOFF") {
      stopToneBuzzer();
      notifyChip("OK BZOFF");
      return;
    }

    // AMP f durMs
    if (cmd.startsWith("AMP ")) {
      int s1 = cmd.indexOf(' ');
      int s2 = cmd.indexOf(' ', s1 + 1);
      if (s2 < 0) {
        notifyChip("ERR AMP");
        return;
      }
      int f = cmd.substring(s1 + 1, s2).toInt();
      int dur = cmd.substring(s2 + 1).toInt();
      startToneAmp(f, dur);
      notifyChip("OK AMP");
      return;
    }

    if (cmd == "AMPOFF") {
      stopToneAmp();
      notifyChip("OK AMPOFF");
      return;
    }

    if (cmd == "AUDIOOFF") {
      stopAllAudio();
      notifyChip("OK AUDIOOFF");
      return;
    }

    if (cmd == "STAT") {
      emitStatus();
      return;
    }

    if (cmd.startsWith("NAME ")) {
      String nm = cmd.substring(5);
      nm.trim();
      if (nm.length() > 0) {
        locoName = nm;
        saveName(locoName);
        notifyChip("OK NAME");
      } else {
        notifyChip("ERR NAME");
      }
      return;
    }

    notifyChip("ERR CMD");
  }
};

void setupPins() {
  pinMode(PIN_SPEED, INPUT_PULLUP);

  for (int i = 0; i < 6; i++) {
    pinMode(PIN_ACC[i], OUTPUT);
    digitalWrite(PIN_ACC[i], LOW);
  }

  pinMode(PIN_MOTOR_FWD, OUTPUT);
  pinMode(PIN_MOTOR_REV, OUTPUT);
  pinMode(PIN_MOTOR_DIR, OUTPUT);
  digitalWrite(PIN_MOTOR_DIR, HIGH);

  pinMode(PIN_AUDIO_BZ, OUTPUT);
  pinMode(PIN_AUDIO_AMP, OUTPUT);

  ledcAttach(PIN_MOTOR_FWD, 1000, 8);
  ledcAttach(PIN_MOTOR_REV, 1000, 8);
  ledcAttach(PIN_AUDIO_BZ, 400, 8);
  ledcAttach(PIN_AUDIO_AMP, 400, 8);

  stopAllAudio();
  fullStopMotor();

  attachInterrupt(digitalPinToInterrupt(PIN_SPEED), speedISR, RISING);
}

void setupBLE() {
  BLEDevice::init(locoName.c_str());

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  pSensorCharacteristic = pService->createCharacteristic(
    SENSOR_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );

  pCmdCharacteristic = pService->createCharacteristic(
    LED_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );

  pCmdCharacteristic->setCallbacks(new MyCommandCallbacks());

  pSensorCharacteristic->addDescriptor(new BLE2902());
  pCmdCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  BLEDevice::startAdvertising();
}

void setup() {
  Serial.begin(115200);
  loadConfig();

  setupPins();
  setSpeedMode(speedMode);
  setMotorMode(motorMode);

  setupBLE();

  Serial.println();
  Serial.printf("%s ready. BLE name: %s\n", FW_NAME, locoName.c_str());
  Serial.println("Pin map active: D0 speed, D1-D6 ACC, D7/D8 PWM, D9 DIR, D10 buzzer, D11 amp");
}

void loop() {
  unsigned long now = millis();

  if (bzOn && bzUntil > 0 && now >= bzUntil) {
    stopToneBuzzer();
    notifyChip("OK BZDONE");
  }

  if (ampOn && ampUntil > 0 && now >= ampUntil) {
    stopToneAmp();
    notifyChip("OK AMPDONE");
  }

  sampleSpeed();

  if (deviceConnected) {
    String payload = String("N:") + notifyCount +
                    ",S:" + String(speedValue, 1) +
                    ",SM:" + (speedMode == SPEED_PULSE ? "P" : "A") +
                    ",MM:" + (motorMode == MOTOR_2PIN ? "2" : "3");
    pSensorCharacteristic->setValue(payload.c_str());
    pSensorCharacteristic->notify();
    notifyCount++;
  }

  delay(200);
}

