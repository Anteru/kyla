import sys
import os
import shutil
sys.path.append (os.path.join (os.path.dirname (os.path.abspath (__file__)), '../scripts'))

from kyla import FileRepositoryBuilder

from glfw.create import GenerateGLFWSamples
import subprocess

if __name__ == '__main__':
    srcDir = sys.argv [1]
    kclBinaryPath = sys.argv [2]
    cacheDir = sys.argv [3]

    print ('Preparing directories', flush=True)
    shutil.rmtree ('staging_sdk', ignore_errors=True)
    shutil.rmtree ('sdk', ignore_errors=True)

    for folder in ['samples/glfw', 'bin', 'bin/platforms', 'include', 'python']:
        os.makedirs(os.path.join ('staging_sdk', folder))

    print ('Copying docs', flush=True)
    shutil.copytree (os.path.join (srcDir, 'doc/build/html'), 'staging_sdk/docs')
    shutil.copyfile (os.path.join (srcDir, 'src/kyla/inc/Kyla.h'), 'staging_sdk/include/Kyla.h')

    print ('Generating samples', flush=True)
    GenerateGLFWSamples ('staging_sdk/samples/glfw', cacheDir)

    print ('Generating SDK installer', flush=True)
    sdk = FileRepositoryBuilder ()

    docs = sdk.AddFileSet ('docs')
    docs.AddFilesFromDirectory ('staging_sdk/docs', prefix='docs')

    shutil.copyfile (os.path.join (srcDir, 'scripts/kyla.py'), 'staging_sdk/python/kyla.py')
    pythonBindings = sdk.AddFileSet ('python-bindings')
    pythonBindings.AddFilesFromDirectory ('staging_sdk/python', prefix='python')

    binaries = sdk.AddFileSet ('binaries')

    print ('Copying binaries', flush=True)
    binaryFiles = [
     'kcl.exe', 'kui.exe', 'kyla.dll',
     'Qt5Core.dll', 'Qt5Gui.dll', 'Qt5Widgets.dll', 'platforms/qwindows.dll'
    ]

    for binary in binaryFiles:
     shutil.copyfile (os.path.join (os.path.dirname (kclBinaryPath), binary), 'staging_sdk/bin/' + binary)

    binaries.AddFilesFromDirectory ('staging_sdk/bin', prefix='bin')
    include_files = sdk.AddFileSet ('include_files')
    include_files.AddFilesFromDirectory ('staging_sdk/include', prefix='include')
    samples = sdk.AddFileSet ('samples')
    samples.AddFilesFromDirectory ('staging_sdk/samples', prefix='samples')
    doc = sdk.Finalize ()
    open ('sdk.xml', 'w').write (doc)

    print ('Packaging installer', flush=True)
    subprocess.check_call ([kclBinaryPath, 'build',
        '--source-directory=staging_sdk', 'sdk.xml', 'sdk'])

    print ('Cleaning up', flush=True)
    shutil.rmtree ('staging_sdk', ignore_errors=True)
