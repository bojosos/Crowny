import os

for f in ["./../Crowny/Source/", "./../Crowny-Editor/Source"]:
  for path, dirname, filename in os.walk(f):
    for fff in filename:
      if fff.endswith(".h"):
        header = open(path + '/' + fff, 'r').read()
        if not header.startswith("#pragma once"):
          print("Header does not have #pragma once: " + path + '/' + fff)
