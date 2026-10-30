#include <Wire.h>
#include <Adafruit_ADS1X15.h>

Adafruit_ADS1115 ads;

const adsGain_t gainSettings[] = {
  GAIN_SIXTEEN, GAIN_EIGHT, GAIN_FOUR,
  GAIN_TWO, GAIN_ONE, GAIN_TWOTHIRDS
};

const float fullScale[] = {
  0.256, 0.512, 1.024, 2.048, 4.096, 6.144
};

// Index 0 = highest gain, index 5 = lowest gain
const int WINDOW_SIZE = 16;
const float HIGH_THRESHOLD = 0.80;
const float LOW_THRESHOLD = 0.15;
const int LOW_WINDOWS_REQUIRED = 3;

int gainIndex = 5;
int lowWindows = 0;

void setPGA(int index) {
  gainIndex = constrain(index, 0, 5);
  ads.setGain(gainSettings[gainIndex]);

  // Discard one conversion after changing PGA
  ads.readADC_SingleEnded(0);
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  if (!ads.begin()) {
    while (true) delay(100);
  }

  ads.setDataRate(RATE_ADS1115_860SPS);
  setPGA(5);

  // Two traces in Serial Plotter
  Serial.println("Voltage_V\tGain_State");
}

void loop() {
  float samples[WINDOW_SIZE];
  float maxAbsV = 0.0;
  bool saturated = false;

  // Acquire a window at the current PGA setting
  for (int i = 0; i < WINDOW_SIZE; i++) {
    int16_t raw = ads.readADC_SingleEnded(0);
    float v = ads.computeVolts(raw);

    samples[i] = v;

    if (fabs(v) > maxAbsV) {
      maxAbsV = fabs(v);
    }

    if (raw >= 32700) {
      saturated = true;
    }
  }

  float range = fullScale[gainIndex];
  bool changed = false;

  // Reduce gain quickly for strong signals
  if ((saturated || maxAbsV > HIGH_THRESHOLD * range)
      && gainIndex < 5) {
    setPGA(gainIndex + 1);
    lowWindows = 0;
    changed = true;
  }
  // Increase gain only after several low-signal windows
  else if (maxAbsV < LOW_THRESHOLD * range) {
    lowWindows++;

    if (lowWindows >= LOW_WINDOWS_REQUIRED &&
        gainIndex > 0) {
      setPGA(gainIndex - 1);
      lowWindows = 0;
      changed = true;
    }
  } else {
    lowWindows = 0;
  }

  // Plot voltage and gain state as separate traces
  // Gain_State: 0=GAIN16, 1=GAIN8, ..., 5=GAIN2/3
  for (int i = 0; i < WINDOW_SIZE; i++) {
    Serial.print(samples[i], 4);
    Serial.print('\t');
    Serial.println(gainIndex);
  }

  // Avoid rapid switching while the new range settles
  if (changed) {
    delay(5);
  }
}