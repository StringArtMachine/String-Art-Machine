# String-Art-Machine

## Overview

The **String Art Machine** is a hardware-software project designed to automate the creation of string art using motors and a microcontroller (such as Arduino or ESP32). The system reads instructions from a file and uses stepper motors and a servo to precisely wind thread across fixed points, creating intricate patterns on a board. The project combines embedded programming, web-based control interfaces, and state persistence to deliver a robust automated string art solution.

## Features

- Automated control of stepper motors and servo for precise thread placement.
- Persistent machine state storage and recovery to resume interrupted jobs.
- Web interface for setting parameters like number of lines.
- Real-time progress tracking and state updates.
- Uses a collection of Arduino-compatible libraries for motor control, debugging, and file parsing.

## Hardware Requirements

- Microcontroller (e.g., ESP32 or Arduino)
- Stepper motors and drivers (for winding thread)
- Servo motor (for needle or thread positioning)
- Supporting electronics (power supply, wiring, etc.)
- Frame/board to mount pins/nails for string art

## Software Components

- `demo/demo.ino`: Main Arduino sketch controlling the hardware, reading instructions, and updating progress.
- `server.js`: Node.js server to provide a web-based interface and manage machine state and progress.
- Libraries:
  - **AccelStepper**: For advanced stepper motor control with acceleration.
  - **ArduinoJson**: For parsing and generating JSON, used for configuration and communication.
  - **Arduino_DebugUtils**: For debugging output.
  - **base64**: For encoding/decoding data (optional, utility).
  - **Arduino_JSON**: For processing JSON data.
  - **ESP_Async_WebServer** (if used on ESP32): For serving the web UI.

## How It Works

1. **Setup**: Hardware is wired as per the machine requirements. The microcontroller is flashed with `demo/demo.ino`.
2. **Initialization**: On boot, the program loads the previous state (progress, thread length, positions) from non-volatile storage (NVS), allowing for job resumption.
3. **Web Interface**: Users interact via a web page (`demo/data/index.html`) to set the number of lines and other parameters.
4. **Operation**: The machine reads step instructions from a file, moves the motors accordingly, and tracks progress. State and progress are periodically saved.
5. **Pause/Resume**: If the machine is stopped or loses power, it can resume from the last saved state without data loss.

## Getting Started

1. **Clone the repository** and install required libraries (see `/libraries/` and `/demo/libraries/`).
2. **Wire the hardware** following standard practices for stepper motors, servos, and your chosen microcontroller.
3. **Upload the firmware** in `demo/demo.ino` to your board.
4. **Start the server** (if using the web interface) with Node.js:  
   ```sh
   node server.js
   ```
5. **Access the web UI** by navigating to the appropriate address (see server logs for details).
6. **Begin string art creation** by uploading or selecting an instruction file and starting the process.

## Example Code Snippet (Motor Control)

```cpp
void loop() { 
    if (!machineRunning) return;
    if (!file || !file.seek(currentLineIndex) && check) {
        Serial.println("Error reading file or EOF reached.");
        check = false;
        return;
    }
    if (!motorMovement1) {
        if (!isStepperRunning) {
            stepper.setMaxSpeed(48);
            stepper.setAcceleration(24);
            line1 = file.readStringUntil('\n');
            line2 = file.readStringUntil('\n');
            char boolStr[6];
            sscanf(line1.c_str(), "%d %d %d %s", &int1, &int2, &int3 , boolStr);
            currentMove = String(int2) + " to " + String(int3);
            // ... (motor movement logic)
        }
    }
}
```

## Libraries

- **[AccelStepper](http://www.airspayce.com/mikem/arduino/AccelStepper/):** Advanced stepper motor control for Arduino.
- **[ArduinoJson](https://arduinojson.org/):** Efficient JSON parsing and serialization for Arduino and embedded platforms.
- **[Arduino_DebugUtils](https://github.com/arduino-libraries/Arduino_DebugUtils):** Debugging utilities for Arduino.
- **[base64_arduino](https://github.com/Densaugeo/base64_arduino):** Base64 encoding/decoding for Arduino.
- **[Arduino_JSON](https://github.com/arduino-libraries/Arduino_JSON):** JSON processing for Arduino.

## License

This repository includes various open source libraries, each under their respective licenses (MIT, GPL, LGPL, etc.). See individual library folders for license details.

## Acknowledgments

- Based on and includes open source libraries from the Arduino community.
- Inspired by the creative possibilities of automated art and CNC systems.

---
**Start making art with code and motors!**
