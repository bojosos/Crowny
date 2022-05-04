import os

result = "mcs -debug+ -target:library -out:CrownySharp.dll "

for (dirpath, dirname, filename) in os.walk("./Source"):
    for ff in filename:
        result += dirpath+ "/" + ff + " "

print(result)
os.system(result)

