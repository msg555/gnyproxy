#! /usr/bin/python

import sys
import os 

if __name__ == '__main__':
    for dirname, dirnames, filenames in os.walk(sys.argv[1]):
        for filename in filenames:
            print '<img src=\"' + filename + '\">'


