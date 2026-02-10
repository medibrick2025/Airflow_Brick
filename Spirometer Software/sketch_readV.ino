#include <Arduino.h>
#include <Wire.h>

#include "Adafruit_MAX1704X.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <SensirionI2CSfm3000.h>
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include <math.h>

// ---------- Pins ----------
#define BOOST_EN_PIN A5          // PowerBoost EN (HIGH = ON, LOW = OFF)
#define BTN_PIN      GPIO_NUM_9  // Deep-sleep button: D9 to GND (active LOW)
#define LED_PIN      8           // Your existing PWM pin

// ---------- Display ----------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET   -1
#define OLED_ADDR    0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------- Battery ----------
Adafruit_MAX17048 maxlipo;
float battV  = NAN;
float battPct = NAN;

// ---------- SFM3300-D ----------
SensirionI2CSfm3000 sfm;
float scalingFactor = 120.0f;
float offset        = 32768.0f;

// ---------- Timing ----------
uint32_t lastBattMS = 0, lastOledMS = 0, lastFlowMS = 0;
const uint32_t BATT_PERIOD_MS = 500;
const uint32_t OLED_PERIOD_MS = 150;

// ---------- Volume integration ----------
float flow    = 0.0f;
float inhaleVol_L = 0.0f;
float exhaleVol_L = 0.0f;

// thresholds
const float flowThreshold  = 0.1f;   // for integration
const float START_THR_SLM  = 0.5f;   // |flow| above this = breath start
const float STOP_THR_SLM   = 0.3f;   // |flow| below this = possible end
const uint32_t STOP_TIME_MS = 800;   // must stay low this long to end breath

// Breath state machine
enum BreathState {
  BREATH_WAIT_START,
  BREATH_MEASURING,
  BREATH_HOLD
};

BreathState breathState     = BREATH_WAIT_START;
BreathState lastBreathState = BREATH_WAIT_START;
uint32_t quietStartMS       = 0;

// NEW: Inhale / exhale phase for continuous monitoring
enum Phase { PHASE_NONE, PHASE_INHALE, PHASE_EXHALE };
Phase currentPhase = PHASE_NONE;

// Tidal-volume display timing
uint32_t holdDisplayStartMS = 0;
bool     holdDisplayActive  = false;

// ---------- Helpers ----------
void drawBatteryBar(float percent) {
  int x=0, y=18, w=128, h=10;
  percent = constrain(percent, 0.0f, 100.0f);
  int fillW = (int)(w * (percent/100.0f));
  display.drawRect(x, y, w, h, SSD1306_WHITE);
  display.fillRect(x+1, y+1, max(0, fillW-2), h-2, SSD1306_WHITE);
}

// Battery + flow screen (WAIT_START / idle)
void showBatteryScreen(float flow_slm) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);

  display.print(F("Batt "));
  if (isnan(battPct)) display.print(F("--.-% "));
  else { display.print(battPct,1); display.print('%'); display.print(' '); }
  if (isnan(battV)) display.print(F("--.--V"));
  else { display.print(battV,2); display.print('V'); }

  drawBatteryBar(isnan(battPct) ? 0.0f : battPct);

  display.setCursor(0, 30-8);
  display.print(F("Flow "));
  display.print(flow_slm, 2);
  display.print(F(" slm"));
  display.display();
}

// Live inhale / exhale screen during MEASURING
void showInhaleExhaleScreen(float inh_L, float exh_L) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print(F("INH: "));
  display.print(inh_L, 2);
  display.println(F(" L"));

  display.setCursor(0, 16);
  display.print(F("EXH: "));
  display.print(exh_L, 2);
  display.println(F(" L"));

  display.display();
}

