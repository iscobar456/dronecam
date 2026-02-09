import { UUID } from "node:crypto";


type Message = {
    type: "ident" | "list" | "sd" | "ice",
    body: null | string | MessageBody,
}

type Node = {
    id: UUID,
    name: string
}

type Response = {
    type: "ident" | "list" | "confirm",
    body: IdentResponseBody | ListResponseBody,
}
type IdentResponseBody = Node;
type ListResponseBody = Node[];

type MessageBody = {
    from: UUID,
    to: UUID,
    data: object
}
