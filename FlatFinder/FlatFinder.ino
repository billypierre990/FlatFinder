#include <ThingSpeak.h>

/*
Project: Flat Finder
* Name: Billy Pierre
* Date: 10/31/2024
* Class Name: Board Presentation
*/

// M5SickC Plus 2 Library
#include <M5StickCPlus2.h>
#include <WiFi.h>
#include "secrets.h"

char ssid[] = WIFI_SSID;   // your network SSID (name) 
char pass[] = WIFI_PASSWORD;   // your network password
WiFiClient client;

unsigned long myChannelNumber = SECRET_CH_ID;
const char* myWriteAPIKey = SECRET_WRITE_APIKEY;

#define LED_BUILTIN 19
#define BUZZER_PIN 2           // Define the buzzer pin for sound
#define CALIBRATION_BUTTON 37  // Button pin for calibration (GPIO 37)

unsigned long lastUpdate = 0; // Declare lastUpdate here
unsigned long horizontalStartTime = 0;
unsigned long verticalStartTime = 0;
bool isLevel = false;                // Track if the device is level
bool displayedLevelMessage = false;  // Track if "LEVEL" message is displayed
bool isCalibrating = false;          // Flag for calibration mode
float referencePitch = 0;            // Reference pitch for calibration
float referenceRoll = 0;             // Reference roll for calibration

void setup() {
    Serial.begin(115200);  // Initialize serial
    auto cfg = M5.config();
    StickCP2.begin(cfg);
    
    while (!Serial) {
        ; // wait for serial port to connect. Needed for Leonardo native USB port only
    }
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);  // Connect to WiFi
    ThingSpeak.begin(client);  // Initialize ThingSpeak

    StickCP2.Display.setRotation(1);  // Initial rotation
    StickCP2.Display.setTextDatum(middle_center);
    StickCP2.Display.setFont(&fonts::FreeSansBold9pt7b);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);                // Initialize buzzer
    pinMode(CALIBRATION_BUTTON, INPUT_PULLUP);  // Set the calibration button pin
}

//********************************************************* Get Battery Percentage Function *********************************************************
int getBatteryPercentage() {
    float voltage = StickCP2.Power.getBatteryVoltage();
    if (voltage >= 4.2) return 100;        // Full charge
    if (voltage >= 3.7) return (voltage - 3.7) * 200;  // Linear scaling between 3.7V and 4.2V
    if (voltage >= 3.5) return (voltage - 3.5) * 100;  // Linear scaling between 3.5V and 3.7V
    return 0;                              // Below 3.5V, battery is low
}
//********************************************************* Get Battery Percentage Function *********************************************************




//********************************************************* Battery Percentage Function *********************************************************
void displayBatteryLevel() {
    int batteryPercent = getBatteryPercentage();
    StickCP2.Display.setTextFont(1); // Set font to a smaller size
    StickCP2.Display.setCursor(StickCP2.Display.width() - 120, 10); // Top right corner

    // Set the color based on battery percentage
    if (batteryPercent <= 5) {
        StickCP2.Display.setTextColor(RED); // Alert to charge device
        StickCP2.Display.drawString("Low Battery! Charge Now!", StickCP2.Display.width() - 150, 30);
    } else if (batteryPercent <= 15) {
        StickCP2.Display.setTextColor(RED); // Red for low battery
    } else if (batteryPercent <= 50) {
        StickCP2.Display.setTextColor(ORANGE); // Orange for medium battery
    } else {
        StickCP2.Display.setTextColor(WHITE); // White for sufficient battery
    }
    StickCP2.Display.printf("Bat: %d%%", batteryPercent); // Display battery percentage
    StickCP2.Display.setFont(&fonts::FreeSansBold9pt7b); // Revert to the default font if needed elsewhere
}
//********************************************************* Battery Percentage Function *********************************************************



