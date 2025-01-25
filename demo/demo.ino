#include <AccelStepper.h>
#include <ESP32Servo.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <FS.h>
#include <Ticker.h>
#include <WiFi.h>

IPAddress local_IP(192, 168, 1, 184);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
const char *ssid = "Airtel_COMPLEX";
const char *password = "C8o7m6p5l4e3x21";

AsyncWebServer server(80);
Preferences prefs;
Ticker updateticker;
Ticker limitSwitchTicker;
Ticker motorTicker;
Ticker drillTicker ;
Ticker servoTicker ;
AccelStepper stepper(AccelStepper::DRIVER, 5, 4);
Servo servo1;
Servo servo2;

void updatePreferences();
void runDrillUp();
void runDrillDown();
void runDrillServo();
void runStepper();
void calibratestepperControl();
void checkLimitSwitch();


const int threadlimitSwitchPin = 2;
const int zerolimitSwitchPin = 1;

bool machineRunning = false;
int totalLines = 100;
int linesCompleted = 0;
String currentMove = "None";
int progress = 0;

int drillProgress = 0;
int drillPosition = 0 ;
int drillCurrentPosition = 0 ;
bool isDrillStartPosition = false ;
bool isDrillEndPosition = false ;
bool isAutoDrill = false ;
bool isDrilling = false ;
bool isDrillSet = false ;
String upPosition = "unset";
String downPosition = "unset";
int servodrillPosition=0 ;
int servoneedlePosition=0 ;
String inPosition = "unset";
String outPosition = "unset";
int in = 180 ;
int out = 60 ;
int up = 0 ;
int down = 0 ;
int maxUp = 0 ;
int maxDown = 0 ;
int maxIn=180 ;
int maxOut =0 ;
int servo2CurrentPosition = 0 ;
int servo1CurrentPosition = 0 ;
int int1, int2, int3;

bool servoMovement1 = false;
bool servoMovement2 = false;
bool motorMovement1 = false;
bool motorMovement2 = false ;
bool stepperMovement2 = false;
bool servoMoving = false ;
int remainingSteps = 0; 
bool isStepperRunning = false;
bool isServoRunning = false ;
int stepperPosition = 0 ;
bool fileExists = false;
int currentLineIndex = 0;
unsigned long servoStartTime = millis() ;
bool isThreadOver = false ;
bool isZero = false ;
bool shouldCalibrate = false ;


