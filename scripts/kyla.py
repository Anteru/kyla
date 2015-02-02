try:
	from lxml import etree
except ImportError:
	import xml.etree.ElementTree as etree

import os
import uuid

def _IdString (s):
	if isinstance (s, uuid.UUID):
		return str (s).upper ()
	else:
		return s

class InstallationBuilder:
	def __init__ (self, productName, productVersion, productId = uuid.uuid4 ()):
		self._productNode = etree.Element ('Product')
		self._productNode.set ('Name', productName)
		self._productNode.set ('Version', productVersion)
		self._productNode.set ('Id', _IdString (productId))
		self._features = []
		self._sourcePackagesEmbedded = False
		self._sourcePackages = []

	def SetEmbeddedSourcePackages (self, value):
		self._sourcePackagesEmbedded = value

	def AddSourcePackage (self, name, embedded=False):
		node = etree.Element ('SourcePackage')
		node.set ('Id', name)
		if embedded:
			node.set ('Embedded', 'yes')
		else:
			node.set ('Embedded', 'no')
		self._sourcePackages.append (node)

	class FeatureBuilder:
		def __init__ (self, name, id):
			self._element = etree.Element ('Feature')
			self._element.set ('Name', name)
			self._element.set ('Id', id)
			self._sourcePackage = None

		def SetSourcePackage (self, sourcePackage):
			self._sourcePackage = sourcePackage

		def AddFilesFromDirectory (self, baseDirectory):
			for directory, _, entry in os.walk (baseDirectory):
				directory = directory [len (baseDirectory) + 1:]
				if entry:
					for e in entry:
						fileElement = etree.SubElement (self._element, 'File')
						fileElement.set ('Source', os.path.join (directory, e))

		def Finalize (self):
			if self._sourcePackage:
				for f in self._element:
					f.set ('SourcePackage', self._sourcePackage)
			return self._element

	def AddFeature (self, name, id = 'Feature_{}'.format (str (uuid.uuid4 ()).upper ())):
		fb = self.FeatureBuilder (name, id)
		self._features.append (fb)
		return fb

	def Finalize (self):
		'''Generate the installer XML and return as a string. The output is
		already preprocessed.'''
		root = etree.Element ('Installer')
		root.append (self._productNode)

		featuresNode = etree.SubElement (self._productNode, 'Features')
		for fb in self._features:
			featuresNode.append (fb.Finalize ())

		sourcePackagesNode = etree.SubElement (self._productNode, 'SourcePackages')
		if self._sourcePackagesEmbedded:
			sourcePackagesNode.set ('Embedded', 'yes')
		else:
			sourcePackagesNode.set ('Embedded', 'no')
		for n in self._sourcePackages:
			sourcePackagesNode.append (n)

		root = Preprocess (root)

		decl = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
		return decl + etree.tostring (root, encoding='utf-8').decode ('utf-8')

def _PreprocessFileNodes (document):
	for node in document.findall ('.//File'):
		if not 'Target' in node.attrib:
			node.set ('Target', node.attrib ['Source'])

def Preprocess (xmlDocument, baseDirectory = os.getcwd ()):
	'''Preprocess a Kyla XML installation description and return an element
	tree corresponding to the preprocessed document. The baseDirectory is used
	to resolve includes.'''
	_PreprocessFileNodes (xmlDocument)
	return xmlDocument
