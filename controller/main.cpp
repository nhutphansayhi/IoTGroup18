#include <Arduino.h> 
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Firebase_ESP_Client.h>


#define FIREBASE_HOST "smartsupportcardevice-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "GiZP3gMeY8tM7n72Qis06Fe6aaAwTaPEEQro7Ga2"

// --- KHAI BÁO HÀM (PROTOTYPES) ---
void setup();
void loop();
void setup_wifi();
void reconnect();
void callback(char* topic, byte* message, unsigned int length);


// --- 1. CẤU HÌNH WIFI & MQTT ---  
const char* ssid = "Highlands Coffee"; 
const char* password = "";
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



// Sửa lỗi 1: Hàm setup_wifi() không chặn
void setup_wifi(){
    delay(10);
    Serial.println();
    Serial.print("Dang khoi tao WiFi: ");
    Serial.println(ssid);

    Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
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

// --- SETUP VÀ LOOP CHÍNH ---

void setup() {
    Serial.begin(9600); // Tốc độ ổn định nhất
    
    // CẤU HÌNH PHẦN CỨNG
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
    pinMode(BUTTON_PIN, INPUT_PULLUP); // ✅ Sửa lỗi: Dùng INPUT_PULLUP
    
    
    
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
    if (millis() - lastSendTime > interval) {
        int currentButtonState = digitalRead(BUTTON_PIN);
        
        if (currentButtonState != lastButtonState) {
            
            // CHỈ GỬI DỮ LIỆU NẾU KẾT NỐI FIREBASE ĐÃ SẴN SÀNG
            if (WiFi.status() == WL_CONNECTED && Firebase.ready()) { 
                lastButtonState = currentButtonState;
                
                // 1. Tạo JSON object
                StaticJsonDocument<200> doc;
                doc["btn_status"] = (currentButtonState == LOW) ? 1 : 0; 

                // 2. Gửi dữ liệu lên FIREBASE
                if (Firebase.setJSON(fbdo, FIREBASE_BUTTON_PATH, doc)) {
                    Serial.print("Da gui trang thai nut len Firebase: ");
                    serializeJson(doc, Serial);
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
}