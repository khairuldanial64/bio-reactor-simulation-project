#include <Wire.h>

#define I2C_ADDR 0x08
String receivedData = "";

float adcVoltage = 0.0;

// Current Measurements
float meanmeasuredRPM = 0.0;
float pH = 0.0;
float Theat = 0.0;

// Target Setpoints
float targetRPM = 800.0;   // SET TARGET RPM
float targetPH = 5.0; // SET TARGET PH
float targetTemp = 30; // SET TARGET TEMP

// Stirring Constants and Variables
const float Kv = 225.0;
const float T = 0.15;
const float Npulses = 70.0;
const float wn = 1.0 / T;
const float zeta = 1.0;
const float wo = 1.0 / T;
const float Kp = 0.0040;
const float Ki = 0.0267;
const float freqToRPM = 60.0 / Npulses;
const byte encoderPin = 4;    // Hall sensor output
const byte motorPin   = 10;   // PWM output to MOSFET gate
volatile unsigned int pulseCount = 0;
float freq = 0.0;
float new_freq = 0.0;
float measuredRPM = 0.0;

float error = 0.0;
float integralError = 0.0;
float deltaT = 0.01;      // 10 ms fixed loop time
int motorPWM = 0;
unsigned long lastLoopTime = 0;


// pH Constants and Variables
#define SensorPin A0

bool pumpRunning = false;
unsigned long pumpStartTime = 0;
int pumpDuration = 0;
int activePumpPin = -1;

// Calibration values
float V10 = 2.50;   // Voltage in pH 10 buffer
float V4  = 1.04;  // Voltage in pH 4 buffer

float slope, intercept;
const int PumpAcidPin = D3;   // lowers pH
const int PumpBasePin = D2;   // raises pH
int lastState = -1; // -1 = no previous state

float lowerThreshold = targetPH - 0.5;  
float upperThreshold = targetPH + 0.5;

unsigned long lastActionTime = 0;
bool waitingPeriod = false;

bool firstPrimeDone = false;

unsigned long lastPHPrint = 0;
const unsigned long PH_PRINT_INTERVAL = 1000; // 1 second

// Heating Constants and Variables
// ===== Thermistor settings =====
const byte thermistorpin = A1;
const float Vcc = 5.0;
const float R = 10000;
const float Ro = 10000;
const float To = 25;
const float beta = 3700;

// Add calibration offset (thermistor reads +1°C too high)
const float T_offset = -1.0;   // Corrects temperature reading

// PWM settings
const int PIoutpin = 5;
const int pwmChannel = 0;
const int pwmFreq = 7800;
const int pwmRes = 10;

// Power limit settings
const float supplyPower = 48.0;
const float maxHeaterPower = 30.0;
const int PWM_MAX = (int)(1023.0 * (maxHeaterPower / supplyPower));

// PI Controller

const float ki = 0.02;

// Gains for scheduling:
const float kp_high = 2500;  // aggressive when far below setpoint
const float kp_low  = 800;   // gentle when near setpoint
float kp;

float Vadc, Vreal, Rth, deltat, Te;
float KIInt = 0;

long currtime, prevtime, T1;
int HeaterPower;


void receiveEvent(int bytes) {
  receivedData = "";
  while (Wire.available()) {
    receivedData += (char)Wire.read();
  }
}

void parseCommand(String cmd) {
  cmd.trim();

  if (cmd.startsWith("SET_RPM")) {
    targetRPM = cmd.substring(7).toFloat();
    return;
  }

  if (cmd.startsWith("SET_PH")) {
    targetPH = cmd.substring(6).toFloat();
    return;
  }

  if (cmd.startsWith("SET_TEMP")) {
    targetTemp = cmd.substring(8).toFloat();
    return;
  }
}

void requestEvent() {
  String response = "";
  response += String(meanmeasuredRPM, 1);
  response += ",";
  response += String(pH, 2);
  response += ",";
  response += String(Theat, 2);

  Wire.write(response.length()); // send length first

  for (int i = 0; i < response.length(); i++) {
    Wire.write(response[i]);
  }
}

// Stirring functions
void IRAM_ATTR onPulse() {
  pulseCount++;
}

