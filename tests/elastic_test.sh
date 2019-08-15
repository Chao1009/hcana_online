#!/bin/bash

echo "This is the elastic testing..."
echo " "
echo "There are currently 0 tests to run!"
which hcana

ls -lrth
ls -lrth build

singularity help build/Singularity.hcana.simg

singularity exec build/Singularity.hcana.simg which hcana

singularity exec build/Singularity.hcana.simg hcana tests/my_root_script.cxx
