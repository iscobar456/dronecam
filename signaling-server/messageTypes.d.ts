export type Message = {
    type: "ident" | "list" | "sd" | "ice" | "select" | "disc",
    body: MessageBody,
}

export type Node = {
    id: string,
    name: string
}

export type Response = {
    type: "ident" | "list" | "confirm",
    body: IdentResponseBody | ListResponseBody,
}
export type IdentResponseBody = Node;
export type ListResponseBody = Node[];

export type MessageBody = {
    from: string,
    to: string,
    data: string,
}

