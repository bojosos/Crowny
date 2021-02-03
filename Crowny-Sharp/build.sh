find ./Source -type f -name *.cs -print0 | xargs -r0 mcs --llvm -debug+ -o- -target:library -out:Crowny.dll && mv Crowny.dll ../Crowny-Editor/Resources/Assemblies
