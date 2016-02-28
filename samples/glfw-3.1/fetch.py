import urllib.request
import zipfile

if __name__=='__main__':
    with urllib.request.urlopen ('https://github.com/glfw/glfw/releases/download/3.1.2/glfw-3.1.2.zip') as dl, open ('glfw-3.1.2.zip', 'wb') as output:
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

    with zipfile.ZipFile ('glfw-3.1.2.zip', 'r') as z:
        z.extractall ('.')
