# [LICENSE BEGIN]
# kyla Copyright (C) 2016 Matthäus G. Chajdas
#
# This file is distributed under the BSD 2-clause license. See LICENSE for
# details.
# [LICENSE END]

try:
	from lxml import etree
except ImportError:
	import xml.etree.ElementTree as etree

import os
import uuid
import xml.dom.minidom

import pathlib

def WalkDirectory(directory, filter, recursive):
    with os.scandir (directory) as it:
        for entry in it:
            if filter is not None:
                if not filter (entry):
                    continue

            if entry.is_dir () and recursive:
                yield from WalkDirectory (entry, filter, recursive)
            elif entry.is_file ():
                yield entry

def _IdString (s):
	if isinstance (s, uuid.UUID):
		return str (s).upper ()
	else:
		return s

from enum import Enum

class RepositoryObject:
	def __init__ (self):
		self.__id = uuid.uuid4 ()
		self.__references = []

	def GetId (self):
		return self.__id

	def AddReference (self, repositoryObject):
		self.__references.append (repositoryObject.GetId ())

	def GetReferences (self):
		return self.__references

class Feature(RepositoryObject):
	def __init__ (self):
		super().__init__()
		self.__dependencies = []
	
	def AddDependency (self, otherFeature):
		assert isinstance (otherFeature, feature)
		self.__dependencies.append (feature)

	def ToXml (self):
		n = etree.Element ('Feature')
		n.set ('Id', str (self.GetId ()))

		for reference in self.GetReferences ():
			r = etree.SubElement (n, 'Reference')
			r.set ('Id', str (reference))

		for dependency in self.__dependencies:
			r = etree.SubElement (n, 'Dependency')
			r.set ('Id', str (dependency))

		return n

class FilePackage (RepositoryObject):
	def __init__ (self, name):
		super().__init__()
		self.__name = name

	def ToXml (self):
		n = etree.Element ('Package')
		n.set ('Name', self.__name)

		for reference in self.GetReferences ():
			r = etree.SubElement (n, 'Reference')
			r.set ('Id', str (reference))
		
		return n

class FileGroup(RepositoryObject):
	def __init__(self):
		super().__init__()
		self.__files = []
	
	def AddDirectory (self, sourceDirectory, outputDirectory, filter=None, recursive=True):
		for file in WalkDirectory (sourceDirectory, filter, recursive):
			p = pathlib.Path (file.path)
			self.__files.append ((
				file.path, 
				outputDirectory / p.relative_to (sourceDirectory),))

	def ToXml (self):
		n = etree.Element ('Group')
		n.set ('Id', str (self.GetId ()))

		for file in self.__files:
			f = etree.SubElement (n, 'File')
			f.set ('Source', str (file [0]))
			f.set ('Target', str (file [1]))
		
		return n

class FeatureTreeNode(RepositoryObject):
	def __init__(self, name, description = None):
		super().__init__ ()
		self.__children = []
		self.__name = name
		self.__description = description

	def AddNode (self, name, description=None):
		n = FeatureTreeNode (name, description)
		self.__children.append (n)
		return n

	def ToXml (self):
		n = etree.Element ('Node')
		n.set ('Name', self.__name)
		if self.__description:
			n.set ('Description', self.__description)

		for c in self.__children:
			n.append (c.ToXml ())

		for reference in self.GetReferences ():
			r = etree.SubElement (n, 'Reference')
			r.set ('Id', str (reference))

		return n


class RepositoryBuilder:
	def __init__ (self):
		self.__root = etree.Element ('Repository')
		self.__features = []
		self.__fileGroups = []
		self.__filePackages = []
		self.__featureTreeNodes = []

	def AddFeature (self):
		f = Feature ()
		self.__features.append (f)
		return f

	def AddFileGroup (self):
		g = FileGroup ()
		self.__fileGroups.append (g)
		return g

	def AddFilePackage (self, name):
		p = FilePackage (name)
		self.__filePackages.append (p)
		return p

	def AddFeatureTreeNode (self, name, description=None):
		n = FeatureTreeNode (name, description)
		self.__featureTreeNodes.append (n)
		return n

	def Finalize (self, prettyPrint = True):
		'''Generate the installer XML and return as a string. The output is
		already preprocessed.'''

		features = etree.SubElement (self.__root, 'Features')
		for feature in self.__features:
			features.append (feature.ToXml ())

		files = etree.SubElement (self.__root, 'Files')
		for fileGroup in self.__fileGroups:
			files.append (fileGroup.ToXml ())

		if self.__filePackages:
			packagesNode = etree.SubElement (files, 'Packages')
			
			for filePackage in self.__filePackages:
				packagesNode.append (filePackage.ToXml ())

		if self.__featureTreeNodes:
			uiNode = etree.SubElement (self.__root, 'UI')
			featureTreeNodeElement = etree.SubElement (uiNode, 'FeatureTree')
			for featureTreeNode in self.__featureTreeNodes:
				featureTreeNodeElement.append (featureTreeNode.ToXml ())

		decl = '<?xml version="1.0" encoding="UTF-8"?>'
		result = decl + etree.tostring (self.__root, encoding='utf-8').decode ('utf-8')

		if prettyPrint:
			d = xml.dom.minidom.parseString (result)
			return d.toprettyxml ()
		else:
			return result
