import WebSocket, { WebSocketServer } from 'ws';
import { randomUUID, } from 'node:crypto';
import { uniqueNamesGenerator, languages } from 'unique-names-generator';
const connections = new Map();
function main() {
    console.log("starting server...");
    const wss = new WebSocketServer({ port: 8080 });
    const config = {
        dictionaries: [languages]
    };
    wss.on('connection', function connection(ws) {
        const conn = {
            type: "unknown",
            name: uniqueNamesGenerator(config),
            socket: ws,
        };
        const connId = randomUUID();
        connections.set(connId, conn);
        const response = {
            type: "ident",
            body: {
                id: connId,
                name: conn.name,
            }
        };
        ws.send(JSON.stringify(response));
        ws.on('error', console.error);
        ws.on('message', messageHandler);
        ws.on('close', () => {
        });
    });
    const messageHandler = (data) => {
        let dataString = data.toString();
        console.log(dataString);
        const message = JSON.parse(dataString);
    };
}
main();
function sendMessage(ws, message) {
    console.log(`Sending message: "${message}" to ___`);
    ws.send(message);
}
//# sourceMappingURL=index.js.map