// Tidal volume result screen during HOLD
void showTidalScreen(float tidal_L) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Tidal Volume"));

  display.setTextSize(2);
  display.setCursor(0, 15);
  display.print(tidal_L, 2);
  display.print(F(" L"));

  display.display();
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  analogWrite(LED_PIN, 255);          // your existing LED behavior

  Serial.begin(115200);
  while (!Serial) { delay(100); }

  // ---- Boost EN ----
  pinMode(BOOST_EN_PIN, OUTPUT);
  digitalWrite(BOOST_EN_PIN, HIGH);   // turn Boost ON while code is running

  // ---- I2C / OLED ----
  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,12);
  display.println(F("Battery Level:"));
  display.setCursor(0,28);
  display.print(F("--%"));
  display.display();
  delay(1500);

  // ---- Fuel Gauge ----
  maxlipo.begin();

  // ---- Flow sensor ----
  uint16_t error;
  char errorMessage[64];

  sfm.begin(Wire, SFM300_I2C_ADDRESS_0);

  uint32_t serialNumber;
  error = sfm.readSerialNumber(serialNumber);
  if (error) {
    Serial.print("Error serialNumber(): ");
    errorToString(error, errorMessage, 64);
    Serial.println(errorMessage);
  } else {
    Serial.print("SerialNumber: ");
    Serial.println(serialNumber);
  }

  error = sfm.startContinuousMeasurement();
  if (error) {
    Serial.print("Error startContinuousMeasurement(): ");
    errorToString(error, errorMessage, 64);
    Serial.println(errorMessage);
  }

  // ---- Button / deep sleep ----
  pinMode((int)BTN_PIN, INPUT_PULLUP);

  rtc_gpio_init(BTN_PIN);
  rtc_gpio_set_direction(BTN_PIN, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(BTN_PIN);
  rtc_gpio_pulldown_dis(BTN_PIN);

  esp_sleep_enable_ext0_wakeup(BTN_PIN, 0);   // wake on LOW
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

  // Welcome if we woke from deep sleep
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause == ESP_SLEEP_WAKEUP_EXT0) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("Welcome"));
    display.display();
    delay(1200);
  }

  lastFlowMS = millis();
  breathState = BREATH_WAIT_START;
  lastBreathState = BREATH_WAIT_START;
  inhaleVol_L = exhaleVol_L = 0.0f;
  currentPhase = PHASE_NONE;
  holdDisplayActive = false;
}

