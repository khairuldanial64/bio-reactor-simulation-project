# Bioreactor Simulation — Embedded Control & IoT Monitoring

An embedded systems project developed as part of a 10-person engineering team at University College London (UCL), from September to December 2025. The project involved developing a simulated bioreactor control system that monitors and regulates stirring speed, pH, and temperature using ESP32 microcontrollers, sensor feedback, and a remote monitoring dashboard.

My main contributions focused on the stirring motor subsystem, closed-loop control, and communication between the embedded controllers and the dashboard using I²C, Wi-Fi, and MQTT.

## System Overview

The system uses two microcontrollers to coordinate control and monitoring:

- **Arduino Nano ESP32:** Reads sensor measurements and executes the stirring, pH regulation, and temperature-control routines.
- **ESP32 DevKitC V4:** Acts as the communication gateway, retrieving measurements from the Nano and exchanging telemetry and setpoint updates with the dashboard.
- **Monitoring Dashboard:** Receives live measurement updates and sends target values for temperature, pH, and stirring speed through MQTT.

## Key Features

### 1. Closed-Loop Stirring Motor Control

Implemented a feedback-control subsystem to regulate stirring motor speed towards a configurable RPM setpoint.

- Uses a Hall-effect sensor and interrupt-driven pulse counting to measure motor rotation.
- Converts encoder pulse frequency into RPM using a configured 70 pulses per revolution.
- Applies signal smoothing to reduce fluctuations in the measured speed.
- Uses a proportional-integral (PI) controller to adjust the motor's PWM output according to the difference between target and measured RPM.
- Applies output constraints and integral clamping to limit controller output and integral windup.

### 2. Automated pH Regulation

Implemented pH measurement and pump-control logic to adjust the simulated bioreactor environment.

- Reads analogue pH sensor measurements and converts voltage into a pH estimate using two-point calibration values.
- Uses a configurable target pH with a ±0.5 pH control band in the implemented logic.
- Activates acid or base pumps according to whether the measured pH falls above or below the control band.
- Implements timed pump activation, initial priming, and a cooldown period to avoid continuous corrective dosing.

### 3. Temperature Monitoring and Control

Implemented thermistor-based temperature measurement and PI-based heater control.

- Estimates temperature from thermistor resistance using a beta-parameter model.
- Applies a calibration offset to the calculated temperature.
- Uses gain scheduling to adjust proportional control behaviour depending on the measured temperature.
- Applies integral limiting and output constraints to regulate heater PWM power towards the temperature setpoint.

### 4. Inter-Microcontroller Communication Using I²C

Implemented two-way communication between the Arduino Nano ESP32 and ESP32 DevKitC V4.

- Configured the Nano as an I²C slave at address `0x08`.
- Used the DevKitC V4 as the I²C controller to request sensor measurements.
- Transmitted RPM, pH, and temperature readings as a comma-separated data string.
- Forwarded dashboard setpoint commands to the Nano using `SET_RPM`, `SET_PH`, and `SET_TEMP` messages.

### 5. MQTT Telemetry and Remote Setpoint Updates

Implemented Wi-Fi and MQTT communication between the ESP32 DevKitC V4 and the monitoring dashboard.

- Published JSON telemetry containing measured RPM, pH, temperature, and their corresponding setpoint values.
- Configured telemetry publishing at a nominal three-second interval.
- Subscribed to MQTT topics for temperature, pH, and RPM setpoint updates.
- Parsed incoming dashboard messages and relayed the updated targets to the Nano over I²C.

## Technical Architecture

```text
          Monitoring Dashboard
                    |
              MQTT over Wi-Fi
                    |
                    v
          ESP32 DevKitC V4
          Communication Gateway
                    |
                I²C Bus
                    |
                    v
           Arduino Nano ESP32
                    |
        +-----------+-----------+
        |           |           |
        v           v           v
    Stirring      pH Control  Temperature
    Motor PI      Acid/Base   PI Controller
    Controller    Pumps       & Heater
        |           |           |
        v           v           v
     Hall Sensor  pH Sensor   Thermistor
```

## Technologies and Concepts

- **Languages:** C/C++ (Arduino)
- **Microcontrollers:** Arduino Nano ESP32, ESP32 DevKitC V4
- **Communication:** I²C, Wi-Fi, MQTT
- **Libraries:** Arduino `Wire`, `WiFi`, `PubSubClient`
- **Control Systems:** PI control, feedback control, gain scheduling, PWM
- **Sensors:** Hall-effect sensor, pH sensor, thermistor
- **Embedded Concepts:** Interrupt-driven pulse counting, analogue measurement, signal smoothing, actuator control, inter-device communication, JSON telemetry

## My Contributions

Within a 10-person engineering team, my work focused on implementing the stirring motor control subsystem and integrating communication between the microcontrollers and the monitoring dashboard. I contributed to the combined embedded code by connecting sensor measurements and control setpoints across the I²C interface and MQTT communication pipeline.

This involved coordinating low-level motor control with higher-level monitoring and control functionality, allowing measurements to be transmitted to the dashboard and incoming target values to be relayed to the embedded controller.

## Key Learning Outcomes

- Applying feedback-control principles to an embedded motor-control system.
- Integrating sensor readings, actuators, and control algorithms on a microcontroller.
- Implementing I²C communication between two microcontrollers with distinct responsibilities.
- Connecting embedded hardware to a dashboard through Wi-Fi and MQTT.
- Integrating multiple subsystems within a collaborative engineering project.

## Future Improvements

- Add robust validation and error handling for I²C messages and MQTT payloads.
- Improve synchronisation between sensor measurements, control updates, and telemetry publishing.
- Evaluate controller performance using measured settling time, steady-state error, and response to setpoint changes.
- Improve MQTT security and move network credentials out of source code.
- Add fault detection for sensor failures and communication interruptions.

---

**Project:** Bioreactor Simulation  
**Team size:** 10  
**Duration:** September–December 2025  
**Institution:** University College London (UCL)
