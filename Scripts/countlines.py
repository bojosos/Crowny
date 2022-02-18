#!/usr/bin/python3

from os import walk, path, chdir
from os.path import isfile, join
import sys

# Set cwd to the script directory
ap = path.abspath(__file__)
chdir(path.dirname(ap))

dirs = ["../Crowny/Source", "../Crowny/Resources/Shaders", "../Crowny-Editor/Source", "../Crowny-Sharp/Source", "../Crowny-Editor/Resources/Shaders", "../Crowny-Sandbox/Source"]
files = ["countlines.py", "../README.md", "../premake5.lua", "genprojects.sh", "genprojects.bat", "../Crowny/Dependencies/freetype-gl/premake5.lua", "../Crowny/Dependencies/freetype2/premake5.lua", "../Crowny/Dependencies/glfw/premake5.lua", "../Crowny/Dependencies/glad/premake5.lua", "../.gitignore"]

linecount = 0
filecount = 0
charcount = 0

fcc = 0
largestfcc = 0
largestfln = 0
largestfn = ""

class File:
    def __init__(self, name, charcount, linecount):
        self.name = name
        self.charcount = charcount
        self.linecount = linecount

    def __str__(self):
        return "File %s, characters %d, lines %d" % (self.name, self.charcount, self.linecount)

readfiles = []

for dir in dirs:
    dirlines = 0
    dirchars = 0
    for (dirpath, dirname, filename) in walk(dir):
        for fff in filename:
            if fff.endswith(".spv"):
              continue
            fcc = 0
            path = join(dirpath, fff)
            f = open(path, "r")
            print(fff)
            flines = f.readlines()
            if "-a" in sys.argv:
                print("%s : %d" % (path, flines))
            filecount += 1
            linecount += len(flines)
            dirlines += len(flines)
            for l in flines:
                dirchars += len(l)
                fcc += len(l)
                charcount += len(l)
            if largestfcc < fcc:
                largestfcc = fcc
                largestfn = fff
                largestfln = len(flines)
            readfiles.append(File(str(path), fcc, len(flines)))

    print("Directory: %s : %d, %d" % (dir, dirlines, dirchars))

for file in files:
    flines = open(file, "r").readlines()
    filecount += 1
    linecount += len(flines)
    filechars = 0
    fcc = 0
    for l in flines:
        charcount += len(l)
        filechars += len(l)
        fcc += len(l)
    if largestfcc < fcc:
        largestfcc = fcc
        largestfn = file
        largestfln = len(flines)
    readfiles.append(File(file, fcc, len(flines)))

    print("%s : %d, %d" % (file, len(flines), filechars))

def short_info():
    print("%d lines in %d files, with an average of %f lines per file with a total character length of %d" % (linecount, filecount, linecount / filecount, charcount))
    print("Largest file %s, with %d lines and %d characters" % (largestfn, largestfln, largestfcc))
def long_info():
    import operator
    readfiles.sort(key=operator.attrgetter('charcount'))
    for f in readfiles:
        print(f)
    short_info()
long_info()

from datetime import datetime
now = datetime.now()

if not '--nowrite' in sys.argv:
    out = open("count.log", "a")
    out.write(now.strftime("%d/%m/%Y %H:%M:%S") + "\n")
    out.write("%d lines, %d files, %f avgl, %d characters\n" % (linecount, filecount, linecount / filecount, charcount))
    out.write("%s, %d lines, %d characters\n" % (largestfn, largestfln, largestfcc))
    out.close()
