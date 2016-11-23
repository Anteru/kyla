PRAGMA foreign_keys = ON;

-- All content objects stored in this file repository
CREATE TABLE content_objects (
	Id INTEGER PRIMARY KEY NOT NULL,
	Hash BLOB NOT NULL UNIQUE,
	Size INTEGER NOT NULL);

-- All file sets stored in this repository
CREATE TABLE file_sets (
	Id INTEGER PRIMARY KEY NOT NULL,
	Uuid BLOB NOT NULL UNIQUE);

CREATE TABLE files (
	Path TEXT PRIMARY KEY NOT NULL,
	ContentObjectId INTEGER NOT NULL,
	FileSetId INTEGER NOT NULL,
	FOREIGN KEY(ContentObjectId) REFERENCES content_objects(Id),
	FOREIGN KEY(FileSetId) REFERENCES file_sets(Id));

CREATE TABLE source_packages (
	Id INTEGER PRIMARY KEY NOT NULL,
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
	-- IV, Salt, etc. - this is algorithm-dependent
	Data BLOB NOT NULL UNIQUE,
	-- Size before and after the compression
	InputSize INTEGER NOT NULL,
	OutputSize INTEGER NOT NULL,
	FOREIGN KEY(StorageMappingId) REFERENCES storage_mapping(Id)
);

-- If populated, this table stores the encryption data for chunks
CREATE TABLE storage_compression (
	StorageMappingId INTEGER PRIMARY KEY NOT NULL,
	Algorithm VARCHAR NOT NULL,
	-- Size before and after the compression
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

INSERT INTO properties (Name, Value) 
VALUES ('database_version', 1);

CREATE INDEX files_file_set_id_idx ON files (FileSetId ASC);
CREATE INDEX storage_mapping_content_object_id_idx ON storage_mapping (ContentObjectId ASC);
CREATE INDEX content_object_hash_idx ON content_objects (Hash ASC);
CREATE INDEX files_path_idx ON files (Path ASC);

CREATE VIEW content_objects_with_reference_count AS 
	SELECT Id, Hash, Size, (SELECT COUNT(*) FROM files WHERE ContentObjectId=Id) AS ReferenceCount
	FROM content_objects;

CREATE VIEW storage_data_view AS 
	SELECT 
		source_packages.Id AS SourcePackageId,
		storage_mapping.PackageOffset AS PackageOffset, 
		storage_mapping.PackageSize AS PackageSize,
		storage_mapping.SourceOffset AS SourceOffset, 
		content_objects.Hash AS ContentHash,
		content_objects.Size as TotalSize,
		storage_mapping.SourceSize AS SourceSize,
		storage_mapping.Id AS StorageMappingId,
		storage_compression.Algorithm AS CompressionAlgorithm,
		storage_compression.InputSize AS CompressionInputSize,
		storage_compression.OutputSize AS CompressionOutputSize,
		storage_encryption.Algorithm AS EncryptionAlgorithm,
		storage_encryption.Data AS EncryptionData,
		storage_encryption.InputSize AS EncryptionInputSize,
		storage_encryption.OutputSize AS EncryptionOutputSize,
		storage_hashes.Hash AS StorageHash
	FROM storage_mapping
	INNER JOIN content_objects ON storage_mapping.ContentObjectId = content_objects.Id
	INNER JOIN source_packages ON storage_mapping.SourcePackageId = source_packages.Id
	LEFT JOIN storage_hashes ON storage_hashes.StorageMappingId = storage_mapping.Id
	LEFT JOIN storage_compression ON storage_compression.StorageMappingId = storage_mapping.Id
	LEFT JOIN storage_encryption ON storage_encryption.StorageMappingId = storage_mapping.Id
	ORDER BY PackageOffset;