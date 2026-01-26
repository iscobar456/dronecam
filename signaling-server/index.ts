import WebSocket, { WebSocketServer } from 'ws';
import type { IdentResponse, Message } from './messageTypes.d.ts';
import type { UUID } from "node:crypto";
import { randomUUID, } from 'node:crypto';
import type { Config } from 'unique-names-generator';
import { uniqueNamesGenerator, languages } from 'unique-names-generator';

type Connection = {
    type: "unknown" | "observer" | "drone",
    name: string,
    socket: WebSocket
}


function main() {
    console.log("starting server...")

    const wss = new WebSocketServer({ port: 8080 });
    const connections: Map<UUID, Connection> = new Map();
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

        const response: IdentResponse = {
            id: connId,
            name: conn.name,
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
    }
}

main();
