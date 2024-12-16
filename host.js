const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const cors = require('cors');
const { userInfo } = require('os');

let userTotal = 0;
const userList = [];

class User {
    constructor(id=null, pass = "12345"){
        userTotal++;
        this.userID=id? id : "00" + String(userTotal);
        this.password=pass;
    }
}
userList.push(new User("Bob","bobo"));
userList.push(new User("Alice","alili"));

// 建立 Express 應用程式
const app = express();
app.use(cors()); // 啟用 CORS 支援 css

// 建立 HTTP 伺服器並讓 Socket.io 使用它
const server = http.createServer(app);
const CLASSNUMBER = "CS3004301";

// 初始化 Socket.io，監聽 HTTP 伺服器
const io = new Server(server, {
    cors: {
        origin: "*", // 允許來自 Live Server 的請求, 根據右下角的數值更改
        methods: ["GET", "POST"]
    }
});

let connectedUsers = new Map();
// 當用戶端連接時觸發 'connection' 事件
io.on('connection', (socket) => {
    if (connectedUsers.length >= 4) {
        // 如果已達到連接限制，拒絕新的連線
        socket.emit("connectionError", { message: "Maximum connection" });
        socket.disconnect();
        return;
    }
    
    // 檢查連接
    connectedUsers.set(socket.id,{userId: null});
    console.log('User connected:', socket.id);

    // 密碼驗證請求
    socket.on("loginRequest", (data) => {
        const { classNumber, userID, password } = data;
        
        if(CLASSNUMBER == classNumber){
            for(i of userList){
                if( userID == i.userID){
                    if (password == i.password) {
                        socket.emit("loginResponse", {
                            success: true,
                            field: "Login",
                            message: "Success.",
                            id:i.userID,
                            password:i.password
                        });
                        return;
                    }
                }
            }
            socket.emit("loginResponse", {
                success: false,
                field: "Error",
                message: "Invalid UID or Password",
            });
            return;
        }
        else{
            socket.emit("loginResponse", {
                success: false,
                field: "Error",
                message: "Unknown Class Number",
            });
            return;
        }
    });

    // 監聽來自用戶端的 'sendMessage' 與廣播
    socket.on('sendMessage', (msg) => {
        console.log('Message received from', msg.name, ":", msg.message);  // 在終端機輸出訊息
        io.emit('allMessage', msg);  // 廣播訊息給所有連線的用戶
    }); 

    // 監聽用戶端斷線事件
    socket.on('userDisconnect', (msg) => {
        if (!msg.socketId) {
            console.error('msg.socketId is undefined or null:', msg);
            return;
        }
        console.log('Before removal:', Array.from(connectedUsers.entries()));

        connectedUsers.delete(msg.socketId);
        
        console.log('After removal:', Array.from(connectedUsers.entries()));
    });
});

// 啟動伺服器
server.listen(4040, () => {
    console.log('Server socket listening on port 4040');
});
