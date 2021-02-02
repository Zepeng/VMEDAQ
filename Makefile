#------------------------------------------------------------------------------ 
# Title:        Makefile for the CT acquisition
# Date:         2009                                                    
# Author:	A.Sarti
# Platform:	Linux 2.4.x
# Language:	GCC 2.95 and 3.0
#------------------------------------------------------------------------------  
# daq for the CAEN bridge v1718


OBJS = v1718_lib.o vme_bridge.o V1742_lib.o X742CorrectionRoutines.o

OBJS_CAENCOMM = X742CorrectionRoutines.o V1742_lib.o 

BINS = acquire acquire_CAENComm

INCLUDE_DIR = .

COPTS = -g -Wall -DLINUX -fPIC -I$(INCLUDE_DIR) -I/usr/include/hdf5/serial/ -lhdf5_cpp -lhdf5_serial 

all: $(BINS)

acquire: $(OBJS) main_acquisition.o 
	g++ -g -DLINUX -o acquire $(OBJS) -lncurses -lm -lCAENVME  -lCAENComm -lCAENDigitizer main_acquisition.o -lhdf5

acquire_CAENComm: $(OBJS_CAENCOMM) testDigitizer.o
	g++ -g -DLINUX -o acquire_CAENComm $(OBJS_CAENCOMM) -lncurses -lm -lCAENVME -lCAENComm -lCAENDigitizer testDigitizer.o

main_acquisition.o: main_acquisition.c main_acquisition.h
	g++ $(COPTS) -c main_acquisition.c

# main_acquisition_digitizer.o: main_acquisition_digitizer.c main_acquisition.h
# 	g++ $(COPTS) -c main_acquisition_digitizer.c

testDigitizer.o: testDigitizer.c
	g++ $(COPTS) -c testDigitizer.c

v1718_lib.o: v1718_lib.c v1718_lib.h 
	g++ $(COPTS) -c v1718_lib.c

V1742_lib.o: V1742_lib.c V1742_lib.h
	g++ $(COPTS) -c  V1742_lib.c

vme_bridge.o: vme_bridge.c $(INCLUDE_DIR)/vme_bridge.h 	
	g++ $(COPTS) -c vme_bridge.c	

X742CorrectionRoutines.o:  X742CorrectionRoutines.h  X742CorrectionRoutines.c
	g++ $(COPTS) -c  X742CorrectionRoutines.c		
clean:
	-rm -f #*#
	-rm -f *~
	-rm -f core
	-rm -f $(BINS)
	-rm -f $(OBJS)
	-rm -f $(OBJS_CAENCOMM)
