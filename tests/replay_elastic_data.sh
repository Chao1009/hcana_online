#!/bin/bash

echo "This is the elastic testing..."
echo " "
echo "There are currently 0 tests to run!"
which hcana

ls -lrth
ls -lrth build
mkdir -p ROOTfiles
git clone git@eicweb.phy.anl.gov:whit/ci_test_data.git 
git clone git@eicweb.phy.anl.gov:jlab/hallc/exp/CSV/hallc_replay_csv.git
git clone git@eicweb.phy.anl.gov:jlab/hallc/exp/CSV/online_csv.git

cd online_csv 
mkdir -p logs
ln -s ../hallc_reaply_csv/PARAM
ln -s ../hallc_reaply_csv/DBASE
ln -s ../hallc_reaply_csv/CALIBRATION
ln -s ../hallc_reaply_csv/DEF-files
ln -s ../hallc_reaply_csv/MAPS
ln -s ../hallc_reaply_csv/SCRIPTS
ln -s ../hallc_reaply_csv/DATFILES
ln -s ../ci_test_data/raw
ln -s ../ROOTfiles
# and the reset

# run replay script

singularity exec build/Singularity.hcana.simg hcana -b -q "scripts/replay_production_coin.cxx(6012,1)"

echo " hcana calls... the coin replay script and outputs blah.root"
