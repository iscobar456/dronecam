export type Message = {
    type: "ident" | "list" | "sd" | "ice" | "select" | "disconnect",
    body: MessageBody,
}


export type PublicNode = {
    id: string,
    name: string,
}

export type Response = {
    type: "ident" | "list" | "confirm",
    body: IdentResponseBody | ListResponseBody,
}
export type IdentResponseBody = PublicNode;
export type ListResponseBody = PublicNode[];

export type MessageBody = {
    from: string,
    to: string,
    data: string,
}

