#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"

#define DHTPIN 3
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

LiquidCrystal_I2C lcd(0x27,16,2);

int mq2 = A0;
int mq5 = A1;
int mq7 = A2;

int flamePin = 4;
int buzzerPin = 5;
int fanPin = 6;
int buttonPin = 7;

int gasThreshold = 600;

float alpha = 0.01;
float mq2_baseline = 0;
float mq5_baseline = 0;
float mq7_baseline = 0;

void setup()
{
  pinMode(flamePin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(fanPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();

  dht.begin();

  Serial.begin(9600);

   // 1. Warm-up 
  Serial.println("Warming up sensors... please wait 60s");
  lcd.print("WARMING UP");
  delay(60000); 

  // 2. Auto-Calibration
  Serial.println("Calibrating baselines in clean air...");
  lcd.setCursor(0,0);
  lcd.print("INITIALIZING");
  float sum2 = 0, sum5 = 0, sum7 = 0;
  int samples = 50;
  
  for(int i = 0; i < samples; i++) {
    int gas1 = analogRead(mq2);
    int gas2 = analogRead(mq5);
    int gas3 = analogRead(mq7);
    Serial.print("MQ2: "); Serial.print(gas1);
    Serial.print(" MQ5: "); Serial.print(gas2);
    Serial.print(" MQ7: "); Serial.print(gas3);

    sum2 += analogRead(A0);
    sum5 += analogRead(A1);
    sum7 += analogRead(A2);
    delay(100);
  }
  
   mq2_baseline = sum2 / samples;
   mq5_baseline = sum5 / samples;
   mq7_baseline = sum7 / samples;

  Serial.print("MQ2_baseline: "); Serial.print(mq2_baseline);
  Serial.print(" MQ5_baseline: "); Serial.print(mq5_baseline);
  Serial.print(" MQ7_baseline: "); Serial.print(mq7_baseline);
  
  Serial.println("System Ready!");
  lcd.setCursor(0,0);
  lcd.print("SYSTEM READY !");
}

void loop()
{
  int gas1 = analogRead(mq2);
  int gas2 = analogRead(mq5);
  int gas3 = analogRead(mq7);
  int flame = digitalRead(flamePin);
  float temp = dht.readTemperature();

  float mq2_norm = (gas1 - mq2_baseline) / mq2_baseline;
  float mq5_norm = (gas2 - mq5_baseline) / mq5_baseline;
  float mq7_norm = (gas3 - mq7_baseline) / mq7_baseline;

   String detectedGas = "None";
  
  if (mq7_norm > 0.2 && mq2_norm < 0.1) {
    detectedGas = "Carbon Monoxide";
  } 
  else if (mq2_norm > 0.2 && mq5_norm > 0.2) {
    detectedGas = "LPG / Propane";
  } 
  else if (mq2_norm > 0.2 && mq5_norm < 0.1) {
    detectedGas = "Smoke / Fire";
  }
  else if (mq5_norm > 0.2 && mq2_norm < 0.15) {
    detectedGas = "Methane / NG";
  }

  Serial.print("MQ2_norm: "); Serial.print(mq2_norm);
  Serial.print(" MQ5_norm: "); Serial.print(mq5_norm);
  Serial.print(" MQ7_norm: "); Serial.print(mq7_norm);
  Serial.print(" Temp: "); Serial.println(temp);
  Serial.print(" Flame: "); Serial.println(flame);
  Serial.print("Detected Gas: "); Serial.println(detectedGas);

  bool gasDetected = (mq2_norm > 0.15 || mq5_norm > 0.15 || mq7_norm > 0.15);
  bool fireDetected = (flame == LOW);

  if (!gasDetected && !fireDetected &&
    abs(mq2_norm) < 0.05 &&
    abs(mq5_norm) < 0.05 &&
    abs(mq7_norm) < 0.05) {

  mq2_baseline = (1 - alpha) * mq2_baseline + alpha * gas1;
  mq5_baseline = (1 - alpha) * mq5_baseline + alpha * gas2;
  mq7_baseline = (1 - alpha) * mq7_baseline + alpha * gas3;

  Serial.println("Baseline updating...");
 }

  // --- ACTION LOGIC ---
  if (gasDetected || fireDetected) {
    digitalWrite(buzzerPin, HIGH);
    digitalWrite(fanPin, HIGH);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("DANGER!");
    lcd.setCursor(0, 1);

    if (gasDetected && fireDetected) {
      lcd.print("FIRE & ");
      lcd.print(detectedGas);
    } else if (gasDetected) {
      lcd.print("Gas: ");
      lcd.print(detectedGas);
    } else {
      lcd.print("Fire Detected!");
    }

    // Serial Logging
    Serial.print("ALERT! Gas:"); Serial.print(detectedGas);
    Serial.print(" Fire:"); Serial.println(fireDetected ? "YES" : "NO");
  } 
  else {
    digitalWrite(buzzerPin, LOW);
    digitalWrite(fanPin, LOW);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("System Safe");
    lcd.setCursor(0, 1);
    lcd.print("Temp: ");
    lcd.print(temp);
    lcd.print("C");
  }

  delay(500);
}