void updateStirring() {
  unsigned long now = micros();

  // Run every 10 ms exactly
  if (now - lastLoopTime >= 10000) {
    lastLoopTime += 10000;

    // Read pulses counted in 10ms window
    unsigned int count = pulseCount;
    pulseCount = 0;

    // pulses per second (Hz)
    new_freq = count * 100.0;   // because (count / 0.01s)
    freq = 0.75f * freq + 0.25f * new_freq; // Take 75% of the old frequency, add 25% of the new measured frequency.

    // Convert to RPM
    measuredRPM = freq * freqToRPM;

    // PI Controller
    meanmeasuredRPM = 0.1 * measuredRPM + 0.9 * meanmeasuredRPM;    

    error = targetRPM - meanmeasuredRPM;

    integralError += Ki * error * deltaT;
    integralError = constrain(integralError, 0, 5);  // anti-windup

    float voltage = Kp * error + integralError;
    voltage = constrain(voltage, 0, 5);

    motorPWM = round((voltage / 5.0) * 255.0);
    motorPWM = constrain(motorPWM, 0, 255);

    analogWrite(motorPin, motorPWM);

    // Debugging Output
   /* Serial.print("Target RPM:");
    Serial.print(targetRPM);
    Serial.print(" | Measured RPM:");
    Serial.println(meanmeasuredRPM); */
  }
}


// pH Functions
float readPH() {
  long sumADC = 0;

  // Take 10 fast readings
  for (int i = 0; i < 10; i++) {
    sumADC += analogRead(SensorPin);
    //delay(5); // FIX THIS maybe? because stirring still okay, 
  }

  float adcValue = sumADC / 10.0;  // average

  // Convert ADC to voltage (no divider in your code)
  adcVoltage = adcValue * (3.3 / 4095.0);

  // Compute calibrated pH
  float pHValue = slope * adcVoltage + intercept;

  return pHValue;
}

void startPump(int pin, int duration_ms) {
  pumpRunning = true;
  pumpStartTime = millis();
  pumpDuration = duration_ms;
  activePumpPin = pin;

  digitalWrite(pin, HIGH);
  Serial.print("Pump ON (pin ");
  Serial.print(pin);
  Serial.println(")");
}

void updatepH() {
  // Non-blocking pump shutdown
if (pumpRunning) {
    if (millis() - pumpStartTime >= pumpDuration) {
        digitalWrite(activePumpPin, LOW);
        pumpRunning = false;
        Serial.print("Pump OFF (pin ");
        Serial.print(activePumpPin);
        Serial.println(")");
    }
    
    return;
}

  pH = readPH();  // smoothed pH value

  // Print pH only every 1 second
  if (millis() - lastPHPrint >= PH_PRINT_INTERVAL) {
    lastPHPrint = millis();
    /*Serial.print("pH: ");
    Serial.println(pH, 2); */
  } 

  // COOL DOWN (15 seconds)
  if (waitingPeriod) {
    if (millis() - lastActionTime < 15000) {
      return;   // still cooling down
    } else {
      waitingPeriod = false;
      Serial.println("Cooldown finished — resuming control.");
    }
  }

  // FIRST 4s PRIMING
  if (!firstPrimeDone) {
    Serial.println("Performing first-time 4s priming...");

    if (pH < lowerThreshold) {
      startPump(PumpBasePin, 4000);
waitingPeriod = true;
lastActionTime = millis();
firstPrimeDone= true;
return;
    } 

    else if (pH > upperThreshold) {
      startPump(PumpAcidPin, 4000);
waitingPeriod = true;
lastActionTime = millis();
firstPrimeDone = true;
return;

    } 
    else {
      Serial.println("pH inside range — skipping priming.");
    }

    firstPrimeDone = true;
    waitingPeriod = true;
    lastActionTime = millis();
    return;
  }


  // BANG-BANG CONTROL (1 sec pump)
int currentState = -1;

if (pH < lowerThreshold) {
    currentState = 0;

    if (lastState != currentState) {
        Serial.println("pH LOW → Adding base (1s)");
    }

    startPump(PumpBasePin, 1000);
    waitingPeriod = true;
    lastActionTime = millis();
}
else if (pH > upperThreshold) {
    currentState = 1;

    if (lastState != currentState) {
        Serial.println("pH HIGH → Adding acid (1s)");
    }

    startPump(PumpAcidPin, 1000);
    waitingPeriod = true;
    lastActionTime = millis();
}
else {
    currentState = 2;

    if (lastState != currentState) {
        Serial.println("pH within range — no pumping.");
    }
}

lastState = currentState;

}


