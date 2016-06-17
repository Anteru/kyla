import argparse
import subprocess
import os
import hashlib
import json
import tempfile
from collections import OrderedDict
import glob
from multiprocessing import Pool
from functools import partial
import time

class KylaRunner:
    def __init__(self, kclBinaryPath, verbose):
        self._kcl = kclBinaryPath
        self._verbose = verbose

    def BuildRepository(self, desc, targetDirectory, sourceDirectory=None):
        args = [self._kcl, 'build']

        if sourceDirectory:
            args.append ('--source-directory')
            args.append (sourceDirectory)

        args.append (desc)
        args.append (targetDirectory)

        if self._verbose:
            print ('Executing: "{}"'.format (' '.join (args)))

        try:
            result = subprocess.check_call (args)
            if self._verbose:
                print ('Result:', result)
            return result == 0
        except:
            if self._verbose:
                print ('Result:', 'ERROR')
            return False

    def Install(self, source, target, filesets=[]):
        return self._ExecuteAction ('install', source, target, filesets)

    def Configure(self, source, target, filesets=[]):
        return self._ExecuteAction ('configure', source, target, filesets)

    def Validate(self, source, target, filesets=[]):
        return self._ExecuteAction ('validate', source, target, filesets)

    def _ExecuteAction(self, action, source, target, filesets):
        args = [self._kcl, action, source, target] + filesets

        # validate doesn't handle source and filesets yet, so we need to strip
        # those
        if action == 'validate':
            args = args[0:2] + ['--summary=false', args[3]]

        if self._verbose:
            print ('Executing: "{}"'.format (' '.join (args)))

        try:
            result = subprocess.run (args,stdout=subprocess.PIPE)
            if self._verbose:
                print ('Result:', result.returncode)
            return result.returncode == 0
        except:
            if self._verbose:
                print ('Result:', 'ERROR')
            return False

class TestEnvironment:
    def __init__(self, kyla : KylaRunner, testDirectory):
        self.testDirectory = testDirectory
        self.kyla = kyla
        self.workingDirectory = os.path.abspath ('.')

class SetupGenerateRepository:
    def Execute(self, env : TestEnvironment, args):
        target = os.path.join (env.testDirectory, args ['target'])
        source = os.path.join (env.workingDirectory, 'tests', args ['source'])
        sourceDirectory = args.get ('source-directory', None)

        if sourceDirectory:
            sourceDirectory = os.path.join (env.workingDirectory, 'tests', sourceDirectory)

        return env.kyla.BuildRepository (source,
            target, sourceDirectory = sourceDirectory)

class ExecuteInstall:
    def Execute(self, env : TestEnvironment, args):
        source = os.path.join (env.testDirectory, args ['source'])
        target = os.path.join (env.testDirectory, args ['target'])
        filesets = args ['filesets']

        return env.kyla.Install (source, target, filesets)

class ExecuteConfigure:
    def Execute(self, env : TestEnvironment, args):
        source = os.path.join (env.testDirectory, args ['source'])
        target = os.path.join (env.testDirectory, args ['target'])
        filesets = args ['filesets']

        return env.kyla.Configure (source, target, filesets)

class ExecuteValidate:
    def Execute(self, env : TestEnvironment, args):
        source = os.path.join (env.testDirectory, args ['source'])
        target = os.path.join (env.testDirectory, args ['target'])
        filesets = args ['filesets']

        result = env.kyla.Validate (source, target, filesets)
        if args.get ('result', 'pass') == 'pass':
            return result
        else:
            return not result

class ZeroFile:
    def Execute (self, env : TestEnvironment, args):
        blockSize = 1 << 20 # 1 MiB sized blocks
        nullBuffer = bytes([0 for _ in range (blockSize)])

        for f in args:
            try:
                filePath = os.path.join (env.testDirectory, f)
                fileSize = os.stat (filePath).st_size
                bytesToWrite = fileSize
                with open (filePath, 'wb') as outputFile:
                    while bytesToWrite > 0:
                        # we write blockSize null bytes in one go
                        nextBlockSize = min (bytesToWrite, blockSize)
                        outputFile.write (nullBuffer [:nextBlockSize])
                        bytesToWrite -= nextBlockSize
            except:
                return False

        return True

