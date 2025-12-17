#include <Arduino.h> 
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Firebase_ESP_Client.h>
#include <LiquidCrystal_I2C.h>  // LCD I2C
#include <Wire.h>
#include <BH1750.h>

#define FIREBASE_HOST "https://smartsupportcardevice-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "GiZP3gMeY8tM7n72Qis06Fe6aaAwTaPEEQro7Ga2"
const char* FIREBASE_STATUS_PATH = "/nhom18/pro_status"; // 👈 Cần khai báo này


FirebaseConfig config;
FirebaseAuth auth;
FirebaseData fbdo;
FirebaseData fbdoStream;

// --- KHAI BÁO HÀM (PROTOTYPES) ---
void setup();
void loop();
void setup_wifi();
void reconnect();
void callback(char* topic, byte* message, unsigned int length);


// --- 1. CẤU HÌNH WIFI & MQTT ---

const char* ssid = "nhutphansayhi"; 
const char* password = "lopdaihoc";
const char* mqtt_server = "broker.hivemq.com";
const char* mqtt_topic = "nhom18/control";
const char* mqtt_data_topic = "nhom18/data/status";



// Khai báo biến Non-Blocking và Debounce
int lastButtonState = HIGH;      // TRẠNG THÁI CHUẨN: HIGH (Nhả nút) do dùng INPUT_PULLUP
unsigned long lastSendTime = 0;
unsigned long lastMqttRetry = 0;
unsigned long lastWifiRetry = 0;
const long interval = 50; // Tần suất kiểm tra nút
const long RECONNECT_INTERVAL = 5000; // Thử lại kết nối sau 5s


// --- 2. ĐỊNH NGHĨA CHÂN ---
#define TRIG_PIN 5      
#define ECHO_PIN 18     
#define LDR_PIN 34      
#define BUTTON_PIN 15   
#define RELAY_PIN 4     
#define BUZZER_PIN 23
#define SDA_PIN 21
#define SCL_PIN 22
// #define SDA_PR_PIN 17
// #define SCL_PR_PIN 19

#define NOTE_C  262
#define NOTE_D  294
#define NOTE_E  330
#define NOTE_F  349
#define NOTE_G  392
#define NOTE_A  440
#define NOTE_B  494

int melody[] = {
  NOTE_C, NOTE_C, NOTE_G, NOTE_G, NOTE_A, NOTE_A, NOTE_G,
  NOTE_F, NOTE_F, NOTE_E, NOTE_E, NOTE_D, NOTE_D, NOTE_C
};

int noteDurations[] = {
  300, 300, 300, 300, 300, 300, 600,
  300, 300, 300, 300, 300, 300, 600
};

WiFiClient espClient;
PubSubClient client(espClient);

// LCD I2C (địa chỉ 0x27, 16 cột, 2 hàng)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Khai bao cam bien anh sang
BH1750 lightMeter(0x5C);
// Biến lưu settings từ Firebase
int lightThreshold = 860;
int warningDistance = 50;
int dangerDistance = 20;
bool proStatus = false;
// bool lastButtonStatus = false;
// bool currentButtonStatus = false;
bool flag = false;
unsigned long lastFirebaseStatusRead = 0;
unsigned long lastMqttSend = 0;
unsigned long lastFirebaseRead = 0;
bool isBuzzerBlinking = false;

bool lightStatus = false;
// Sửa lỗi 1: Hàm setup_wifi() không chặn
void setup_wifi(){
    delay(10);
    Serial.println();
    Serial.print("Dang khoi tao WiFi: ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);


    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
        delay(500);
        Serial.print(".");

    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Connected!");
        
        // Chỉ sau WiFi OK mới init Firebase
        config.database_url = FIREBASE_HOST; 
        config.signer.tokens.legacy_token = FIREBASE_AUTH;

        config.timeout.wifiReconnect = 10000;
        config.timeout.socketConnection = 10000;
        config.timeout.serverResponse = 10000;

        Firebase.begin(&config, &auth); 
        Firebase.reconnectWiFi(true);
    }

    // Đã loại bỏ vòng lặp blocking. Sẽ chờ trong loop().
}

// Sửa lỗi 2: Hàm reconnect() non-blocking
void reconnect(){
    Serial.println("function reconnect");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    Serial.print("Dang ket noi MQTT...");
    if (client.connect(clientId.c_str())){
        Serial.println("Thanh cong!");
        client.subscribe("nhom18/control");
        // client.subscribe("nhom18/data/status"); // Đăng ký lại topic data nếu cần thiết
    } else {
        Serial.print("Loi, rc=");
        Serial.print(client.state());
        Serial.println(" (Se thu lai sau)");
    }
}

