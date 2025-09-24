const net = require('net');
const express = require('express');
const { io } = require('socket.io-client'); // Cliente de WebSocket
const fs = require('fs');
const path = require('path');

const app = express();
const PORT = 3000;

// Conectarse a un WebSocket existente
//const websocketURL = 'https://ane-monitoreo-C0gxaxcud5gbfvds.canadacentral-01.azurewebsites.net'; // Reemplaza con la URL de tu WebSocket
const websocketURL = 'http://192.168.181.246:3000'; // Reemplaza con la URL de tu WebSocket

const socket = io(websocketURL);
const tcpClient = new net.Socket();

tcpClient.connect(2000, '127.0.0.1', function() {
	console.log('Client Connected');
});

// Manejo de eventos del WebSocket
socket.on('connect', () => {
    tcpClient.write('init,0');
    console.log("Solicitando init al interno:");
});

tcpClient.on('data', function(serverData) {
	console.log(`server sent ${serverData}`);
    const regex = /{(\w+):({.*})}/;
    const data = `${serverData}`.match(regex);
    
    if(data[1] === "initResponse") {
        socket.emit('init', JSON.parse(data[2] || '{}')); // Enviar init solo una vez al conectar
    }else if(data[1] === "dataStreaming") {
        console.log(`server sent ${data[1]}`);
        const filePath = '../JSON/0'
        fs.readFile(filePath, 'utf8', 
            (err,data) => {
                if(err) {
                    console.log(err);
                    return;
                }
                try {
                    const jsonData = JSON.parse(data);
                    console.log("Streaming Data OK")
                    socket.emit('dataStreaming', JSON.stringify(jsonData.data));
                } catch(parseError) {
                    console.log(parseError);
                }
            }
        )    
    }else if(data[1] === "data") {
        console.log(`server sent ${data[1]}`);
        const filePath = '../JSON/0'
        fs.readFile(filePath, 'utf8', 
            (err,data) => {
                if(err) {
                    console.log(err);
                    return;
                }
                try {
                    const jsonData = JSON.parse(data);
                    console.log("REC Data OK")
                    socket.emit('data', JSON.stringify(jsonData.data));
                } catch(parseError) {
                    console.log(parseError);
                }
            }
        )    
    }else {
        console.log(`server sent ${data[1]}`);
    }
});

socket.on('command', (response) => {
    if(response.command === 'startLiveData') {
        tcpClient.write(JSON.stringify(response.data));
        console.log('Envio de startLiveData');
    } else if(response.command ==='scheduleMeasurement') {
        tcpClient.write(JSON.stringify(response.data));
        console.log('Envio de startLiveDataRec');
    } else if(response.command ==='stopLiveData') {
        tcpClient.write('stop,0');
        console.log('Envio de stopLiveData');
    }
}); 

socket.on('disconnect', () => {
    console.log('Desconectado del WebSocket');
}); 

tcpClient.on('close', function() {
	console.log('Connection closed');
    tcpClient.destroy(); // kill client after server's response
    process.exit(0);
});

// Configurar Express
app.get('/', (req, res) => {
    res.send('Servidor Express conectado a WebSocket');
});

// Iniciar servidor Express
app.listen(PORT, () => {
    console.log(`Servidor Express escuchando en http://localhost:${PORT}`);
});
