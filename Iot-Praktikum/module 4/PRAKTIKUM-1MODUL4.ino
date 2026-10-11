
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>

// ---------------- KONFIGURASI WIFI ----------------
const char* ssid = "BRO'WN CAFE";
const char* password = "browncafe";

// ---------------- KONFIGURASI PIN ----------------
const byte dhtPin = 2;       // D4 (GPIO 2)
const byte buttonPin = 4;    // D2 (GPIO 4)
const byte ledPin = 12;      // D6 (GPIO 12), LED ON/OFF
const byte pwmLedPin = 5;    // D1 (GPIO 5), LED PWM

DHT dht(dhtPin, DHT11);

// ---------------- VARIABEL STATUS ----------------
bool ledState = false;
int pwmValue = 0;

String currentTemp = "--";
String currentHum = "--";

int buttonState = LOW;
int lastButtonState = LOW;

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

unsigned long lastTime = 0;
const unsigned long dhtInterval = 3000;

// ---------------- WEB SERVER ----------------
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ---------------- HTML & JAVASCRIPT ----------------
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Smart Lighting</title>

  <style>
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      margin: 20px;
    }

    .card {
      background: #f0f0f0;
      margin: 20px auto;
      padding: 20px;
      max-width: 400px;
      border-radius: 10px;
    }

    button {
      padding: 15px 30px;
      font-size: 20px;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      color: white;
    }

    .btn-on {
      background-color: #4CAF50;
    }

    .btn-off {
      background-color: #f44336;
    }

    input[type=range] {
      width: 100%;
      cursor: pointer;
    }

    .pwm-number {
      font-size: 22px;
      font-weight: bold;
    }
  </style>
</head>

<body>
  <h1>Smart Room</h1>

  <div class="card">
    <h2>Suhu: <span id="tempValue">--</span> Celcius</h2>
  </div>

  <div class="card">
    <h2>Kelembapan: <span id="humValue">--</span> %</h2>
  </div>

  <div class="card">
    <h2>LED ON/OFF: <span id="ledStatus">OFF</span></h2>

    <button id="toggleBtn"
            class="btn-off"
            onclick="toggleLed()">
      Turn ON
    </button>
  </div>

  <!-- TUGAS 4.7: SLIDER PWM -->
  <div class="card">
    <h2>Intensitas Cahaya LED</h2>

    <input
      type="range"
      min="0"
      max="1023"
      value="0"
      id="pwmSlider"
      oninput="sendPWM(this.value)"
    >

    <p>Nilai PWM:
      <span id="pwmValue" class="pwm-number">0</span>
    </p>

    <p>0 = Mati, 1023 = Terang Maksimal</p>
  </div>

  <script>
    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;

    window.addEventListener('load', onLoad);

    function onLoad() {
      initWebSocket();
    }

    function initWebSocket() {
      websocket = new WebSocket(gateway);

      websocket.onopen = onOpen;
      websocket.onclose = onClose;
      websocket.onmessage = onMessage;
    }

    function onOpen() {
      console.log("WebSocket terkoneksi");
    }

    function onClose() {
      console.log("WebSocket terputus");
      setTimeout(initWebSocket, 2000);
    }

    // Mengontrol LED ON/OFF D6
    function toggleLed() {
      if (websocket &&
          websocket.readyState === WebSocket.OPEN) {
        websocket.send("toggle");
      }
    }

    // TUGAS 4.7: Mengirim nilai PWM ke NodeMCU
    function sendPWM(value) {
      document.getElementById("pwmValue").textContent = value;

      if (websocket &&
          websocket.readyState === WebSocket.OPEN) {
        websocket.send("pwm," + value);
      }
    }

    // Menerima data dari NodeMCU
    function onMessage(event) {
      var dataObj = JSON.parse(event.data);

      if (dataObj.suhu !== undefined) {
        document.getElementById("tempValue").textContent =
          dataObj.suhu;
      }

      if (dataObj.hum !== undefined) {
        document.getElementById("humValue").textContent =
          dataObj.hum;
      }

      if (dataObj.led !== undefined) {
        var btn = document.getElementById("toggleBtn");
        var status = document.getElementById("ledStatus");

        if (dataObj.led == "1") {
          status.textContent = "ON";
          btn.textContent = "Turn OFF";
          btn.className = "btn-on";
        } else {
          status.textContent = "OFF";
          btn.textContent = "Turn ON";
          btn.className = "btn-off";
        }
      }

      // Menyesuaikan slider dengan nilai PWM dari server
      if (dataObj.pwm !== undefined) {
        document.getElementById("pwmSlider").value =
          dataObj.pwm;

        document.getElementById("pwmValue").textContent =
          dataObj.pwm;
      }
    }
  </script>
