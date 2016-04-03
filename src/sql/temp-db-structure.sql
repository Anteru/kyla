PRAGMA foreign_keys = ON;

# This is used during configure to store the source content objects
# Notice it's non-null, as we don't care about duplicates
CREATE TABLE source_content_objects (
    Hash BLOB NOT NULL);
