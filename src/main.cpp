/*
M5Dial MQTT Fan speed controller with inside and outside temperature display via MQTT topics

Bjoern Heller <tec att sixtopia.net>
*/

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "M5Dial.h"

// WiFi credentials
const char* ssid = "";
const char* password = "";
const IPAddress dns(8, 8, 8, 8); //DNS server

// MQTT Broker details
const char* mqtt_server = "";
const int mqtt_port = 1883;     // Default MQTT port
const char* mqtt_user = ""; // If applicable
const char* mqtt_password = "";     // If applicable
const char* mqtt_topic1 = "/T9602-1/temp";          // First topic
const char* mqtt_topic2 = "/T9602/temp";            // Second topic

// Messages storage
String message1 = "Waiting...";
String message2 = "Waiting...";

WiFiClient espClient;
PubSubClient client(espClient);
String message = "";

unsigned long lastUpdateTime = 0; // Stores the last update time
const long updateInterval = 1000; // Update interval in milliseconds (1000ms = 1 second)

int currentFillHeight = 0; // Add currentFillHeight variable

void fillScreenBasedOnPercentage(int percentage);
long oldPosition = -999;
bool isLocalUpdate = false; // Global flag to distinguish between local and remote updates


void setupWiFi() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    M5Dial.Display.clear();
    M5Dial.Display.setFont(&fonts::AsciiFont8x16);
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.drawString("Connecting to:", M5Dial.Display.width() / 2, M5Dial.Display.height() / 2 - 20);
    M5Dial.Display.drawString(String(ssid), M5Dial.Display.width() / 2, M5Dial.Display.height() / 2 - 10);
    delay(1000);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, dns);
    Serial.println("");
    Serial.println("WiFi connected");
    M5Dial.Display.clear();
    M5Dial.Display.drawString("WiFi connected!", M5Dial.Display.width() / 2, M5Dial.Display.height() / 2 - 20);
}

void reconnect() {
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        M5Dial.Display.clear();
        M5Dial.Display.setFont(&fonts::AsciiFont8x16);
        M5Dial.Display.setTextSize(1);
        M5Dial.Display.drawString("Connecting MQTT...", M5Dial.Display.width() / 2, M5Dial.Display.height() / 2 - 20);
        if (client.connect("M5DialClient", mqtt_user, mqtt_password)) {
            Serial.println("connected");
            client.subscribe("fan01");
            client.subscribe(mqtt_topic1);
            client.subscribe(mqtt_topic2);
            M5Dial.Display.clear();
            M5Dial.Display.setFont(&fonts::Orbitron_Light_24);
            M5Dial.Display.setTextColor(GREEN);
            M5Dial.Display.setTextSize(1);
            M5Dial.Display.drawString("Connected!", M5Dial.Display.width() / 2, M5Dial.Display.height() / 2 - 20);
            delay(500);
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            M5Dial.Display.clear();
            M5Dial.Display.setFont(&fonts::Orbitron_Light_24);
            M5Dial.Display.setTextColor(GREEN);
            M5Dial.Display.setTextSize(1);
            M5Dial.Display.drawString("reconnecting...", M5Dial.Display.width() / 2, M5Dial.Display.height() / 2 - 20);
            delay(5000);
        }
    }
}

uint32_t calculateColor(int percentage) {
    // Starting with full blue at 0% and transitioning to full red at 100%
    // Blue decreases from 255 to 0 as percentage increases from 0 to 100
    // Red increases from 0 to 255 as percentage increases from 0 to 100

    int blue = map(percentage, 0, 100, 0, 255); // Linearly increase red as percentage increases
    int green = map(percentage, 0, 100, 255, 0); // Linearly decrease blue as percentage increases
    int red = 0; // Green remains off

    // Construct the color in RGB565 format and return it
    // This format combines the red, green, and blue components into a single 16-bit number
    // with 5 bits for red, 6 bits for green, and 5 bits for blue.
    return M5Dial.Display.color565(red, green, blue);
}




void displayMessage(int percentage) {
    // Calculate dynamic colors for the background based on the currentFillHeight
    uint32_t backgroundColor = calculateColor(percentage);

    int textHeight = 16; // Height of the text font
    int padding = 4; // Padding around text
    int insideTempPosY = M5Dial.Display.height() / 2 - 80;
    int outsideTempPosY = M5Dial.Display.height() / 2 - 60;
    
    // Calculate the height to fill based on the percentage
    int height = M5Dial.Display.height();
    int fillHeight = (height * percentage) / 100; // This is the dynamic height of the blue background

    // Determine if the background color should change based on position
    uint16_t backgroundColorOutside = (M5Dial.Display.height() - currentFillHeight <= outsideTempPosY) ? backgroundColor : BLACK;
    uint16_t backgroundColorInside = (M5Dial.Display.height() - currentFillHeight <= insideTempPosY) ? backgroundColor : BLACK;

    // Update only the areas behind the text
    M5Dial.Display.fillRect(0, outsideTempPosY - padding, M5Dial.Display.width(), textHeight + (2 * padding), backgroundColorOutside);
    M5Dial.Display.fillRect(0, insideTempPosY - padding, M5Dial.Display.width(), textHeight + (2 * padding), backgroundColorInside);

    M5Dial.Display.setTextColor(WHITE);
    M5Dial.Display.setFont(&fonts::AsciiFont8x16);
    M5Dial.Display.setTextSize(1);

    M5Dial.Display.drawString("Inside Temp:" + message1 + (char)223 + " C", M5Dial.Display.width() / 2, outsideTempPosY);
    M5Dial.Display.drawString("Outside Temp:" + message2 + (char)223 + " C", M5Dial.Display.width() / 2, insideTempPosY);
}