void callback(char* topic, byte* message, unsigned int length){
    String msgTemp;
    for (int i = 0; i < length; i++) {
        msgTemp += (char)message[i];
    }
    Serial.print("Nhan lenh: ");
    Serial.println(msgTemp);

    StaticJsonDocument<200> doc; 
    DeserializationError error = deserializeJson(doc, msgTemp);

    if (!error) {
        // --- BƯỚC SỬA LỖI QUAN TRỌNG: KIỂM TRA SỰ TỒN TẠI CỦA KEY ---
        if (doc.containsKey("device") && doc.containsKey("status")) {
            const char* device = doc["device"]; 
            const char* status = doc["status"]; 
            
            // CHỈ XỬ LÝ LỆNH NẾU CHÚNG TỒN TẠI
            if (strcmp(device,"RELAY") == 0){
                if (strcmp(status, "ON") == 0) digitalWrite(RELAY_PIN, HIGH);
                else digitalWrite(RELAY_PIN, LOW);
            }
            else if (strcmp(device,"BUZZER") == 0){
                if (strcmp(status,"ON") == 0){
                    isBuzzerBlinking = true;
                }
                else 
                    isBuzzerBlinking = false;
            }
        } else {
            // Đây là tin nhắn dữ liệu trạng thái (btn_status), bỏ qua.
            Serial.println("Tin nhan trang thai, khong xu ly bang callback.");
        }
    } else {
        Serial.println("Lỗi giải mã JSON!");
    }
}


// Hàm cập nhật LCD hiển thị settings
void updateLCD() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("L:");
    lcd.print(lightThreshold);
    lcd.setCursor(8,0);
    lcd.print(" W:");
    lcd.print(warningDistance);
    
    lcd.setCursor(0, 1);
    lcd.print("D:");
    lcd.print(dangerDistance);
    lcd.print("cm");
}

// // Callback khi stream bị lỗi
void streamCallback(FirebaseStream data) {
    String path = data.dataPath();
    
    // In log để kiểm tra (Debug)
    Serial.println("\n--- STREAM DATA RECEIVED ---");
    Serial.print("PATH: "); Serial.println(path);
    Serial.print("TYPE: "); Serial.println(data.dataType());
    
    if (data.dataType() == "json") {
        Serial.print("JSON RAW: ");
        Serial.println(data.jsonString());
    }

    // ----------------------------------------------------
    // 1. XỬ LÝ TRẠNG THÁI NÚT BẤM (/pro_status)
    // ----------------------------------------------------
    if (path == "/pro_status") {
        FirebaseJson json = data.jsonObject();
        FirebaseJsonData result;
        if (json.get(result, "status")) {
            proStatus = result.boolValue;
            Serial.print("=> Button Status: "); Serial.println(proStatus);
            // Nếu hệ thống bật -> cập nhật LCD ngay
            if (proStatus) updateLCD();
        }
    }
    
    // ----------------------------------------------------
    // 2. XỬ LÝ CÀI ĐẶT LẺ (Khi chỉnh từng cái một)
    // ----------------------------------------------------
    else if (path == "/settings/lightThreshold") {
        lightThreshold = data.to<int>(); 
        Serial.print("=> Single Light Update: "); Serial.println(lightThreshold);
        if (proStatus) updateLCD();
    }
    else if (path == "/settings/warningDistance") {
        warningDistance = data.to<int>();
        Serial.print("=> Single Warning Update: "); Serial.println(warningDistance);
        if (proStatus) updateLCD();
    }
    else if (path == "/settings/dangerDistance") {
        dangerDistance = data.to<int>();
        Serial.print("=> Single Danger Update: "); Serial.println(dangerDistance);
        if (proStatus) updateLCD();
    }

    // ----------------------------------------------------
    // 3. XỬ LÝ GỘP TẠI NHÁNH /settings (Trường hợp ít gặp hơn)
    // ----------------------------------------------------
    else if (path == "/settings") {
        Serial.println("=> Bulk Update at /settings");
        FirebaseJson json = data.jsonObject();
        FirebaseJsonData result;
        bool changed = false;

        if (json.get(result, "lightThreshold")) { 
            lightThreshold = result.to<int>(); changed = true; 
        }
        if (json.get(result, "warningDistance")) { 
            warningDistance = result.to<int>(); changed = true; 
        }
        if (json.get(result, "dangerDistance")) { 
            dangerDistance = result.to<int>(); changed = true; 
        }

        if (changed && proStatus) {
            updateLCD();
            Serial.println("   LCD Updated!");
        }
    }

    // ----------------------------------------------------
    else if (path == "/") {
        Serial.println("=> Bulk Update at ROOT (/)");
        FirebaseJson json = data.jsonObject();
        FirebaseJsonData result;
        bool changed = false;

        // Lưu ý: Key lúc này sẽ kèm theo đường dẫn con
        if (json.get(result, "settings/lightThreshold")) {
            lightThreshold = result.to<int>();
            Serial.print("   - Light: "); Serial.println(lightThreshold);
            changed = true;
        }

        if (json.get(result, "settings/warningDistance")) {
            warningDistance = result.to<int>();
            Serial.print("   - Warning: "); Serial.println(warningDistance);
            changed = true;
        }

        if (json.get(result, "settings/dangerDistance")) {
            dangerDistance = result.to<int>();
            Serial.print("   - Danger: "); Serial.println(dangerDistance);
            changed = true;
        }

        if (changed && proStatus) {
            updateLCD();
            Serial.println("   LCD Updated from ROOT!");
        }
    }
}