//********************************************************* User Calibration Function *********************************************************
void calibrateDevice(float pitch, float roll) {
    referencePitch = pitch; // Set reference pitch
    referenceRoll = roll; // Set reference roll
    isCalibrating = false; // Exit calibration mode
    StickCP2.Display.fillScreen(BLACK); // Clear the screen
    StickCP2.Display.setTextColor(GREEN);
    StickCP2.Display.drawString("Calibration Complete!", StickCP2.Display.width() / 2, StickCP2.Display.height() / 2);
    delay(2000); // Delay to show the message
}
//********************************************************* User Calibration Function *********************************************************



//********************************************************* Draw Bubble Function *********************************************************
void drawLevelIndicator(float pitch, float roll) {
    // Clear previous frame and prepare for new drawing
    StickCP2.Display.fillScreen(BLACK);

    // Bubble Level Parameters
    int centerX = StickCP2.Display.width() / 2;
    int centerY = StickCP2.Display.height() / 2;
    int bubbleRadius = 5;  // Radius of the bubble
    int levelRadius = 40;  // Radius of the circular level indicator boundary

    // Calculate bubble position based on pitch and roll
    int bubbleX = centerX + roll * 0.5;   // Adjust sensitivity by scaling factors
    int bubbleY = centerY - pitch * 0.5;

    // Keep the bubble within the level circle
    float dist = sqrt((bubbleX - centerX) * (bubbleX - centerX) + (bubbleY - centerY) * (bubbleY - centerY));
    if (dist > levelRadius - bubbleRadius) {
        float angle = atan2(bubbleY - centerY, bubbleX - centerX);
        bubbleX = centerX + (levelRadius - bubbleRadius) * cos(angle);
        bubbleY = centerY + (levelRadius - bubbleRadius) * sin(angle);
    }

    // Draw the outer circle for the level indicator
    StickCP2.Display.drawCircle(centerX, centerY, levelRadius, WHITE);
    
    // Draw the bubble
    StickCP2.Display.fillCircle(bubbleX, bubbleY, bubbleRadius, GREEN);

    // Draw the digital horizon line based on roll
    int horizonY1 = centerY - roll * 0.5;
    int horizonY2 = centerY + roll * 0.5;
    StickCP2.Display.drawLine(0, horizonY1, StickCP2.Display.width(), horizonY2, WHITE);
}
//********************************************************* Draw Bubble Function *********************************************************