class CheckHash:
    def Execute (self, env : TestEnvironment, args):
        for k,v in args.items ():
            try:
                contents = open (os.path.join (env.testDirectory, k), 'rb').read()
                actualHash = hashlib.sha256 (contents).digest().hex()
                if actualHash != v:
                    print ('Wrong hash', k, 'expected', v, 'actual', actualHash)
                    return False
            except:
                print ('Could not hash', k)
                return False
        return True

class CheckNotExistant:
    def Execute (self, env : TestEnvironment, args):
        for arg in args:
            if os.path.exists (os.path.join (env.testDirectory, arg)):
                return False
        return True

class CheckExistant:
    def Execute (self, env : TestEnvironment, args):
        for arg in args:
            if not os.path.exists (os.path.join (env.testDirectory, arg)):
                return False
        return True

hooks = {
    'generate-repository' : SetupGenerateRepository,
    'install' : ExecuteInstall,
    'configure' : ExecuteConfigure,
    'validate' : ExecuteValidate,
    'check-hash' : CheckHash,
    'check-not-existant' : CheckNotExistant,
    'check-existant' : CheckExistant,
    'zero-file' : ZeroFile
}

class Test:
    def __init__(self, kyla, testDescription):
        self.__kyla = kyla
        self.__test = json.load (open(testDescription, 'r'),
            object_pairs_hook=OrderedDict)

    def Execute(self):
        with tempfile.TemporaryDirectory () as tempDir:
            env = TestEnvironment (self.__kyla, tempDir)

            for phase in ['setup', 'execute', 'test']:
                for step in self.__test [phase]:
                    k = list (step.keys ()) [0]
                    v = step [k]
                    hook = hooks [k] ()
                    r = hook.Execute (env, v)

                    if r == False:
                        print (hook, 'failed')
                        return False
        return True

def check_negative(invalue):
    v = int(invalue)
    if v < 0:
         raise argparse.ArgumentTypeError("{} is not a valid - must be an integer greater than or equal 0".format (invalue))
    return v

def ExecuteTest (testFilename, kyla, verbose):
    testName = os.path.splitext (os.path.basename (testFilename))[0]
    tr = Test (KylaRunner (kyla, verbose=verbose), testFilename)
    return (testName, tr.Execute (),)

def FormatResult(r):
    return ('{} {}'.format (r[0], 'PASS' if r[1] else 'FAIL'))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Process some integers.')
    parser.add_argument ('binary', metavar='BINARY', type=str,
        help='Path to kcl binary')
    parser.add_argument ('-v', '--verbose', action='store_true',
        default=False,
        help='Enable verbose output')
    parser.add_argument ('-r', '--regex', default='*',
        help='Only execute tests matching this regex')
    parser.add_argument ('-p', '--parallel', type=check_negative,
        default=1,
        help="Run tests in parallel. Set to 0 to use as many threads as available on the machine.")

    args = parser.parse_args ()
    startTime = time.time ()

    tests = glob.glob ('tests/' + args.regex + '.json')
    results = []

    func = partial (ExecuteTest, kyla=args.binary, verbose=args.verbose)

    if args.parallel == 1:
        for i, test in enumerate (tests, 1):
            print ('{}/{}'.format (i, len (tests)), FormatResult (func (test)))
    else:
        processCount = args.parallel if args.parallel != 0 else None
        with Pool(processCount) as p:
            for i, r in enumerate(p.imap (func, tests), 1):
                print ('{}/{}'.format (i, len (tests)), FormatResult (r))

    endTime = time.time ()
    print ('Elapsed time: {0:.3} sec'.format (endTime - startTime))