void callback(char* topic, byte* payload, unsigned int length) {
    char msg[length + 1];
    for (unsigned int i = 0; i < length; i++) {
        msg[i] = (char)payload[i];
    }
    msg[length] = '\0'; // Null-terminate the message string
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    Serial.println(msg);

    // Process the fan speed topic messages
    if (strcmp(topic, "fan01") == 0) {
        long newFanSpeed = atol(msg);       // Parse the new fan speed value from the message
        if (!isLocalUpdate) {               // Only apply remote updates if the last change wasn't local
            oldPosition = newFanSpeed;
            fillScreenBasedOnPercentage(newFanSpeed);       // Reflect the remote change
            M5Dial.Encoder.write(newFanSpeed);              // Synchronize the encoder with the new fan speed
            Serial.print("Fan speed updated to: ");
            Serial.println(newFanSpeed);
        } else {
            // If it was a local update, simply reset the flag to allow future MQTT updates
            isLocalUpdate = false;
        }
    } else if (strcmp(topic, mqtt_topic1) == 0) {
        message1 = String(msg); // Update the first message
    } else if (strcmp(topic, mqtt_topic2) == 0) {
        message2 = String(msg); // Update the second message
    }
}

void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    M5Dial.begin(cfg, true, true);
    M5Dial.Display.setTextColor(GREEN);
    M5Dial.Display.setTextDatum(middle_centre);
    M5Dial.Display.setFont(&fonts::Orbitron_Light_32);
    M5Dial.Display.setTextSize(2);
    M5Dial.Encoder.readAndReset();
    M5Dial.Encoder.write(10);
    setupWiFi();
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
    Serial.println("Setup complete");
}

void fillScreenBasedOnPercentage(int percentage) {
    // Clear the display
    M5Dial.Display.clear();

    // Dimensions for the display
    int width = M5Dial.Display.width();
    int height = M5Dial.Display.height();

    // Calculate the color based on the fan speed
    uint32_t fillColor = calculateColor(percentage);

    // Calculate the height to fill based on the percentage
    int fillHeight = (height * percentage) / 100;

    // Fill the bottom part of the screen based on the percentage value
    M5Dial.Display.fillRect(0, height - fillHeight, width, fillHeight, BLUE); // Using blue as the fill color

    // Draw the percentage text
    M5Dial.Display.setTextDatum(middle_centre);
    M5Dial.Display.setTextColor(GREEN);
    M5Dial.Display.setFont(&fonts::Orbitron_Light_32);
    M5Dial.Display.setTextSize(2);
    M5Dial.Display.drawString(String(percentage) + "%", M5Dial.Display.width() / 2, M5Dial.Display.height() / 2);
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    unsigned long currentMillis = millis();

    M5Dial.Display.setTextDatum(top_center);
    M5Dial.Display.setTextColor(WHITE);
    M5Dial.Display.setFont(&fonts::Orbitron_Light_32);
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.drawString("FAN01", M5Dial.Display.width() / 2, M5Dial.Display.height() / 2 + 40);
    M5Dial.Display.setTextColor(GREEN);
    M5Dial.Display.setFont(&fonts::Orbitron_Light_24);
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.drawString("Speed", M5Dial.Display.width() / 2, M5Dial.Display.height() / 2 + 80);
    M5Dial.update();
    long newPosition = M5Dial.Encoder.read();
    newPosition = constrain(newPosition, 0, 100);

    if (newPosition != oldPosition) {
        oldPosition = newPosition;
        isLocalUpdate = true;       // Indicate this is a local update
        M5Dial.Speaker.tone(8000, 20);
        fillScreenBasedOnPercentage(newPosition);    // Update the screen fill based on the new position

        // Convert newPosition to String and publish to MQTT topic fan01
        char msg[50];
        snprintf(msg, 50, "%ld", newPosition);
        client.publish("fan01", msg);

        Serial.print("Published to fan01: ");
        Serial.println(msg);
    }
        if (currentMillis - lastUpdateTime >= updateInterval) {
        lastUpdateTime = currentMillis; // Save the last update time

        // Update the Temp topic display
        displayMessage(newPosition); // Update display with new temperature readings
    }

    if (M5Dial.BtnA.wasPressed()) {
        M5Dial.Encoder.readAndReset();
        M5Dial.Encoder.write(10);
        oldPosition = -999;
        
    }

    if (M5Dial.BtnA.pressedFor(2000)) {
        M5Dial.Display.clear();
        M5Dial.Display.drawString("power off!", M5Dial.Display.width() / 2, M5Dial.Display.height() / 2 - 20);
        delay(1000);
        M5Dial.Power.powerOff(); //shutdown
        oldPosition = -999; // Ensure everything updates correctly*/
    }
}