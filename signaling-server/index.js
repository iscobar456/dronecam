import WebSocket, { WebSocketServer } from 'ws';
import { randomUUID, } from 'node:crypto';
import { uniqueNamesGenerator, languages } from 'unique-names-generator';
const wss = new WebSocketServer({ port: 8080 });
const connections = new Map();
const config = {
    dictionaries: [languages]
};
function main() {
    console.log("hello, starting server...");
    wss.on('connection', function connection(ws) {
        const conn = {
            type: "unknown",
            name: uniqueNamesGenerator(config),
            socket: ws,
        };
        const connId = randomUUID();
        connections.set(connId, conn);
        const response = {
            id: connId,
            name: conn.name,
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
//# sourceMappingURL=index.js.map