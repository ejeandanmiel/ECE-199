#include <WiFi.h>         // WiFi control for ESP32
#include <ThingsBoard.h>  // ThingsBoard SDK
#include <ArduinoJson.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

#define WIFI_AP_NAME "Baluyot Congressional 2.4"  // WiFi access point
#define WIFI_PASSWORD "Lot17Blk18735*"             // WiFi password
#define TOKEN "zT9RIAyoEsigUoP6IskC"
#define THINGSBOARD_SERVER "thingsboard.cloud"  // ThingsBoard server instance
#define SERIAL_DEBUG_BAUD 115200                // Baud rate for debug serial

// Function Prototypes
void InitWiFi();
void reconnect();
void readFile(fs::FS &fs, const char *path);
void writeFile(fs::FS &fs, const char *path, const char *message);
void appendFile(fs::FS &fs, const char *path, const char *message);
void deleteFile(fs::FS &fs, const char *path);
void updateSerial();
void storeMessage1();
void storeMessage2();
void storeMessage3();
void unpackMessage();
void sendMessageS1();
void sendMessageS2();
void sendMessageS3();
void parseDate();
void saveDateS1();
void saveDateS2();
void saveDateS3();


// Initialization of the device
ThingsBoardSized<JSON_OBJECT_SIZE(2)> thingsboard;
WiFiClient espClient;                     // Initialize ThingsBoard client
ThingsBoardSized<32> tb(espClient, 256);  // Initialize ThingsBoard instance

// Initialize global variables
int status = WL_IDLE_STATUS;  // Wifi radio status
bool subscribed;
uint8_t cardType;
uint64_t cardSize;
String message, date_and_time, sensor_reading;
String passwordS1 = "201900044", passwordS2 = "201900045", passwordS3 = "201900046", passwordSMS;
int sensor_reading_int[12];
int s1_day = 0, s1_month = 0, s1_year = 0, s1_hour = 0, s1_minute = 0;
int s2_day = 0, s2_month = 0, s2_year = 0, s2_hour = 0, s2_minute = 0;
int s3_day = 0, s3_month = 0, s3_year = 0, s3_hour = 0, s3_minute = 0;
int day_new = 0, month_new = 0, year_new = 0, hour_new, minute_new = 0;


void setup() {
  Serial.begin(SERIAL_DEBUG_BAUD);
  Serial2.begin(SERIAL_DEBUG_BAUD);
  WiFi.begin(WIFI_AP_NAME, WIFI_PASSWORD);
  InitWiFi();
  tb.setBufferSize(256);
  delay(3000);


  // Check SD card
  if (!SD.begin(5)) {
    Serial.println("Card Mount Failed");
    return;
  }

  cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");  // Check microSD card type
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  cardSize = SD.cardSize() / (1024 * 1024);  // Check microSD card size
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  // Delete existing sensor data text files before creating a new one
  //deleteFile(SD, "/sensor1_data.txt");
  //deleteFile(SD, "/sensor2_data.txt");
  //deleteFile(SD, "/sensor3_data.txt");

  // Check if sensor1_data.txt exists, if not create sensor1_data.txt
  File file = SD.open("/sensor1_data.txt");
  if (!file) {
    Serial.println("File doesn't exist");
    Serial.println("Creating file...");
    writeFile(SD, "/sensor1_data.txt", "date&time, 415 nm, 445 nm, 480 nm, 515 nm, Clear 1 , NIR 1, 555 nm, 590 nm, 630 nm, 680 nm, Clear 2, NIR 2 \r\n");
  } else {
    Serial.println("File already exists");
  }
  file.close();

  // Check if sensor2_data.txt exists, if not create sensor2_data.txt
  file = SD.open("/sensor2_data.txt");
  if (!file) {
    Serial.println("File doesn't exist");
    Serial.println("Creating file...");
    writeFile(SD, "/sensor2_data.txt", "date&time, 415 nm, 445 nm, 480 nm, 515 nm, Clear 1 , NIR 1, 555 nm, 590 nm, 630 nm, 680 nm, Clear 2, NIR 2 \r\n");
  } else {
    Serial.println("File already exists");
  }
  file.close();

  // Check if sensor3_data.txt exists, if not create sensor3_data.txt
  file = SD.open("/sensor3_data.txt");
  if (!file) {
    Serial.println("File doesn't exist");
    Serial.println("Creating file...");
    writeFile(SD, "/sensor3_data.txt", "date&time, 415 nm, 445 nm, 480 nm, 515 nm, Clear 1 , NIR 1, 555 nm, 590 nm, 630 nm, 680 nm, Clear 2, NIR 2 \r\n");
  } else {
    Serial.println("File already exists");
  }
  file.close();


  Serial2.println("AT");  // Once the handshake test is successful, it will back to OK
  updateSerial();
  Serial2.println("AT+CMGF=1");  // Configuring TEXT mode
  updateSerial();
  Serial2.println("AT+CNMI=1,2,0,0,0");  // Decides how newly arrived SMS messages should be handled
  updateSerial();
}

