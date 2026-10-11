#include <DHT.h>

const byte ldrPin = 3;        
const byte dhtPin = 13;       
#define DHTTYPE DHT22         

const int relayPin = 12;      
const int ledPin = 5;         

DHT dht(dhtPin, DHTTYPE);

void setup() {  
  Serial.begin(115200);  
  dht.begin();   

  pinMode(relayPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  digitalWrite(relayPin, HIGH); 
  digitalWrite(ledPin, LOW);    

  Serial.println("Sistem Otomasi Smart Warehouse Dimulai...");  
}

void loop() {  
  delay(2000); 

  float temp = dht.readTemperature();   
  float hum = dht.readHumidity();     

  if (isnan(temp) || isnan(hum)) {  
    Serial.println("Peringatan: Gagal membaca data dari sensor DHT!");  
    return; 
  }

  int ldrValue = analogRead(ldrPin);   

  Serial.print("Suhu: ");  
  Serial.print(temp);  
  Serial.print(" C | Kelembapan: ");  
  Serial.print(hum);  
  Serial.print(" % | LDR ADC: ");  
  Serial.println(ldrValue);  

  if (temp > 34.0 || ldrValue < 300) {  
    digitalWrite(relayPin, LOW);   
    digitalWrite(ledPin, HIGH);    
    Serial.println("Peringatan: Aktuator Aktif! (Kondisi Ruangan Buruk)");  
  } 
  else {  
    digitalWrite(relayPin, HIGH);  
    digitalWrite(ledPin, LOW);     
    Serial.println("Kondisi Aman: Lingkungan Normal.");  
  }
  
  Serial.println("--------------------------------------------------");
}