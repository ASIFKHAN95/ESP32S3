#include <WiFi.h>
#include <WebServer.h>

// Wi-Fi credentials
const char* ssid = "SSID";
const char* password = "Password";

// GPIO Pins
#define PIR_PIN       23   // Motion sensor
#define GREEN_LED     22    // Presence indicator
#define RED_LED       4    // No presence indicator
#define BUZZER_PIN    18   // Alarm

WebServer server(80);
bool presenceDetected = false;

String getHTML() {
  return "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
         "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
         "<title>Room Presence</title>"
         "<script>setTimeout(()=>location.reload(),2000);</script></head>"
         "<body style='font-family:Arial;text-align:center;padding-top:40px;'>"
         "<h2>Room Presence Detection</h2>"
         "<p><strong>Status:</strong> <span style='color:"
         + String(presenceDetected ? "green" : "red") + "'>"
         + String(presenceDetected ? "Occupied" : "Vacant") + "</span></p>"
         "</body></html>";
}

void handleRoot() {
  server.send(200, "text/html", getHTML());
}

void setup() {
  Serial.begin(115200);

  // Set pin modes
  pinMode(PIR_PIN, INPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Initial states
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);
  digitalWrite(BUZZER_PIN, LOW);

  // Wi-Fi Connection
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected.");
  Serial.print("ESP32 IP address: ");
  Serial.println(WiFi.localIP());

  // Start WebServer
  server.on("/", handleRoot);
  server.begin();
  Serial.println("Web server started.");
}

void loop() {
  // Read PIR sensor state
  presenceDetected = digitalRead(PIR_PIN);

  // Handle web server
  server.handleClient();

  // Update LEDs and buzzer
  if (presenceDetected) {
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER_PIN, HIGH);
    Serial.println("Motion Detected: OCCUPIED");
  } else {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println("No Motion: VACANT");
  }

  delay(500); // slight debounce / control rate of updates
}