void loop() {
  // Check if there is received message
  // Reconnect to WiFi, if needed
  if (WiFi.status() != WL_CONNECTED) {
    reconnect();
    return;
  }

  while (Serial.available()) {
    Serial2.write(Serial.read());
  }
  while (Serial2.available()) {

    String sms = Serial2.readString();

    int index_header = sms.lastIndexOf("\n\n\r\n");
    message = sms.substring(51, index_header);  // SMS header contains of 51 characters

    Serial.println();
    Serial.print(message);

    if (passwordS1 == message.substring(0, 9) || passwordS2 == message.substring(0, 9) || passwordS3 == message.substring(0, 9)) {
      int index_message = message.indexOf("\n");
      passwordSMS = message.substring(0, index_message);
      message = message.substring(index_message + 1);

      index_message = message.indexOf("\n");
      date_and_time = message.substring(0, index_message);
      sensor_reading = message.substring(index_message + 1);
      parseDate();

      // Reconnect to ThingsBoard
      tb.disconnect();

      if (!tb.connected()) {
        subscribed = false;

        // Connect to the ThingsBoard
        Serial.print("Connecting to: ");
        Serial.print(THINGSBOARD_SERVER);
        Serial.print(" with token ");
        Serial.println(TOKEN);
        if (!tb.connect(THINGSBOARD_SERVER, TOKEN)) {
          Serial.println("Failed to connect");
          return;
        } else {
          Serial.println("Connected.\n");
        }
      }

      if (passwordS1 == passwordSMS) {
        storeMessage1();
        if (year_new > s1_year) {
          saveDateS1();
          unpackMessage();
          sendMessageS1();
        } else if (month_new > s1_month) {
          saveDateS1();
          unpackMessage();
          sendMessageS1();
        } else if (day_new > s1_day) {
          saveDateS1();
          unpackMessage();
          sendMessageS1();
        } else if (hour_new > s1_hour) {
          saveDateS1();
          unpackMessage();
          sendMessageS1();
        } else if (minute_new > s1_minute) {
          saveDateS1();
          unpackMessage();
          sendMessageS1();
        } else if (year_new == s1_year && month_new == s1_month && day_new == s1_day && hour_new == s1_hour && minute_new == s1_minute) {
          unpackMessage();
          sendMessageS1();
        } else {
          Serial.println("Invalid.");
        }
      }
      if (passwordS2 == passwordSMS) {
        storeMessage2();
        if (year_new > s2_year) {
          saveDateS2();
          unpackMessage();
          sendMessageS2();
        } else if (month_new > s2_month) {
          saveDateS2();
          unpackMessage();
          sendMessageS2();
        } else if (day_new > s2_day) {
          saveDateS2();
          unpackMessage();
          sendMessageS2();
        } else if (hour_new > s2_hour) {
          saveDateS2();
          unpackMessage();
          sendMessageS2();
        } else if (minute_new > s2_minute) {
          saveDateS2();
          unpackMessage();
          sendMessageS2();
        } else if (year_new == s2_year && month_new == s2_month && day_new == s2_day && hour_new == s2_hour && minute_new == s2_minute) {
          unpackMessage();
          sendMessageS2();
        } else {
          return;
        }
      }
      if (passwordS3 == passwordSMS) {
        storeMessage3();
        if (year_new > s3_year) {
          saveDateS3();
          unpackMessage();
          sendMessageS3();
        } else if (month_new > s3_month) {
          saveDateS3();
          unpackMessage();
          sendMessageS3();
        } else if (day_new > s3_day) {
          saveDateS3();
          unpackMessage();
          sendMessageS3();
        } else if (hour_new > s3_hour) {
          saveDateS3();
          unpackMessage();
          sendMessageS3();
        } else if (minute_new > s3_minute) {
          saveDateS3();
          unpackMessage();
          sendMessageS3();
        } else if (year_new == s3_year && month_new == s3_month && day_new == s3_day && hour_new == s3_hour && minute_new == s3_minute) {
          unpackMessage();
          sendMessageS3();
        } else {
          return;
        }
      }
    } else {
      return;
    }
  }
}


