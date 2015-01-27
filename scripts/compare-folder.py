#!/usr/bin/env python3

import os
import sys
import pathlib
import hashlib
import codecs

def CompareFolders (folderA, folderB):
	def WalkFolder (folder):
		result = {}
		for dir, _, files in os.walk (str (folder)):
			for f in files:
				path = pathlib.Path (os.path.join (dir, f))
				relativePath = path.relative_to (folder)
				stat = os.stat (str (path))
				result [relativePath] = {
					'Size' : stat.st_size,
					# We only care about a short hash string, so MD5 is as good
					# as any other hash here
					'MD5' : hashlib.md5 (open (str (path), 'rb').read()).digest ()
				}
		return result

	a = WalkFolder (pathlib.Path (folderA))
	b = WalkFolder (pathlib.Path (folderB))

	ak = set (a.keys ())
	bk = set (b.keys ())

	diff = set.difference (ak, bk)

	mismatchCount = 0

	for d in diff:
		print ('File {} present in one folder but not the other'.format (d))
		mismatchCount += 1
	if diff:
		return mismatchCount

	# Walk one and lookup in the other is O(n^2), so this shouldn't be called
	# for large amounts of files
	for k, v in a.items ():
		bv = b [k]
		if v ['Size'] != bv ['Size']:
			print ('File {} has different size: {}, {}'.format (k, v ['Size'], bv ['Size']))
			mismatchCount += 1
		if v ['MD5'] != bv ['MD5']:
			print ('File {} has different hash: {}, {}'.format (k,
				codecs.encode (v ['MD5'], 'hex'),
				codecs.encode (bv ['MD5'], 'hex')))
			mismatchCount += 1
	return mismatchCount

if __name__=='__main__':
	print (CompareFolders (sys.argv [1], sys.argv [2]))
