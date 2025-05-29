import { Server, WebSocket } from "ws";
import * as net from "net";
import { IncomingMessage, ServerResponse } from "http";
import * as https from "https";
import * as fs from "fs";

const PORT = 8080;

// Create https server with self-signed cert or CA authorised cert by certbot
// Run gen.sh to get self-signed cert for https
const server = https.createServer({
	key: fs.readFileSync("./privkey.pem"),
	cert: fs.readFileSync("./cert.pem")
}, (req: IncomingMessage, res: ServerResponse) => {
	res.writeHead(200, { "Content-Type": "text/plain" });
	res.end("https://github.com/night0721");
}).listen(PORT, () => {
	console.log(`HTTPS server is running on https://localhost:${PORT}`);
});

const wss = new Server({ server });

wss.on("connection", (ws: WebSocket) => {
	console.log("WebSocket client connected");

	// Create a TCP socket to the relay server
	const tcpSocket = new net.Socket();
	tcpSocket.connect(20247, "127.0.0.1", () => {
		console.log(`Connected to TCP server at 127.0.0.1:20247`);
	});

	// Handle WebSocket messages (from client to TCP server)
	ws.on("message", (message: ArrayBuffer) => {
		// Extract the first 6 bytes of the message as the header
		const header = message.slice(0, 5);
		const data = message.slice(5);
		const headerArray = new Uint8Array(header);
		const dataArray = new Uint8Array(data);
		
		console.log(`Header: ${headerArray}`);
		console.log(`WebSocket -> TCP: ${dataArray}`);
		
		tcpSocket.write(headerArray);
		// Send the rest of the message
		tcpSocket.write(dataArray);
	});

	// Handle TCP socket messages (from TCP server to WebSocket client)
	tcpSocket.on("data", (data: ArrayBuffer) => {
		console.log(`TCP -> WebSocket: ${new Uint8Array(data)}`);
		ws.send(data);
	});

	// Handle WebSocket close event
	ws.on("close", () => {
		console.log("WebSocket client disconnected");
		tcpSocket.end();
	});

	// Handle TCP socket close event
	tcpSocket.on("close", () => {
		console.log("Disconnected from TCP server");
		ws.close();
	});

	// Handle errors
	ws.on("error", (err: Error) => console.error("WebSocket error:", err));
	tcpSocket.on("error", (err: Error) => console.error("TCP socket error:", err));
});

console.log(`WebSocket server running on wss://localhost:${PORT}`);
