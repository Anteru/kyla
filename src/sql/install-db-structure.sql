PRAGMA foreign_keys = ON;

-- All content objects stored in this file repository
CREATE TABLE content_objects (
    Id INTEGER PRIMARY KEY NOT NULL,
    Hash BLOB NOT NULL UNIQUE,
    Size INTEGER NOT NULL);

-- All file sets stored in this repository
CREATE TABLE file_sets (
    Id INTEGER PRIMARY KEY NOT NULL,
    Uuid BLOB NOT NULL UNIQUE,
    Name VARCHAR NOT NULL UNIQUE);

CREATE TABLE files (Path TEXT PRIMARY KEY NOT NULL,
    ContentObjectId INTEGER NOT NULL,
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

CREATE INDEX files_file_set_id_idx ON files (FileSetId ASC);
CREATE INDEX storage_mapping_content_object_id_idx ON storage_mapping (ContentObjectId ASC);
CREATE INDEX content_object_hash_idx ON content_objects (Hash ASC);
CREATE INDEX files_path_idx ON files (Path ASC);

CREATE VIEW content_objects_with_reference_count
    AS SELECT Id, Hash, Size, (SELECT COUNT(*) FROM files WHERE ContentObjectId=Id) AS ReferenceCount
    FROM content_objects;