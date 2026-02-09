import WebSocket, { WebSocketServer } from 'ws';
import type { Response, Message, MessageBody } from './messageTypes.d.ts';
import type { UUID } from "node:crypto";
import { randomUUID, } from 'node:crypto';
import type { Config } from 'unique-names-generator';
import { uniqueNamesGenerator, languages } from 'unique-names-generator';

type Connection = {
    type: "unknown" | "observer" | "drone",
    name: string,
    socket: WebSocket
}

const connections: Map<UUID, Connection> = new Map();

function main() {
    console.log("starting server...")

    const wss = new WebSocketServer({ port: 8080 });
    const config: Config = {
        dictionaries: [languages]
    }

    wss.on('connection', function connection(ws: WebSocket) {
        const conn: Connection = {
            type: "unknown",
            name: uniqueNamesGenerator(config),
            socket: ws,
        }
        const connId = randomUUID();
        connections.set(connId, conn);

        const response: Response = {
            type: "ident",
            body: {
                id: connId,
                name: conn.name,
            }
        }

        ws.send(JSON.stringify(response));

        ws.on('error', console.error);
        ws.on('message', messageHandler);
        ws.on('close', () => {

        })
    });

    const messageHandler = (data: Buffer) => {
        let dataString = data.toString()
        console.log(dataString);
        const message: Message = JSON.parse(dataString);
        if (message.type === 'sd' || message.type === "ice") {
            console.log("forwarding message")
            const messageBody = message.body as MessageBody;
            const ws = connections.get(messageBody.to);
            ws?.socket.send(JSON.stringify(messageBody.data));
        }
    }
}

main();

function sendMessage(ws: WebSocket, message: string) {
    console.log(`Sending message: "${message}" to ___`)
    ws.send(message);
}
