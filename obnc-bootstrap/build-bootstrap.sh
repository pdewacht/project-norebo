#!/bin/bash
set -e

rm -f ../Bootstrap/*
wget -nc http://miasap.se/obnc/downloads/obnc_0.16.1.tar.gz
tar xfvz obnc_0.16.1.tar.gz
cd obnc-0.16.1
./build --prefix="$(pwd)/local" --c-int-type=int
./install
mkdir stage1 stage2
cp ../../Norebo/CoreLinker.Mod stage1
cp ../../Oberon/{Texts,ORS,ORB,ORG,ORP}.Mod stage1
cp ../../Oberon/*.Mod stage2
cp ../../Norebo/*.Mod stage2
dos2unix stage1/*.Mod stage2/*.Mod
sed s/LONGINT/INTEGER/g -i stage1/*.Mod
cp ../*.Mod stage1
patch -d stage1 <../stage1.patch
patch -d stage2 <../stage2.patch
cd stage1
../local/bin/obnc ORP.Mod
../local/bin/obnc CoreLinker.Mod
cd ../stage2
echo "Norebo.Mod/s Kernel.Mod/s FileDir.Mod/s Files.Mod/s Modules.Mod/s">CommandLine
../stage1/ORP
echo "Fonts.Mod/s Texts.Mod/s RS232.Mod/s Oberon.Mod/s">CommandLine
../stage1/ORP
echo "CoreLinker.Mod/s">CommandLine
../stage1/ORP
echo "ORS.Mod/s ORB.Mod/s">CommandLine
../stage1/ORP
echo "ORG.Mod/s">CommandLine
../stage1/ORP
echo "ORP.Mod/s">CommandLine
../stage1/ORP
for i in Norebo Kernel FileDir Files Modules; do cp $i.rsc $i.rsx; done
echo "Modules InnerCore">CommandLine
../stage1/CoreLinker
cp InnerCore {CoreLinker,Fonts,Texts,RS232,Oberon,ORS,ORB,ORG,ORP}.rsc ../../../Bootstrap
cd ../..
echo Done.
