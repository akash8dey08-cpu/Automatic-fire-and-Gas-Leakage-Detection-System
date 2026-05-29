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

//EMA to stabilize basesline 
float alpha = 0.01;
float mq2_baseline = 0;
float mq5_baseline = 0;
float mq7_baseline = 0;

//EMA to stabilize raw analog inputs from MQ sensors 
float alpha_signal = 0.20; // Filter weight (0.1 to 0.3 is ideal for noise)
float mq2_filtered = 0;
float mq5_filtered = 0;
float mq7_filtered = 0;

String lastLine1 = "";
String lastLine2 = "";

String detectedGas = "None";
String candidateGas = "None";
unsigned long hazardStartTime = 0;
unsigned long confirmationTime = 2000;

void updateLCD(String line1, String line2)
{
  if(line1 != lastLine1 || line2 != lastLine2)
  {
    lcd.clear();

    lcd.setCursor(0,0);
    lcd.print(line1);

    lcd.setCursor(0,1);
    lcd.print(line2);

    lastLine1 = line1;
    lastLine2 = line2;
  }
}

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
  // Serial.println("Warming up sensors... please wait 15 min");
  lcd.print("WARMING UP");
  //delay(900000); //idealy 15 mins for warming up.
  
  // 2. Auto-Calibration
  // Serial.println("Calibrating baselines in clean air...");
  lcd.setCursor(0,0);
  lcd.print("INITIALIZING");
  float sum2 = 0, sum5 = 0, sum7 = 0;
  int samples = 50; 
  
  for(int i = 0; i < samples; i++) {
    int gas1 = analogRead(mq2);
    int gas2 = analogRead(mq5);
    int gas3 = analogRead(mq7);
    // Serial.print("MQ2: "); Serial.print(gas1);
    // Serial.print(" MQ5: "); Serial.print(gas2);
    // Serial.print(" MQ7: "); Serial.print(gas3);
    // Serial.println("");


    sum2 += gas1;
    sum5 += gas2;
    sum7 += gas3;
    delay(100);
  }
  
  mq2_baseline = sum2 / samples;
  mq5_baseline = sum5 / samples;
  mq7_baseline = sum7 / samples;

  mq2_filtered = mq2_baseline;
  mq5_filtered = mq5_baseline;
  mq7_filtered = mq7_baseline;

  // Serial.print("MQ2_baseline: "); Serial.println(mq2_baseline);
  // Serial.print(" MQ5_baseline: "); Serial.println(mq5_baseline);
  // Serial.print(" MQ7_baseline: "); Serial.println(mq7_baseline);
  
  //Serial.println("System Ready!");
  lcd.setCursor(0,0);
  lcd.print("SYSTEM READY !");

  Serial.println("time,raw1,raw2,raw3,filt1,filt2,filt3,base1,base2,base3,norm1,norm2,norm3,temp,humidity,fire,gas");

}