void streamTimeoutCallback(bool timeout) {
    if (timeout) {
        Serial.println("Stream timeout, attempting to resume...");
    }
}

// Hàm nháy đền "cảnh báo"
void buzzerWarring(int level){
    if (level == 1)
    {
        if ((millis()/2000)%2==0)
            digitalWrite(BUZZER_PIN,HIGH);
        else 
            digitalWrite(BUZZER_PIN,LOW);
    }
    else if (level == 0){
        if ((millis()/1000)%2==0)
            digitalWrite(BUZZER_PIN,HIGH);
        else 
            digitalWrite(BUZZER_PIN,LOW);
    }
    else if (level == 2){
        digitalWrite(BUZZER_PIN,LOW);
    }
}

// Hàm nhay đèn "nguy hiểm"


// Hàm đọc khoảng cách từ cảm biến Ultrasonic (cm)
float getDistance() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    long duration = pulseIn(ECHO_PIN, HIGH, 30000); // timeout 30ms
    if (duration == 0) return -1; // Không nhận được phản hồi
    return duration * 0.034 / 2;
}

void shutdownSystem(){

    digitalWrite(RELAY_PIN, LOW);
    // noTone(BUZZER_PIN);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Smart Parking");
    lcd.setCursor(0,1);
    lcd.print("System OFF");
}



void startupSystem() {
  // Khi bật lại sản phẩm
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Smart Parking");
  lcd.setCursor(0, 1);
  lcd.print("System ON");

//   for (int i = 0; i < sizeof(melody) / sizeof(int); i++) {
//     ledcWriteTone(0, melody[i]);      // Phát nốt
//     delay(noteDurations[i]);          // Giữ nốt
//     ledcWrite(0, 0);                  // Tắt giữa nốt
//     // delay(50);                        // Nhịp nghỉ nhỏ
//   }
}
// Hàm đọc settings từ Firebase
void readProductStatusFirebase(){
    if (Firebase.RTDB.getJSON(&fbdo, "/nhom18/pro_status")){
        FirebaseJson &json = fbdo.jsonObject();
        FirebaseJsonData result;

        if (json.get(result, "btn_status")){
            proStatus = result.boolValue;
        }
    }
}
void readSettingsFromFirebase() {
    if (Firebase.RTDB.getJSON(&fbdo, "/nhom18/settings")) {


        FirebaseJson &json = fbdo.jsonObject();
        FirebaseJsonData result;
        
        if (json.get(result, "lightThreshold")) {
            lightThreshold = result.intValue;
        }
        if (json.get(result, "warningDistance")) {
            warningDistance = result.intValue;
        }
        if (json.get(result, "dangerDistance")) {
            dangerDistance = result.intValue;
        }
        
        Serial.print("Settings from Firebase - Light: ");
        Serial.print(lightThreshold);
        Serial.print(", Warning: ");
        Serial.print(warningDistance);
        Serial.print(", Danger: ");
        Serial.println(dangerDistance);
        
        updateLCD();
    } else {
        Serial.print("Loi doc Firebase settings: ");
        Serial.println(fbdo.errorReason());
    }
}



// --- SETUP VÀ LOOP CHÍNH ---

