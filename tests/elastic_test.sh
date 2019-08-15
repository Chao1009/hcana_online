#!/bin/bash

#echo "This is the elastic testing..."
#echo " "
#echo "There are currently 0 tests to run!"
#which hcana
#
#ls -lrth
#ls -lrth build
#
#git clone git@eicweb.phy.anl.gov:jlab/hallc/exp/CSV/hallc_replay_csv.git
#git clone git@eicweb.phy.anl.gov:jlab/hallc/exp/CSV/online_csv.git
#cd online_csv 
#ln -s ../hallc_reaply_csv/PARAM
## and the reset
#
#mkdir raw 
#pushd raw
#  wget coin.dat
#popd


singularity help build/Singularity.hcana.simg

singularity exec build/Singularity.hcana.simg which hcana

singularity exec build/Singularity.hcana.simg hcana tests/my_root_script.cxx
