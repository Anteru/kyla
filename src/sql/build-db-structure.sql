CREATE TABLE chunks (ContentObjectId INTEGER NOT NULL,
    Path VARCHAR NOT NULL);
CREATE TABLE files (SourcePath VARCHAR NOT NULL,
    TargetPath VARCHAR NOT NULL,
    FeatureId INTEGER NOT NULL,
    SourcePackageId INTEGER);
