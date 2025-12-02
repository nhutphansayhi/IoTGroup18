#include <WiFi.h> // Thành mạng -> nối wifi
#include <PubSubClient.h> // Lib ngoài -> xử lý giao thức MQTT
#include <ArduinoJson.h> // Giải mã định dạng thành json

// --- 1. CẤU HÌNH WIFI & MQTT ---
const char* ssid = "nhutphansayhi"; 
const char* password = "lopdaihoc";
const char* mqtt_server = "broker.hivemq.com";

// --- 2. ĐỊNH NGHĨA CHÂN (PIN MAPPING CHO ESP32 DEVKIT V1) ---
// Input
#define TRIG_PIN 5      // GPIO 5 (An toàn)
#define ECHO_PIN 18     // GPIO 18 (An toàn)
#define LDR_PIN 34      // GPIO 34 (Chỉ Input - ADC1, tốt khi dùng WiFi)
#define BUTTON_PIN 15   // GPIO 15 (Có sẵn pull-up nội bộ)

// Output
#define RELAY_PIN 4     // GPIO 4 (An toàn)
#define BUZZER_PIN 21   // GPIO 21 (Chọn chân khác chân đèn hệ thống)


WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;// mốc thời gian tính bằng mili s

void setup(){
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(LDR_PIN, INPUT); // ESP32 ADC
    pinMode(BUTTON_PIN, INPUT_PULLUP); // Nút nhấn nối xuống GND
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    // Trạng thái ban đầu
    digitalWrite(RELAY_PIN, LOW); // Tắt Relay (tùy lại relay kích mức cao/thấp)
    digitalWrite(BUZZER_PIN, LOW);
}

void loop(){

}

// --- XỬ LÝ ĐIỀU KIỆN TỪ WEB ---
// Hàm xử lý sự kiện 
void callback(char* topic, byte* message, unsigned int length){
    String msgTemp;
    for (int i = 0; i < length; i++) {
        msgTemp += (char)message[i];
    }
    Serial.print("Nhan lenh: ");
    Serial.println(msgTemp);

    // Giải mã JSON lệnh: {"device": "RELAY", "status": "ON"}
    // cấp phát một vùng nhớ tạm thời để chứa dữ liệu JSON sắp giải mã
    // 200 bytes
    StaticJsonDocument<200> doc; 
    DeserializationError error = deserializeJson(doc, msgTemp);

    if (!error) {
        const char* device = doc["device"];
        const char* status = doc["status"];
        if (strcmp(device,"RELAY") == 0){
            if (strcmp(status, "ON") == 0) digitalWrite(RELAY_PIN, HIGH);
            else digitalWrite(RELAY_PIN, LOW);
        }
        else if (strcmp(device,"BUZZER" == 0)){
            if (strcmp(status,"ON") == 0){
                digitalWrite(BUZZER_PIN, HIGH);
            }
            else (){
                digitalWrite(BUZZER_PIN, LOW);
            }
        }
    }
}
    

void setup_wifi(){
    delay(10);
    Serial.println();
    Serial.print("Dang ket noi WiFi: ");
    Serial.println(ssid);

    Wifi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED){
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWifi da ket noi!");
}

void reconnect(){
    while(!client.connectedd()){
        Serial.print("Dang ket noi MQTT...");
        String clientId = "ESP32Client-";
        cliendID += String(random(0xffff), HEX);
    

        if (client.connect(clientId.c_str())){
            Serial.println("Thanh cong!");
            client.subscribe("nhom18/control");
        } else {
            Serial.print(client.state());
            Serial.print("Loi, rc=");
            Serial.println(" thu lai sau 5s");
            delay(5000);
        }
    }
}