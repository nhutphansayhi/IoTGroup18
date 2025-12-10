# 🚗 Smart Support Car Device - IoT Group 18

Hệ thống IoT điều khiển thiết bị thông minh sử dụng ESP32, MQTT và Firebase.

## 📁 Cấu Trúc Dự Án

```
IoTGroup18/
├── backend/           # Server Node.js (Express + MQTT + Socket.io + Firebase)
│   ├── server.js
│   └── serviceAccountKey.json
├── controller/        # Code ESP32 (PlatformIO)
│   ├── main.cpp
│   ├── platformio.ini
│   └── secrets.h
├── frontend/          # Giao diện Web (HTML/CSS)
│   ├── index.html
│   ├── login.html
│   ├── register.html
│   └── style.css
└── package.json
```

## ⚙️ Yêu Cầu Hệ Thống

### Phần mềm cần cài đặt:
- [Node.js](https://nodejs.org/) (v16 trở lên)
- [PlatformIO](https://platformio.org/) (IDE hoặc CLI)
- [VS Code](https://code.visualstudio.com/) (khuyên dùng)

### Phần cứng:
- Board ESP32
- Các cảm biến/thiết bị điều khiển (relay, sensor,...)

---

## 🚀 Hướng Dẫn Chạy Dự Án

### 1️⃣ Cài đặt Dependencies

Mở Terminal tại thư mục gốc của dự án và chạy:

```bash
npm install
```

### 2️⃣ Cấu hình Firebase

1. Đảm bảo file `backend/serviceAccountKey.json` đã được cấu hình đúng với Firebase project của bạn.
2. Kiểm tra `databaseURL` trong file `backend/server.js` khớp với Firebase Realtime Database của bạn.

### 3️⃣ Chạy Backend Server

```bash
# Chạy từ thư mục gốc
npm start

# Hoặc chạy trực tiếp từ thư mục backend
cd backend
node server.js
```

Server sẽ chạy tại: **http://localhost:3001**

### 4️⃣ Nạp Code ESP32 (Controller)

1. Mở thư mục `controller/` bằng VS Code với PlatformIO extension.
2. Cấu hình WiFi và các thông số trong file `secrets.h`:
   ```cpp
   #define WIFI_SSID "your-wifi-ssid"
   #define WIFI_PASSWORD "your-wifi-password"
   ```
3. Kết nối ESP32 qua USB.
4. Click **Upload** trên PlatformIO hoặc chạy:
   ```bash
   cd controller
   pio run -t upload
   ```

### 5️⃣ Truy cập Frontend

Sau khi backend đang chạy, mở trình duyệt và truy cập:

- **Trang chính**: http://localhost:3001/index.html
- **Đăng nhập**: http://localhost:3001/login.html
- **Đăng ký**: http://localhost:3001/register.html

---

## 🔄 Luồng Hoạt Động

```
┌─────────────┐      MQTT       ┌─────────────┐      Socket.io     ┌─────────────┐
│   ESP32     │ ◄─────────────► │   Backend   │ ◄─────────────────► │   Frontend  │
└─────────────┘                 └─────────────┘                     └─────────────┘
                                       │
                                       │ Firebase
                                       ▼
                               ┌─────────────┐
                               │  Firebase   │
                               │  Database   │
                               └─────────────┘
```

1. **ESP32 → Web**: ESP32 gửi dữ liệu qua MQTT → Backend nhận và đẩy tới Web qua Socket.io
2. **Web → ESP32**: Web gửi lệnh qua Socket.io → Backend publish MQTT → ESP32 nhận và thực thi
3. **Firebase**: Lưu trữ trạng thái và đồng bộ dữ liệu realtime

---

## 📡 MQTT Topics

| Topic | Mô tả |
|-------|-------|
| `nhom18/control` | Gửi/nhận lệnh điều khiển thiết bị |

---

## 🛠️ Troubleshooting

### Lỗi kết nối MQTT
- Kiểm tra kết nối internet
- Broker mặc định: `mqtt://broker.hivemq.com`

### Lỗi Firebase
- Kiểm tra file `serviceAccountKey.json` hợp lệ
- Xác nhận quyền truy cập Firebase Database

### ESP32 không upload được
- Kiểm tra driver USB-Serial (CP2102 hoặc CH340)
- Thử đổi cổng COM trong PlatformIO

---

## 👥 Thành Viên Nhóm 18

> Thêm thông tin thành viên tại đây

---

## 📝 License

MIT License
