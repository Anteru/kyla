CREATE TABLE content_objects (Id INTEGER PRIMARY KEY NOT NULL,
    Hash BLOB NOT NULL UNIQUE,
    Size INTEGER NOT NULL,
    ChunkCount INTEGER NOT NULL);
CREATE TABLE features (Id INTEGER PRIMARY KEY NOT NULL,
    Name VARCHAR NOT NULL UNIQUE);

-- If a file has no ContentObjectId, it's an empty file
CREATE TABLE files (Path TEXT PRIMARY KEY NOT NULL,
    ContentObjectId INTEGER,
    FeatureId INTEGER NOT NULL,
    FOREIGN KEY(ContentObjectId) REFERENCES content_ojects(Id),
    FOREIGN KEY(FeatureId) REFERENCES features(Id));
CREATE TABLE directories (Path TEXT PRIMARY KEY NOT NULL,
    FeatureId INTEGER NOT NULL,
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
CREATE INDEX files_feature_id_idx ON files (FeatureId ASC);
CREATE INDEX storage_mapping_content_object_id_idx ON storage_mapping (ContentObjectId ASC);
