#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <AccelStepper.h>
#include <Arduino.h>

// Static IP configuration
IPAddress local_IP(192, 168, 1, 184);  // Change this to the desired static IP
IPAddress gateway(192, 168, 1, 1);     // Gateway address (usually your router IP)
IPAddress subnet(255, 255, 255, 0);    // Subnet mask
// Wi-Fi credentials
const char* ssid = "Airtel_COMPLEX";
const char* password = "C8o7m6p5l4e3x21";

// Create an instance of the web server
AsyncWebServer server(80);
Preferences prefs;
AccelStepper stepper(AccelStepper::DRIVER, 4, 5);

const int stepsPerRevolution = 200;  // Standard motor steps
const int microsteps = 32;  // Microstep setting
const int stepsPerMicrostepRevolution = stepsPerRevolution * microsteps;

// Define initial values for machine state
bool machineRunning = false;
int totalLines = 100;
int currentLine = 0;
int linesCompleted = 0 ;
String currentMove = "None";
int progress = 0;
bool fileExists = false;

// Initialize SPIFFS
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Connecting to WiFi...");
  
// Connect to Wi-Fi
    WiFi.config(local_IP, gateway, subnet);  // Set static IP
    WiFi.begin(ssid, password);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");
  Serial.println(WiFi.localIP());

  // Initialize SPIFFS for storing HTML files
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS mount failed");
    return;
  }
  Serial.printf("SPIFFS total size: %u bytes\n", SPIFFS.totalBytes());
  Serial.printf("SPIFFS used size: %u bytes\n", SPIFFS.usedBytes());

  // Open Preferences with "machineState" namespace
    prefs.begin("machine_state", false);  // Open for reading only (true)

    // Check if the machine state is already initialized in NVS
    if (prefs.isKey("progress")) {
        // Read saved machine state from NVS
        machineRunning = prefs.getBool("machineRunning", false);
        totalLines = prefs.getInt("totalLines", 100);
        currentLine = prefs.getInt("currentLine", 0);
        currentMove = prefs.getString("currentMove", "None");
        progress = prefs.getInt("progress", 0);
        fileExists = prefs.getBool("fileExists", false);

        Serial.println("Machine state loaded from NVS");
    } else {
        // State is not initialized in NVS, do nothing or set defaults manually
        Serial.println("Machine state not found in NVS. Default values will be used.");
    }

    // Close the Preferences session after reading
    prefs.end();

    // Print the loaded or default values for debugging
    Serial.println("Machine state:");
    Serial.print("Running: "); Serial.println(machineRunning);
    Serial.print("Total lines: "); Serial.println(totalLines);
    Serial.print("Current line: "); Serial.println(currentLine);
    Serial.print("Current move: "); Serial.println(currentMove);
    Serial.print("Progress: "); Serial.println(progress);
    Serial.print("File exists: "); Serial.println(fileExists);

    stepper.setMaxSpeed(5000);  // Adjust max speed (steps per second)
    stepper.setAcceleration(10000);  // Adjust acceleration (steps per second squared)

  // Serve HTML files
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });

  // Start the server
  server.begin();
  // Serve control.html
    server.on("/control.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/control.html", "text/html");
    });
    
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/style.css", "text/css");
});


server.on("/upload", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
});