</body>
</html>
)rawliteral";

// ---------------- BACK-END & WEBSOCKET ----------------

// Mengirim status semua perangkat ke browser
void notifyClients() {
  String jsonString = "{\"led\":\"";
  jsonString += String(ledState ? 1 : 0);
  jsonString += "\",\"suhu\":\"";
  jsonString += currentTemp;
  jsonString += "\",\"hum\":\"";
  jsonString += currentHum;
  jsonString += "\",\"pwm\":";
  jsonString += String(pwmValue);
  jsonString += "}";

  ws.textAll(jsonString);
}

// Menangani pesan WebSocket
void handleWebSocketMessage(void *arg,
                            uint8_t *data,
                            size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;

  if (!(info->final &&
        info->index == 0 &&
        info->len == len &&
        info->opcode == WS_TEXT)) {
    return;
  }

  // Menyalin pesan ke buffer agar aman diproses
  char message[32];

  if (len == 0 || len >= sizeof(message)) {
    return;
  }

  memcpy(message, data, len);
  message[len] = '\0';

  // Perintah tombol ON/OFF
  if (strcmp(message, "toggle") == 0) {
    ledState = !ledState;
    notifyClients();
  }

  // TUGAS 4.7: Parsing perintah PWM
  else if (strncmp(message, "pwm,", 4) == 0) {
    char *endPtr;
    long value = strtol(message + 4, &endPtr, 10);

    // Pastikan angka valid dan berada dalam rentang 0-1023
    if (endPtr != message + 4 &&
        *endPtr == '\0' &&
        value >= 0 &&
        value <= 1023) {

      pwmValue = (int)value;

      // Mengatur kecerahan LED pada pin D1
      analogWrite(pwmLedPin, pwmValue);

      notifyClients();
    }
  }
}

// Event WebSocket
void onEvent(AsyncWebSocket *server,
             AsyncWebSocketClient *client,
             AwsEventType type,
             void *arg,
             uint8_t *data,
             size_t len) {

  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("Client #%u terhubung\n", client->id());
      notifyClients();
      break;

    case WS_EVT_DISCONNECT:
      Serial.printf("Client #%u terputus\n", client->id());
      break;

    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;

    default:
      break;
  }
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(pwmLedPin, OUTPUT);

  digitalWrite(ledPin, LOW);

  // Rentang PWM ESP8266: 0 sampai 1023
  analogWriteRange(1023);
  analogWrite(pwmLedPin, pwmValue);

  dht.begin();

  // Menghubungkan ke Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Menghubungkan ke Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Halaman web
  server.on("/", HTTP_GET,
    [](AsyncWebServerRequest *request) {
      request->send_P(200, "text/html", index_html);
    }
  );

  // WebSocket
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  server.begin();

  Serial.println("Web Server dan WebSocket aktif");
}

// ---------------- LOOP ----------------
void loop() {
  ws.cleanupClients();

  // LED ON/OFF pada D6
  digitalWrite(ledPin, ledState ? HIGH : LOW);

  // Membaca tombol fisik dengan debounce
  int reading = digitalRead(buttonPin);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;

      if (buttonState == HIGH) {
        ledState = !ledState;
        notifyClients();
      }
    }
  }

  lastButtonState = reading;

  // Membaca sensor DHT11 setiap 3 detik
  if (millis() - lastTime >= dhtInterval) {
    lastTime = millis();

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    bool dataUpdated = false;

    if (!isnan(t)) {
      currentTemp = String(t, 1);
      dataUpdated = true;
    } else {
      Serial.println("Gagal membaca suhu DHT11");
    }

    if (!isnan(h)) {
      currentHum = String(h, 1);
      dataUpdated = true;
    } else {
      Serial.println("Gagal membaca kelembapan DHT11");
    }

    if (dataUpdated) {
      Serial.print("Suhu: ");
      Serial.print(currentTemp);
      Serial.print(" C | Kelembapan: ");
      Serial.print(currentHum);
      Serial.println(" %");

      notifyClients();
    }
  }
}
