#include <Arduino.h>
#include <WiFi.h> 
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define ssid "Hieu_TkZ"
#define password "123123123"

const char *mqtt_server = "broker.emqx.io";
#define mqtt_port 1883
#define mqtt_username "hieutk1302"
#define mqtt_password "hieutk1302"

WiFiClient espClient; 
PubSubClient client(espClient);

bool isLoginProcessed = false; 

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


#define DC 16        // động cơ
#define BUTTON_DC 4  // nút động cơ
#define AC 12        // đèn
#define BUTTON_AC 13 // nút đèn

#define Light 14 // cảm biến ánh sáng
#define Flame 26 // cảm biến lửa

#define cambienxe1 32 // Chân cảm biến xe 1
#define cambienxe2 33 // Chân cảm biến xe 2
#define cambienxe3 25 // Chân cảm biến xe 3

int xe1 = 0;
int xe2 = 0;
int xe3 = 0;

bool ACState = false;          // Trạng thái đèn/motor
bool lastButtonACState = HIGH; // Trạng thái nút trước đó
bool overrideACSensor = false; // Biến để xác định có bỏ qua cảm biến hay không
int lastLightState = HIGH;     // Lưu trạng thái cảm biến trước đó

bool DCState = false;          // Trạng thái đèn/motor
bool lastButtonDCState = HIGH; // Trạng thái nút trước đó
bool overrideDCSensor = false; // Biến để xác định có bỏ qua cảm biến hay không
int lastFlameState = HIGH;     // Lưu trạng thái cảm biến trước đó

void connectWiFi();
void reconnect();
void callback(char *topic, byte *payload, unsigned int length);
void oled();
void parking();
void DongcoAC();
void DongcoDC();

void setup()
{
  Serial.begin(115200);
  connectWiFi(); 
  client.setServer(mqtt_server, mqtt_port);
  Serial.println("setServer completed !");
  client.setCallback(callback); 

  pinMode(cambienxe1, INPUT);
  pinMode(cambienxe2, INPUT);
  pinMode(cambienxe3, INPUT);

  pinMode(DC, OUTPUT);
  pinMode(BUTTON_DC, INPUT_PULLUP);
  digitalWrite(DC, digitalRead(Flame));

  pinMode(AC, OUTPUT);
  pinMode(BUTTON_AC, INPUT_PULLUP);
  digitalWrite(AC, digitalRead(Light)); 

  pinMode(Flame, INPUT_PULLUP);
  pinMode(Light, INPUT_PULLUP);

  Wire.begin(21, 22);
  display.begin(0x3C, true);          
  display.clearDisplay();             
  display.setTextSize(0.7);           
  display.setTextColor(SH110X_WHITE); 

  display.setCursor(0, 0);
  display.println("Hello, PBL3");
  display.setCursor(0, 10);
  display.println("ESP32 & OLED");
  display.display(); 
  delay(2000);       
}

void loop() {
  if (!client.connected()) {
    Serial.println("Connecting to broker...");
    reconnect();
  }
  client.loop(); 
  static unsigned long lastTime = 0;
  unsigned long currentTime = millis();
 
  if (currentTime - lastTime >= 2000) {  
    parking();
    client.publish("esp32/motorState", DCState ? "On" : "Off");
    client.publish("esp32/ledState", ACState ? "Off" : "On");
    lastTime = currentTime;  
  }

  DongcoDC();
  DongcoAC();
  oled();
}


void callback(char *topic, byte *payload, unsigned int length) {
    String msg = "";
    for (int i = 0; i < length; i++) {
        msg += (char)payload[i]; 
    }
    Serial.print("Tin nhắn nhận từ topic ");
    Serial.print(topic);
    Serial.print(": ");
    Serial.println(msg);

    if (strcmp(topic, "web/ledControl") == 0) {
        if (msg == "Turn on") {
            ACState = true;
            digitalWrite(AC, HIGH); 
            overrideACSensor = true;
        } else if (msg == "Turn off") {
            ACState = false;
            digitalWrite(AC, LOW);
            overrideACSensor = true;
        }
        client.publish("esp32/ledState", ACState ? "On" : "Off");

    } else if (strcmp(topic, "web/motorControl") == 0) {
        if (msg == "Turn on") {
            DCState = true;
            digitalWrite(DC, HIGH); 
            overrideDCSensor = true;
        } else if (msg == "Turn off") {
            DCState = false;
            digitalWrite(DC, LOW); 
            overrideDCSensor = true;
        }
        client.publish("esp32/motorState", DCState ? "On" : "Off");

      }else if (strcmp(topic, "web/Logout") == 0) {
      if (msg == "True") {
        isLoginProcessed = false;
        Serial.println("Logout successful, isLoginProcessed reset.");
    }
      
    } else if (strcmp(topic, "web/Login") == 0) {
        if (!isLoginProcessed) {
            if (msg == "admin|admin") {
                client.publish("web/Login", "true");  
            } else {
                client.publish("web/Login", "false"); 
            }
            isLoginProcessed = true; 
        }

    } 
}

