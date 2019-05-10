FROM  hallac/root_base:latest
LABEL maintainer "Whitney Armstrong <warmstrong@anl.gov>"
#

RUN   ls -lrth /usr/local/lib/lib*.so \
  && export PYTHONPATH=/usr/local/lib:$PYTHONPATH  \
  && export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH  \
  && export PATH=/usr/local/bin:$PATH  \
  && source /usr/local/bin/thisroot.sh \
  && cd /tmp  \
  && git clone https://eicweb.phy.anl.gov/jlab/hallac_evio.git \
  && mkdir hallac_evio/build && cd hallac_evio/build  \
  && cmake ../.  && make -j20 && make install \
  && cd /tmp && rm -rf hallac_evio \
  && cd /tmp  \
  && export PYTHONPATH=/usr/local/lib:$PYTHONPATH  \
  && export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH  \
  && export PATH=/usr/local/bin:$PATH  \
  && git clone https://eicweb.phy.anl.gov/jlab/analyzer.git \
  && mkdir analyzer/build && cd analyzer/build   && git pull \
  && cmake ../.  && make -j20 && make install \
  && cd /tmp  && rm -rf analyzer \
  && git clone https://eicweb.phy.anl.gov/jlab/hcana.git \
  && mkdir hcana/build && cd hcana/build  \
  && git pull \
  && cmake ../.  && make -j20 && make install \ 
  && cd /tmp && rm -rf hcana 


#-DCMAKE_CXX_FLAGS=" -march=haswell -O3 -mfma -malign-data=cacheline -finline-functions "
#&& wget -O- https://root.cern.ch/download/root_v6.14.06.source.tar.gz | tar -zxvf - \
#&& mv root-6.14.06 root_master \
#RUN which c++ && ls -lrth /usr/bin/c++ && cd /tmp/builds/root_build && make -j38 VERBOSE=1 && make install \
#      && cd /tmp && rm -rf /tmp/root_master && rm -rf /tmp/builds/root_build 

#RUN useradd -ms /bin/bash -d /opt/user user
#USER user
#WORKDIR /opt/bubble_user

##CMD ["-c" ]
#ENTRYPOINT ["/bin/bash"]

