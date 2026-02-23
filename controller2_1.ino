#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Preferences.h>   // NVS for loco name
#include <driver/i2s.h>    // I2S audio

// -----------------------------
// Pin map (XIAO ESP32-C3)
// -----------------------------
const int PIN_MOTOR_A   = D7;  // PWM forward
const int PIN_MOTOR_B   = D8;  // PWM reverse
const int PIN_MOTOR_DIR = D9;  // reserved for future 3-wire mode

const int PIN_ACC1      = D1;
const int PIN_ACC2      = D2;

const int PIN_SPEED     = D0;  // speed sensor (interrupt / analog)

// I2S pins
const int PIN_I2S_DATA  = D6;  // GPIO10
const int PIN_I2S_BCLK  = D4;  // GPIO8
const int PIN_I2S_LRCLK = D5;  // GPIO9

// -----------------------------
// BLE UUIDs
// -----------------------------
#define SERVICE_UUID                "19b10000-e8f2-537e-4f6c-d104768a1214"
#define SENSOR_CHARACTERISTIC_UUID  "19b10001-e8f2-537e-4f6c-d104768a1214"
#define LED_CHARACTERISTIC_UUID     "19b10002-e8f2-537e-4f6c-d104768a1214"

// -----------------------------
// Globals
// -----------------------------
BLEServer* pServer = nullptr;
BLECharacteristic* pSensorCharacteristic = nullptr;
BLECharacteristic* pLedCharacteristic = nullptr;

bool deviceConnected = false;
uint32_t notifyValue = 0;

Preferences prefs;

// loco state
String locoName = "Controller2.1";

int throttlePercent = 0;   // 0–100
int direction = 1;         // 1 = forward, 0 = reverse
bool accessory1 = false;
bool accessory2 = false;

// speed sensor
volatile uint32_t speedPulses = 0;
unsigned long lastSpeedSample = 0;
float currentSpeed = 0.0;  // placeholder (e.g. scale MPH)

// boost
bool boosting = false;
unsigned long boostStart = 0;
int boostDuration = 0;

// -----------------------------
// Speed sensor ISR
// -----------------------------
void IRAM_ATTR speedISR() {
  speedPulses++;
}

// -----------------------------
// Motor control
// -----------------------------
void applyMotorOutput() {
  int pwm = map(throttlePercent, 0, 100, 0, 255);

  if (boosting) {
    // during boost, pwm is handled elsewhere
    return;
  }

  if (throttlePercent == 0) {
    analogWrite(PIN_MOTOR_A, 0);
    analogWrite(PIN_MOTOR_B, 0);
    return;
  }

  if (direction == 1) {
    analogWrite(PIN_MOTOR_A, pwm);
    analogWrite(PIN_MOTOR_B, 0);
  } else {
    analogWrite(PIN_MOTOR_A, 0);
    analogWrite(PIN_MOTOR_B, pwm);
  }
}

void setThrottle(int percent) {
  throttlePercent = constrain(percent, 0, 100);
  applyMotorOutput();
}

void setDirection(int dir) {
  direction = dir ? 1 : 0;
  applyMotorOutput();
}

void fullStop() {
  throttlePercent = 0;
  analogWrite(PIN_MOTOR_A, 0);
  analogWrite(PIN_MOTOR_B, 0);
}

// -----------------------------
// Accessories
// -----------------------------
void setAccessory1(bool state) {
  accessory1 = state;
  digitalWrite(PIN_ACC1, state ? HIGH : LOW);
}

void setAccessory2(bool state) {
  accessory2 = state;
  digitalWrite(PIN_ACC2, state ? HIGH : LOW);
}

// -----------------------------
// Boost
// -----------------------------
void triggerBoost(int power, int durationMs) {
  power = constrain(power, 0, 100);
  int pwm = map(power, 0, 100, 0, 255);

  boosting = true;
  boostStart = millis();
  boostDuration = durationMs;

  if (direction == 1) {
    analogWrite(PIN_MOTOR_A, pwm);
    analogWrite(PIN_MOTOR_B, 0);
  } else {
    analogWrite(PIN_MOTOR_A, 0);
    analogWrite(PIN_MOTOR_B, pwm);
  }
}

// -----------------------------
// NVS: loco name
// -----------------------------
void loadLocoName() {
  prefs.begin("controller", true);
  String stored = prefs.getString("name", "");
  prefs.end();

  if (stored.length() > 0) {
    locoName = stored;
  }
}

void saveLocoName(const String& name) {
  prefs.begin("controller", false);
  prefs.putString("name", name);
  prefs.end();
}

// -----------------------------
// I2S audio init (skeleton)
// -----------------------------
void initI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 22050,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = PIN_I2S_BCLK,
    .ws_io_num = PIN_I2S_LRCLK,
    .data_out_num = PIN_I2S_DATA,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  // TODO: add real audio playback later
}

