#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# [LICENSE BEGIN]
# kyla Copyright (C) 2016 Matthäus G. Chajdas
#
# This file is distributed under the BSD 2-clause license. See LICENSE for
# details.
# [LICENSE END]

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
import sys
import io

def PrintOutput(result):
    if result.stdout:
        print ('Captured stdout:', result.stdout.decode ('utf-8'))
    if result.stderr:
        print ('Captured stderr:', result.stderr.decode ('utf-8'))

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

        result = subprocess.run (args,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        if self._verbose:
            print ('Result:', result.returncode)
            PrintOutput (result)
        return result.returncode == 0

    def Install(self, source, target, features=[], key=None):
        return self._ExecuteAction ('install', source, target, features, key)

    def Configure(self, source, target, features=[], key=None):
        return self._ExecuteAction ('configure', source, target, features, key)

    def Validate(self, source, target, features=[], key=None):
        return self._ExecuteAction ('validate', source, target, features, key)

    def Query (self, path, query, queryArgs = [], key=None):
        return self._ExecuteQuery (query, queryArgs, path)

    def _ExecuteAction(self, action, source, target, features, key):
        args = [self._kcl, action]
        if key:
            args += ['--key', key]

        args +=  [source, target]
        
        if action == 'validate':
            args += ['--summary=false']
        else:
            args += features

        if self._verbose:
            print ('Executing: "{}"'.format (' '.join (args)))

        try:
            result = subprocess.run (args,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            if self._verbose:
                print ('Result:', result.returncode)
                PrintOutput (result)
            return result.returncode == 0
        except:
            if self._verbose:
                print ('Result:', 'ERROR')
            return False
        
    def _ExecuteQuery(self, query, queryArgs, path, key = None):
        args = [self._kcl, query]
        
        if queryArgs:
            args += queryArgs

        args.append (path)    
        
        if self._verbose:
            print ('Executing: "{}"'.format (' '.join (args)))

        try:
            result = subprocess.run (args,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            if self._verbose:
                print ('Result:', result.returncode)
                PrintOutput (result)
            items = result.stdout.decode ('utf-8').splitlines ()
            return result.returncode == 0, set (items)
        except:
            if self._verbose:
                print ('Result:', 'ERROR')
            return False

class TestEnvironment:
    def __init__(self, kyla : KylaRunner, testDirectory):
        self.testDirectory = testDirectory
        self.kyla = kyla
        self.workingDirectory = os.path.abspath ('.')
        self._errorLog = io.StringIO()

    def LogError (self, *args):
        self._errorLog.write (' '.join (map (str, args)) + '\n')

class TestAction:
    def Execute (self, env : TestEnvironment, args):
        pass

class GenerateRepositoryAction (TestAction):
    def Execute(self, env : TestEnvironment, args):
        target = os.path.join (env.testDirectory, args ['target'])
        source = os.path.join (env.workingDirectory, 'tests', args ['source'])
        sourceDirectory = args.get ('source-directory', None)

        if sourceDirectory:
            sourceDirectory = os.path.join (env.workingDirectory, 'tests', sourceDirectory)

        return env.kyla.BuildRepository (source,
            target, sourceDirectory = sourceDirectory)

class CheckRepositoryFeaturesPresentAction (TestAction):
    def Execute(self, env : TestEnvironment, args):
        path = os.path.join (env.testDirectory, args ['path'])
        
        ok, features = env.kyla.Query (path, 'query-repository', ['features'])

        if not ok:
            return False
        else:
            return set (args ['features']) == set (features)

class CheckSubfeaturesFeaturesPresentAction (TestAction):
    def Execute(self, env : TestEnvironment, args):
        path = os.path.join (env.testDirectory, args ['path'])
        
        ok, features = env.kyla.Query (path, 'query-feature', 
            ['subfeatures', args ['id']])

        if not ok:
            return False
        else:
            return set (args ['subfeatures']) == set (features)

class InstallAction (TestAction):
    def Execute(self, env : TestEnvironment, args):
        source = os.path.join (env.testDirectory, args ['source'])
        target = os.path.join (env.testDirectory, args ['target'])
        features = args ['features']

        return env.kyla.Install (source, target, features, args.get ('key', None))

class ConfigureAction (TestAction):
    def Execute(self, env : TestEnvironment, args):
        source = os.path.join (env.testDirectory, args ['source'])
        target = os.path.join (env.testDirectory, args ['target'])
        features = args ['features']

        return env.kyla.Configure (source, target, features, args.get ('key', None))

class ValidateAction (TestAction):
    def Execute(self, env : TestEnvironment, args):
        source = os.path.join (env.testDirectory, args ['source'])
        target = os.path.join (env.testDirectory, args ['target'])
        features = args ['features']

        result = env.kyla.Validate (source, target, features, args.get ('key', None))
        if args.get ('result', 'pass') == 'pass':
            return result
        else:
            return not result

class ZeroFileAction (TestAction):
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

class CheckHashAction (TestAction):
    def Execute (self, env : TestEnvironment, args):
        for k,v in args.items ():
            try:
                contents = open (os.path.join (env.testDirectory, k), 'rb').read()
                actualHash = hashlib.sha256 (contents).digest().hex()
                if actualHash != v:
                    env.LogError ('Wrong hash', k, 'expected', v, 'actual', actualHash)
                    return False
            except:
                env.LogError ('Could not hash', k)
                return False
        return True

class CheckNotExistantAction (TestAction):
    def Execute (self, env : TestEnvironment, args):
        for arg in args:
            if os.path.exists (os.path.join (env.testDirectory, arg)):
                return False
        return True

class CheckExistantAction (TestAction):
    def Execute (self, env : TestEnvironment, args):
        for arg in args:
            if not os.path.exists (os.path.join (env.testDirectory, arg)):
                return False
        return True

class DamageFileAction (TestAction):
    def Execute (self, env : TestEnvironment, args):
        filename = os.path.join (env.testDirectory, args ['filename'])
        offset = args.get ('offset', 0)
        size = args.get ('size', os.path.getsize (filename) - offset)

        with open (filename, 'r+b') as outputFile:
            outputFile.seek (offset)
            nullBuffer = bytes([0 for _ in range (size)])
            outputFile.write (nullBuffer)
        
        return True

class TruncateFileAction (TestAction):
    def Execute (self, env : TestEnvironment, args):
        filename = os.path.join (env.testDirectory, args ['filename'])
        size = args.get ('size', -1)

        if size < 0:
            size = os.path.getsize (filename) + size
        
        os.truncate (filename, size)

        return True

actions = {
    'generate-repository' : GenerateRepositoryAction,
    'install' : InstallAction,
    'configure' : ConfigureAction,
    'validate' : ValidateAction,
    'check-hash' : CheckHashAction,
    'check-not-existant' : CheckNotExistantAction,
    'check-existant' : CheckExistantAction,
    'zero-file' : ZeroFileAction,
    'damage-file' : DamageFileAction,
    'truncate-file' : TruncateFileAction,
    'check-features-present' : CheckRepositoryFeaturesPresentAction,
    'check-subfeatures-present' : CheckSubfeaturesFeaturesPresentAction
}

class Test:
    def __init__(self, kyla, testDescription, keep):
        self.__kyla = kyla
        self.__keep = keep
        self.__test = json.load (open(testDescription, 'r'),
            object_pairs_hook=OrderedDict)

    def Execute(self):
        if self.__keep:
            tempDir = tempfile.mkdtemp()
            return self.__Execute (tempDir)
        with tempfile.TemporaryDirectory () as tempDir:
            return self.__Execute (tempDir)

    def __Execute (self, tempDir):
        env = TestEnvironment (self.__kyla, tempDir)
        for action in self.__test ['actions']:
            a = actions [action ['name']] ()
            args = action ['args']
            expectedResult = action.get ('result', 'pass')

            r = a.Execute (env, args)

            if expectedResult == 'pass' and r:
                continue
            if expectedResult == 'fail' and r == False:
                continue
            return False
        return True

def check_negative(invalue):
    v = int(invalue)
    if v < 0:
         raise argparse.ArgumentTypeError("{} is not a valid - must be an integer greater than or equal 0".format (invalue))
    return v

def ExecuteTest (testFilename, kyla, verbose, keep):
    testName = os.path.splitext (os.path.basename (testFilename))[0]
    tr = Test (KylaRunner (kyla, verbose=verbose), testFilename, keep=keep)
    try:
        return (testName, tr.Execute (),)
    except Exception as e:
        import traceback
        if verbose:
            traceback.print_exc ()
        return (testName, False,)

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
    parser.add_argument ('--keep', action='store_true', default=False,
        help='Do not cleanup directories after the test finishes')

    args = parser.parse_args ()
    startTime = time.time ()

    tests = glob.glob ('tests/' + args.regex + '.json')
    failures = 0

    func = partial (ExecuteTest, kyla=args.binary, verbose=args.verbose, keep=args.keep)

    if args.parallel == 1:
        for i, test in enumerate (tests, 1):
            testResult = func (test)
            print ('{}/{}'.format (i, len (tests)), FormatResult (testResult))

            if not testResult [1]:
                failures += 1
    else:
        processCount = args.parallel if args.parallel != 0 else None
        with Pool(processCount) as p:
            for i, r in enumerate(p.imap (func, tests), 1):
                print ('{}/{}'.format (i, len (tests)), FormatResult (r))

                if not r [1]:
                    failures += 1

    endTime = time.time ()
    print ('Elapsed time: {0:.3} sec'.format (endTime - startTime))

    sys.exit (failures)
