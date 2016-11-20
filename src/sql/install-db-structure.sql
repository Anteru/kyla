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
	Name VARCHAR);

CREATE TABLE files (
	Path TEXT PRIMARY KEY NOT NULL,
	ContentObjectId INTEGER NOT NULL,
	FileSetId INTEGER NOT NULL,
	FOREIGN KEY(ContentObjectId) REFERENCES content_objects(Id),
	FOREIGN KEY(FileSetId) REFERENCES file_sets(Id));

CREATE TABLE source_packages (
	Id INTEGER PRIMARY KEY NOT NULL,
	Name VARCHAR NOT NULL UNIQUE,
	Filename VARCHAR NOT NULL UNIQUE,
	Uuid BLOB NOT NULL UNIQUE);

-- Maps one content object to one or more source packages
CREATE TABLE storage_mapping (
	Id INTEGER PRIMARY KEY NOT NULL,
	ContentObjectId INTEGER,
	SourcePackageId INTEGER,
	-- Offset inside the source package
	PackageOffset INTEGER NOT NULL,
	-- Size inside a package with compression etc.
	PackageSize INTEGER NOT NULL,
	-- Offset in the output file, in case one content object is split
	SourceOffset INTEGER NOT NULL,
	-- Source size - a chunk may be smaller than the whole content object
	SourceSize INTEGER NOT NULL,
	FOREIGN KEY(ContentObjectId) REFERENCES content_objects(Id),
	FOREIGN KEY(SourcePackageId) REFERENCES source_packages(Id));

-- If populated, this table stores the hashes of each storage mapping chunk
CREATE TABLE storage_hashes (
	StorageMappingId INTEGER PRIMARY KEY NOT NULL,
	Hash BLOB NOT NULL,
	FOREIGN KEY(StorageMappingId) REFERENCES storage_mapping(Id)
);

-- If populated, this table stores the encryption data for chunks
CREATE TABLE storage_encryption (
	StorageMappingId INTEGER PRIMARY KEY NOT NULL,
	Algorithm VARCHAR NOT NULL,
	-- IV, Salt, etc.
	Data BLOB NOT NULL UNIQUE,
	-- Encryption may add padding, so store input/output sizes
	InputSize INTEGER NOT NULL,
	OutputSize INTEGER NOT NULL,
	FOREIGN KEY(StorageMappingId) REFERENCES storage_mapping(Id)
);

-- If populated, this table stores the encryption data for chunks
CREATE TABLE storage_compression (
	StorageMappingId INTEGER PRIMARY KEY NOT NULL,
	Algorithm VARCHAR NOT NULL,
	-- Compression will always change the size
	InputSize INTEGER NOT NULL,
	OutputSize INTEGER NOT NULL,
	FOREIGN KEY(StorageMappingId) REFERENCES storage_mapping(Id)
);

-- Take advantage of SQLite's dynamic types here so we don't have to store
-- whether it is an int, a blob or a string
CREATE TABLE properties (
	Name VARCHAR PRIMARY KEY,
	Value NOT NULL
);

-- Required feature support for this package
CREATE TABLE features (
	NAME VARCHAR PRIMARY KEY
);

CREATE INDEX files_file_set_id_idx ON files (FileSetId ASC);
CREATE INDEX storage_mapping_content_object_id_idx ON storage_mapping (ContentObjectId ASC);
CREATE INDEX content_object_hash_idx ON content_objects (Hash ASC);
CREATE INDEX files_path_idx ON files (Path ASC);

CREATE VIEW content_objects_with_reference_count
	AS SELECT Id, Hash, Size, (SELECT COUNT(*) FROM files WHERE ContentObjectId=Id) AS ReferenceCount
	FROM content_objects;
