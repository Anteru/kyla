import urllib.request
import zipfile

def DownloadFile (url, filename):
    with urllib.request.urlopen (url) as dl, open (filename, 'wb') as output:
        cl = dl.getheader ('Content-Length')

        if cl:
            cl = int (cl)
            bytesRead = 0
            while bytesRead < cl:
                chunk = dl.read (65536)
                bytesRead += len (chunk)
                output.write (chunk)
                print ('Received: {}/{}'.format (bytesRead, cl), end='\r')
            print (' ' * 40, end='\r')
            print ('File downloaded')
        else:
            output.write (dl.read ())

def DownloadAndExtract (url, filename):
    DownloadFile (url, filename)
    with zipfile.ZipFile (filename, 'r') as z:
        z.extractall ('.')

if __name__=='__main__':
    DownloadAndExtract ('https://github.com/glfw/glfw/releases/download/3.1.2/glfw-3.1.2.zip', 'glfw-3.1.2.zip')
    DownloadAndExtract ('https://github.com/glfw/glfw/releases/download/3.1/glfw-3.1.zip', 'glfw-3.1.zip')
