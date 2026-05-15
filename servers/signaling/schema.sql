CREATE TABLE node (
    id INTEGER PRIMARY KEY,
    public_id INTEGER UNIQUE,
    name TEXT,
    connection_id INTEGER,
    CONSTRAINT fk_connection FOREIGN KEY connection REFERENCES connection(connection_id)
);

CREATE TABLE connection (
    id INTEGER PRIMARY KEY,
)