// Function Definitions
void InitWiFi() {
  Serial.println("Connecting to AP ...");
  // attempt to connect to WiFi network

  WiFi.begin(WIFI_AP_NAME, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to AP");
}
void reconnect() {
  // Loop until we're reconnected
  status = WiFi.status();
  if (status != WL_CONNECTED) {
    WiFi.begin(WIFI_AP_NAME, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("Connected to AP");
  }
}
void readFile(fs::FS &fs, const char *path) {
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
}
void writeFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}
void appendFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}
void deleteFile(fs::FS &fs, const char *path) {
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path)) {
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}
void updateSerial() {
  delay(500);
  while (Serial.available()) {
    Serial2.write(Serial.read());  //Forward what Serial received to Software Serial Port
  }
  while (Serial2.available()) {
    Serial.write(Serial2.read());  //Forward what Software Serial received to Serial Port
  }
}
void storeMessage1() {
  String log_entry;

  Serial.println("Saving data...");
  Serial.print("Log entry: " + date_and_time + "," + sensor_reading);

  log_entry = date_and_time + "," + sensor_reading + "\n";
  appendFile(SD, "/sensor1_data.txt", log_entry.c_str());  // Append the data to file
}
void storeMessage2() {
  String log_entry;

  Serial.println("Saving data...");
  Serial.print("Log entry: " + date_and_time + "," + sensor_reading);

  log_entry = date_and_time + "," + sensor_reading + "\n";
  appendFile(SD, "/sensor2_data.txt", log_entry.c_str());  // Append the data to file
}
void storeMessage3() {
  String log_entry;

  Serial.println("Saving data...");
  Serial.print("Log entry: " + date_and_time + "," + sensor_reading);

  log_entry = date_and_time + "," + sensor_reading + "\n";
  appendFile(SD, "/sensor3_data.txt", log_entry.c_str());  // Append the data to file
}
void unpackMessage() {
  String sr_str = sensor_reading;  // sr_str = sensor reading buffer
  String temp_sr_str;

  for (int i = 0; i <= 11; i++) {
    int index_message = sr_str.indexOf(",");
    temp_sr_str = sr_str.substring(0, index_message);
    sensor_reading_int[i] = temp_sr_str.toInt();
    // add print here for checking
    sr_str = sr_str.substring(index_message + 1);
  }
}
void sendMessageS1() {
  const int data_items = 12;
  Telemetry data[data_items] = {
    { "S1 415 nm", sensor_reading_int[0] },
    { "S1 445 nm", sensor_reading_int[1] },
    { "S1 480 nm", sensor_reading_int[2] },
    { "S1 515 nm", sensor_reading_int[3] },
    { "S1 Clear1", sensor_reading_int[4] },
    { "S1 NIR1", sensor_reading_int[5] },
    { "S1 555 nm", sensor_reading_int[6] },
    { "S1 590 nm", sensor_reading_int[7] },
    { "S1 630 nm", sensor_reading_int[8] },
    { "S1 680 nm", sensor_reading_int[9] },
    { "S1 Clear2", sensor_reading_int[10] },
    { "S1 NIR2", sensor_reading_int[11] }
  };

  tb.sendTelemetry(data, data_items);
  tb.loop();
}
void sendMessageS2() {
  const int data_items = 12;
  Telemetry data[data_items] = {
    { "S2 415 nm", sensor_reading_int[0] },
    { "S2 445 nm", sensor_reading_int[1] },
    { "S2 480 nm", sensor_reading_int[2] },
    { "S2 515 nm", sensor_reading_int[3] },
    { "S2 Clear1", sensor_reading_int[4] },
    { "S2 NIR1", sensor_reading_int[5] },
    { "S2 555 nm", sensor_reading_int[6] },
    { "S2 590 nm", sensor_reading_int[7] },
    { "S2 630 nm", sensor_reading_int[8] },
    { "S2 680 nm", sensor_reading_int[9] },
    { "S2 Clear2", sensor_reading_int[10] },
    { "S2 NIR2", sensor_reading_int[11] }
  };

  tb.sendTelemetry(data, data_items);
  tb.loop();
}
void sendMessageS3() {
  const int data_items = 12;
  Telemetry data[data_items] = {
    { "S3 415 nm", sensor_reading_int[0] },
    { "S3 445 nm", sensor_reading_int[1] },
    { "S3 480 nm", sensor_reading_int[2] },
    { "S3 515 nm", sensor_reading_int[3] },
    { "S3 Clear1", sensor_reading_int[4] },
    { "S3 NIR1", sensor_reading_int[5] },
    { "S3 555 nm", sensor_reading_int[6] },
    { "S3 590 nm", sensor_reading_int[7] },
    { "S3 630 nm", sensor_reading_int[8] },
    { "S3 680 nm", sensor_reading_int[9] },
    { "S3 Clear2", sensor_reading_int[10] },
    { "S3 NIR2", sensor_reading_int[11] }
  };

  tb.sendTelemetry(data, data_items);
  tb.loop();
}
void parseDate() {
  String dtb = date_and_time;  // dtb = date and time buffer
  String temp_str;

  int i = dtb.indexOf("/");
  temp_str = dtb.substring(0, i);
  day_new = temp_str.toInt();
  dtb = dtb.substring(i + 1);

  i = dtb.indexOf("/");
  temp_str = dtb.substring(0, i);
  month_new = temp_str.toInt();
  dtb = dtb.substring(i + 1);

  i = dtb.indexOf(" ");
  temp_str = dtb.substring(0, i);
  year_new = temp_str.toInt();
  dtb = dtb.substring(i + 1);

  i = dtb.indexOf(":");
  temp_str = dtb.substring(0, i);
  hour_new = temp_str.toInt();

  temp_str = dtb.substring(i + 1);
  minute_new = temp_str.toInt();
}
void saveDateS1() {
  s1_year = year_new;
  s1_month = month_new;
  s1_day = day_new;
  s1_hour = hour_new;
  s1_minute = minute_new;
}
void saveDateS2() {
  s2_year = year_new;
  s2_month = month_new;
  s2_day = day_new;
  s2_hour = hour_new;
  s2_minute = minute_new;
}
void saveDateS3() {
  s3_year = year_new;
  s3_month = month_new;
  s3_day = day_new;
  s3_hour = hour_new;
  s3_minute = minute_new;
}