void reconnect()
{
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    String clientID = "ESPClient-";
    clientID += String(random(0xffff), HEX);

    if (client.connect(clientID.c_str(), mqtt_username, mqtt_password))
    {
      Serial.println("connected MQTT");

      client.subscribe("esp32/ledState");
      Serial.println("connected topic esp32/ledState");

      client.subscribe("esp32/motorState");
      Serial.println("connected topic esp32/motorState");

      client.subscribe("web/ledControl");
      Serial.println("connected topic web/ledControl");

      client.subscribe("esp32/parkingStatus");
      Serial.println("connected topic esp32/parkingStatus"); 

      client.subscribe("web/motorControl");
      Serial.println("connected topic web/motorControl");

      client.subscribe("web/Login");
      client.subscribe("web/Logout");
      Serial.println("connected topic web/Login");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void connectWiFi()
{
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi..");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi !");
}

void oled(){

xe1 = digitalRead(cambienxe1);
xe2 = digitalRead(cambienxe2);
xe3 = digitalRead(cambienxe3);


display.clearDisplay();
display.setCursor(30, 0);
display.println("Welcome to DUT");


int xe[] = {xe1, xe2, xe3};
const char* slots[] = {"Slot1", "Slot2", "Slot3"};
const char* statuses[] = {"Co xe", "Khong xe"};

for (int i = 0; i < 3; i++) {
    display.setCursor(30, 10 + (i * 10));
    display.print(slots[i]);
    display.print(": ");
    display.println(statuses[xe[i]]);
}

if (xe1 == LOW && xe2 == LOW && xe3 == LOW) {
    display.clearDisplay();
    display.setCursor(30, 20);
    display.println("Parking Full");
    display.setCursor(30, 40);
    display.println("See you again");
}


display.display();
delay(100);
}

void parking() {
  int giaTri1 = digitalRead(cambienxe1);
  int giaTri2 = digitalRead(cambienxe2);
  int giaTri3 = digitalRead(cambienxe3);

    client.publish("esp32/parkingStatus", giaTri1 == HIGH ? "Slot1:Khong xe" : "Slot1:Co xe");
    client.publish("esp32/parkingStatus", giaTri2 == HIGH ? "Slot2:Khong xe" : "Slot2:Co xe");
    client.publish("esp32/parkingStatus", giaTri3 == HIGH ? "Slot3:Khong xe" : "Slot3:Co xe");
}

void DongcoAC()
{

  bool currentButtonACState = digitalRead(BUTTON_AC);
  int currentLightState = digitalRead(Light);
  if (lastButtonACState == HIGH && currentButtonACState == LOW)
  {
    overrideACSensor = true; 
    ACState = !ACState;      
    digitalWrite(AC, ACState ? LOW : HIGH);
  }
  lastButtonACState = currentButtonACState;
  if (currentLightState != lastLightState)
  {
    overrideACSensor = false;          
    lastLightState = currentLightState; 
  }
  if (!overrideACSensor)
  {
    if (currentLightState == HIGH)
    {
      digitalWrite(AC, HIGH); 
      ACState = false;      
    }
    else
    {
      digitalWrite(AC, LOW); 
      ACState = true;         
    }
    delay(10);
  }
}

void DongcoDC()
{

  bool currentButtonDCState = digitalRead(BUTTON_DC);
  int currentFlameState = digitalRead(Flame);
  if (lastButtonDCState == HIGH && currentButtonDCState == LOW)
  {
    overrideDCSensor = true; 
    DCState = !DCState;      
    digitalWrite(DC, DCState ? HIGH : LOW);
  }
  lastButtonDCState = currentButtonDCState;
  if (currentFlameState != lastFlameState)
  {
    overrideDCSensor = false;           
    lastFlameState = currentFlameState; 
  }

  if (!overrideDCSensor)
  {
    if (currentFlameState == LOW)
    {
      digitalWrite(DC, LOW); 
      DCState = false;      
    }
    else
    {
      digitalWrite(DC, HIGH); 
      DCState = true;         
    }
    delay(10);
  }
}

