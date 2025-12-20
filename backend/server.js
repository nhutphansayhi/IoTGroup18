const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const mqtt = require('mqtt');

const admin = require("firebase-admin");
const bodyParser = require('body-parser');
const serviceAccount = require("./serviceAccountKey.json");

//Pushsafer register
const Pushsafer = require("pushsafer-notifications");

const pushsafer = new Pushsafer({
    k: "ZX1Yh6xxkqyU818uQtkj" // ← key của bạn
});

// Nodemailer Config
const nodemailer = require('nodemailer');
const transporter = nodemailer.createTransport({
    service: 'gmail',
    auth: {
        user: 'webservicee99@gmail.com', //  Thay bằng email của bạn
        pass: 'rlrp fkfp gskg hlty'      //  Thay bằng App Password
    }
});


const app = express();
const server = http.createServer(app);
const io = socketIo(server);

let productOn = false;
let lastSettings = {
    lightThreshold: null,
    warningDistance: null,
    dangerDistance: null
};


app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));

admin.initializeApp({
    credential: admin.credential.cert(serviceAccount),
    databaseURL: "https://smartsupportcardevice-default-rtdb.firebaseio.com"
});

const db = admin.database();

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
const buttonRef = db.ref("nhom18/pro_status"); // Đường dẫn phải khớp với code ESP32

buttonRef.on("value", (snapshot) => {
    const data = snapshot.val(); // Lấy dữ liệu: { btn_status: 1 } hoặc 0
    // const isOn = data.btn_status === 1;
    if (!data || data.status === undefined) return;

    const proState = data.status;

    io.emit('button-update', { productOn: proState });
    console.log("Trạng thái nút", proState);

    // --- PUSHSAFER NOTICE (System Status) ---
    // Chỉ gửi khi trạng thái thực sự thay đổi (để tránh spam khi khởi động server)
    if (proState !== productOn) {
        productOn = proState; // Cập nhật trạng thái

        const statusMsg = proState ? "🟢 System Powered ON" : "🔴 System Powered OFF";

        pushsafer.send(
            {
                m: statusMsg,
                t: "Smart Parking Status",
                s: 11, // sound
                v: 1,  // vibration
                i: 5   // icon
            },
            (err, result) => {
                if (err) console.error("Pushsafer error:", err);
                else console.log("System Status Push sent:", result);
            }
        );
    }


}, (errorObject) => {
    console.log("Lỗi đọc Firebase: " + errorObject.name);
});

const realTimes = db.ref("nhom18/sensorRealtime");

realTimes.on("value", (snapshot) => {
    const data = snapshot.val(); // Lấy dữ liệu: { btn_status: 1 } hoặc 0
    // const isOn = data.btn_status === 1;
    if (!data || data.distance === undefined) return;
    if (!data || data.light === undefined) return;

    const distance = data.distance;
    const light = data.light;

    io.emit('realtime-update', {
        distance: distance,
        light: light 
    });
    console.log("thông số hiện tại distance ", distance );
    console.log("thông số hiện tại light", light );



})
const settingsRef = db.ref("nhom18/settings");

settingsRef.on("value", (snapshot) => {
    const data = snapshot.val();
    if (!data) return;

    const messages = [];

    if (
        lastSettings.lightThreshold !== null &&
        data.lightThreshold !== lastSettings.lightThreshold
    ) {
        messages.push(
            `💡 Light Threshold changed: ${lastSettings.lightThreshold} → ${data.lightThreshold} lux`
        );
    }

    if (
        lastSettings.warningDistance !== null &&
        data.warningDistance !== lastSettings.warningDistance
    ) {
        messages.push(
            `⚠️ Warning Distance changed: ${lastSettings.warningDistance} → ${data.warningDistance} cm`
        );
    }

    if (
        lastSettings.dangerDistance !== null &&
        data.dangerDistance !== lastSettings.dangerDistance
    ) {
        messages.push(
            `🚨 Danger Distance changed: ${lastSettings.dangerDistance} → ${data.dangerDistance} cm`
        );
    }

    // Nếu có ít nhất 1 thay đổi → gửi email
    if (messages.length > 0) {

        // --- GỬI EMAIL ---
        const receiver = data.lastUser || 'default-admin@gmail.com'; 
        const mailOptions = {
            from: '"Smart Parking System" <no-reply@smartparking.com>',
            to: receiver, 
            subject: '⚠️ Xác nhận: Bạn vừa thay đổi cài đặt hệ thống',
            text: messages.join("\n") + "\n\nĐây là tin nhắn tự động.",
            html: `
                <h3>Smart Parking Settings Updated</h3>
                <p>Xin chào <b>${receiver}</b>,</p>
                <p>Hệ thống ghi nhận bạn vừa thay đổi các cài đặt sau:</p>
                <ul>
                    ${messages.map(msg => `<li>${msg}</li>`).join('')}
                </ul>
                <p><i>Nếu không phải bạn, vui lòng kiểm tra lại tài khoản.</i></p>
            `
        };

        transporter.sendMail(mailOptions, function(error, info){
            if (error) {
                console.log("Email Error:", error);
            } else {
                console.log('Email sent: ' + info.response);
            }
        });
    }

    // Cập nhật giá trị cũ
    lastSettings = {
        lightThreshold: data.lightThreshold,
        warningDistance: data.warningDistance,
        dangerDistance: data.dangerDistance
    };
});


// ----------------------------------------------------------

client.on("connect", () => {
    console.log("Đã kết nối MQTT Broker");
    client.subscribe("nhom18/data/sensor");
});



// --- LUỒNG 1: Nhận từ ESP32 -> Đẩy ra Web ---
client.on('message', (topic, message) => {
    if (topic === 'nhom18/data/sensor') {
        // Chuyển Buffer thành String rồi thành JSON object
        try {
            const data = JSON.parse(message.toString());
            console.log('Data từ ESP:', data);
            io.emit('sensor-update', data); // Gửi socket xuống Web

            // --- QUAN TRỌNG: Lưu lịch sử để thỏa yêu cầu "Lưu trữ theo thời gian" ---
            // const historyRef = db.ref("nhom18/history");
            // historyRef.push({
            //     distance: data.distance,
            //     light: data.light,
            //     timestamp: admin.database.ServerValue.TIMESTAMP
            // });
        } catch (e) {
            console.error("Lỗi parse JSON:", e);
        }
    }
    if (topic === 'nhom18/data/timestamp') {
        const historyRef = db.ref("nhom18/history");
        historyRef.push({
            distance: data.distance,
            light: data.light,
            timestamp: admin.database.ServerValue.TIMESTAMP
        });
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

server.listen(5000, () => {
    console.log('🏃‍♂️‍➡️🏃‍♂️‍➡️🏃‍♂️‍➡️ Server chạy tại: http://localhost:5000');
});
