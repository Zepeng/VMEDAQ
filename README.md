VMEDAQ V0.1
============

Applications to read and support DAQ using VME and CAEN V1718 bus controller

Dependencies:
a) CAENComm-1.2
b) CAENDigitizer_2.15.0
c) CAENUSBdrvB-1.5.3
d) CAENVMELib-2.50

Usage:
Edit the modules base addresses in modules_config.h 

Compile:
make all

# Main DAQ application
./acquire -p <#events to average the rate> -n <number of total event> -f <output_raw_file> 

Modules debugged in this version are V1718, V1742, DT5751
