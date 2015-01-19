CREATE TABLE chunks (ContentObjectId INTEGER,
    Path VARCHAR NOT NULL,
    Size INTEGER NOT NULL);
CREATE TABLE files (SourcePath VARCHAR NOT NULL,
    TargetPath VARCHAR NOT NULL,
    FeatureId INTEGER NOT NULL,
    SourcePackageId INTEGER);

CREATE INDEX chunks_content_object_id_idx ON chunks (ContentObjectId ASC);
