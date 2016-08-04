import sys
import os
import shutil
sys.path.append ('../scripts')

from kyla import FileRepositoryBuilder

if __name__ == '__main__':
    sdk = FileRepositoryBuilder ()

    sdk.AddFileSet ('qt').AddFilesFromDirectory (r"F:\Libs\Qt\5.7")
    doc = sdk.Finalize ()
    open ('qt-5.7.xml', 'w').write (doc)
