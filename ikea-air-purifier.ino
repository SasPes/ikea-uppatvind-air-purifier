#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

const char* ssid = "";
const char* password = "";

ESP8266WebServer server(80);

// Pins
const int mosfetPin = D1; // 24V MOSFET
const int fanPin = D5;    // Fan PWM

// Fan control
volatile int fanFreq = 75; // Start at minimum
volatile bool fanState = false;
os_timer_t fanTimer;

// Frequency limits
const int minFreq = 75;   // Minimum 75 Hz
const int maxFreq = 300;  // Maximum 300 Hz;

// Map percentage (0-100%) to frequency
int speedToFreq(int speedPercent) {
  speedPercent = constrain(speedPercent, 0, 100);
  return map(speedPercent, 0, 100, minFreq, maxFreq);
}

// Toggle fan pin (ISR)
void ICACHE_RAM_ATTR toggleFan() {
  fanState = !fanState;
  digitalWrite(fanPin, fanState);
}

// Update timer interval based on fanFreq
void updateFanTimer() {
  if (fanFreq <= 0) {
    os_timer_disarm(&fanTimer);
    digitalWrite(fanPin, LOW);
    return;
  }

  uint32_t halfPeriodMs = 500 / fanFreq; // half period in ms
  os_timer_disarm(&fanTimer);
  os_timer_setfn(&fanTimer, (os_timer_func_t*)toggleFan, NULL);
  os_timer_arm(&fanTimer, halfPeriodMs, true);
}

// ======================
// Web Handlers
// ======================
void handleRoot() {
  int speedPercent = map(fanFreq, minFreq, maxFreq, 0, 100);
  String html = "<h1>ESP8266 Fan Control</h1>"
                "<h2>MOSFET</h2>"
                "<a href='/on'><button>ON</button></a> "
                "<a href='/off'><button>OFF</button></a><br><br>"
                "<h2>Fan Speed (%)</h2>"
                "<form action='/setFan' method='get'>"
                "Speed (0-100%): <input type='number' name='speed' min='0' max='100' value='" + String(speedPercent) + "'>"
                "<input type='submit' value='Set'>"
                "</form>"
                "<p>Current fan speed: " + String(speedPercent) + "% (Frequency: " + String(fanFreq) + " Hz)</p>";
  server.send(200, "text/html", html);
}

void handleOn() {
  digitalWrite(mosfetPin, HIGH);
  server.send(200, "text/plain", "MOSFET ON");
}

void handleOff() {
  digitalWrite(mosfetPin, LOW);
  server.send(200, "text/plain", "MOSFET OFF");
}

void handleSetFan() {
  if (server.hasArg("speed")) {
    int speedPercent = constrain(server.arg("speed").toInt(), 0, 100);
    fanFreq = speedToFreq(speedPercent);
    updateFanTimer();
    server.sendHeader("Location", "/");
    server.send(303);
    Serial.printf("Fan set to %d%% -> %d Hz\n", speedPercent, fanFreq);
    return;
  }
  server.send(400, "text/plain", "Missing speed parameter");
}

// ======================
// Setup
// ======================
void setup() {
  Serial.begin(115200);
  Serial.println("\nBooting...");

  // SAFEST MOSFET STARTUP — force pin low BEFORE setting as output
  digitalWrite(mosfetPin, LOW);  
  pinMode(mosfetPin, OUTPUT);

  // Fan pin also forced LOW first
  digitalWrite(fanPin, LOW);
  pinMode(fanPin, OUTPUT);

  // WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected! IP:");
  Serial.println(WiFi.localIP());

  // Web server
  server.on("/", handleRoot);
  server.on("/on", handleOn);
  server.on("/off", handleOff);
  server.on("/setFan", handleSetFan);
  server.begin();

  // Start fan timer
  os_timer_disarm(&fanTimer);
  updateFanTimer();
}

// ======================
// Loop
// ======================
void loop() {
  server.handleClient();
}