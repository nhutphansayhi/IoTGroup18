#include <Arduino.h> 
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Firebase_ESP_Client.h>
#include <LiquidCrystal_I2C.h>  // LCD I2C

#define FIREBASE_HOST "smartsupportcardevice-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "GiZP3gMeY8tM7n72Qis06Fe6aaAwTaPEEQro7Ga2"
const char* FIREBASE_BUTTON_PATH = "/nhom18/button_status"; // 👈 Cần khai báo này


FirebaseConfig config;
FirebaseAuth auth;
FirebaseData fbdo;


// --- KHAI BÁO HÀM (PROTOTYPES) ---
void setup();
void loop();
void setup_wifi();
void reconnect();
void callback(char* topic, byte* message, unsigned int length);


// --- 1. CẤU HÌNH WIFI & MQTT ---

const char* ssid = "Thien Nhan^.^"; 
const char* password = "22092005.";
const char* mqtt_server = "broker.hivemq.com";
const char* mqtt_topic = "nhom18/control";
const char* mqtt_data_topic = "nhom18/data/status";

// Khai báo biến Non-Blocking và Debounce
int lastButtonState = HIGH;      // TRẠNG THÁI CHUẨN: HIGH (Nhả nút) do dùng INPUT_PULLUP
unsigned long lastSendTime = 0;
unsigned long lastReconnectAttempt = 0;
const long interval = 50; // Tần suất kiểm tra nút
const long RECONNECT_INTERVAL = 5000; // Thử lại kết nối sau 5s

// --- 2. ĐỊNH NGHĨA CHÂN ---
#define TRIG_PIN 5      
#define ECHO_PIN 18     
#define LDR_PIN 34      
#define BUTTON_PIN 15   
#define RELAY_PIN 4     
#define BUZZER_PIN 21

WiFiClient espClient;
PubSubClient client(espClient);

// LCD I2C (địa chỉ 0x27, 16 cột, 2 hàng)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Biến lưu settings từ Firebase
int lightThreshold = 860;
int warningDistance = 50;
int dangerDistance = 20;



// Sửa lỗi 1: Hàm setup_wifi() không chặn
void setup_wifi(){
    delay(10);
    Serial.println();
    Serial.print("Dang khoi tao WiFi: ");
    Serial.println(ssid);

    config.database_url = FIREBASE_HOST; 
    config.signer.tokens.legacy_token = FIREBASE_AUTH;

    Firebase.begin(&config, &auth); 
    Firebase.reconnectWiFi(true);

    WiFi.begin(ssid, password);
    // Đã loại bỏ vòng lặp blocking. Sẽ chờ trong loop().
}

// Sửa lỗi 2: Hàm reconnect() non-blocking
void reconnect(){
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    Serial.print("Dang ket noi MQTT...");
    if (client.connect(clientId.c_str())){
        Serial.println("Thanh cong!");
        client.subscribe("nhom18/control");
        client.subscribe("nhom18/data/status"); // Đăng ký lại topic data nếu cần thiết
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
                    digitalWrite(BUZZER_PIN, HIGH);
                }
                else{
                    digitalWrite(BUZZER_PIN, LOW);
                }
            }
        } else {
            // Đây là tin nhắn dữ liệu trạng thái (btn_status), bỏ qua.
            Serial.println("Tin nhan trang thai, khong xu ly bang callback.");
        }
    } else {
        Serial.println("Lỗi giải mã JSON!");
    }
}

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

// Hàm cập nhật LCD hiển thị settings
void updateLCD() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("L:");
    lcd.print(lightThreshold);
    lcd.print(" W:");
    lcd.print(warningDistance);
    
    lcd.setCursor(0, 1);
    lcd.print("D:");
    lcd.print(dangerDistance);
    lcd.print("cm");
}

// Hàm đọc settings từ Firebase
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
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
}

void loop() {
    // --- PHẦN 1: LOGIC KẾT NỐI MẠNG (NON-BLOCKING) ---
    
    // 1. Kiểm tra và kết nối lại WiFi
    if (WiFi.status() != WL_CONNECTED) {
        if (millis() - lastReconnectAttempt > RECONNECT_INTERVAL) {
            Serial.println("WiFi bị ngắt/chưa kết nối, dang thu lai...");
            WiFi.reconnect();
            lastReconnectAttempt = millis();
        }
        delay(10); // Ngăn loop chạy quá nhanh khi không kết nối
        return; 
    } 
    
    // 2. Kiểm tra và kết nối lại MQTT (Chỉ khi WiFi đã có)
    if (!client.connected()) {
        if (millis() - lastReconnectAttempt > RECONNECT_INTERVAL) {
            reconnect();
            lastReconnectAttempt = millis();
        }
        delay(10); // Ngăn loop chạy quá nhanh
        return; 
    }
    
    

    client.loop(); // Giữ kết nối MQTT và xử lý tin nhắn đến

    // --- PHẦN 3: LOGIC ĐỌC NÚT VÀ GỬI DỮ LIỆU ---
    // Logic này không bị delay() trong reconnect chặn lại nữa
    // TRONG HÀM loop():
    if (millis() - lastSendTime > interval) {
        int currentButtonState = digitalRead(BUTTON_PIN);
        
        if (currentButtonState != lastButtonState) {
            
            // CHỈ GỬI DỮ LIỆU NẾU KẾT NỐI FIREBASE ĐÃ SẴN SÀNG
            if (WiFi.status() == WL_CONNECTED && Firebase.ready()) { 
                lastButtonState = currentButtonState;
                
                // ✅ SỬA LỖI: Sử dụng FirebaseJson thay vì ArduinoJson
                FirebaseJson json;
                // LOW (Nhấn) = 1, HIGH (Nhả) = 0
                json.set("btn_status", (currentButtonState == LOW) ? 1 : 0); 

                // Gửi dữ liệu lên FIREBASE
                if (Firebase.RTDB.setJSON(&fbdo, FIREBASE_BUTTON_PATH, &json)) {
                    Serial.print("Da gui trang thai nut len Firebase: ");
                    // In ra Serial để debug
                    json.toString(Serial, true);
                    Serial.println();
                } else {
                    Serial.print("Loi gui Firebase: ");
                    Serial.println(fbdo.errorReason());
                }
            } else {
                // Nếu Firebase chưa sẵn sàng, thử gửi lại sau
                Serial.println("Chua gui duoc, Firebase hoac WiFi chua san sang.");
            }
        }
        lastSendTime = millis();
    }

    // --- GỬI DỮ LIỆU ULTRASONIC QUA MQTT LIÊN TỤC (100ms) ---
    static unsigned long lastMqttSend = 0;
    if (millis() - lastMqttSend > 100) {  // 100ms = 10 lần/giây
        float distance = getDistance();
        int light = analogRead(LDR_PIN);
        
        StaticJsonDocument<200> sensorDoc;
        sensorDoc["distance"] = distance;
        sensorDoc["light"] = light;
        
        String payload;
        serializeJson(sensorDoc, payload);
        client.publish("nhom18/control", payload.c_str());
        
        Serial.print("Sent MQTT: ");
        Serial.println(payload);
        
        lastMqttSend = millis();
    }

    // --- ĐỌC SETTINGS TỪ FIREBASE MỖI 0.1 GIÂY ---
    static unsigned long lastFirebaseRead = 0;
    if (millis() - lastFirebaseRead > 100) {
        if (Firebase.ready()) {
            readSettingsFromFirebase();
        }
        lastFirebaseRead = millis();
    }
}