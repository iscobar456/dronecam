import { UUID } from "node:crypto";


type Message = {
    type: "ident" | "list" | "sd" | "ice" | "select" | "disc",
    body: MessageBody,
}

type Node = {
    id: string,
    name: string
}

type Response = {
    type: "ident" | "list" | "confirm",
    body: IdentResponseBody | ListResponseBody,
}
type IdentResponseBody = Node;
type ListResponseBody = Node[];

type MessageBody = {
    from: string,
    to: string,
    data: string,
}
