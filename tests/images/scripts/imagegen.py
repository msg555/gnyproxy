
import os
import sys

if __name__ == '__main__':
    filename = sys.argv[1]
    count = int(sys.argv[2])
    for i in xrange(count):
        os.system('cp ' + filename + ' ' + filename[:-4]+ '_' +str(i) + '.bmp')