void setup() {
    Serial.begin(9600); // Tốc độ ổn định nhất
    
    // CẤU HÌNH PHẦN CỨNG
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
    pinMode(BUTTON_PIN, INPUT_PULLUP); // ✅ Sửa lỗi: Dùng INPUT_PULLUP
    pinMode(TRIG_PIN, OUTPUT);  // Ultrasonic trigger
    pinMode(ECHO_PIN, INPUT);   // Ultrasonic echo
    
    Wire.begin(21, 22);
    lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
    // Khởi tạo LCD
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Smart Parking");
    lcd.setCursor(0, 1);
    lcd.print("Starting...");
    
    
    
    WiFi.disconnect(true); // Xóa thông tin WiFi cũ

    // KHỞI TẠO MẠNG
    setup_wifi();

    fbdo.setResponseSize(4096); // Tăng bộ đệm phản hồi lên 4KB
    fbdoStream.setResponseSize(4096);
    
    // Cấu hình bộ đệm SSL (Rất quan trọng cho HTTPS)
    client.setBufferSize(4096); // Cho MQTT

    if (!Firebase.RTDB.beginStream(&fbdoStream, "/nhom18")) {
        Serial.print("Stream begin failed: ");
        Serial.println(fbdoStream.errorReason());
    }
    
    // Gắn callback vào stream
    Firebase.RTDB.setStreamCallback(&fbdoStream, streamCallback, streamTimeoutCallback);

    readSettingsFromFirebase();
    
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);

    // ledcSetup(0, 1000, 8);
    // ledcAttachPin(BUZZER_PIN, 0);
    
}

void loop() {
    // --- PHẦN 0: KHỞI ĐỘNG HỆ THỐNG
    // --- PHẦN : LOGIC ĐỌC NÚT VÀ GỬI DỮ LIỆU ---
        // Logic này không bị delay() trong reconnect chặn lại nữa
        // TRONG HÀM loop():
        

        // 1. Xử lý WiFi
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("if 1");
        if (millis() - lastWifiRetry > RECONNECT_INTERVAL) {
            Serial.println("Reconnecting WiFi...");
            WiFi.reconnect();
            lastWifiRetry = millis();
        }
    
    } else if (!client.connected()) {
        Serial.println("if 2");
        if (millis() - lastMqttRetry > RECONNECT_INTERVAL) {
        reconnect();
        lastMqttRetry = millis();//serviceAccountKey.json
        }
    } else {
        
        // ✅ KHI NÚT VẬT LÝ BẤM
        bool currentButtonState = digitalRead(BUTTON_PIN);
        if (currentButtonState != lastButtonState) {
            if (currentButtonState == HIGH){
                proStatus = ! proStatus;
                if (Firebase.ready()) {
                    // Dùng setBool gửi trực tiếp vào đường dẫn con "/status"
                    // Đường dẫn ghép: "/nhom18/pro_status/status"
                    String path = String(FIREBASE_STATUS_PATH) + "/status";
                    
                    if (Firebase.RTDB.setBool(&fbdo, path, proStatus)) {
                        Serial.print("Da gui thanh cong: ");
                        Serial.println(proStatus);
                    } else {
                        Serial.print("Loi gui Firebase: ");
                        Serial.println(fbdo.errorReason());
                    }
                }
            }
            lastButtonState = currentButtonState;
        }
        
        // 2. MQTT handling
        if (!client.connected()) {
            if (millis() - lastMqttRetry > RECONNECT_INTERVAL) {
                reconnect();
                lastMqttRetry = millis();
            }
        } else {
            client.loop();
        }
        
        // 3. proStatus logic (Firebase sẽ tự update qua stream, không cần đọc)
        if (proStatus == true) {
            if (flag == false) {
                // startupSystem();
                updateLCD();
                flag = true;
            }
            
            


            float distance = getDistance();
            float light = lightMeter.readLightLevel();

            // bao dong khi qua gan 
            if (distance <= 20){
                buzzerWarring(0);
            }
            else if (distance <=50){
                buzzerWarring(1);
            }
            else 
                buzzerWarring(2);

            
            
            // Gửi sensor
            if (millis() - lastMqttSend > 1000 && client.connected()) {
                

                Serial.println(distance);
                Serial.println(light);
                
                StaticJsonDocument<200> sensorDoc;
                sensorDoc["distance"] = distance;
                sensorDoc["light"] = light;
                
                String payload;
                serializeJson(sensorDoc, payload);
                client.publish("nhom18/data/sensor", payload.c_str());
                
                lastMqttSend = millis();
            }
            
            // Đọc settings (1 lần mỗi 5 giây, không ưu tiên cao)
            // if (millis() - lastSettingsRead > 5000 && Firebase.ready()) {
            //     readSettingsFromFirebase();
            //     lastSettingsRead = millis();
            // }
            
        } else {  // proStatus == false
            if (flag == true) {
                shutdownSystem();
                flag = false;
            }
        }


        // buzzer theo nuts tren web
        if (isBuzzerBlinking && proStatus == false){
            if ((millis()/1000)%2==0){
                digitalWrite(BUZZER_PIN,HIGH);
            }
            if ((millis()/1000)%2==1){
                digitalWrite(BUZZER_PIN,LOW);
            }
        }
        else 
            digitalWrite(BUZZER_PIN,LOW);
    }
}
