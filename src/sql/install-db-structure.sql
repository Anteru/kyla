CREATE TABLE content_objects (Id INTEGER PRIMARY KEY NOT NULL,
    Hash BLOB NOT NULL UNIQUE,
    Size INTEGER NOT NULL,
    ChunkCount INTEGER NOT NULL);
CREATE TABLE features (Id INTEGER PRIMARY KEY NOT NULL,
    Name VARCHAR NOT NULL UNIQUE,
    ParentId INTEGER,
    UIName TEXT NOT NULL,
    UIDescription TEXT,
    FOREIGN KEY(ParentId) REFERENCES features(Id));

CREATE TABLE files (Path TEXT PRIMARY KEY NOT NULL,
    -- If a file has no ContentObjectId, it's an empty file
    ContentObjectId INTEGER,
    FeatureId INTEGER NOT NULL,
    FOREIGN KEY(ContentObjectId) REFERENCES content_objects(Id),
    FOREIGN KEY(FeatureId) REFERENCES features(Id));
CREATE TABLE source_packages (Id INTEGER PRIMARY KEY NOT NULL,
    Name VARCHAR NOT NULL UNIQUE,
    Filename VARCHAR NOT NULL UNIQUE,
    Hash BLOB NOT NULL);

-- Maps one content object to one or more source packages
CREATE TABLE storage_mapping (
    ContentObjectId INTEGER NOT NULL,
    SourcePackageId INTEGER NOT NULL,
    FOREIGN KEY(ContentObjectId) REFERENCES content_objects(Id),
	FOREIGN KEY(SourcePackageId) REFERENCES source_packages(Id));

-- Take advantage of SQLite's dynamic types here so we don't have to store
-- whether it is an int, a blob or a string
CREATE TABLE properties (
    Name VARCHAR PRIMARY KEY,
    Value NOT NULL
)