void setup() {

  Serial.begin(115200);
  delay(1000);
  Serial.println("Connecting to WiFi...");
  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.println(WiFi.localIP());
  

  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS mount failed");
    return;
  }
  Serial.printf("SPIFFS used size: %u bytes\n", SPIFFS.usedBytes());
  Serial.printf("SPIFFS total size: %u bytes\n", SPIFFS.totalBytes());
  

  
  prefs.begin("machine_state", false);
  if (prefs.isKey("progress")) {
    totalLines = prefs.getInt("totalLines", 100);
    linesCompleted = prefs.getInt("linesCompleted", 0);
    currentMove = prefs.getString("currentMove", "None");
    progress = prefs.getInt("progress", 0);
    in = prefs.getInt("in",0);
    out = prefs.getInt("out",0);
    if (progress>0) shouldCalibrate = true ;
    fileExists = true ;
    Serial.println("Machine state loaded from NVS");
    Serial.print("Total lines: ");
    Serial.println(totalLines);
    Serial.print("Current move: ");
    Serial.println(currentMove);
    Serial.print("Progress: ");
    Serial.println(progress);
  } else {
    Serial.println(
        "Machine state not found in NVS. Default values will be used.");
  }
  prefs.end();

  servo1.attach(27);
  servo2.attach(8);
  stepper.setMaxSpeed(4000);
  stepper.setAcceleration(250);
  pinMode(threadlimitSwitchPin, INPUT);
  pinMode(zerolimitSwitchPin, INPUT);
  servo1.write(in) ;
  servo1CurrentPosition = in ;
  servo2.write(maxUp);
  servo2CurrentPosition = maxUp ;


  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/index.html", "text/html");
  });


  server.begin();


  server.on("/control.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/control.html", "text/html");
  });

  
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/style.css", "text/css");
  });


  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {

    StaticJsonDocument<1024> jsonDoc;
    DeserializationError error = deserializeJson(jsonDoc, data, len);
    if (error) {
        request->send(400, "application/json", "{\"error\": \"Invalid JSON\"}");
        return;
    }
    if (!jsonDoc.containsKey("numberOfLines") || !jsonDoc.containsKey("content")) {
        request->send(400, "application/json", "{\"error\": \"Missing parameters\"}");
        return;
    }
    String numberOfLinesString = jsonDoc["numberOfLines"].as<String>();
    String content = jsonDoc["content"].as<String>();


    totalLines = numberOfLinesString.toInt();


    if (totalLines <= 0 || content.isEmpty()) {
        request->send(400, "application/json", "{\"error\": \"Invalid data\"}");
        return;
    }


    if (!SPIFFS.begin()) {
        request->send(500, "application/json", "{\"error\": \"SPIFFS initialization failed\"}");
        return;
    }


    File file = SPIFFS.open("/output.txt", FILE_APPEND);
    if (file) {
        file.print(content);
        file.close();
        Serial.println("Data saved to output.txt");
        String response = "{\"status\": \"success\", \"message\": \"Data saved\"}";
        request->send(200, "application/json", response);
    } else {
        request->send(500, "application/json", "{\"error\": \"Failed to save data\"}");
    }

    updateticker.attach(30, updatePreferences);
    limitSwitchTicker.attach_ms(100, checkLimitSwitch);
    
});


  server.on("/machine-state", HTTP_GET, [](AsyncWebServerRequest *request) {

    DynamicJsonDocument doc(1024);
    doc["isThreadOver"] = isThreadOver ;
    doc["fileExists"] = fileExists;
    doc["totalLines"] = totalLines;
    doc["currentMove"] = currentMove;
    doc["progress"] = progress;
    doc["drillprogress"] = drillProgress;
    doc["isDrillSet"] = isDrillSet ;
    doc["drillposition"] = drillPosition;
    doc["upPosition"] = upPosition;
    doc["downPosition"] = downPosition;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
});


  server.on("/start-or-resume", HTTP_POST, [](AsyncWebServerRequest *request) {

    machineRunning = true;
    isThreadOver = false ;
    request->send(200, "text/plain", "Machine started");
});


  server.on("/stop", HTTP_POST, [](AsyncWebServerRequest *request) {

    machineRunning = false;
    isStepperRunning = false ;
    isServoRunning= false ;
    request->send(200, "text/plain", "Machine stopped");
  });


  server.on("/restart", HTTP_POST, [](AsyncWebServerRequest *request) {

    machineRunning = false;
    totalLines = 100;
    linesCompleted = 0;
    currentMove = "None";
    progress = 0;
    isDrilling = false;
    drillProgress = 0;
    drillPosition = 0 ;
    isDrillSet = false ;
    upPosition = "unset";
    downPosition = "unset";
    servodrillPosition=0 ;
    servoneedlePosition=0 ;
    inPosition = "unset";
    outPosition = "unset";
    in = 0 ;
    out = 0 ;
    up = 0 ;
    down = 0 ;
    servoMovement1 = false;
    servoMovement2 = false;
    motorMovement1 = false;
    stepperMovement2 = false;
    remainingSteps = 0; 
    isStepperRunning = false;
    isServoRunning = false ;
    stepperPosition = 0 ;
    fileExists = false;
    currentLineIndex = 0;
    isThreadOver = false ;
    updateticker.detach();
    prefs.clear();
    SPIFFS.remove("/output.txt");
    request->send(
        200, "text/plain",
        "Machine restarted, NVS file deleted, and output.txt removed");
  });


  server.on("/togglecalibrate", HTTP_POST, [](AsyncWebServerRequest *request){
    
    motorMovement1 = false ; 
    servoMovement1 = false ;
    motorMovement2 = false ;
    servoMovement2 = false ;
    machineRunning = false ;
    isStepperRunning = false;
    isServoRunning = false;
    shouldCalibrate = false ;
    request->send(200, "text/plain", "Calibration started");
  });


  server.on("/autocalibrate", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println("Starting auto calibration...");
    stepper.moveTo(1000000);
    motorTicker.attach_ms(100, calibratestepperControl);
    request->send(200, "text/plain", "Auto Calibration started");
  });


  server.on("/move", HTTP_POST, [](AsyncWebServerRequest *request) {

    if (request->hasParam("direction")) {
      String direction = request->getParam("direction")->value();
      motorTicker.attach_ms(1,runStepper) ;
      if (direction == "cw") {
        Serial.println("Moving motor clockwise...");
        stepper.move(-38400);
      } else if (direction == "ccw") {
        Serial.println("Moving motor counter-clockwise...");
        stepper.move(38400);
      }
      request->send(200, "application/json",
                    "{\"message\":\"Motor move command sent\"}");
    } else {
      request->send(400, "application/json",
                    "{\"error\":\"Direction not provided\"}");
    }
  });


  server.on("/increment-move", HTTP_POST, [](AsyncWebServerRequest *request) {

    if (request->hasParam("direction")) {
      String direction = request->getParam("direction")->value();
      motorTicker.attach_ms(1,runStepper) ;
      if (direction == "cw") {
        Serial.println("Incrementing motor clockwise (small increment)...");
        stepper.move(-10);
      } else if (direction == "ccw") {
          Serial.println("Incrementing motor counter-clockwise (small increment)...");
          stepper.move(10);
      } else if (direction == "lcw") {
          Serial.println("Incrementing motor clockwise (large increment)...");
          stepper.move(-150);
      } else if (direction == "lccw") {
          Serial.println("Incrementing motor counter-clockwise (large increment)...");
          stepper.move(150); 
      } else if (direction == "up") {
          Serial.println("Moving drill up...");
          if ((servo2CurrentPosition+10) >= maxUp) servo2.write(servo2CurrentPosition-10);
          servo2CurrentPosition-=10 ;
      } else if (direction == "down") {
          Serial.println("Moving drill down...");
          if ((servo2CurrentPosition+10) <= maxUp)  servo2.write(servo2CurrentPosition+10);
          servo2CurrentPosition+=10;
      } else if (direction == "in") {
          Serial.println("Moving drill in...");
          if ((servo1CurrentPosition+10) <= maxIn) {
            servo1.write(servo1CurrentPosition+10);
            servo1CurrentPosition+=10 ;
          }

          else {
            servo1.write(maxIn) ;
            servo1CurrentPosition=maxIn;
          }
      } else if (direction == "out") {
          Serial.println("Moving drill out...");
          if ((servo1CurrentPosition-10)>=maxOut) {
            servo1.write(servo1CurrentPosition-10);
            servo1CurrentPosition-=10;
          }
          
          else {
            servo1.write(maxOut) ;
            servo1CurrentPosition=maxOut;
          }
          
      }
      request->send(200, "application/json", "{\"message\":\"Motor increment move command sent\"}");
    }
      else {
        request->send(400, "application/json",
                      "{\"error\":\"Direction not provided\"}");
      }
    
    
  });


  server.on("/stop-manual-calibration", HTTP_POST, [](AsyncWebServerRequest *request) {

      Serial.println("Stopping motor...");
      motorTicker.detach() ;
      request->send(200, "application/json",
                    "{\"message\":\"Manual calibration stopped\"}");
    });
            
  server.on("/start-drilling", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("type"))
    {
        request->send(400, "application/json", "{\"error\": \"Missing type parameter\"}");
        return;
    }
    String type = request->getParam("type")->value();

    if (type == "man") {
        servo2.write(up);
        servo2CurrentPosition = up;
        drillTicker.attach(10, runDrillServo);
        String response = "{\"status\": \"success\", \"message\": \"" + type + " drilling started\"}";
        request->send(200, "application/json", response);
        Serial.println(type + " drilling started");
    }
    else if (type == "auto") {
        stepper.moveTo(1000000);
        isDrillStartPosition = false;
        isAutoDrill = true;
        servo2.write(up);
        servo2CurrentPosition = up;
        motorTicker.attach_ms(100, calibratestepperControl);
    }
    else {
        request->send(400, "application/json", "{\"error\": \"Invalid drilling type\"}");
    }
});




  server.on("/stop-drilling", HTTP_POST, [](AsyncWebServerRequest *request) {

    drillTicker.detach();
    if (isAutoDrill && !isDrillStartPosition) {
        stepper.stop();
    }
    request->send(200, "application/json", "Stopped");
});


