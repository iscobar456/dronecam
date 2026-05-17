import WebSocket, { WebSocketServer } from 'ws';
import type { Message, MessageBody } from './messageTypes.d.ts';
import type { Config } from 'unique-names-generator';
import { uniqueNamesGenerator, animals } from 'unique-names-generator';
import type { IncomingMessage } from 'node:http';

export interface Node extends WebSocket {
    clientType: string,
    name: string,
    id: string,
}

type Connection = [Node, Node];

const nodes: Node[] = new Array();
const connections: Connection[] = new Array();

function main() {
    const wss = new WebSocketServer({ port: 8080 });

    wss.on('connection', function connection(node: Node, req: IncomingMessage) {
        const url = new URL(req.url || "", "http://localhost");
        const nodeId = url.searchParams.get("id");
        const nodeName = url.searchParams.get("name");
        const nodeType = url.searchParams.get("type");
        if (!nodeId || !nodeType) {
            node.send("id and type params required");
            return;
        };
        node.id = nodeId;
        node.name = nodeName || uniqueNamesGenerator({ dictionaries: [animals] });
        node.clientType = nodeType;
        nodes.push(node);

        console.log(`Set id ${nodeId} to type ${nodeType}`)

        node.on('error', console.error);
        node.on('message', (data: Buffer) => { messageHandler(node, data) });
        node.on('close', () => {
            closeConnection(getConnection(node), node);
            nodes.splice(nodes.indexOf(node), 1);
        })
    });

    const closeConnection = (conn: Connection | null, from?: Node) => {
        if (!conn) return;

        conn.forEach(node => {
            // skip sending disconnect message if is source node
            if (from && node.id == from.id) return;
            sendMessage(node, {
                type: 'disconnect',
                body: {
                    from: from ? from.id : '',
                    to: node.id,
                    data: "",
                }
            })
        })

        connections.splice(connections.indexOf(conn), 1);
    }

    const messageHandler = (node: Node, data: Buffer) => {
        let dataString = data.toString()
        const message: Message = JSON.parse(dataString);
        let messageBody = message.body as MessageBody;
        let dest;
        switch (message.type) {
            case 'list':
                sendMessage(node, {
                    type: 'list',
                    body: {
                        from: node.id,
                        to: node.id,
                        data: JSON.stringify(getDroneList())
                    }
                })
                break;

            case 'select':
                console.log(`Observer selecting ${messageBody.to}: ${JSON.stringify(message)}`);

                dest = getNodeFromId(messageBody.to);
                if (!dest) {
                    console.warn("Attempted to select a non-extant peer");
                    return;
                };
                connections.push([node, dest]);
                dest.send(JSON.stringify(message));

                break;

            case 'disconnect':
                console.log("received: " + dataString)
                console.log("closing connection");
                const conn = getConnection(node);
                closeConnection(conn, node);
                break;

            case 'sd':
            case 'ice':
                console.log(`Forwarding ${message.type} to ${messageBody.to}`);

                dest = getNodeFromId(messageBody.to);
                dest?.send(JSON.stringify(message));
                break;
        }
    }

    const getDroneList = () => {
        return nodes.filter(
            node => node.clientType == "drone"
        ).map(
            node => { return { id: node.id, name: node.name } }
        );
    }
}

main();

function sendMessage(node: Node, message: Message) {
    console.log(JSON.stringify(message));
    node.send(JSON.stringify(message));
}

const getConnection = (node: Node): Connection | null => {
    const c = connections.filter(c => c[0].id == node.id || c[1].id == node.id).at(0);
    if (!c) {
        console.warn("Could not find connection for node " + node.id);
    }
    return c || null;
}


// this one could probably be gotten rid of by closing over
const getNodeFromId = (id: string): Node | undefined => {
    let n;
    nodes.forEach((node) => {
        if (node.id == id) {
            n = node
        }
    })
    return n;
}
