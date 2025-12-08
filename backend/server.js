const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const mqtt = require('mqtt');

const app = express();
const server = http.createServer(app);
const io = socketIo(server);

// Kết nối MQTT Broker (Dùng chung broker với ESP32)
const client = mqtt.connect('mqtt://broker.hivemq.com');

// Trỏ về thư mục frontend (đi ra ngoài 1 cấp thư mục)
app.use(express.static('../frontend'));

client.on('connect', () => {
    console.log('Đã kết nối MQTT Broker');
    client.subscribe('nhom18/data/status');
});



// --- LUỒNG 1: Nhận từ ESP32 -> Đẩy ra Web ---
client.on('message', (topic, message) => {
    if (topic === 'nhom18/control') {
        // Chuyển Buffer thành String rồi thành JSON object
        try {
            const data = JSON.parse(message.toString());
            console.log('Data từ ESP:', data);
            io.emit('sensor-update', data); // Gửi socket xuống Web
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
