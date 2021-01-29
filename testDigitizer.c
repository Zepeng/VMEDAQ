#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h> 
#include <time.h> 

#include "modules_config.h"
#include "V1742_lib.h"

#include <fstream>
#include <iostream>
#include <vector>

int main(int argc, char** argv)
{
  int handleV1742;
  int ret = 1-  CAEN_DGTZ_OpenDigitizer(CAEN_DGTZ_USB,0,0,V1742_0_BA,&handleV1742);
  if (ret != 1)
    {
      printf("Error opening V1742... STOP!\n");
      return(1);
    }

  printf("V1742 digitizer initialization %d\n", handleV1742);
  ret*=(1-init_V1742(handleV1742));
  if (ret != 1)
    {
      printf("Error initializing V1742... STOP!\n");
      return(1);
    }
  
  int triggerMask = (1 << 1); //put to 1 bit 1 veto beam triggers;

  int i=0;
  short daq_status;
  int hm_evt_read=1;

}
