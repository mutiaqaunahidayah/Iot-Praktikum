#include <painlessMesh.h>
#include <ArduinoJson.h>

#define MESH_PREFIX "LabIoTMesh"
#define MESH_PASSWORD "iotmeshpassword"
#define MESH_PORT 7777

#define LED_PIN 12

Scheduler userScheduler;
painlessMesh mesh;

float suhuTerakhir = NAN;
int adcTerakhir = -1;

bool dataSuhuDiterima = false;
bool dataCahayaDiterima = false;
bool ledState = false;

void receivedCallback(uint32_t from, String &msg) {
  StaticJsonDocument<200> doc;

  DeserializationError error = deserializeJson(doc, msg);

  if (error) {
    Serial.println("[ERROR] Data JSON tidak valid");
    return;
  }

  const char* tipe = doc["tipe"] | "";

  if (strcmp(tipe, "suhu_node") == 0) {
    if (!doc["suhu"].is<float>() ||
        !doc["kelembapan"].is<float>()) {
      Serial.println("[ERROR] Data suhu tidak lengkap");
      return;
    }

    suhuTerakhir = doc["suhu"].as<float>();
    float kelembapan = doc["kelembapan"].as<float>();

    dataSuhuDiterima = true;

    Serial.println("================================");
    Serial.printf("Data suhu dari Node ID: %u\n", from);
    Serial.printf("Suhu       : %.2f C\n", suhuTerakhir);
    Serial.printf("Kelembapan : %.2f %%\n", kelembapan);
    Serial.println("================================");
  }
  else if (strcmp(tipe, "cahaya_node") == 0) {
    if (!doc["adc"].is<int>()) {
      Serial.println("[ERROR] Data ADC tidak valid");
      return;
    }

    adcTerakhir = doc["adc"].as<int>();
    dataCahayaDiterima = true;

    Serial.println("================================");
    Serial.printf("Data cahaya dari Node ID: %u\n", from);
    Serial.printf("Nilai ADC: %d\n", adcTerakhir);
    Serial.println("================================");
  }
  else {
    Serial.println("[INFO] Tipe telemetri tidak dikenal");
    return;
  }

  // RULE ENGINE:
  // LED menyala jika suhu > 31 C ATAU ADC < 300.
  // Data kedua sensor harus sudah diterima sebelum keputusan dibuat.

  if (dataSuhuDiterima && dataCahayaDiterima) {
    bool kondisiKritis =
      (suhuTerakhir > 31.0) ||
      (adcTerakhir < 300);

    if (kondisiKritis) {
      digitalWrite(LED_PIN, HIGH);
      ledState = true;
      Serial.println("[AKTUATOR] LED MENYALA");
    }
    else {
      digitalWrite(LED_PIN, LOW);
      ledState = false;
      Serial.println("[AKTUATOR] LED MATI");
    }

    Serial.printf(
      "[STATUS] Suhu: %.2f C | ADC: %d | LED: %s\n",
      suhuTerakhir,
      adcTerakhir,
      ledState ? "ON" : "OFF"
    );
  }
  else {
    Serial.println(
      "[INFO] Menunggu data dari kedua sensor..."
    );
  }
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf(
    "--> Koneksi Baru Terdeteksi! Node ID: %u\n",
    nodeId
  );
}

void changedConnectionCallback() {
  Serial.println(
    "--> Topologi rantai mesh telah diperbarui"
  );
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  mesh.setDebugMsgTypes(ERROR | STARTUP);

  mesh.init(
    MESH_PREFIX,
    MESH_PASSWORD,
    &userScheduler,
    MESH_PORT
  );

  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);

  // Node 3 tidak menjadwalkan taskSendMessage.
  // Node ini hanya menerima data dan mengendalikan LED.

  Serial.println("Node 3 Hub Kontrol LED aktif");
}

void loop() {
  mesh.update();
}