server.on("/setPosition", HTTP_POST,
    [](AsyncWebServerRequest *request) {
    if (!request->hasParam("position"))
    {
        request->send(400, "application/json", "{\"error\": \"Missing position parameter\"}");
        return;
    }

    String position = request->getParam("position")->value();

        bool isNumber = true;
        for (size_t i = 0; i < position.length(); i++) {
            if (!isDigit(position[i])) {
                isNumber = false;
                break;
            }
        }

        if (isNumber) {
            drillPosition = position.toInt();
            isDrillSet = true;
            String response = "{\"status\": \"success\", \"message\": \"Current drill position updated to: " + String(drillPosition) + "\"}";
            request->send(200, "application/json", response);
            Serial.println("Current drill position updated to: " + String(drillPosition));
        } else if (position == "upPosition") {
            upPosition = "set";
            up = servo2CurrentPosition;
            request->send(200, "application/json", "{\"status\": \"success\", \"message\": \"Up position set\"}");
            Serial.println("Up position set");
        } else if (position == "downPosition") {
            downPosition = "set";
            down = servo2CurrentPosition;
            request->send(200, "application/json", "{\"status\": \"success\", \"message\": \"Down position set\"}");
            Serial.println("Down position set");
        } else if (position == "inPosition") {
            inPosition = "set";
            in = servo1CurrentPosition;
            request->send(200, "application/json", "{\"status\": \"success\", \"message\": \"In position set\"}");
            Serial.println("In position set");
        } else if (position == "outPosition") {
            outPosition = "set";
            out = servo1CurrentPosition;
            request->send(200, "application/json", "{\"status\": \"success\", \"message\": \"Out position set\"}");
            Serial.println("Out position set");
        } else {
            request->send(400, "application/json", "{\"error\": \"Invalid position\"}");
        }
    }
);





}



