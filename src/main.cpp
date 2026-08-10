//#include "Arduino.h"
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "LedControl.h"
#include "Delay.h"

#define MATRIX_A  1
#define MATRIX_B  0
// Values are 260/330/400
//#define ACC_THRESHOLD_LOW 300
//#define ACC_THRESHOLD_HIGH 600
#define ACC_THRESHOLD_LOW -5
#define ACC_THRESHOLD_HIGH 5
// This takes into account how the matrixes are mounted
#define ROTATION_OFFSET 90
#define DELAY_LONG 100

#define DEBUG_OUTPUT 1

// pin constants
const int DATA_PIN = 5;
#if defined(ARDUINO_ARCH_ESP32)
const int CLK_PIN = 18;
#else
const int CLK_PIN = 3;
#endif
const int LOAD_PIN = 4;
#if defined(ARDUINO_ARCH_ESP32)
const int X_PIN = A0;
const int Y_PIN = A3;
#else
const int X_PIN = A1;
const int Y_PIN = A2;
#endif
const int BUZZ_PIN = 14;

bool alarmWentOff = false;
byte delayHours = 0;
byte delayMinutes = 1;
int gravity;
int resetCounter = 0;
NonBlockDelay d;

Adafruit_MPU6050 mpu;
LedControl lc = LedControl(DATA_PIN, CLK_PIN, LOAD_PIN, 2);

/**
   Get delay between particle drops (in seconds)
*/
long getDelayDrop() {
  return delayMinutes + delayHours * 60;
}


#if DEBUG_OUTPUT
void printmatrix() {
  Serial.println(" 0123-4567 ");
  for (int y = 0; y < 8; y++) {
    if (y == 4) {
      Serial.println("|----|----|");
    }
    Serial.print(y);
    for (int x = 0; x < 8; x++) {
      if (x == 4) {
        Serial.print("|");
      }
      Serial.print(lc.getXY(0, x, y) ? "X" : " ");
    }
    Serial.println("|");
  }
  Serial.println("-----------");
}
#endif


coord getDown(int x, int y) {
  coord xy;
  xy.x = x - 1;
  xy.y = y + 1;
  return xy;
}
coord getLeft(int x, int y) {
  coord xy;
  xy.x = x - 1;
  xy.y = y;
  return xy;
}
coord getRight(int x, int y) {
  coord xy;
  xy.x = x;
  xy.y = y + 1;
  return xy;
}

bool canGoLeft(int addr, int x, int y) {
  if (x == 0) return false;
  return !lc.getXY(addr, getLeft(x, y));
}
bool canGoRight(int addr, int x, int y) {
  if (y == 7) return false;
  return !lc.getXY(addr, getRight(x, y));
}
bool canGoDown(int addr, int x, int y) {
  if (y == 7) return false;
  if (x == 0) return false;
  if (!canGoLeft(addr, x, y)) return false;
  if (!canGoRight(addr, x, y)) return false;
  return !lc.getXY(addr, getDown(x, y));
}

void goDown(int addr, int x, int y) {
  lc.setXY(addr, x, y, false);
  lc.setXY(addr, getDown(x, y), true);
}
void goLeft(int addr, int x, int y) {
  lc.setXY(addr, x, y, false);
  lc.setXY(addr, getLeft(x, y), true);
}
void goRight(int addr, int x, int y) {
  lc.setXY(addr, x, y, false);
  lc.setXY(addr, getRight(x, y), true);
}

int countParticles(int addr) {
  int c = 0;
  for (byte y = 0; y < 8; y++) {
    for (byte x = 0; x < 8; x++) {
      if (lc.getXY(addr, x, y)) {
        c++;
      }
    }
  }
  return c;
}

bool moveParticle(int addr, int x, int y) {
  if (!lc.getXY(addr, x, y)) {
    return false;
  }

  bool can_GoLeft = canGoLeft(addr, x, y);
  bool can_GoRight = canGoRight(addr, x, y);

  if (!can_GoLeft && !can_GoRight) {
    return false;
  }
  //Serial.println("Here");
  bool can_GoDown = canGoDown(addr, x, y);

  if (can_GoDown) {
    goDown(addr, x, y);
  } else if (can_GoLeft && !can_GoRight) {
    goLeft(addr, x, y);
  } else if (can_GoRight && !can_GoLeft) {
    goRight(addr, x, y);
  } else if (random(2) == 1) {
    goLeft(addr, x, y);
  } else {
    goRight(addr, x, y);
  }
  return true;
}

void fill(int addr, int maxcount) {
  int n = 8;
  byte x, y;
  int count = 0;
  for (byte slice = 0; slice < 2 * n - 1; ++slice) {
    byte z = slice < n ? 0 : slice - n + 1;
    for (byte j = z; j <= slice - z; ++j) {
      y = 7 - j;
      x = (slice - j);
      lc.setXY(addr, x, y, (++count <= maxcount));
    }
  }
}