void loop() {

//********************************************************* Check Wifi Connection *********************************************************
  if (WiFi.status() != WL_CONNECTED) {
        StickCP2.Display.fillScreen(BLACK);
        StickCP2.Display.setTextColor(WHITE);
        StickCP2.Display.setTextDatum(middle_center);
        StickCP2.Display.drawString("Connecting to WiFi...", StickCP2.Display.width() / 2, StickCP2.Display.height() / 2);
        
        Serial.print("Attempting to connect to SSID: ");
        Serial.println(WIFI_SSID);
        
        // Attempt to connect to WiFi
        while (WiFi.status() != WL_CONNECTED) {
            delay(1000);  // Delay to avoid rapid connection attempts
        }
        
        // Display "Connected" message when connected
        StickCP2.Display.fillScreen(BLACK); // Clear the previous message
        StickCP2.Display.setTextColor(GREEN);
        StickCP2.Display.drawString("WiFi Connected", StickCP2.Display.width() / 2, StickCP2.Display.height() / 2);
        
        delay(2000);  // Show the message for 2 seconds
        StickCP2.Display.fillScreen(BLACK); // Clear screen for next use
        Serial.println("\nConnected.");
    }


//********************************************************* Check Calibration Button Press *********************************************************
   if (digitalRead(CALIBRATION_BUTTON) == LOW) {
        isCalibrating = true; // Enter calibration mode
        StickCP2.Display.fillScreen(BLACK);
        StickCP2.Display.setTextColor(WHITE);
        StickCP2.Display.drawString("Place on flat surface", StickCP2.Display.width() / 2, StickCP2.Display.height() / 2 - 10);
        StickCP2.Display.drawString("Press to calibrate", StickCP2.Display.width() / 2, StickCP2.Display.height() / 2 + 10);
        delay(1000); // Delay to prevent multiple presses
    }


//********************************************************* Get Pitch & Roll, User Calibration, & Current Time *********************************************************
    auto imu_update = StickCP2.Imu.update();
    if (imu_update) {
        auto data = StickCP2.Imu.getImuData();
        float pitch = atan2(data.accel.y, sqrt(data.accel.x * data.accel.x + data.accel.z * data.accel.z)) * 180 / PI;
        float roll = atan2(data.accel.x, sqrt(data.accel.y * data.accel.y + data.accel.z * data.accel.z)) * 180 / PI;

        // If in calibration mode, wait for button press to set reference
        if (isCalibrating) {
            if (digitalRead(CALIBRATION_BUTTON) == LOW) {
                calibrateDevice(pitch, roll); // Calibrate the device
            }
            return; // Skip further processing in calibration mode
        }
        unsigned long currentTime = millis();

        // Check if the device is level
        bool isCurrentlyLevel = ((abs(roll - referenceRoll) <= 5) && (abs(pitch - referencePitch) <= 5));


//********************************************************* Log Pitch, Roll, and Level to ThingSpeak (every 20 seconds) *********************************************************
      if (currentTime - lastUpdate > 20000) {
            int pitchStatus = ThingSpeak.setField(1, pitch);  // Field 1: Pitch
            int rollStatus = ThingSpeak.setField(2, roll);    // Field 2: Roll
            int levelStatus = ThingSpeak.setField(3, isCurrentlyLevel ? 1 : 0);  // Field 3: Level status (1 = level, 0 = not level)
            
            int responseCode = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

            if (responseCode == 200) {
                Serial.println("Channel update successful.");
            } else {
                Serial.println("Problem updating channel. HTTP error code " + String(responseCode));
            }

            lastUpdate = currentTime;
        }


//********************************************************* Check if Pitch & Roll = Level *********************************************************
        // Check if both pitch and roll are in the "level" range relative to the reference values
        if (((roll >= referenceRoll - 5 && roll <= referenceRoll + 5) || (roll <= -(referenceRoll + 5) && roll >= -(referenceRoll - 5))) ||
            ((pitch >= referencePitch - 5 && pitch <= referencePitch + 5) || (pitch <= -(referencePitch + 5) && pitch >= -(referencePitch - 5)))) {

            // Device is level
            if (!isLevel) {
                horizontalStartTime = currentTime;
                verticalStartTime = currentTime;
                isLevel = true;
                displayedLevelMessage = false;  // Reset level message flag
            }

            // Check if it has been level for 3 seconds
            if ((currentTime - horizontalStartTime >= 3000) && (currentTime - verticalStartTime >= 3000) && !displayedLevelMessage) {
                StickCP2.Display.fillScreen(BLACK);  // Clear only once when level
                StickCP2.Display.setTextColor(GREEN);
                StickCP2.Display.setFont(&fonts::FreeSansBold12pt7b);  // Larger font for "LEVEL"
                StickCP2.Display.drawString("LEVEL", StickCP2.Display.width() / 2, StickCP2.Display.height() / 2);

                digitalWrite(BUZZER_PIN, HIGH);
                delay(100);  // Beep duration
                digitalWrite(BUZZER_PIN, LOW);

                displayBatteryLevel();         // Display battery percentage without flashing
                displayedLevelMessage = true;  // Indicate "LEVEL" has been displayed
            }
        } else {
            // Device is not level, reset level flag
            isLevel = false;
            displayedLevelMessage = false;  // Allow "LEVEL" message to show again later

            // Draw the graphical level indicator
            drawLevelIndicator(pitch, roll);
        }

        delay(100);  
    }
}