void loop()
{
  int gas1 = analogRead(mq2);
  int gas2 = analogRead(mq5);
  int gas3 = analogRead(mq7);
  int flame = digitalRead(flamePin);
  float temp = dht.readTemperature();


//EMA for raw mq sensor values 
  mq2_filtered = (alpha_signal * gas1) + ((1.0 - alpha_signal) * mq2_filtered);
  mq5_filtered = (alpha_signal * gas2) + ((1.0 - alpha_signal) * mq5_filtered);
  mq7_filtered = (alpha_signal * gas3) + ((1.0 - alpha_signal) * mq7_filtered);

  float mq2_norm = (mq2_filtered - mq2_baseline) / mq2_baseline;
  float mq5_norm = (mq5_filtered - mq5_baseline) / mq5_baseline;
  float mq7_norm = (mq7_filtered - mq7_baseline) / mq7_baseline;

  String currentCandidate = "None";

  if (mq7_norm > 0.2 && mq2_norm < 0.1) {
    currentCandidate = "Carbon Monoxide";
  } 
  else if (mq2_norm > 0.2 && mq5_norm > 0.2) {
    currentCandidate = "LPG/Propane";
  } 
  else if (mq2_norm > 0.2 && mq5_norm < 0.1) {
    currentCandidate = "Smoke/Fire";
  }
  else if (mq5_norm > 0.2 && mq2_norm < 0.15) {
    currentCandidate = "Methane/NG";
  }

  // 2. TEMPORAL CONFIRMATION LOGIC
  if (currentCandidate != "None") {
    if (currentCandidate == candidateGas) {
      if (millis() - hazardStartTime >= confirmationTime) {
        detectedGas = candidateGas; 
      }
    } else {
      candidateGas = currentCandidate;
      hazardStartTime = millis();
    }
  } else {
    candidateGas = "None";
    detectedGas = "None";
    hazardStartTime = 0;
  }

  bool gasDetected = (mq2_norm > 0.15 || mq5_norm > 0.15 || mq7_norm > 0.15);
  bool fireDetected = (flame == LOW);

  if (!gasDetected && !fireDetected &&
    abs(mq2_norm) < 0.05 &&
    abs(mq5_norm) < 0.05 &&
    abs(mq7_norm) < 0.05) {

  mq2_baseline = (1 - alpha) * mq2_baseline + alpha * gas1;
  mq5_baseline = (1 - alpha) * mq5_baseline + alpha * gas2;
  mq7_baseline = (1 - alpha) * mq7_baseline + alpha * gas3;

  //Serial.println("Baseline updating...");
 }

  // --- ACTION LOGIC ---
  if (gasDetected || fireDetected) {
    digitalWrite(buzzerPin, HIGH);
    digitalWrite(fanPin, HIGH);

    // Dynamic LCD text generation based on hazard type
    if (gasDetected && fireDetected) {
      updateLCD("DANGER!", "FIRE & " + detectedGas);
    } else if (gasDetected) {
      updateLCD("DANGER!", "Gas: " + detectedGas);
    } else {
      updateLCD("DANGER!", "Fire Detected!");
    }
  } 


  else {
    digitalWrite(buzzerPin, LOW);
    digitalWrite(fanPin, LOW);

    // Normal operation display
    updateLCD("System Safe", "Temp: " + String(temp) + "C");
  }

//--- HUMAN-READABLE-DATA---//
  // Serial.println("=========================================");
  // Serial.print("STATUS:\t\tGas: "); Serial.print(detectedGas); Serial.print("\tFire: "); Serial.print(fireDetected ? "YES" : "NO"); Serial.print("\tTemp: "); Serial.print(temp,1); Serial.println("C");
  // Serial.println("-----------------------------------------");
  // Serial.println("\t\tMQ2\tMQ5\tMQ7");
  // Serial.print("RAW:\t\t");      Serial.print(gas1);           Serial.print("\t"); Serial.print(gas2);           Serial.print("\t"); Serial.println(gas3);
  // Serial.print("FILTERED:\t");    Serial.print(mq2_filtered,0); Serial.print("\t"); Serial.print(mq5_filtered,0); Serial.print("\t"); Serial.println(mq7_filtered,0);
  // Serial.print("BASELINE:\t");    Serial.print(mq2_baseline,0); Serial.print("\t"); Serial.print(mq5_baseline,0); Serial.print("\t"); Serial.println(mq7_baseline,0);
  // Serial.print("NORMALIZED:\t");  Serial.print(mq2_norm,2);     Serial.print("\t"); Serial.print(mq5_norm,2);     Serial.print("\t"); Serial.println(mq7_norm,2);
  // Serial.println("=========================================\n");
//------------------------------------------------------------//

  // --- PRODUCTION MACHINE-READABLE CSV STREAM ---
  // Format: timestamp,mq2,mq5,mq7,temp,humidity,flame,label
  // --- PRODUCTION ML CSV STREAM ---
  Serial.print(millis());               Serial.print(",");
  Serial.print(gas1);                   Serial.print(","); // Raw values
  Serial.print(gas2);                   Serial.print(",");
  Serial.print(gas3);                   Serial.print(",");
  Serial.print(mq2_filtered, 1);        Serial.print(","); // Filtered values
  Serial.print(mq5_filtered, 1);        Serial.print(",");
  Serial.print(mq7_filtered, 1);        Serial.print(",");
  Serial.print(mq2_baseline, 1);        Serial.print(","); // Baselines
  Serial.print(mq5_baseline, 1);        Serial.print(",");
  Serial.print(mq7_baseline, 1);        Serial.print(",");
  Serial.print(mq2_norm, 4);            Serial.print(","); // Normalized values
  Serial.print(mq5_norm, 4);            Serial.print(",");
  Serial.print(mq7_norm, 4);            Serial.print(",");
  Serial.print(temp, 1);                Serial.print(",");
  Serial.print(dht.readHumidity(), 1);  Serial.print(",");
  Serial.print(fireDetected ? 1 : 0);   Serial.print(",");
  Serial.println(detectedGas);                             // Ends line, NO trailing comma

  delay(1000); 
}