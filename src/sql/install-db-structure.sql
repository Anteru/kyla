PRAGMA foreign_keys = ON;

-- All content objects stored in this file repository
CREATE TABLE content_objects (
    Id INTEGER PRIMARY KEY NOT NULL,
    Hash BLOB NOT NULL UNIQUE,
    Size INTEGER NOT NULL,
    ChunkCount INTEGER NOT NULL,
    ReferenceCount INTEGER NOT NULL DEFAULT 1);

-- All file sets stored in this repository
CREATE TABLE file_sets (Id INTEGER PRIMARY KEY NOT NULL,
    Name VARCHAR NOT NULL UNIQUE);

CREATE TABLE files (Path TEXT PRIMARY KEY NOT NULL,
    -- If NULL, the file is an empty file
    ContentObjectId INTEGER,
    FileSetId INTEGER NOT NULL,
    FOREIGN KEY(ContentObjectId) REFERENCES content_objects(Id),
    FOREIGN KEY(FileSetId) REFERENCES file_sets(Id));

CREATE TABLE source_packages (Id INTEGER PRIMARY KEY NOT NULL,
    Name VARCHAR NOT NULL UNIQUE,
    Filename VARCHAR NOT NULL UNIQUE,
    Uuid BLOB NOT NULL,
    Hash BLOB NOT NULL);

-- Maps one content object to one or more source packages
-- If a content object is not found here, the file itself is the content
-- object. This can happen if the repository is "loose", i.e. installed
CREATE TABLE storage_mapping (
    ContentObjectId INTEGER NOT NULL,
    -- if NULL the content object is not stored in this repository
    -- useful for patches, which rely on content being present in the
    -- target already
    SourcePackageId INTEGER,
    FOREIGN KEY(ContentObjectId) REFERENCES content_objects(Id),
	FOREIGN KEY(SourcePackageId) REFERENCES source_packages(Id));

-- Take advantage of SQLite's dynamic types here so we don't have to store
-- whether it is an int, a blob or a string
CREATE TABLE properties (
    Name VARCHAR PRIMARY KEY,
    Value NOT NULL
);