// Heating Functions
void updateHeating() {
  currtime = micros();

  if (currtime - T1 >= 100000) {  // 100 ms loop
    deltat = (currtime - prevtime) * 1e-6;
    prevtime = currtime;
    T1 = currtime;

    // Read thermistor
    Vadc = analogRead(thermistorpin) * (3.3 / 4095.0);
    Vreal = Vadc;

    Rth = R * (Vreal / (Vcc - Vreal));
    if (Rth < 1) Rth = 1;

    // Calculate temperature
    Theat = (To + 273.15) * beta / (beta + (To + 273.15) * log(Rth / Ro)) - 273.15;

    // Apply calibration offset here
    Theat += T_offset;

    // Temperature error
    Te = targetTemp - Theat;

    // Gain Scheduling
    if (Theat < 27.0) kp = kp_high;  // heat fast
    else          kp = kp_low;   // slow approach

    // Anti-windup
    float Pterm = kp * Te;
    float PI_unlimited = Pterm + KIInt;

    if (PI_unlimited > 0 && PI_unlimited < 30) {
      KIInt += ki * Te * deltat;
    }

    // Clamp integral
    if (KIInt < 0) KIInt = 0;
    if (KIInt > 30) KIInt = 30;

    // Final PI Output
    float PIoutput = Pterm + KIInt;
    PIoutput = constrain(PIoutput, 0, 30);

    HeaterPower = (int)(PIoutput * (1023.0 / 30.0));
    HeaterPower = constrain(HeaterPower, 0, PWM_MAX);

    ledcWrite(pwmChannel, HeaterPower);

    // ----- Serial Output -----
   /* Serial.print(" Measured Temp = ");
    Serial.print(Theat);
    Serial.print("targetTemp = ");
    Serial.println(targetTemp);
    Serial.print(" C | PWM = ");
    Serial.print(HeaterPower);
    Serial.print(" | KP = ");
    Serial.print(kp);
    Serial.print(" | INT = ");
    Serial.println(KIInt); */
  } 
}


//       Setup
void setup() {
  Serial.begin(115200);

  Wire.begin(I2C_ADDR); 
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);

  pinMode(encoderPin, INPUT_PULLUP);
  pinMode(motorPin, OUTPUT);

  pinMode(PumpAcidPin, OUTPUT);
  pinMode(PumpBasePin, OUTPUT);

  pinMode(thermistorpin, INPUT);
  pinMode(PIoutpin, OUTPUT); 

// Stirring setup
  attachInterrupt(digitalPinToInterrupt(encoderPin), onPulse, RISING);
  analogWrite(motorPin, 0);
  lastLoopTime = micros();

// pH setup
  analogReadResolution(12);
  digitalWrite(PumpAcidPin, LOW);
  digitalWrite(PumpBasePin, LOW);
  slope = (10.00 - 4.00) / (V10 - V4);
  intercept = 10.00 - slope * V10;

// Heating setup
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  ledcSetup(pwmChannel, pwmFreq, pwmRes);
  ledcAttachPin(PIoutpin, pwmChannel);
  prevtime = micros();
  T1 = prevtime;
}


void loop() {
  updateStirring();
  updatepH();
  updateHeating();

  Serial.print("Target RPM: ");
  Serial.print(targetRPM);
  Serial.print(" RPM: ");
  Serial.print(meanmeasuredRPM, 1);
  Serial.print(" Target PH: ");
  Serial.print(targetPH);
  Serial.print(" PH: ");
  Serial.print(pH, 2);
  Serial.print(" Target TEMP: ");
  Serial.print(targetTemp);
  Serial.print(" TEMP: ");
  Serial.println(Theat); 
  /*Serial.print(pH);
  Serial.println(adcVoltage); */

  if (receivedData.length() > 0) {
  parseCommand(receivedData);
  receivedData = "";
}
}
