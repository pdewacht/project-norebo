#!/bin/bash
set -e
make

ROOT="$PWD"

if [ -e build1 ] || [ -e build2 ] || [ -e build3 ]; then
  echo >&2 "Build directories already exist, delete them using 'make clean' first."
  exit 1
fi
mkdir build1 build2 build3

function rename {
  for i in *.$1; do
    mv $i ${i%.$1}.$2
  done
}

function compile_everything {
  ../norebo ORP.Compile \
	Norebo.Mod/s \
	Kernel.Mod/s \
  	FileDir.Mod/s \
  	Files.Mod/s \
  	Modules.Mod/s \
  	Fonts.Mod/s \
  	Texts.Mod/s \
  	RS232.Mod/s \
  	Oberon.Mod/s \
  	CoreLinker.Mod/s \
  	ORS.Mod/s \
  	ORB.Mod/s \
  	ORG.Mod/s \
  	ORP.Mod/s \
  	ORTool.Mod/s
  rename rsc rsx
  ../norebo CoreLinker.LinkSerial Modules InnerCore
  rename rsx rsc
}

echo '=== Stage 1 ==='
cd build1
export NOREBO_PATH="$ROOT/Norebo:$ROOT/Oberon:$ROOT/Bootstrap"
compile_everything
rename smb smx
cd ..

echo
echo '=== Stage 2 ==='
cd build2
export NOREBO_PATH="$ROOT/Norebo:$ROOT/Oberon:$ROOT/build1"
compile_everything
rename smb smx
cd ..

echo
echo '=== Stage 3 ==='
cd build3
export NOREBO_PATH="$ROOT/Norebo:$ROOT/Oberon:$ROOT/build2"
compile_everything
cd ..

# Unhide the symbol files
cd build2
rename smx smb
cd ..

echo
echo '=== Verification === '
diff -r build2 build3 && echo 'OK: Stage 2 and Stage 3 are identical.'
