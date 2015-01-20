#!/usr/bin/env python3
from lxml import etree as ET
import os
import sys
import uuid

srcData = {
	'Linux_3_18' : 'linux-3.18',
	'Linux_3_18_1' : 'linux-3.18.1',
	'Linux_3_18_2' : 'linux-3.18.2',
	'Linux_3_18_3' : 'linux-3.18.3'
}

root = ET.Element ('Installer')
product = ET.SubElement (root, 'Product')
product.set ('Name', 'Linux-3.18')
product.set ('Version', '3.18')
product.set ('Id', str(uuid.uuid4()).upper ())

features = ET.SubElement (product, 'Features')
sourcePackages = ET.SubElement (product, 'SourcePackages')

for key, value in sorted (srcData.items ()):
	feature = ET.SubElement (features, 'Feature')
	feature.set ('Id', 'Feature_{}'.format (key))
	fileGroupRef = ET.SubElement (feature, 'FileGroupReference')
	fileGroupRef.set ('Id', 'Files_{}'.format (key))

	dataRoot = os.path.join (os.getcwd (), 'data', value)
	fileGroup = ET.SubElement (product, 'FileGroup')
	fileGroup.set ('Id', 'Files_{}'.format (key))

	for directory, _, entry in os.walk (dataRoot):
		directory = directory [len (os.path.join (os.getcwd (), 'data')) + 1:]
		if entry:
			for e in entry:
				fileElement = ET.SubElement (fileGroup, 'File')
				fileElement.set ('Source', os.path.join (directory, e))

sys.stdout.write (ET.tostring (root, pretty_print=True).decode ('utf-8'))
