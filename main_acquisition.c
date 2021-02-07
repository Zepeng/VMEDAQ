#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

//I'd like to get rid of those!!!
#include "my_vmeio.h"
#include "my_vmeint.h"

//Bridge!
#include "CAENComm.h"
#include "vme_bridge.h"

#include "time_utils.h"

#include "main_acquisition.h" 
#include "v1718_lib.h"
#include "V1742_lib.h"
#include "V1751_lib.h"
#include <fstream>
#include <iostream>
#include <vector>
#include "hdf5.h"
#include "H5public.h"

#define update_scaler 0
using namespace std;

int main(int argc, char** argv)
{

  unsigned long  max_evts=10;
  int nevent = 1;
  short daq_status, status_init;
  int i; 
  double rate;
  int32_t BHandle(0);

  int n_value = 0; int p_value = 50;
  //float b_value = 0.0;
  char* f_value = "dumb";

  ofstream myOut;

  ref_date.tm_hour = 0;
  ref_date.tm_min = 0;
  ref_date.tm_sec = 0;
  ref_date.tm_year = 114;
  ref_date.tm_mon = 4;
  ref_date.tm_mday = 1;
  ref_time=mktime(&ref_date);

  for (i = 1; i < argc; i++) {
    /* Check for a switch (leading "-"). */
    if (argv[i][0] == '-') {
      /* Use the next character to decide what to do. */
      switch (argv[i][1]) {
      case 'n': n_value = atoi(argv[++i]);
	break;
      case 'p':p_value = atoi(argv[++i]);
	break;
      case 'f':f_value = argv[++i];
	break;
      }
    }
  }

  //Check options
  struct timeval tv,tv1,tv2,tv3;
  int    n_microseconds = 0, n_microseconds_dt = 0, elapsed_microseconds_dt=0, delta_micro_seconds=0; 


  printf("n_evts(max) = %d\n", n_value);
  if (f_value != NULL) printf("file = \"%s\"\n", f_value);
  if(n_value) max_evts = n_value;


  int handleV1742;

  /* Bridge VME initialization */
  int ret = 1-CAEN_DGTZ_OpenDigitizer(CAEN_DGTZ_USB,0,0,V1742_0_BA,&handleV1742);
      
  //hack to get VME Handle (normally this handle is 0, can be also hardcoded...)
  CAEN_DGTZ_BoardInfo_t myBoardInfo;
  ret *= 1-CAEN_DGTZ_GetInfo(handleV1742, &myBoardInfo);  
  ret *= 1-CAENComm_Info(myBoardInfo.CommHandle, CAENComm_VMELIB_handle ,&BHandle);
  if (ret != 1)
  {
	  printf("VME Initialization error ... STOP!\n");
	  return(1);
  }
  printf("Opened V1742 and initialized VME crate\n");
  // Modules initialization 
  status_init=1;

  printf("V1742 digitizer initialization\n");
  status_init *= (1-init_V1742(handleV1742));
  if (status_init != 1)
  {
      printf("Error initializing V1742... STOP!\n");
      return(1);
  }

  // connect to V1751 
  int dt5751; int err = CAEN_DGTZ_Success;
  err = CAEN_DGTZ_OpenDigitizer(CAEN_DGTZ_OpticalLink, 0, 0, 0, &dt5751);
  if (err) err = CAEN_DGTZ_OpenDigitizer(CAEN_DGTZ_USB, 0, 0, V1751_0_BA, &dt5751);
  status_init *=(1-init_V1751(dt5751));
  vector<V1751_Event_t> my_dig5751_OD;
  
  printf("================================================\nVME and modules initialization completed\n\nStart data acquisition\n================================================\n");
  

  /* Output file initialization  */
  int start_hea, end_hea;
  int headWords;

  vector<int> myOE;
  vector<int> my_adc_OD;
  vector<uint32_t> my_scal_OD, my_scal_WD, tmpscaD;
  vector<V1742_Event_t> my_dig1742_OD;
  vector<int> my_header_OD;
  vector<float> my_V1742_Event;
  vector<float> my_V1751_Event;

  myOut.open(f_value,ios::out);

  
  int in_evt_read = 1; 
  int hm_evt_read; 

  //Clear of header info
  my_header_OD.clear();

  /*  Start counting: check that the scaler's channels are empty */
  status_init *= reset_nim_scaler_1718(BHandle) ;
  if (status_init != 1) 
    {
      printf("Error resetting the V1718... STOP!\n");
      return(1);
    }

  //Get the start time!
  gettimeofday(&tv1, NULL);
  tv2=tv1;

  sleep(2);
  printf("Start of the event collection cycle\n");
  /* Start of the event collection cycle */
  while(nevent<(int)max_evts)
    {
      daq_status = 1;
      hm_evt_read = in_evt_read;

      //nasty... 
      if(nevent<in_evt_read) {hm_evt_read = 1; }

      /* Attach a TIMESPAMP to the event */
      gettimeofday(&tv, NULL);
      n_microseconds = timevaldiff(&tv1,&tv);
      n_microseconds_dt = elapsed_microseconds_dt;

      my_header_OD.push_back(n_microseconds);
      my_header_OD.push_back(n_microseconds_dt);
      my_header_OD.push_back(gettimestamp(&tv)); //timestamp(msec wrt to reference time)

      tv1=tv; //Update last time

      headWords = my_header_OD.size();

      nevent++;

	//read the Digitizer 1742
	if(DIG1742) {
	  my_dig1742_OD.clear();
	  daq_status = 1 - read_V1742(handleV1742,hm_evt_read,my_dig1742_OD);
	  if (daq_status != 1)
	    {
	      printf("\nError reading DIGI 1742... STOP!\n");
	      return(1);
	    }
      daq_status = 1 - read_V1751(dt5751,1,my_dig5751_OD, false);

	}
	
	//HEADER with timing will be always present
	start_hea = 0;

	for(int ie=0; ie<hm_evt_read; ie++) {
	  myOE.clear();
	  
	  //Write the event in unformatted style
	  //myOE.push_back(nevent-hm_evt_read+ie+1);
	  
	  int eventSize_dig1742=0;
	  if (DIG1742)
	    {
	      if (ie<(int)my_dig1742_OD.size())
	  	{
	  	  daq_status *= 1-writeEventToOutputBuffer_V1742(&my_V1742_Event,&((my_dig1742_OD[ie]).eventInfo),&((my_dig1742_OD[ie]).event));
	  	  if (my_V1742_Event.size()>0)
	  	    {
	  	      eventSize_dig1742=my_V1742_Event.size();
	  	    }
	  	}
	      else
	  	cout<<"V1742::ERROR::DATA ARE CORRUPTED" <<endl;
	      //myOE.push_back(eventSize_dig1742);
	    }

	  
	  if(headWords/hm_evt_read != 3) { 
	    cout<<"Problem with the HEADER!! "<<endl;
	    cout<<"HEADER words in the multiple events"<<headWords<<" "<<hm_evt_read<<endl;
	  }
	  //myOE.push_back(headWords/hm_evt_read);
	  
	  //Ok, finished writing the Number of words.
	  //Now writing the events
	  
	  
	  if(DIG1742) {
	    myOE.insert(myOE.end(),my_V1742_Event.begin(),my_V1742_Event.end());
	  }

	  //ADD the header info
	  end_hea = start_hea + 3;
	  for(int idum = start_hea; idum<end_hea; idum++) {
	    //myOE.push_back( my_header_OD.at(idum) );
	  }
	  start_hea = end_hea;
	  
	  daq_status *= writeFastEvent(myOE,&myOut);
	}
	

	//Clears the HEADER after writing the EVENT
	my_header_OD.clear();
      
      /* if(daq_status!=1){ */
      /* 	printf("\nError writing the Event... STOP!\n"); */
      /* 	return(1);  */
      /* } */
    

      if((nevent-(p_value*((int)(nevent/p_value))))==0) 
	{
	  delta_micro_seconds = timevaldiff(&tv2,&tv);

	  //Printing run statistics
	  if(delta_micro_seconds) rate = ((double)p_value)/(double)delta_micro_seconds;
	  printf("_____ Event number: %d El time (s): %f Freq (Hz): %lf ______\n",
		 nevent,(float)delta_micro_seconds/1000000.,rate*1000000.);

	  //Flushing output
          fflush(stdout);

	  tv2=tv;
	}     
      

      gettimeofday(&tv3, NULL);
      elapsed_microseconds_dt = timevaldiff(&tv,&tv3); //time to readout the event

    }

  std::cout << "++++++ Final run statistics +++++++" << std::endl;
  fflush(stdout);

  if(DIG1742) {
    daq_status = 1 - stop_V1742(handleV1742);
    if (daq_status != 1)
      {
  	printf("\nError stopping DIGI 1742... STOP!\n");
  	return(1);
      }
  }

  /* Output File finalization */  
  printf("\n Closing output file!\n");
  myOut.close();

  /* VME deinitialization */
  bridge_deinit(BHandle);

  printf("\n VME and modules deinitialization completed \n\n Data acquisition stopped\n");
  fflush(stdout);

  return(0);
}


// Write FAST event (no decoding)
unsigned short writeFastEvent(vector<int> wriD, ofstream *Fouf)
{

  /*
    Event FORMAT:
    Evt Size
    Channel Mask
    Evt number
    Trigger time
    And for the non zero channels
    N. of ch words (+1, tot charge)
  */

  unsigned short status = 1;
  int size = wriD.size();
  int myD[size];
  cout<<"++++++++ New Event of size " << size <<endl;
  for(int dum=0; dum<size; dum++) {
    myD[dum] = wriD[dum];
    //cout<<dum << "\t" << myD[dum] <<endl;
    *Fouf << myD[dum] << " ";
  }
  *Fouf << "\n";

  std::string fname = "test.h5";
  hid_t file = H5Fcreate(fname.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
  //H5File file(fname, H5F_ACC_TRUNC);
  //Fouf->write((char *) &size,sizeof(int));
  //Fouf->write((char *) myD,wriD.size()*sizeof(int));
  return (status);

}