void loop() { 

  if (!machineRunning) {
        return;
    }

    File file = SPIFFS.open("/output.txt", "r+");
    if (!file || !file.seek(currentLineIndex)) {
        Serial.println("Error reading file or EOF reached.");
        return;
    }

    if (!motorMovement1) {
        if (!isStepperRunning) {
            String line1 = file.readStringUntil('\n');
            String line2 = file.readStringUntil('\n');
            Serial.print(line1);
            Serial.print(line2);
            sscanf(line1.c_str(), "%d %d %d", &int1, &int2, &int3);
            currentMove = String(int2) + " to " + String(int3);
            int steps = line2.toInt();
            remainingSteps = steps - stepper.currentPosition();
            stepper.move(remainingSteps) ;
            isStepperRunning=true;
        }

        if (stepper.distanceToGo() == 0) {
            motorMovement1 = true;
            isStepperRunning = false;
            remainingSteps = 0;
        }
        else {
            stepper.run();
        }
    }

    if (motorMovement1 && !servoMovement1) {
      if (!isServoRunning) {
            servo1.write(out);
            servo1CurrentPosition = out ;
            servoStartTime = millis();
            servoMoving = true;
        }
        if (millis() - servoStartTime >= 3000) {
            servoMovement1 = true;
            isServoRunning = false;
        }
    }

    if (motorMovement1 && servoMovement1 && !motorMovement2) {
        if (!isStepperRunning) {
            int steps = 150;
            remainingSteps = steps - stepper.currentPosition();
            stepper.move(remainingSteps) ;
            Serial.println("Stepper moved 150");
            isStepperRunning=true;
        }

        if (stepper.distanceToGo() == 0) {
            motorMovement2 = true;
            isStepperRunning = false;
            remainingSteps = 0;
        }
        else {
            stepper.run();
        }
    }

    if (motorMovement1 && servoMovement1 && motorMovement2 && !servoMovement2) {
      if (!isServoRunning) {
            servo1.write(in);
            servo1CurrentPosition = in ;
            servoStartTime = millis();
            isServoRunning = true;
        }
        if (millis() - servoStartTime >= 3000) {
            servoMovement2 = true;
            isServoRunning = false;
        }
    }

    if (motorMovement1 && servoMovement1 && motorMovement2 && servoMovement2) {
        motorMovement1 = false ; 
        servoMovement1 = false ;
        motorMovement2 = false ;
        servoMovement2 = false ;
        currentLineIndex = file.position();
        linesCompleted++ ;
        progress = (linesCompleted*100)*totalLines ;
    }

    file.close();
  
}

