const int buttonPin = 4;
const int ledPin = 5;

bool ledState = false;

void setup() {
  Serial.begin(115200);

  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);

  digitalWrite(ledPin, LOW);

  Serial.println("Sistem Toggle LED Dimulai");
}

void loop() {
  int buttonState = digitalRead(buttonPin);

  if (buttonState == HIGH) {
    ledState = !ledState;

    digitalWrite(ledPin, ledState);

    if (ledState == true) {
      Serial.println("Tombol ditekan -> LED ON");
    } else {
      Serial.println("Tombol ditekan -> LED OFF");
    }

    delay(200);

    while (digitalRead(buttonPin) == HIGH) {
      delay(10);
    }
  }
}