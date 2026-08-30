#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "config.h"

U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE, PIN_I2C_SCL, PIN_I2C_SDA);

enum class EvidenceStatus : uint8_t {
  PROBABLE_SURVIVOR,
  AMBIGUOUS,
  ENVIRONMENTAL_MOVEMENT,
  NO_RELIABLE_EVIDENCE
};

enum class OperationMode : uint8_t {
  LIVE_SENSOR,
  AUTO_DEMO
};

enum class DemoScenario : uint8_t {
  PROBABLE_SURVIVOR,
  FALSE_POSITIVE_ENVIRONMENTAL,
  AMBIGUOUS_SIGNAL
};

struct Assessment {
  bool rcwl = false;
  bool pir = false;
  bool buttonPressed = false;  // active low, converted to logical pressed
  uint8_t confidence = 0;      // 0..100
  uint8_t quality = 0;         // 0..100
  EvidenceStatus status = EvidenceStatus::NO_RELIABLE_EVIDENCE;
  const char* reason = "no motion from either sensor";
  const char* scenarioName = "Live Sensor Input";
};

static constexpr uint32_t DEMO_SCENARIO_MS = 12000;

static Assessment gAssessment;
static OperationMode gMode = OperationMode::LIVE_SENSOR;
static DemoScenario gDemoScenario = DemoScenario::PROBABLE_SURVIVOR;
static uint32_t gModeToggleCount = 0;
static uint32_t gDemoModeStartMs = 0;

static uint32_t gLastSampleMs = 0;
static uint32_t gLastTelemetryMs = 0;
static uint32_t gLastDisplayMs = 0;

static bool gBtnPrev = HIGH;

static const char* statusToText(EvidenceStatus s) {
  switch (s) {
    case EvidenceStatus::PROBABLE_SURVIVOR:      return "probable survivor";
    case EvidenceStatus::AMBIGUOUS:              return "ambiguous";
    case EvidenceStatus::ENVIRONMENTAL_MOVEMENT: return "environment movement";
    case EvidenceStatus::NO_RELIABLE_EVIDENCE:   return "no reliable evidence";
    default:                                     return "no reliable evidence";
  }
}

static const char* modeToText(OperationMode mode) {
  return (mode == OperationMode::AUTO_DEMO) ? "AUTO_DEMO" : "LIVE_SENSOR";
}

static DemoScenario getCurrentDemoScenario(uint32_t nowMs) {
  const uint32_t elapsed = nowMs - gDemoModeStartMs;
  const uint32_t slot = (elapsed / DEMO_SCENARIO_MS) % 3;

  if (slot == 0) return DemoScenario::PROBABLE_SURVIVOR;
  if (slot == 1) return DemoScenario::FALSE_POSITIVE_ENVIRONMENTAL;
  return DemoScenario::AMBIGUOUS_SIGNAL;
}

static void assessLiveEvidence() {
  gAssessment.rcwl = (digitalRead(PIN_RCWL_OUT) == HIGH);
  gAssessment.pir = (digitalRead(PIN_PIR_OUT) == HIGH);
  gAssessment.buttonPressed = (digitalRead(PIN_BUTTON) == LOW);
  gAssessment.scenarioName = "Live Sensor Input";

  if (gAssessment.rcwl && gAssessment.pir) {
    gAssessment.status = EvidenceStatus::PROBABLE_SURVIVOR;
    gAssessment.confidence = 90;
    gAssessment.quality = 95;
    gAssessment.reason = "RCWL and PIR both detect motion";
  } else if (gAssessment.pir && !gAssessment.rcwl) {
    gAssessment.status = EvidenceStatus::AMBIGUOUS;
    gAssessment.confidence = 60;
    gAssessment.quality = 65;
    gAssessment.reason = "thermal motion only (PIR), radar not confirmed";
  } else if (gAssessment.rcwl && !gAssessment.pir) {
    gAssessment.status = EvidenceStatus::ENVIRONMENTAL_MOVEMENT;
    gAssessment.confidence = 45;
    gAssessment.quality = 60;
    gAssessment.reason = "radar-only motion, likely environmental";
  } else {
    gAssessment.status = EvidenceStatus::NO_RELIABLE_EVIDENCE;
    gAssessment.confidence = 10;
    gAssessment.quality = 35;
    gAssessment.reason = "no motion from either sensor";
  }
}

static void assessDemoEvidence() {
  gDemoScenario = getCurrentDemoScenario(millis());
  gAssessment.buttonPressed = (digitalRead(PIN_BUTTON) == LOW); // still reported

  switch (gDemoScenario) {
    case DemoScenario::PROBABLE_SURVIVOR:
      gAssessment.scenarioName = "Probable Survivor";
      gAssessment.rcwl = true;
      gAssessment.pir = true;
      gAssessment.status = EvidenceStatus::PROBABLE_SURVIVOR;
      gAssessment.confidence = 94;
      gAssessment.quality = 92;
      gAssessment.reason = "scripted dual-sensor convergence";
      break;

    case DemoScenario::FALSE_POSITIVE_ENVIRONMENTAL:
      gAssessment.scenarioName = "False Positive / Environmental Movement";
      gAssessment.rcwl = true;
      gAssessment.pir = false;
      gAssessment.status = EvidenceStatus::ENVIRONMENTAL_MOVEMENT;
      gAssessment.confidence = 25;
      gAssessment.quality = 80;
      gAssessment.reason = "scripted radar-only environmental artifact";
      break;

    case DemoScenario::AMBIGUOUS_SIGNAL:
    default:
      gAssessment.scenarioName = "Ambiguous Signal";
      gAssessment.rcwl = false;
      gAssessment.pir = true;
      gAssessment.status = EvidenceStatus::AMBIGUOUS;
      gAssessment.confidence = 58;
      gAssessment.quality = 55;
      gAssessment.reason = "scripted thermal-only intermittent profile";
      break;
  }
}

