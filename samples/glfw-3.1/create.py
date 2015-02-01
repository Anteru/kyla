#!/usr/bin/env python3
from lxml import etree as ET
import os
import sys
import uuid

root = ET.Element ('Installer')
product = ET.SubElement (root, 'Product')
product.set ('Name', 'GLFW')
product.set ('Version', '3.1')
product.set ('Id', str (uuid.uuid4()).upper ())

features = ET.SubElement (product, 'Features')
sourcePackages = ET.SubElement (product, 'SourcePackages')

# For this installer, we want everything packed nicely into one file
sourcePackages.set ('Embedded', 'yes')

feature = ET.SubElement (features, 'Feature')
feature.set ('Id', 'Feature_{}'.format (uuid.uuid4().hex))

dataRoot = os.path.join (os.getcwd (), 'data')

for directory, _, entry in os.walk (dataRoot):
	directory = directory [len (os.path.join (os.getcwd (), 'data')) + 1:]
	if entry:
		for e in entry:
			fileElement = ET.SubElement (feature, 'File')
			fileElement.set ('Source', os.path.join (directory, e))

sys.stdout.write (ET.tostring (root, pretty_print=True).decode ('utf-8'))
