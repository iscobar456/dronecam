import { UUID } from "node:crypto";


type Message = {
    type: "ident" | "list" | "sd" | "ice",
    body: null | string | MessageBody,
}

type Node = {
    id: UUID,
    name: string
}

type IdentResponse = Node;
type ListResponse = Node[];

type MessageBody = {
    from: UUID,
    to: UUID,
    data: string
}