int getGravity() {////////////////////////////////////////////////////////////////

    /* Get new sensor events with the readings */
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  /* Print out the values */
  //Serial.print("Acceleration X: ");
  //Serial.print(a.acceleration.x);
  //Serial.print(", Y: ");
  //Serial.print(a.acceleration.y);
  //Serial.print(", Z: ");
  //Serial.print(a.acceleration.z);
  //Serial.println(" m/s^2");

  int x = (int)a.acceleration.x;
  int y = (int)a.acceleration.y;

  //int x = analogRead(X_PIN);
  //int y = analogRead(Y_PIN);
  if (y < ACC_THRESHOLD_LOW)  {
    Serial.println("0");
    return 0;
  }
  if (x > ACC_THRESHOLD_HIGH) {
    Serial.println("90");
    return 90;
  }
  if (y > ACC_THRESHOLD_HIGH) {
    Serial.println("180");
    return 180;
  }
  if (x < ACC_THRESHOLD_LOW)  {
    Serial.println("270");
    return 270;
  }
  return gravity;
}

int getTopMatrixLong() {
  return (getGravity() == 180) ? MATRIX_B : MATRIX_A;
}
int getBottomMatrixLong() {
  return (getGravity() == 180) ? MATRIX_A : MATRIX_B;
}

void resetTime() {
  for (byte i = 0; i < 2; i++) {
    lc.clearDisplay(i);
  }
  fill(getTopMatrixLong(), 64);
  d.Delay(getDelayDrop() * 1000);
}

bool updateMatrixLong() {
  int n = 8;
  bool somethingMoved = false;
  byte x, y;
  bool direction;
  for (byte slice = 0; slice < 2 * n - 1; ++slice) {
    direction = (random(2) == 1);
    byte z = slice < n ? 0 : slice - n + 1;
    for (byte j = z; j <= slice - z; ++j) {
      y = direction ? (7 - j) : (7 - (slice - j));
      x = direction ? (slice - j) : j;
      if (moveParticle(MATRIX_B, x, y)) {
        somethingMoved = true;
      };
      if (moveParticle(MATRIX_A, x, y)) {
        somethingMoved = true;
      }
    }
  }
  return somethingMoved;
}

boolean dropParticleLong() {
  if (d.Timeout()) {
    d.Delay(getDelayDrop() * 1000);
    if (gravity == 0 || gravity == 180) {
      int top = (gravity == 180) ? MATRIX_B : MATRIX_A;
      int bottom = (gravity == 180) ? MATRIX_A : MATRIX_B;
      byte topCoord = (top == MATRIX_A) ? 0 : 7;
      byte bottomCoord = (bottom == MATRIX_A) ? 0 : 7;
      if (lc.getRawXY(top, topCoord, topCoord) && !lc.getRawXY(bottom, bottomCoord, bottomCoord)) {
        lc.invertRawXY(top, topCoord, topCoord);
        lc.invertRawXY(bottom, bottomCoord, bottomCoord);
        tone(BUZZ_PIN, 440, 10);
        return true;
      }
    }
  }
  return false;
}

void alarm() {
  for (int i = 0; i < 5; i++) {
    tone(BUZZ_PIN, 440, 200);
    delay(1000);
  }
}

/**
   Setup
*/
void setup() {
  Serial.begin(9600);
    // Try to initialize!
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPU6050 Found!");

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  Serial.print("Accelerometer range set to: ");
  switch (mpu.getAccelerometerRange()) {
  case MPU6050_RANGE_2_G:
    Serial.println("+-2G");
    break;
  case MPU6050_RANGE_4_G:
    Serial.println("+-4G");
    break;
  case MPU6050_RANGE_8_G:
    Serial.println("+-8G");
    break;
  case MPU6050_RANGE_16_G:
    Serial.println("+-16G");
    break;
  }
  mpu.setGyroRange(MPU6050_RANGE_2000_DEG);
  Serial.print("Gyro range set to: ");
  switch (mpu.getGyroRange()) {
  case MPU6050_RANGE_250_DEG:
    Serial.println("+- 250 deg/s");
    break;
  case MPU6050_RANGE_500_DEG:
    Serial.println("+- 500 deg/s");
    break;
  case MPU6050_RANGE_1000_DEG:
    Serial.println("+- 1000 deg/s");
    break;
  case MPU6050_RANGE_2000_DEG:
    Serial.println("+- 2000 deg/s");
    break;
  }

  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.print("Filter bandwidth set to: ");
  switch (mpu.getFilterBandwidth()) {
  case MPU6050_BAND_260_HZ:
    Serial.println("260 Hz");
    break;
  case MPU6050_BAND_184_HZ:
    Serial.println("184 Hz");
    break;
  case MPU6050_BAND_94_HZ:
    Serial.println("94 Hz");
    break;
  case MPU6050_BAND_44_HZ:
    Serial.println("44 Hz");
    break;
  case MPU6050_BAND_21_HZ:
    Serial.println("21 Hz");
    break;
  case MPU6050_BAND_10_HZ:
    Serial.println("10 Hz");
    break;
  case MPU6050_BAND_5_HZ:
    Serial.println("5 Hz");
    break;
  }
  randomSeed(analogRead(A3));

  for (byte i = 0; i < 2; i++) {
    lc.shutdown(i, false);
    lc.setIntensity(i, 1);
  }

  resetTime();
}

/**
   Main loop
*/
void loop() {
  delay(DELAY_LONG);




  gravity = getGravity();
  lc.setRotation((ROTATION_OFFSET + gravity) % 360);

  bool moved = updateMatrixLong();
  bool dropped = dropParticleLong();


  if (!moved && !dropped && !alarmWentOff && (countParticles(getTopMatrixLong()) == 0)) {
    alarmWentOff = true;
    alarm();
  }
  if (dropped) {
    alarmWentOff = false;
  }

}