/*
 * Description: A test code for lorawan network transmission
 * By: Binay Kumar Sah
 * Uses: 
 * Arduino Mega 2560
 * HC-SR04 + LoRa-E5 AT firmware
 *
 * HC-SR04:
 *   TRIG -> Mega pin 6
 *   ECHO -> Mega pin 7
 *
 * LoRa-E5:
 *   Mega TX1 pin 18 -> LoRa-E5 RX
 *   Mega RX1 pin 19 <- LoRa-E5 TX
 *   Mega GND       -> LoRa-E5 GND
 *   
 *   Disclaimer: 
 *  -  While using Arduino UNO, it is recommended to disconnect the Arduino <-> LoRa-E5,
 *     as it only have one set of TX-RX pins
 *  - Other board rather than Arduino Mega, may face problem with serial monitor 
 */

const uint8_t TRIG_PIN = 6;
const uint8_t ECHO_PIN = 7;

const unsigned long SEND_INTERVAL = 30000UL; // time interval to send data , set at 30sec

const unsigned long JOIN_TIMEOUT = 120000UL;  // Static time out to join the network,

unsigned long lastSend = 0;
bool joined = false;    // used for handshake status

void setup() {
  Serial.begin(115200);   //baud rate for arduino mega
  Serial1.begin(9600);

  delay(2000);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  Serial.println(F("\n=== Ultrasonic + LoRa-E5 ==="));

  // Discard old bytes from the UART buffer.
  clearLoRaInput();

  sendCommandAndWait("AT", 3000);
  sendCommandAndWait("AT+MODE=LWOTAA", 3000);
  sendCommandAndWait("AT+DR=EU868", 3000);

  // Optional: use this only if network requires channels 0-2.
  sendCommandAndWait("AT+CH=NUM,0-2", 3000);

  // Do not transmit until this function confirms success.
  // Validate double side network established 
  joined = joinLoRaWAN();

  if (joined) {
    Serial.println(F("LoRaWAN is ready."));
    lastSend = millis() - SEND_INTERVAL;  // Send first reading immediately.
  } else {
    Serial.println(F("LoRaWAN join failed or timed out."));
  }
}

void loop() {
  // Check the network has been established first
  if (!joined) {
    Serial.println(F("Retrying LoRaWAN join in 10 seconds..."));
    delay(10000);
    joined = joinLoRaWAN();
    return;
  }

  float distance = readUltrasonic();

  if (distance <= 0.0 || distance >= 400.0) {
    Serial.println(F("Ultrasonic read error or out of range"));
    delay(2000);
    return;
  }

//  Serial.print(F("Distance: "));
//  Serial.print(distance, 1);
//  Serial.println(F(" cm"));

  if (millis() - lastSend >= SEND_INTERVAL) {
    lastSend = millis();
      Serial.print(F("Distance: "));
      Serial.print(distance, 1);
      Serial.println(F(" cm"));
    if (!sendDistanceViaLoRa(distance)) {
      Serial.println(F("Uplink failed."));
    }
  }

  delay(2000);
}

// Establish the network connection
// Return bool, true for connection establish and false for fail status 
bool joinLoRaWAN() {
  Serial.println(F("Starting LoRaWAN join..."));

  clearLoRaInput();

  Serial1.print(F("AT+JOIN\r\n"));

  unsigned long start = millis();
  String line;

  while (millis() - start < JOIN_TIMEOUT) {
    if (Serial1.available()) {
      line = Serial1.readStringUntil('\n');
      line.trim();

      if (line.length() == 0) {
        continue;
      }

      Serial.print(F("RX: "));
      Serial.println(line);

      if (line.indexOf("+JOIN: Network joined") >= 0) {
        // Continue reading briefly so NetID and Done are also displayed.
        unsigned long finishWait = millis() + 2000UL;

        while (millis() < finishWait) {
          if (Serial1.available()) {
            String extra = Serial1.readStringUntil('\n');
            extra.trim();

            if (extra.length() > 0) {
              Serial.print(F("RX: "));
              Serial.println(extra);
            }
          }
        }

        Serial.println(F("Join successful."));
        return true;
      }

      if (line.indexOf("+JOIN: Join failed") >= 0 ||
          line.indexOf("+JOIN: Failed") >= 0 ||
          line.indexOf("ERROR") >= 0) {
        Serial.println(F("Join failed."));
        return false;
      }
    }
  }

  Serial.println(F("Join timeout."));
  return false;
}

// raw sensor data send
// Argument : data from sensor 
// Return : Bool -> true for successful transmission 

bool sendDistanceViaLoRa(float distance) {
  uint16_t distance10 = (uint16_t)(distance * 10.0f + 0.5f);

  char hexPayload[5];
  snprintf(
    hexPayload,
    sizeof(hexPayload),
    "%02X%02X",
    (distance10 >> 8) & 0xFF,
    distance10 & 0xFF
  );

  Serial.print(F("Sending payload: "));
  Serial.println(hexPayload);

  clearLoRaInput();

  Serial1.print(F("AT+MSGHEX=\""));
  Serial1.print(hexPayload);
  Serial1.print(F("\"\r\n"));

  unsigned long start = millis();

  while (millis() - start < 15000UL) {
    if (Serial1.available()) {
      String line = Serial1.readStringUntil('\n');
      line.trim();

      if (line.length() == 0) {
        continue;
      }

      Serial.print(F("RX: "));
      Serial.println(line);

      if (line.indexOf("+MSGHEX: Done") >= 0) {
        return true;
      }

      if (line.indexOf("Please join network first") >= 0 ||
          line.indexOf("ERROR") >= 0 ||
          line.indexOf("Failed") >= 0) {
        return false;
      }
    }
  }

  Serial.println(F("Uplink response timeout."));
  return false;
}

// Function to send AT command
// Argument : Charcter AT command and timeout for wait
// Return: Bool -> true for successfull command execution 
bool sendCommandAndWait(const char* command, unsigned long timeout) {
  Serial.print(F("TX: "));
  Serial.println(command);

  clearLoRaInput();

  Serial1.print(command);
  Serial1.print(F("\r\n"));

  unsigned long start = millis();

  while (millis() - start < timeout) {
    if (Serial1.available()) {
      String line = Serial1.readStringUntil('\n');
      line.trim();

      if (line.length() > 0) {
        Serial.print(F("RX: "));
        Serial.println(line);

        if (line.indexOf("ERROR") >= 0) {
          return false;
        }
      }
    }
  }

  return true;
}

// To clear the stored buffer 
void clearLoRaInput() {
  while (Serial1.available()) {
    Serial1.read();
  }
}


// raw data read from ultransonic sensor
float readUltrasonic() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(3);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000UL);

  if (duration == 0) {
    return -1.0f;
  }

  return (duration * 0.0343f) / 2.0f;
}
