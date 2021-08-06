#!/bin/bash

declare -a dirs=("$HOME/Desktop/dev/Crowny/Crowny/Source" "$HOME/Desktop/dev/Crowny/Crowny-Editor/Source")

for dir in "${dirs[@]}"; do
  echo $dir
  cd $dir
  echo -n "Running dos2unix     "
  find . -name "*\.h" -o -name "*\.cpp"|xargs -I {} sh -c "dos2unix '{}' 2>/dev/null; echo -n '.'"
  echo
  echo -n "Running clang-format "
  find . -name "*\.h" -o -name "*\.cpp"|xargs -I {} sh -c "clang-format -i {}; echo -n '.'"
  echo
done
