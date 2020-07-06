# TDigiTES
Parameter parser and configure part are based on digiTes.  

This is a simple data taking program for CAEN digitizers with PSD, PHA, or Waveform firmware.  The example is shown as main.cpp and pa.cpp with TDataTaking class.  
This expects the ROOT is already installed into a computer (and also CAEN software libraries).

## main.cpp
This is a very simple example how to take data from CAEN digitizer.

## pa.cpp with TDataTaking class
This is an example using with Waveform FW.  Recording 1 ms waveform.  Be careful, the data size is easy to become more than data transfer speed of between computer and digitizer.
