find ./Source -type f -name *.cs -print0 | xargs -r0 mcs -debug+ -o- -lib:../Crowny-Editor/Resources/Assemblies -reference:Crowny.dll -target:library -out:Client.dll && mv Client.dll ../Crowny-Editor/Resources/Assemblies && mv Client.dll.mdb ../Crowny-Editor/Resources/Assemblies
