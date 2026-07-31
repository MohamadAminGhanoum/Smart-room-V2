#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <RTClib.h>
#include <Adafruit_SHT4x.h>
#include <GxEPD2_BW.h>

#define PIN_LED_PWM       1   // MOSFET Gate
#define PIN_POTENTIOMETER 2   // ADC Input (10k Pot)

#define PIN_RTC_SDA       4   // I2C Bus 1 (RTC)
#define PIN_RTC_SCL       5

#define PIN_EPD_BUSY      7   // SPI E-Ink Display
#define PIN_EPD_RST       8
#define PIN_EPD_DC        9
#define PIN_EPD_CS        10
#define PIN_EPD_MOSI      11
#define PIN_EPD_SCK       12

#define PIN_SHT_SDA       13  // I2C Bus 2 (SHT40)
#define PIN_SHT_SCL       14

#define PIN_MOTOR_INA     15  // AM4659 Motor Control A
#define PIN_MOTOR_INB     16  // AM4659 Motor Control B

#define PIN_BTN_OPEN      17  // Button 1 (Active-LOW)
#define PIN_BTN_CLOSE     18  // Button 2 (Active-LOW)

TwoWire I2C_RTC = TwoWire(0);
TwoWire I2C_SHT = TwoWire(1);

RTC_DS3231 rtc;
Adafruit_SHT4x sht40 = Adafruit_SHT4x();

const int PWM_FREQ = 5000;       // 5 kHz PWM
const int PWM_RESOLUTION = 10;   // 10-bit resolution (0 - 1023)

unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL = 5000; // Read sensors every 5 seconds

enum MotorState { STOPPED, OPENING, CLOSING };
MotorState currentMotorState = STOPPED;

void stopMotor() {
  digitalWrite(PIN_MOTOR_INA, LOW);
  digitalWrite(PIN_MOTOR_INB, LOW);
  currentMotorState = STOPPED;
  Serial.println("Motor: STOPPED");
}

void openCurtain() {
  digitalWrite(PIN_MOTOR_INB, LOW);
  digitalWrite(PIN_MOTOR_INA, HIGH);
  currentMotorState = OPENING;
  Serial.println("Motor: OPENING CURTAIN");
}

void closeCurtain() {
  digitalWrite(PIN_MOTOR_INA, LOW);
  digitalWrite(PIN_MOTOR_INB, HIGH);
  currentMotorState = CLOSING;
  Serial.println("Motor: CLOSING CURTAIN");
}

void updateLEDBrightness() {
  static int smoothedADC = 0;
  int rawADC = analogRead(PIN_POTENTIOMETER); // 0 to 4095

  smoothedADC = (smoothedADC * 7 + rawADC) / 8;

  int pwmValue = map(smoothedADC, 0, 4095, 0, 1023);

  if (pwmValue < 15) pwmValue = 0;

  analogWrite(PIN_LED_PWM, pwmValue);
}

void handleButtons() {
  bool openPressed = (digitalRead(PIN_BTN_OPEN) == LOW);
  bool closePressed = (digitalRead(PIN_BTN_CLOSE) == LOW);

  if (openPressed && currentMotorState != OPENING) {
    openCurtain();
    delay(200); // Simple debounce
  } else if (closePressed && currentMotorState != CLOSING) {
    closeCurtain();
    delay(200); // Simple debounce
  } else if (!openPressed && !closePressed && currentMotorState != STOPPED) {
    // Stop motor if no button is pressed
    stopMotor();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("--- Starting Smart Room Controller ---");

  pinMode(PIN_MOTOR_INA, OUTPUT);
  pinMode(PIN_MOTOR_INB, OUTPUT);
  stopMotor();

  pinMode(PIN_BTN_OPEN, INPUT_PULLUP);
  pinMode(PIN_BTN_CLOSE, INPUT_PULLUP);

  pinMode(PIN_LED_PWM, OUTPUT);
  analogWriteFrequency(PIN_LED_PWM, PWM_FREQ);
  analogWriteResolution(PIN_LED_PWM, PWM_RESOLUTION);

  I2C_RTC.begin(PIN_RTC_SDA, PIN_RTC_SCL, 100000);
  if (!rtc.begin(&I2C_RTC)) {
    Serial.println("Error: DS3231 RTC not found on I2C Bus 1!");
  } else {
    Serial.println("DS3231 RTC Initialized.");
    if (rtc.lostPower()) {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Set to compile time
    }
  }

  I2C_SHT.begin(PIN_SHT_SDA, PIN_SHT_SCL, 100000);
  if (!sht40.begin(&I2C_SHT)) {
    Serial.println("Error: SHT40 Sensor not found on I2C Bus 2!");
  } else {
    Serial.println("SHT40 Temp/Humidity Sensor Initialized.");
    sht40.setPrecision(SHT4X_HIGH_PRECISION);
  }

  Serial.println("--- Hardware Initialization Complete ---");
}

void loop() {
  // 1. Continuous smooth LED dimming from Potentiometer
  updateLEDBrightness();

  handleButtons();

  if (millis() - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = millis();

    DateTime now = rtc.now();
    Serial.printf("Time: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());

    sensors_event_t humidity, temp;
    if (sht40.getEvent(&humidity, &temp)) {
      Serial.printf("Temp: %.1f C | Humidity: %.1f %%\n", temp.temperature, humidity.relative_humidity);
    }

    // TODO: Trigger E-Ink Display refresh here with updated values
  }
}
