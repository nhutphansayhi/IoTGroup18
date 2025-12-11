const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const mqtt = require('mqtt');

const admin = require("firebase-admin");
const bodyParser = require('body-parser');
const serviceAccount = require("./serviceAccountKey.json");

const app = express();
const server = http.createServer(app);
const io = socketIo(server);

app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));

admin.initializeApp({
  credential: admin.credential.cert(serviceAccount),
  databaseURL: "https://smartsupportcardevice-default-rtdb.firebaseio.com"
});

const db = admin.database();

// Kết nối telegram
const axios = require('axios');

const TELEGRAM_TOKEN = "YOUR_BOT_TOKEN";
const CHAT_ID = ["8116174154"
];

function sendTelegram(msg) {
    const url = `https://api.telegram.org/bot${TELEGRAM_TOKEN}/sendMessage`;

    CHAT_IDS.forEach(chatId => {
        axios.post(url, {
            chat_id: chatId,
            text: msg
        })
        .then(() => console.log("Đã gửi Telegram đến:", chatId))
        .catch(err => console.error("Không gửi được đến", chatId, err));
    });
}


//--- gửi telegram mỗi khi biến threshold cập nhật
db.ref("nhom18/settings").on("value", (snapshot) => {
    const s = snapshot.val();
    if (!s) return;

    const msg = `
⚙️ *Cập nhật Setting mới:*
• Light Threshold: ${s.lightThreshold}
• Warning Distance: ${s.warningDistance}
• Danger Distance: ${s.dangerDistance}
    `;

    sendTelegram(msg).then(() => {
        console.log("Đã gửi thông báo Telegram");
    }).catch(err => {
        console.error("Lỗi gửi Telegram:", err);
    });
});

// Kết nối MQTT Broker (Dùng chung broker với ESP32)
const client = mqtt.connect('mqtt://broker.hivemq.com');


// Trỏ về thư mục frontend (đi ra ngoài 1 cấp thư mục)
app.use(express.static('../frontend'));

app.post('/api/register', async (req, res) => {
    // Lấy dữ liệu (fullname, email, password) được gửi từ Frontend qua req.body
    const { email, password, fullname } = req.body;

    if (!email || !password || !fullname) {
        return res.status(400).send({ message: 'Thiếu thông tin đăng ký.' });
    }

    try {
        // 1. Gửi yêu cầu tạo người dùng mới đến Firebase Authentication
        const userRecord = await admin.auth().createUser({
            email: email,
            password: password,
            displayName: fullname, // Lưu tên
            emailVerified: false
        });

        // 2. Gửi phản hồi thành công về Frontend (Mã 201 Created)
        res.status(201).send({ 
            message: 'Đăng ký thành công!', 
            uid: userRecord.uid 
        });

    } catch (error) {
        // 3. Xử lý lỗi (ví dụ: email đã tồn tại, mật khẩu quá ngắn)
        console.error("Lỗi đăng ký Firebase:", error);
        res.status(409).send({ // Dùng mã 409 Conflict cho trường hợp email đã tồn tại
            message: `Lỗi đăng ký: ${error.message}`
        });
    }
});


// --- PHẦN MỚI: LẮNG NGHE FIREBASE (Luồng ESP32 -> Web) ---
const buttonRef = db.ref("nhom18/button_status"); // Đường dẫn phải khớp với code ESP32


buttonRef.on("value", (snapshot) => {
    const data = snapshot.val(); // Lấy dữ liệu: { btn_status: 1 } hoặc 0
    console.log("🔥 Dữ liệu từ Firebase:", data);
    
    if (data) {
        // Gửi xuống Web qua Socket.io
        io.emit('button-update', data); 
    }
}, (errorObject) => {
    console.log("Lỗi đọc Firebase: " + errorObject.name);
});
// ----------------------------------------------------------

client.on("connect", () => {
  console.log("Đã kết nối MQTT Broker");
  client.subscribe("nhom18/control");
});


const sensorRef = db.ref("nhom18/sensor_history");
// --- LUỒNG 1: Nhận từ ESP32 -> Đẩy ra Web ---
client.on('message', (topic, message) => {
    if (topic === 'nhom18/control') {
        // YÊU CẦU 4: Chuyển dữ liệu thời gian thực cho firebase

        // Khi nhận mỗi gói dữ liệu từ ESP32
        // Chuyển Buffer thành String rồi thành JSON object
        try {
            const data = JSON.parse(message.toString());
            console.log('Data từ ESP:', data);            
            io.emit('sensor-update', data);// Gửi realtime ra web

            // --- LƯU LỊCH SỬ LÊN FIREBASE ---
            const timestamp = Date.now();
            sensorRef.push({
                time: timestamp,     // thời gian UNIX
                distance: data.distance,
                light: data.light
            });

        } catch (e) {
            console.error("Lỗi parse JSON:", e);
        }
    }
});


// --- LUỒNG 2: Nhận từ Web -> Đẩy xuống ESP32 ---
io.on('connection', (socket) => {
    console.log('Web User đã kết nối');

    socket.on('control-cmd', (cmd) => {
        // cmd dạng: { device: "RELAY", status: "ON" }
        const payload = JSON.stringify(cmd);
        client.publish('nhom18/control', payload);
        console.log('tx Lệnh gửi đi:', payload);
    });
});




server.listen(3000, () => {
    console.log('🏃‍♂️‍➡️🏃‍♂️‍➡️🏃‍♂️‍➡️ Server chạy tại: http://localhost:3000');
});