// -----------------------------
// BLE callbacks
// -----------------------------
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
  }
  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
    pServer->startAdvertising();
  }
};

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pLedCharacteristic) override {
    String raw = pLedCharacteristic->getValue().c_str();
    if (raw.length() == 0) return;

    String cmd = raw;
    cmd.trim();

    Serial.print("BLE Command: ");
    Serial.println(cmd);

    // Throttle: Txx
    if (cmd.startsWith("T")) {
      int t = cmd.substring(1).toInt();
      setThrottle(t);
      Serial.printf("Throttle set to %d%%\n", t);
      return;
    }

    // Direction: D0 / D1
    if (cmd.startsWith("D")) {
      int d = cmd.substring(1).toInt();
      setDirection(d);
      Serial.printf("Direction: %s\n", d ? "FORWARD" : "REVERSE");
      return;
    }

    // Boost: BOOST p d
    if (cmd.startsWith("BOOST")) {
      int s1 = cmd.indexOf(' ');
      int s2 = cmd.indexOf(' ', s1 + 1);
      int power = cmd.substring(s1 + 1, s2).toInt();
      int duration = cmd.substring(s2 + 1).toInt();
      Serial.printf("BOOST %d%% for %d ms\n", power, duration);
      triggerBoost(power, duration);
      return;
    }

    // Name: NAME xxxx
    if (cmd.startsWith("NAME")) {
      String name = cmd.substring(5);
      name.trim();
      if (name.length() > 0) {
        locoName = name;
        saveLocoName(locoName);
        Serial.printf("Loco name set to: %s\n", locoName.c_str());
      }
      return;
    }

    // Accessory 1: A1 0/1
    if (cmd.startsWith("A1")) {
      int val = cmd.substring(3).toInt();
      setAccessory1(val != 0);
      Serial.printf("Accessory 1: %s\n", val ? "ON" : "OFF");
      return;
    }

    // Accessory 2: A2 0/1
    if (cmd.startsWith("A2")) {
      int val = cmd.substring(3).toInt();
      setAccessory2(val != 0);
      Serial.printf("Accessory 2: %s\n", val ? "ON" : "OFF");
      return;
    }

    // Audio hooks (skeleton)
    if (cmd == "HORN") {
      Serial.println("HORN trigger (audio TODO)");
      // TODO: play horn sample
      return;
    }

    if (cmd == "BELL") {
      Serial.println("BELL trigger (audio TODO)");
      // TODO: play bell sample
      return;
    }

    Serial.println("Unknown command.");
  }
};

// -----------------------------
// Setup
// -----------------------------
void setup() {
  Serial.begin(115200);

  // Load loco name from NVS
  loadLocoName();
  Serial.print("Loco name: ");
  Serial.println(locoName);

  // Pins
  pinMode(PIN_MOTOR_A, OUTPUT);
  pinMode(PIN_MOTOR_B, OUTPUT);
  pinMode(PIN_ACC1, OUTPUT);
  pinMode(PIN_ACC2, OUTPUT);
  pinMode(PIN_SPEED, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(PIN_SPEED), speedISR, RISING);

  // I2S audio (skeleton)
  initI2S();

  // BLE
  BLEDevice::init(locoName.c_str());
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  pSensorCharacteristic = pService->createCharacteristic(
    SENSOR_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );

  pLedCharacteristic = pService->createCharacteristic(
    LED_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );

  pLedCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

  pSensorCharacteristic->addDescriptor(new BLE2902());
  pLedCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  BLEDevice::startAdvertising();

  Serial.println("Controller2.1 ready — advertising BLE.");
}

// -----------------------------
// Loop
// -----------------------------
void loop() {
  // Handle boost timeout
  if (boosting && (millis() - boostStart >= (unsigned long)boostDuration)) {
    boosting = false;
    Serial.println("Boost ended, restoring throttle.");
    applyMotorOutput();
  }

  // Speed sampling (skeleton)
  if (millis() - lastSpeedSample >= 500) {
    noInterrupts();
    uint32_t pulses = speedPulses;
    speedPulses = 0;
    interrupts();

    // TODO: convert pulses to speed (RPM / scale MPH)
    currentSpeed = pulses; // placeholder

    lastSpeedSample = millis();
  }

  // Notify HTML (simple counter + placeholder speed)
  if (deviceConnected) {
    String payload = String("N:") + notifyValue + ",S:" + String(currentSpeed, 1);
    pSensorCharacteristic->setValue(payload.c_str());
    pSensorCharacteristic->notify();
    notifyValue++;
    delay(200);
  } else {
    delay(200);
  }
}
