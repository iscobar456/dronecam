import WebSocket, { WebSocketServer } from 'ws';
import type { Message, MessageBody, Node } from './messageTypes.d.ts';
import type { Config } from 'unique-names-generator';
import { uniqueNamesGenerator, languages } from 'unique-names-generator';
import type { IncomingMessage } from 'node:http';

type Connection = {
    type: "unknown" | "observer" | "drone",
    name: string,
    socket: WebSocket,
}

const connections: Map<string, Connection> = new Map();

function main() {
    const wss = new WebSocketServer({ port: 8080 });
    const config: Config = {
        dictionaries: [languages]
    }

    wss.on('connection', function connection(ws: WebSocket, req: IncomingMessage) {
        const url = new URL(req.url || "", "http://localhost");
        const clientId = url.searchParams.get("id");
        const clientType = url.searchParams.get("type");
        if (!clientId || (clientType != "drone" && clientType != "observer")) {
            console.log("No ID or Invalid type")
            return;
        }
        const conn: Connection = {
            type: clientType,
            name: uniqueNamesGenerator(config),
            socket: ws,
        }
        console.log(`Set id ${clientId} to type ${clientType}`)
        connections.set(clientId, conn);

        ws.on('error', console.error);
        ws.on('message', messageHandler);
        ws.on('close', () => {

        })
    });

    const messageHandler = (data: Buffer) => {
        let dataString = data.toString()
        const message: Message = JSON.parse(dataString);
        let messageBody = message.body as MessageBody;
        let response;
        let conn;
        switch (message.type) {
            case 'sd':
            case 'ice':
                conn = connections.get(messageBody.to);
                console.log(`Forwarding ${message.type} to ${messageBody.to}`);
                conn?.socket.send(JSON.stringify(message));
                break;

            case 'list':
                conn = connections.get(messageBody.from);
                if (conn == null) {
                    return;
                }
                const droneList = getDroneList();
                response = {
                    type: 'list',
                    body: JSON.stringify(droneList)
                }
                sendMessage(conn.socket, JSON.stringify(response));
                break;

            case 'select':
                conn = connections.get(messageBody.to);
                console.log(`Observer selecting ${messageBody.to}: ${JSON.stringify(message)}`);
                conn?.socket.send(JSON.stringify(message));
                break;

            case 'disc': // disconnect

        }
    }

    const getDroneList = () => {
        let droneList: Array<Node> = [];
        connections.forEach((connection, id) => {
            if (connection.type === "drone") {
                droneList.push({ id: id, name: connection.name })
            }
        })
        return droneList;
    }
}

main();

function sendMessage(ws: WebSocket, message: string) {
    let wsId;
    connections.forEach((conn, id) => {
        if (conn.socket === ws) {
            wsId = id;
        }
    })
    ws.send(message);
}
