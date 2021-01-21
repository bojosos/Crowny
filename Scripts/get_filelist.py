from os import chdir, path

ap = path.abspath(__file__)
chdir(path.dirname(ap))

headers = open("../Crowny/Source/Crowny.h").readlines()

res = ""

for header in headers:
    incl = header.strip()
    if incl.startswith("#pragma") or incl == "" or incl.endswith("Input.h\""):
        continue

    incl = incl.replace("#include ", "")
    incl = incl.replace('"', "")
    incl = "/mnt/c/dev/C++/Crowny/Crowny/Source/" + incl
    res += incl.replace(".h", ".cpp") + " "

print(res)
