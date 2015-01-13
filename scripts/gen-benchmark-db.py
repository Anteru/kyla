#!/usr/bin/env python3
import uuid
import random
import sys
import sqlite3

def GenerateFiles (fileCount = 100000, featureCount=64, packageCount=32):
	conn = sqlite3.connect ('benchmark.sqlite')

	c = conn.cursor ()
	c.execute ('''CREATE TABLE files (Path TEXT PRIMARY KEY NOT NULL UNIQUE,
									  ContentObjectId INTEGER NOT NULL,
									  FeatureId INTEGER NOT NULL)''')
	c.execute ('''CREATE TABLE content_objects (Id INTEGER PRIMARY KEY NOT NULL,
												Hash VARCHAR NOT NULL UNIQUE,
                                                Size INTEGER NOT NULL)''')
	c.execute ('''CREATE TABLE features (Id INTEGER PRIMARY KEY NOT NULL,
										Name VARCHAR NOT NULL)''')
	c.execute ('''CREATE TABLE source_packages (Id INTEGER PRIMARY KEY NOT NULL,
												Name VARCHAR NOT NULL UNIQUE)''')
	c.execute ('''CREATE TABLE storage_mapping (ContentObjectId INTEGER NOT NULL,
												SourcePackageId INTEGER NOT NULL)''')

	packages = ['Package_{}'.format (uuid.uuid4().hex) for i in range (packageCount)]
	features = ['Feature_{}'.format (uuid.uuid4().hex) for i in range (featureCount)]

	filesElements = []
	packageElements = []
	featureElements = []

	for i, package in enumerate (packages):
		c.execute ('''INSERT INTO source_packages VALUES (?, ?)''', (i, package))

	for i, feature in enumerate (features):
		c.execute ('''INSERT INTO features VALUES (?, ?)''', (i, feature))

	for featureIndex in range (featureCount):
		# Assign this feature randomly to a package
		package = random.randrange (packageCount)
		feature = featureIndex

		for fileIndex in range (fileCount // featureCount):
			filename = 'File_{}'.format (uuid.uuid4().hex)
			contentObject = 'ContentObject_{}'.format (uuid.uuid4().hex)
			c.execute ('''INSERT INTO content_objects (Hash, Size) VALUES (?, ?)''',
                (contentObject, random.randrange (2**24)))

			contentObjectId = c.lastrowid

			c.execute ('''INSERT INTO files VALUES (?, ?, ?)''',
                (filename, contentObjectId, feature))
			c.execute ('''INSERT INTO storage_mapping VALUES (?, ?)''',
				(contentObjectId, package))
	conn.commit ()
	conn.close ()

if __name__=='__main__':
	GenerateFiles ()