void updatePreferences() {
    if (machineRunning) {
        prefs.begin("machine_state", false);
        prefs.putInt("totalLines", totalLines);
        prefs.putString("currentMove", currentMove);
        prefs.putInt("progress", progress);
        prefs.putInt("in",0);
        prefs.putInt("out",0);
        prefs.end();
        Serial.println("Preferences updated:");
    } else {
        Serial.println("Machine is not running. Skipping update.");
    }
}

void checkLimitSwitch() {
    if (digitalRead(threadlimitSwitchPin) == HIGH) {
        isThreadOver = true;
        Serial.println("Thread Limit switch activated!");
    }
    if (digitalRead(zerolimitSwitchPin) == HIGH) {
        isZero = true;
        Serial.println("Zero Limit switch activated!");
    }
    else
    {
        isZero = false ;
    }
}

void calibratestepperControl() {
    stepper.run();
    if (isZero && isAutoDrill) {
        stepper.stop();
        motorTicker.detach();
        Serial.println("Motor stopped at zero");
        stepper.move(-1*drillPosition*150) ;
        motorTicker.attach_ms(100, runStepper) ;
    }
    else if (isZero) {
        stepper.stop();
        motorTicker.detach();
        Serial.println("Motor stopped at zero");
        stepper.move(-1*int2*150) ;
        motorTicker.attach_ms(100, runStepper) ;
    }
}

void runStepper() {
    stepper.run() ;
    if (isAutoDrill && stepper.distanceToGo() == 0) {
      motorTicker.detach();
      drillTicker.attach(10,runDrillServo) ;
      drillCurrentPosition = drillPosition ;
      isAutoDrill = false ;
      isDrillStartPosition = true ;
    }
    if (drillCurrentPosition==257) {
      drillTicker.detach() ;
      isDrillEndPosition = true ;
      isDrilling = false ;
    }
    else if (stepper.distanceToGo() == 0) {
      motorTicker.detach();
      isDrilling=false ;
    }
}

void runDrillServo() {
    if (!isDrilling) {
      servoTicker.attach_ms(100,runDrillDown) ;
      isDrilling=true ;
    }
}

void runDrillDown() {
  if (!isServoRunning) {
      servo2.write(down);
      servo2CurrentPosition = down ;
      servoStartTime = millis();
      isServoRunning = true ;
    }
    if (millis() - servoStartTime >= 1000) {
        isServoRunning = false ;
        servoTicker.detach();
        servoTicker.attach_ms(100,runDrillUp) ;
    }
}

void runDrillUp(){
    if (!isServoRunning) {
      servo2.write(up);
      servo2CurrentPosition = up ;
      servoStartTime = millis();
      isServoRunning = true ;
    }
    if (millis() - servoStartTime >= 1000) {
        isServoRunning = false ;
        servoTicker.detach();
        stepper.move(150) ;
        if (isDrillStartPosition) drillCurrentPosition++ ;
        motorTicker.attach_ms(100, runStepper) ;
    }
}
