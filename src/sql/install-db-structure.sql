CREATE TABLE content_objects (Id INTEGER PRIMARY KEY NOT NULL,
    Hash VARCHAR NOT NULL UNIQUE,
    Size INTEGER NOT NULL);
CREATE TABLE features (Id INTEGER PRIMARY KEY NOT NULL,
    Name VARCHAR NOT NULL);
CREATE TABLE files (Path TEXT PRIMARY KEY NOT NULL,
    ContentObjectId INTEGER NOT NULL,
    FeatureId INTEGER NOT NULL);
CREATE TABLE source_packages (Id INTEGER PRIMARY KEY NOT NULL,
    Name VARCHAR NOT NULL UNIQUE);
CREATE TABLE storage_mapping (ContentObjectId INTEGER NOT NULL,
    SourcePackageId INTEGER NOT NULL);
CREATE INDEX files_feature_id_idx ON files (FeatureId ASC);
CREATE INDEX storage_mapping_content_object_id_idx ON storage_mapping (ContentObjectId ASC);