server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println("Received request on /upload endpoint");

    // Add CORS headers for cross-origin requests
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "CORS headers included");
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");

        if (request->contentLength() > 0) {
        // Retrieve the plain text body from the request
        String body = request->arg("plain");
        
        Serial.println("Received body: " + body);

        // Save G-code data to SPIFFS
        if (!SPIFFS.begin()) {
            Serial.println("SPIFFS initialization failed");
            request->send(500, "text/plain", "SPIFFS initialization failed");
            return;
        }

        File gcodeFile = SPIFFS.open("/output.txt", FILE_WRITE);
        if (gcodeFile) {
            gcodeFile.print(body); // Save the raw text to the file
            gcodeFile.close();
            Serial.println("G-code saved as output.txt");
        } else {
            Serial.println("Failed to save G-code");
            request->send(500, "text/plain", "Failed to save G-code");
            return;
        }
        prefs.begin("machine_state", false);

        // Initialize machine state in Preferences if not already created
        if (!prefs.getBool("stateInitialized", false)) {
            prefs.putBool("machineRunning", false);
            prefs.putInt("totalLines", 100);
            prefs.putInt("currentLine", 10);
            prefs.putString("currentMove", "None");
            prefs.putInt("progress", 0);
            prefs.putBool("fileExists", true);
            prefs.putBool("stateInitialized", true); // Mark state as initialized
            
            Serial.println("Machine state initialized in Preferences");
        } else {
            Serial.println("Machine state already exists in Preferences");
        }
        
        
        // Check the state after saving it
        if (prefs.isKey("progress")) {
            Serial.println("Machine-state key exists in NVS.");
            Serial.print("Machine Running: ");
            Serial.println(prefs.getBool("machineRunning", false));
            Serial.print("Total Lines: ");
            Serial.println(prefs.getInt("totalLines", 100));
            // Add more as needed
        } else {
            Serial.println("Machine-state key does not exist in NVS.");
        }
        
        // End preferences session after all operations
        prefs.end();



        fileExists = true;
        request->send(200, "text/plain", "Data received successfully");
    } else {
        Serial.println("No content received");
        request->send(400, "text/plain", "Missing data");
    }
});
  
  // Simulate machine state for control page
  server.on("/machine-state", HTTP_GET, [](AsyncWebServerRequest *request){
    DynamicJsonDocument doc(1024);
  
    // Simulate the machine state
    doc["fileExists"] = fileExists;
    doc["totalLines"] = totalLines;
    doc["currentMove"] = currentMove;
    doc["currentLine"] = currentLine;
    doc["progress"] = progress;
  
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });
  
  // Handle start/resume request
  server.on("/start-or-resume", HTTP_POST, [](AsyncWebServerRequest *request){
    machineRunning = true;
    currentMove = "Moving to next line";
    request->send(200, "text/plain", "Machine started or resumed");
  });
  
  // Handle stop request
  server.on("/stop", HTTP_POST, [](AsyncWebServerRequest *request){
    machineRunning = false;
    currentMove = "None";
    request->send(200, "text/plain", "Machine stopped");
  });
  
 server.on("/restart", HTTP_POST, [](AsyncWebServerRequest *request){
    // Reset machine state
    machineRunning = false;
    currentMove = "None";
    progress = 0;
    currentLine = 0;
    fileExists = false;

    
  
    // Delete the NVS preferences file for the machine state
    prefs.clear();  // This will clear all preferences saved under the current namespace
  
    // Delete the output.txt file from SPIFFS
    SPIFFS.remove("/output.txt");

    // Send the response
    request->send(200, "text/plain", "Machine restarted, NVS file deleted, and output.txt removed");
});

  
  
  // Handle auto-calibrate request
  server.on("/autocalibrate", HTTP_POST, [](AsyncWebServerRequest *request){
    // Simulating auto-calibration process
    Serial.println("Starting auto calibration...");
    request->send(200, "text/plain", "Auto Calibration started");
  });
  
  // Handle calibration status request
  server.on("/calibration-status", HTTP_GET, [](AsyncWebServerRequest *request){
    DynamicJsonDocument doc(1024);
    doc["isCalibrating"] = false;  // Simulate that calibration is finished
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

   server.on("/move", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("direction")) {
      String direction = request->getParam("direction")->value();
      
      if (direction == "cw") 
        {
            Serial.println("Moving motor clockwise...");
            stepper.move(640000); // Set speed for CW
        } 
        else if (direction == "ccw") 
        {
            Serial.println("Moving motor counter-clockwise...");
            stepper.move(-640000); // Set speed for CCW
        }

      // Send response
      request->send(200, "application/json", "{\"message\":\"Motor move command sent\"}");
    } else {
      request->send(400, "application/json", "{\"error\":\"Direction not provided\"}");
    }
  });

  server.on("/increment-move", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("direction")) {
      String direction = request->getParam("direction")->value();
      
      // Incremental movement control
      if (direction == "cw") {
        Serial.println("Incrementing motor clockwise...");
        stepper.move(25); // Increment by 640 microsteps (1/10th of a revolution)
      } else if (direction == "ccw") {
        Serial.println("Incrementing motor counter-clockwise...");
        stepper.move(-25); // Decrement by 640 microsteps
      }

      // Send response
      request->send(200, "application/json", "{\"message\":\"Motor increment move command sent\"}");
    } else {
      request->send(400, "application/json", "{\"error\":\"Direction not provided\"}");
    }
  });

  server.on("/stop-manual-calibration", HTTP_POST, [](AsyncWebServerRequest *request) {
    // Stop the stepper motor
    Serial.println("Stopping motor...");
    stepper.stop();

    // Send response
    request->send(200, "application/json", "{\"message\":\"Manual calibration stopped\"}");
  });


}

void loop() {
  stepper.run();
}