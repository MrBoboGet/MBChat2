R"xdlmao(

CREATE TABLE Databases(
    Hash BLOB PRIMARY KEY,
    Type TEXT,
    Time INTEGER
);
CREATE TABLE Resources(
    Hash BLOB PRIMARY KEY,
    Name TEXT,
    ContentType INTEGER,
    UpType INTEGER,
    Time INTEGER,
    ContentSize INTEGER,
    DatabaseHash BLOB,
    ParentHash BLOB,
    ContentHash BLOB,
    UploaderID BLOB,
    Signature BLOB,
    --not sent by wire, but stored in the database
    RecievedTimestamp INTEGER,
    StoredLocaly INTEGER,
    StoredInline INTEGER, --1 means that content is the actual content, otherwise a path
    Content BLOB
);

CREATE TABLE ActiveTree(
    ID INTEGER PRIMARY KEY,
    Name TEXT,
    ParentID INTEGER,
    ResourceID BLOB,
    DatabaseID BLOB
);

CREATE TABLE Subscriptions
(
    PeerID BLOB,
    DatabaseID  BLOB,
    PRIMARY KEY(PeerID,DatabaseID)
);
CREATE TABLE DatabaseParticipants(
    DatabaseID BLOB,
    ParticipantID BLOB,
    PRIMARY KEY(DatabaseID,ParticipantID)
);

CREATE TABLE DatabaseForks(
    OriginalDB BLOB,
    DBFork BLOB,
    PRIMARY KEY(OriginalDB,DBFork)
);

CREATE TABLE Peers(
    ID BLOB PRIMARY KEY,
    Key BLOB,
    IP INTEGER,
    Port INTEGER,
    LastSeen INTEGER
);

)xdlmao"
