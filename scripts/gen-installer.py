#!/usr/bin/env python3
import uuid
import random
import xml.etree.ElementTree as ET
import sys

def GenerateFiles (fileCount = 100000, featureCount=64, packageCount=32):
	packages = ['Package_{}'.format (uuid.uuid4().hex) for i in range (packageCount)]
	features = ['Feature_{}'.format (uuid.uuid4().hex) for i in range (featureCount)]

	filesElements = []
	packageElements = []
	featureElements = []

	for package in packages:
		packageElement = ET.Element ('SourcePackage')
		packageElement.set ('Id', package)
		packageElements.append (packageElement)

	for feature in features:
		featureElement = ET.Element ('Feature')
		featureElement.set ('Id', feature)
		featureElements.append (featureElement)

	for featureIndex in range (featureCount):
		# Assign this feature randomly to a package
		package = packages [random.randrange (packageCount)]
		feature = features [featureIndex]

		filesElement = ET.Element ('Files')
		filesElement.set ('Feature', feature)
		filesElement.set ('SourcePackage', package)

		for fileIndex in range (fileCount // featureCount):
			filename = 'File_{}'.format (uuid.uuid4().hex)
			fileElement = ET.SubElement (filesElement, 'File')
			fileElement.set ('Source', filename)

		filesElements.append (filesElement)

	return (filesElements, featureElements, packageElements)

if __name__=='__main__':
	root = ET.Element ('Nim')
	product = ET.SubElement (root, 'Product')

	filesElements, featureElements, packageElements = GenerateFiles ()

	for filesElement in filesElements:
		product.append (filesElement)

	sourcePackagesElement = ET.SubElement (product, 'SourcePackages')
	for sourcePackageElement in packageElements:
		sourcePackagesElement.append (sourcePackageElement)
	featuresElement = ET.SubElement (product, 'Features')
	for featureElement in featureElements:
		featuresElement.append (featureElement)

	doc = ET.ElementTree (root)
	doc.write (sys.stdout, encoding='unicode')
