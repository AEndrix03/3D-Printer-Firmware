void setup() {
  Serial.begin(9600);  // Imposta il baud rate a 9600 bps
  pinMode(2, OUTPUT);
}

enum Motor : int {
  X = 0,
  Y = 1,
  Z = 2,
  A = 3
};

#define X_DRIVER A4988
#define Y_DRIVER A4988
#define Z_DRIVER A4988
#define A_DRIVER A4988

//Motor invert direction (true = Normally left on positive step)
#define X_INVERT_DIRECTION false
#define Y_INVERT_DIRECTION false
#define Z_INVERT_DIRECTION true
#define A_INVERT_DIRECTION true

#define X_STEP 2
#define Y_STEP 3
#define Z_STEP 4
#define A_STEP 12

//Direction of the Stepper Motors
#define X_DIR 5
#define Y_DIR 6
#define Z_DIR 7
#define A_DIR 13

//End Stops of the Axis
#define X_ENDSTOP 9
#define Y_ENDSTOP 10
#define Z_ENDSTOP 11

#define X_MM_PER_STEP 0.433
#define Y_MM_PER_STEP 0.410
#define Z_MM_PER_STEP 0.013
#define A_MM_PER_STEP 0.287

//Axis Max Speed
#define X_MAX_SPEED 1
#define Y_MAX_SPEED 1
#define Z_MAX_SPEED 1
#define A_MAX_SPEED 1

//Bed Size (in mm)
#define X_BED_SIZE 100
#define Y_BED_SIZE 150
#define Z_BED_SIZE 1500

//Offsets (in mm)
#define X_OFFSET_MIN 10
#define Y_OFFSET_MIN 10
#define Z_OFFSET_MIN 10

#define X_OFFSET_MAX 90
#define Y_OFFSET_MAX 140
#define Z_OFFSET_MAX 200

//Extrusors
#define NOZZLE_SENSIBILITY 0.2  //in mm
#define RETRACTION 5            //in mm

#define HOME_LOCATION \
  { X_OFFSET_MIN, Y_OFFSET_MIN, Z_OFFSET_MIN }

#define PRINTER_NAME "AE PRINTER"
#define PRINTER_VERSION "2.0"
#define PRINTER_AUTHOR "Andrea Redegalli"

#define SERIAL_BAUDRATE 9600

#define FIRMWARE_VERSION "0.0.1"
#define FIRMWARE_AUTHOR "Andrea Redegalli"
#define FIRMWARE_NAME "3DP-Firmware"
#define FIRMWARE_WEBSITE "www.link.com"

void step(Motor motor, int steps, int dir);

void step(Motor motor, int steps, int dir) {
  int pinStep = 0, pinDir, speed;

  if (steps == 0)
    return;
  else if (steps < 0) {
    steps *= -1;
    dir *= -1;
  }

  switch (motor) {
    case X:
      {
        dir *= X_INVERT_DIRECTION ? -1 : 1;
        speed = X_MAX_SPEED;
        pinStep = X_STEP;
        pinDir = X_DIR;
        break;
      }
    case Y:
      {
        dir *= Y_INVERT_DIRECTION ? -1 : 1;
        speed = Y_MAX_SPEED;
        pinStep = Y_STEP;
        pinDir = Y_DIR;
        break;
      }
    case Z:
      {
        dir *= Z_INVERT_DIRECTION ? -1 : 1;
        speed = Z_MAX_SPEED;
        pinStep = Z_STEP;
        pinDir = Z_DIR;
        break;
      }
    case A:
      {
        dir *= A_INVERT_DIRECTION ? -1 : 1;
        speed = A_MAX_SPEED;
        pinStep = A_STEP;
        pinDir = A_DIR;
        break;
      }
  }

  digitalWrite(pinDir, dir > 0 ? HIGH : LOW);

  for (int i = 0; i < steps; i++) {
    digitalWrite(pinStep, HIGH);
    delay(speed);
    digitalWrite(pinStep, LOW);
  }
}

void loop() {
  delay(1000);
  step(X, 100, 1);
}