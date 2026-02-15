import WebSocket, { WebSocketServer } from 'ws';
import type { Response, Message, MessageBody, Node } from './messageTypes.d.ts';
import { randomUUID, } from 'node:crypto';
import type { Config } from 'unique-names-generator';
import { uniqueNamesGenerator, languages } from 'unique-names-generator';

type Connection = {
    type: "unknown" | "observer" | "drone",
    name: string,
    socket: WebSocket
}

const connections: Map<string, Connection> = new Map();

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
        const connId = randomUUID().toString();
        connections.set(connId, conn);

        const response: Response = {
            type: "ident",
            body: {
                id: connId,
                name: conn.name,
            }
        }

        sendMessage(ws, JSON.stringify(response));

        ws.on('error', console.error);
        ws.on('message', messageHandler);
        ws.on('close', () => {

        })
    });

    const messageHandler = (data: Buffer) => {
        let dataString = data.toString()
        console.log(`Received message: ${dataString}`);
        const message: Message = JSON.parse(dataString);
        let messageBody = message.body as MessageBody;
        let response;
        let conn;
        switch (message.type) {
            case 'sd':
            case 'ice':
                console.log("forwarding message");
                conn = connections.get(messageBody.to);
                conn?.socket.send(messageBody.data);
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

            case 'ident':
                let id = messageBody.from;
                conn = connections.get(id);
                if (conn == null) {
                    return
                }
                const clientType = messageBody.data;
                if (clientType == 'observer' || clientType == 'drone') {
                    conn.type = clientType;
                }
                console.log(connections);
                break;

            case 'select':
                conn = connections.get(messageBody.to);
                if (conn == null) {
                    return;
                }
                response = JSON.stringify({ type: 'select', body: messageBody.from });
                sendMessage(conn.socket, response);
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
    console.log(`Sending message: "${message}" to ${wsId}`);
    ws.send(message);
}