static void assessEvidence() {
  if (gMode == OperationMode::AUTO_DEMO) {
    assessDemoEvidence();
  } else {
    assessLiveEvidence();
  }
}

static void handleButtonToggle() {
  const bool btnNow = digitalRead(PIN_BUTTON); // HIGH idle, LOW pressed (INPUT_PULLUP)
  if (btnNow == LOW && gBtnPrev == HIGH) {
    gMode = (gMode == OperationMode::LIVE_SENSOR) ? OperationMode::AUTO_DEMO : OperationMode::LIVE_SENSOR;
    gModeToggleCount++;

    if (gMode == OperationMode::AUTO_DEMO) {
      gDemoModeStartMs = millis();
    }
  }
  gBtnPrev = btnNow;
}

static void updateBuzzerAlert(bool highConfidenceAlert) {
  static bool toneOn = false;
  static uint32_t lastToggle = 0;

  if (!highConfidenceAlert) {
    noTone(PIN_BUZZER);
    toneOn = false;
    lastToggle = millis();
    return;
  }

  const uint32_t now = millis();
  const uint32_t interval = toneOn ? 120 : 180;

  if (now - lastToggle >= interval) {
    lastToggle = now;
    toneOn = !toneOn;
    if (toneOn) {
      tone(PIN_BUZZER, BUZZER_FREQ_HZ);
    } else {
      noTone(PIN_BUZZER);
    }
  }
}

static void renderDisplay() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);

  char line[64];

  snprintf(line, sizeof(line), "Mode: %s", (gMode == OperationMode::AUTO_DEMO) ? "AUTO_DEMO" : "LIVE_SENSOR");
  oled.drawStr(0, 10, line);

  if (gMode == OperationMode::AUTO_DEMO) {
    snprintf(line, sizeof(line), "Scenario: %s", gAssessment.scenarioName);
  } else {
    snprintf(line, sizeof(line), "Scenario: Live Sensor");
  }
  oled.drawStr(0, 21, line);

  snprintf(line, sizeof(line), "Status: %s", statusToText(gAssessment.status));
  oled.drawStr(0, 32, line);

  snprintf(line, sizeof(line), "Conf:%3u%% Qual:%3u%%", gAssessment.confidence, gAssessment.quality);
  oled.drawStr(0, 43, line);

  snprintf(line, sizeof(line), "RCWL:%d PIR:%d BTN:%d", gAssessment.rcwl ? 1 : 0, gAssessment.pir ? 1 : 0, gAssessment.buttonPressed ? 1 : 0);
  oled.drawStr(0, 54, line);

  oled.drawStr(0, 64, gAssessment.reason);

  oled.sendBuffer();
}

static void printTelemetry() {
  Serial.printf(
    "{\"ms\":%lu,\"mode\":\"%s\",\"scenario\":\"%s\",\"rcwl\":%d,\"pir\":%d,\"button\":%d,"
    "\"confidence\":%u,\"quality\":%u,\"status\":\"%s\",\"reason\":\"%s\",\"mode_toggles\":%lu}\n",
    millis(),
    modeToText(gMode),
    gAssessment.scenarioName,
    gAssessment.rcwl ? 1 : 0,
    gAssessment.pir ? 1 : 0,
    gAssessment.buttonPressed ? 1 : 0,
    gAssessment.confidence,
    gAssessment.quality,
    statusToText(gAssessment.status),
    gAssessment.reason,
    static_cast<unsigned long>(gModeToggleCount)
  );
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_RCWL_OUT, INPUT);
  pinMode(PIN_PIR_OUT, INPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  noTone(PIN_BUZZER);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  oled.setI2CAddress(OLED_I2C_ADDR << 1); // U8g2 expects 8-bit address
  oled.begin();

  gDemoModeStartMs = millis();

  assessEvidence();
  renderDisplay();
  printTelemetry();
}

void loop() {
  const uint32_t now = millis();

  handleButtonToggle();

  if (now - gLastSampleMs >= SENSOR_SAMPLE_MS) {
    gLastSampleMs = now;
    assessEvidence();

    const bool highConfidenceAlert =
      (gAssessment.status == EvidenceStatus::PROBABLE_SURVIVOR) &&
      (gAssessment.confidence >= 80) &&
      (gAssessment.quality >= 75);

    updateBuzzerAlert(highConfidenceAlert);
  }

  if (now - gLastDisplayMs >= DISPLAY_REFRESH_MS) {
    gLastDisplayMs = now;
    renderDisplay();
  }

  if (now - gLastTelemetryMs >= TELEMETRY_MS) {
    gLastTelemetryMs = now;
    printTelemetry();
  }
}
