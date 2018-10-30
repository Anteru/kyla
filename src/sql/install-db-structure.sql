PRAGMA foreign_keys = ON;

CREATE TABLE features (
	Id INTEGER PRIMARY KEY NOT NULL,
	Uuid BLOB NOT NULL UNIQUE,
	ParentId INTEGER DEFAULT NULL,
	Title TEXT,
	Description TEXT,
	FOREIGN KEY(ParentId) REFERENCES features(Id)
);

CREATE INDEX features_uuid_idx ON features (Uuid ASC);

CREATE TABLE feature_dependencies (
	SourceId INTEGER NOT NULL,
	TargetId INTEGER NOT NULL,
	-- This will be typically 'requires'
	Relation TEXT NOT NULL,
	FOREIGN KEY(SourceId) REFERENCES features(Id),
	FOREIGN KEY(TargetId) REFERENCES features(Id)
);

CREATE VIEW feature_fs_contents_size AS 
	SELECT 
		features.Uuid, 
		SUM(Size) AS Size
	FROM fs_contents 
		JOIN fs_files ON fs_contents.Id=fs_files.ContentID 
		JOIN features ON fs_files.FeatureId=features.Id
	GROUP BY features.Id;

CREATE VIEW feature_dependencies_with_uuid AS
	SELECT
		source_features.Uuid AS SourceUuid,
		target_features.Uuid AS TargetUuid,
		Relation
	FROM feature_dependencies
		JOIN features
			AS source_features
			ON feature_dependencies.SourceId=source_features.Id
		JOIN features
			AS target_features
			ON feature_dependencies.TargetId=target_features.Id;

-- Store embedded resources like logos, UI info, etc.
CREATE TABLE ui_resources (
	Uuid BLOB NOT NULL UNIQUE,
	Hash BLOB NOT NULL,
	Content BLOB NOT NULL,
	CompressionAlgorithm VARCHAR,
	SourceSize INTEGER NOT NULL
);

-- FileStorage engine
-- All contents stored in this repository
CREATE TABLE fs_contents (
	Id INTEGER PRIMARY KEY NOT NULL,
	Hash BLOB NOT NULL UNIQUE,
	Size INTEGER NOT NULL);

-- Maps file contents to file paths
CREATE TABLE fs_files (
	Path TEXT PRIMARY KEY NOT NULL,
	ContentId INTEGER NOT NULL,
	FeatureId INTEGER NOT NULL,
	FOREIGN KEY(ContentId) REFERENCES fs_contents(Id),
	FOREIGN KEY(FeatureId) REFERENCES features(Id));

CREATE INDEX fs_files_feature_id_idx ON fs_files (FeatureId ASC);
CREATE INDEX fs_files_content_id_idx ON fs_files (ContentId ASC);

-- Content is stored in packages -- in a loose repository,
-- each content goes into its own package
CREATE TABLE fs_packages (
	Id INTEGER PRIMARY KEY NOT NULL,
	Filename VARCHAR NOT NULL UNIQUE);

-- Content is stored in chunks which are placed in packages
CREATE TABLE fs_chunks (
	Id INTEGER PRIMARY KEY NOT NULL,
	ContentId INTEGER,
	PackageId INTEGER,
	-- Offset inside the source package
	PackageOffset INTEGER NOT NULL,
	-- Size inside a package with compression etc.
	PackageSize INTEGER NOT NULL,
	-- Offset in the output file, in case one content object is split
	SourceOffset INTEGER NOT NULL,
	-- Source size - a chunk may be smaller than the whole content object
	SourceSize INTEGER NOT NULL,
	FOREIGN KEY(ContentId) REFERENCES fs_contents(Id),
	FOREIGN KEY(PackageId) REFERENCES fs_packages(Id),
	-- Any given content can be stored once in a package
	UNIQUE(ContentId, PackageId, SourceOffset));

CREATE INDEX fs_chunks_content_id_idx ON fs_chunks (ContentId ASC);
CREATE INDEX fs_chunks_package_id_idx ON fs_chunks (PackageId ASC);

-- This table stores the hashes of each chunk
-- If compression is enabled, this will be the hash of the
-- compressed data. Encryption will always happen afterwards
CREATE TABLE fs_chunk_hashes (
	ChunkId INTEGER PRIMARY KEY NOT NULL,
	Hash BLOB NOT NULL,
	FOREIGN KEY(ChunkId) REFERENCES fs_chunks(Id)
);

-- If populated, this table stores the encryption data for chunks
CREATE TABLE fs_chunk_encryption (
	ChunkId INTEGER PRIMARY KEY NOT NULL,
	Algorithm VARCHAR NOT NULL,
	-- IV, Salt, etc. - this is algorithm-dependent
	Data BLOB NOT NULL UNIQUE,
	-- Size before and after the compression
	InputSize INTEGER NOT NULL,
	OutputSize INTEGER NOT NULL,
	FOREIGN KEY(ChunkId) REFERENCES fs_chunks(Id)
);

-- If populated, this table stores the compression data for chunks
CREATE TABLE fs_chunk_compression (
	ChunkId INTEGER PRIMARY KEY NOT NULL,
	Algorithm VARCHAR NOT NULL,
	-- Size before and after the compression
	InputSize INTEGER NOT NULL,
	OutputSize INTEGER NOT NULL,
	FOREIGN KEY(ChunkId) REFERENCES fs_chunks(Id)
);

-- Take advantage of SQLite's dynamic types here so we don't have to store
-- whether it is an int, a blob or a string
CREATE TABLE properties (
	Name VARCHAR PRIMARY KEY,
	Value NOT NULL
);

INSERT INTO properties (Name, Value)
VALUES ('database_version', 1);

CREATE VIEW fs_contents_with_reference_count AS
	SELECT Id, Hash, Size, (SELECT COUNT(*) FROM fs_files WHERE ContentId=Id) AS ReferenceCount
	FROM fs_contents;

CREATE VIEW fs_content_view AS
	SELECT
		fs_packages.Id AS PackageId,
		fs_chunks.PackageOffset AS PackageOffset,
		fs_chunks.PackageSize AS PackageSize,
		fs_chunks.SourceOffset AS SourceOffset,
		fs_contents.Hash AS ContentHash,
		fs_contents.Size as TotalSize,
		fs_chunks.SourceSize AS SourceSize,
		fs_chunks.Id AS ChunkId,
		fs_chunk_compression.Algorithm AS CompressionAlgorithm,
		fs_chunk_compression.InputSize AS CompressionInputSize,
		fs_chunk_compression.OutputSize AS CompressionOutputSize,
		fs_chunk_encryption.Algorithm AS EncryptionAlgorithm,
		fs_chunk_encryption.Data AS EncryptionData,
		fs_chunk_encryption.InputSize AS EncryptionInputSize,
		fs_chunk_encryption.OutputSize AS EncryptionOutputSize,
		fs_chunk_hashes.Hash AS StorageHash
	FROM fs_chunks
	INNER JOIN fs_contents ON fs_chunks.ContentId = fs_contents.Id
	INNER JOIN fs_packages ON fs_chunks.PackageId = fs_packages.Id
	LEFT JOIN fs_chunk_hashes ON fs_chunk_hashes.ChunkId = fs_chunks.Id
	LEFT JOIN fs_chunk_compression ON fs_chunk_compression.ChunkId = fs_chunks.Id
	LEFT JOIN fs_chunk_encryption ON fs_chunk_encryption.ChunkId = fs_chunks.Id
	ORDER BY PackageId, PackageOffset;
