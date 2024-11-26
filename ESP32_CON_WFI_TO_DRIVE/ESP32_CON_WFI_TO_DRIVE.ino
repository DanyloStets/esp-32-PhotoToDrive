#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <EEPROM.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "MyBase64.h"
#include "esp_camera.h"

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#define FLASH_LED_PIN     4


#define EEPROM_SIZE 96
#define SSID_ADDR 0
#define PASS_ADDR 32


String ssid;
String password;

WebServer server(80);
WiFiClientSecure client;


String myDeploymentID = "AKfycbyQ8gO7z9Cxoc9Kzkp7__wDuAMFoxIDgRv68N7IDoFKnPM_S2RRDWm27wJgS_N0c1GWWg";
String myMainFolderName = "ESP32-CAM";


unsigned long previousMillis = 0;
const int Interval = 20000;

bool LED_Flash_ON = true;

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);

  EEPROM.begin(EEPROM_SIZE);

  pinMode(FLASH_LED_PIN, OUTPUT);


  ssid = readEEPROM(SSID_ADDR);
  password = readEEPROM(PASS_ADDR);

  if (ssid == "") {

    startAccessPoint();
  } else {
    connectToWiFi();
  }
  
  initCamera();
}

void loop() {
  server.handleClient();
  
  unsigned long currentMillis = millis();
  if (WiFi.status() == WL_CONNECTED && currentMillis - previousMillis >= Interval) {
    previousMillis = currentMillis;
    SendCapturedPhotos();
  }
}

void startAccessPoint() {
  WiFi.softAP("ESP_AP", "PASSWORD");
  

  server.on("/", []() {
    server.send(200, "text/html", "<form action='/save' method='POST'>SSID: <input type='text' name='ssid'><br>Password: <input type='password' name='password'><br><input type='submit' value='Save'></form>");
  });


  server.on("/save", []() {
    ssid = server.arg("ssid");
    password = server.arg("password");

    writeEEPROM(SSID_ADDR, ssid);
    writeEEPROM(PASS_ADDR, password);

    server.send(200, "text/html", "<h1>data saved ESP.</h1>");
    ESP.restart();
  });

  server.begin();
}

void connectToWiFi() {
 //!importland!!!
  WiFi.begin(ssid.c_str(), password.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Підключено до Wi-Fi!");
    clearEEPROM();
  } else {
    startAccessPoint();
  }
}

void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 8;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }
}


void writeEEPROM(int address, String data) {
  for (int i = 0; i < data.length(); i++) {
    EEPROM.write(address + i, data[i]);
  }
  EEPROM.write(address + data.length(), '\0');
  EEPROM.commit();
}


String readEEPROM(int address) {
  char data[32];
  int i = 0;
  while (i < 32) {
    data[i] = EEPROM.read(address + i);
    if (data[i] == '\0') break;
    i++;
  }
  data[i] = '\0';
  return String(data);
}
void clearEEPROM() {

  EEPROM.begin(EEPROM_SIZE);


  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  }


  EEPROM.commit();

  Serial.println("EEPROM cleared successfully!");
}
void SendCapturedPhotos() {
  const char* host = "script.google.com";
  Serial.println();
  Serial.println("-----------");
  Serial.println("Connect to " + String(host));
  
  client.setInsecure(); 


  digitalWrite(FLASH_LED_PIN, HIGH);
  delay(100);
  digitalWrite(FLASH_LED_PIN, LOW);
  delay(100);


  if (client.connect(host, 443)) {
    Serial.println("З'єднання успішне.");
    

    Serial.println("Захоплення фото...");
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Не вдалося захопити фото.");
      ESP.restart(); 
      return;
    }

    Serial.println("Фото захоплено успішно.");
    Serial.println("Відправка зображення на Google Drive...");
    Serial.println("Розмір зображення: " + String(fb->len) + " байт");


    String url = "/macros/s/" + myDeploymentID + "/exec?folder=" + myMainFolderName;
    client.println("POST " + url + " HTTP/1.1");
    client.println("Host: " + String(host));
    client.println("Transfer-Encoding: chunked");
    client.println();


    char *input = (char *)fb->buf;
    int fbLen = fb->len;
    int chunkSize = 3000; 
    char output[my_base64_enc_len(chunkSize) + 1];

    for (int i = 0; i < fbLen; i += chunkSize) {
      int l = my_base64_encode(output, input, min(fbLen - i, chunkSize));
      client.print(String(l, HEX));
      client.print("\r\n");
      client.print(output);
      client.print("\r\n");
      input += chunkSize;
    }
    client.print("0\r\n");
    client.print("\r\n");

    esp_camera_fb_return(fb); 

    Serial.println("Очікування відповіді від сервера...");
    while (client.connected() && !client.available()) delay(100);
    while (client.available()) {
      String line = client.readStringUntil('\n');
      Serial.println(line); 
    }
  } else {
    Serial.println("Не вдалося підключитися до " + String(host));
  }

  client.stop();
  Serial.println("-----------");
}