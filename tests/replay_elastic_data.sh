#!/bin/bash

echo "This is the elastic testing..."
echo " "
echo "There are currently 0 tests to run!"
which hcana

ls -lrth
ls -lrth build
mkdir -p ROOTfiles
git clone https://eicweb.phy.anl.gov/whit/ci_test_data.git 
git clone https://eicweb.phy.anl.gov/jlab/hallc/exp/CSV/hallc_replay_csv.git
git clone https://eicweb.phy.anl.gov/jlab/hallc/exp/CSV/online_csv.git

cd online_csv 
mkdir -p logs
ln -s ../hallc_replay_csv/PARAM
ln -s ../hallc_replay_csv/DBASE
ln -s ../hallc_replay_csv/CALIBRATION
ln -s ../hallc_replay_csv/DEF-files
ln -s ../hallc_replay_csv/MAPS
ln -s ../hallc_replay_csv/SCRIPTS
ln -s ../hallc_replay_csv/DATFILES
ln -s ../ci_test_data/raw
ln -s ../ROOTfiles
# and the reset

ls -lrth
ls -lrth raw/
ls -lrth ROOTfiles/
pwd
# run replay script
df -h

singularity exec ../build/Singularity.hcana.simg hcana -b -q "SCRIPTS/COIN/PRODUCTION/replay_production_coin_hElec_pProt.C+(6012,-1)"

echo "hcana calls... the coin replay script and outputs blah.root"