void loop() {
  uint16_t error;
  char errorMessage[64];
  uint32_t now = millis();

  // ---- Read flow ----
  float flow_slm = 0.0f;
  error = sfm.readMeasurement(flow, scalingFactor, offset);
  if (!error) {
    flow_slm = flow; // SLM (L/min)
  } else {
    Serial.print("readMeasurement error: ");
    errorToString(error, errorMessage, 64);
    Serial.println(errorMessage);
  }

  float dt = (now - lastFlowMS) / 1000.0f;  // seconds
  if (dt < 0) dt = 0;
  lastFlowMS = now;

  float absFlow = fabs(flow_slm);

  // ---- Breath state transitions ----
  if (breathState == BREATH_HOLD && lastBreathState != BREATH_HOLD) {
    // Just entered HOLD: start showing tidal volume for this last cycle
    holdDisplayActive  = true;
    holdDisplayStartMS = now;
  }
  lastBreathState = breathState;

  switch (breathState) {
    case BREATH_WAIT_START:
      // Reset everything while idle
      inhaleVol_L  = 0.0f;
      exhaleVol_L  = 0.0f;
      currentPhase = PHASE_NONE;
      quietStartMS = 0;

      if (absFlow > START_THR_SLM) {
        breathState = BREATH_MEASURING;
        Serial.println(F("Breath started"));
      }
      break;

    case BREATH_MEASURING:
      // ---- Continuous monitoring: each inhale/exhale cycle independent ----
      if (flow_slm > flowThreshold) {
        // New exhale cycle starts
        if (currentPhase != PHASE_EXHALE) {
          exhaleVol_L  = 0.0f;       // reset exhale for this cycle
          currentPhase = PHASE_EXHALE;
        }
        float Lps = flow_slm / 60.0f; // L/min -> L/s
        exhaleVol_L += Lps * dt;

      } else if (flow_slm < -flowThreshold) {
        // New inhale cycle starts
        if (currentPhase != PHASE_INHALE) {
          inhaleVol_L  = 0.0f;       // reset inhale for this cycle
          currentPhase = PHASE_INHALE;
        }
        float Lps = (-flow_slm) / 60.0f;
        inhaleVol_L += Lps * dt;

      } else {
        // Near zero -> no active phase
        currentPhase = PHASE_NONE;
      }

      // Check for breath end (flow near zero long enough)
      if (absFlow < STOP_THR_SLM) {
        if (quietStartMS == 0) {
          quietStartMS = now;
        } else if ((now - quietStartMS) >= STOP_TIME_MS) {
          breathState = BREATH_HOLD;
          currentPhase = PHASE_NONE;
          Serial.println(F("Breath finished, volumes held"));
        }
      } else {
        quietStartMS = 0;
      }
      break;

    case BREATH_HOLD:
      // Do not integrate; volumes stay fixed
      break;
  }

  // ---- Serial debug ----
  Serial.print("Flow = ");
  Serial.print(flow_slm, 2);
  Serial.print(" SLM, INH = ");
  Serial.print(inhaleVol_L, 3);
  Serial.print(" L, EXH = ");
  Serial.print(exhaleVol_L, 3);
  Serial.print(" L, State = ");
  if (breathState == BREATH_WAIT_START) Serial.println("WAIT_START");
  else if (breathState == BREATH_MEASURING) Serial.println("MEASURING");
  else Serial.println("HOLD");

  // ---- Battery update ----
  if (now - lastBattMS >= BATT_PERIOD_MS) {
    lastBattMS = now;
    float v = maxlipo.cellVoltage();
    if (!isnan(v)) {
      battV   = v;
      battPct = maxlipo.cellPercent();
    }
  }

  // ---- OLED update ----
  if (now - lastOledMS >= OLED_PERIOD_MS) {
    lastOledMS = now;

    bool showTidal  = false;
    bool showInhExh = false;

    // Tidal volume window
    if (breathState == BREATH_HOLD && holdDisplayActive) {
      if ((now - holdDisplayStartMS) <= 20000UL) {      // 20 seconds
        showTidal = true;
      } else {
        // 20 s finished → go back to WAIT_START for next breath
        holdDisplayActive = false;
        breathState = BREATH_WAIT_START;
        inhaleVol_L = exhaleVol_L = 0.0f;
        currentPhase = PHASE_NONE;
      }
    }

    // During active breath, show live INH / EXH
    if (breathState == BREATH_MEASURING) {
      showInhExh = true;
    }

    if (showTidal) {
      float tidal_L = max(inhaleVol_L, exhaleVol_L);    // last cycle’s volume
      showTidalScreen(tidal_L);
    } else if (showInhExh) {
      showInhaleExhaleScreen(inhaleVol_L, exhaleVol_L);
    } else {
      // Idle / between breaths
      showBatteryScreen(flow_slm);
    }
  }

  // ---- Deep-sleep button ----
  int btnLevel = digitalRead((int)BTN_PIN);   // LOW when pressed
  if (btnLevel == LOW) {
    delay(40);
    if (digitalRead((int)BTN_PIN) == LOW) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println(F("Going to"));
      display.println(F("deep sleep..."));
      display.display();

      delay(5000); // 5 s grace

      // Turn OLED OFF & Boost OFF for low power
      display.ssd1306_command(SSD1306_DISPLAYOFF);
      digitalWrite(BOOST_EN_PIN, LOW);

      Serial.flush();
      esp_deep_sleep_start(); // never returns until wake
    }
  }

  delay(50);   // ~20 Hz loop
}
