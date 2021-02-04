#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <CAENDigitizerType.h>
#include "DT5751_lib.h"

#define DEFAULT_CONFIG_FILE  "/home/nexo/work/DAQ/VMEDAQ/DT5751_config.txt"

typedef enum  {
  ERR_NONE= 0,
  ERR_CONF_FILE_NOT_FOUND,
  ERR_DGZ_OPEN,
  ERR_BOARD_INFO_READ,
  ERR_INVALID_BOARD_TYPE,
  ERR_DGZ_PROGRAM,
  ERR_MALLOC,
  ERR_RESTART,
  ERR_INTERRUPT,
  ERR_READOUT,
  ERR_EVENT_BUILD,
  ERR_HISTO_MALLOC,
  ERR_UNHANDLED_BOARD,
  ERR_MISMATCH_EVENTS,
  ERR_FREE_BUFFER,
  /* ERR_OUTFILE_WRITE, */

  ERR_DUMMY_LAST,
} ERROR_CODES;

static RUN_DT5751_t WDcfg;

void SaveCurrentTime()
{
  struct timeval tv;
  gettimeofday(&tv,NULL);
  WDcfg.tsec = tv.tv_sec;
  WDcfg.tus = tv.tv_usec;
}

uint32_t GetDCOffset(int ch, int adc)
{
  if (adc<0 || adc>65535) {
    printf("DC offset %d is not in [0,65535], set to 900\n",adc);
    adc=900;
  }
  // in case of 16-bit DAC input
  if (adc>1023) return adc;
  // in case of ADC input
  float value = 10000;
  if (ch==0) value = 63736.3-56.6*adc;
  if (ch==1) value = 67168.0-57.8*adc;
  if (ch==2) value = 67295.0-58.1*adc;
  if (ch==3) value = 67574.0-58.1*adc;
  if (value<0 || value>65535) {
    printf("DC offset value %.0f is out of range. Set it to 10000.\n", value);
    value=10000;
  }
  return (uint32_t) value;
}

int ParseConfigDT(FILE *f_ini) 
{
  char setting[256], option[256];
  int i, ch=-1, parameter, post;

  // default cfg
  WDcfg.ver=VERSION;
  WDcfg.ns=1001; // 7 x 143
  WDcfg.mask=0x0; // disable all channels
  WDcfg.swTrgMod=CAEN_DGTZ_TRGMODE_DISABLED;
  WDcfg.exTrgMod=CAEN_DGTZ_TRGMODE_DISABLED;
  WDcfg.chTrgMod=CAEN_DGTZ_TRGMODE_DISABLED;
  WDcfg.exTrgSrc=CAEN_DGTZ_IOLevel_TTL;
  WDcfg.post=75;
  WDcfg.polarity=0xf; // trigger on falling edge
  WDcfg.trgMask=0x0; // trigger on none of the channels
  for (i=0; i<Nch; i++) { WDcfg.thr[i]=770; WDcfg.offset[i]=1000; }

  // parse cfg file
  while (!feof(f_ini)) {
    // read a word from the file
    int read = fscanf(f_ini, "%s", setting);

    // skip empty lines
    if(read<=0 || !strlen(setting)) continue;

    // skip comments
    if(setting[0] == '#') {
      fgets(setting, 256, f_ini);
      continue;
    }

    // upper case to lower case
    for(i=0; setting[i]; i++) setting[i] = tolower(setting[i]);

    // Section (Common or individual channel)
    if (setting[0] == '[') {
      if (strstr(setting, "common")) {
	ch = -1;
	continue; 
      }
      sscanf(setting+1, "%d", &parameter);
      if (parameter < 0 || parameter >= Nch) {
	printf("%d: invalid channel number\n", parameter);
	return 1;
      } else
	ch = parameter;
      continue;
    }
    printf(" %s: ",setting);

    // number of waveform samples
    if (strstr(setting, "number_of_samples")!=NULL) {
      read = fscanf(f_ini, "%u", &WDcfg.ns);
      if (WDcfg.ns%7==0) { printf("%d\n",WDcfg.ns); continue; }
      printf("Number of samples %d is not divisible by 7,",WDcfg.ns);
      if (WDcfg.ns%7<4) WDcfg.ns=WDcfg.ns/7*7;
      else WDcfg.ns=(WDcfg.ns/7+1)*7;
      printf("rounded to %d\n", WDcfg.ns);
      continue;
    }

    // post trigger percentage after trigger
    if (strstr(setting, "post_trigger_percentage")!=NULL) {
      read = fscanf(f_ini, "%d", &post);
      WDcfg.post=post;
      printf("%d\n", WDcfg.post);
      continue;
    }

    // software trigger mode
    if (strstr(setting, "software_trigger_mode")!=NULL) {
      read = fscanf(f_ini, "%s", option);
      for(i=0; option[i]; i++) option[i] = tolower(option[i]);
      if (strcmp(option, "disabled")==0)
	WDcfg.swTrgMod = CAEN_DGTZ_TRGMODE_DISABLED;
      else if (strcmp(option, "extout_only")==0)
	WDcfg.swTrgMod = CAEN_DGTZ_TRGMODE_EXTOUT_ONLY;
      else if (strcmp(option, "acq_only")==0) {
	WDcfg.swTrgMod = CAEN_DGTZ_TRGMODE_ACQ_ONLY;
        WDcfg.trgMask |= (1 << 31);
      } else if (strcmp(option, "acq_and_extout")==0) {
	WDcfg.swTrgMod = CAEN_DGTZ_TRGMODE_ACQ_AND_EXTOUT;
        WDcfg.trgMask |= (1 << 31);
      } else {
	printf("%s: invalid trigger mode\n", option);
	return 1;
      }
      printf("%d\n", WDcfg.swTrgMod);
      continue;
    }

    // internal trigger mode 
    if (strstr(setting, "internal_trigger_mode")!=NULL) {
      read = fscanf(f_ini, "%s", option);
      for(i=0; option[i]; i++) option[i] = tolower(option[i]);
      if (strcmp(option, "disabled")==0)
	WDcfg.chTrgMod = CAEN_DGTZ_TRGMODE_DISABLED;
      else if (strcmp(option, "extout_only")==0)
	WDcfg.chTrgMod = CAEN_DGTZ_TRGMODE_EXTOUT_ONLY;
      else if (strcmp(option, "acq_only")==0)
	WDcfg.chTrgMod = CAEN_DGTZ_TRGMODE_ACQ_ONLY;
      else if (strcmp(option, "acq_and_extout")==0)
	WDcfg.chTrgMod = CAEN_DGTZ_TRGMODE_ACQ_AND_EXTOUT;
      else {
	printf("%s: invalid internal trigger mode\n", option);
	return 1;
      }
      printf("%d\n", WDcfg.chTrgMod);
      continue;
    }

    // external trigger mode
    if (strstr(setting, "external_trigger_mode")!=NULL) {
      read = fscanf(f_ini, "%s", option);
      for(i=0; option[i]; i++) option[i] = tolower(option[i]);
      if (strcmp(option, "disabled")==0)
	WDcfg.exTrgMod = CAEN_DGTZ_TRGMODE_DISABLED;
      else if (strcmp(option, "extout_only")==0)
	WDcfg.exTrgMod = CAEN_DGTZ_TRGMODE_EXTOUT_ONLY;
      else if (strcmp(option, "acq_only")==0) {
	WDcfg.exTrgMod = CAEN_DGTZ_TRGMODE_ACQ_ONLY;
        WDcfg.trgMask |= (1 << 30);
      } else if (strcmp(option, "acq_and_extout")==0) {
	WDcfg.exTrgMod = CAEN_DGTZ_TRGMODE_ACQ_AND_EXTOUT;
        WDcfg.trgMask |= (1 << 30);
      } else {
	printf("%s: invalid trigger mode\n", option);
	return 1;
      }
      printf("%d\n", WDcfg.exTrgMod);
      continue;
    }

    // external trigger source
    if (strstr(setting, "external_trigger_source")!=NULL) {
      read = fscanf(f_ini, "%s", option);
      for(i=0; option[i]; i++) option[i] = toupper(option[i]);
      if (strcmp(option, "TTL")==0)
	WDcfg.exTrgSrc=CAEN_DGTZ_IOLevel_TTL;
      else if (strcmp(option, "NIM")==0)
	WDcfg.exTrgSrc=CAEN_DGTZ_IOLevel_NIM;
      else {
	printf("%s: invalid trigger source\n", option);
	return 1;
      }
      printf("%d\n", WDcfg.exTrgSrc);
      continue;
    }

    // channel DC offset
    if (strstr(setting, "channel_dc_offset")!=NULL) {
      read = fscanf(f_ini, "%d", &parameter);
      if (ch==-1) {
	for (i=0;i<Nch;i++) {
	  WDcfg.offset[i]=GetDCOffset(i,parameter);
	  printf("%d(ch %d) ", WDcfg.offset[i], i);
	}
	printf("\n");
      } else {
	WDcfg.offset[ch] = GetDCOffset(ch,parameter);
	printf("%d(ch %d)\n", WDcfg.offset[ch], ch);
      }
      continue;
    }

    // channel threshold
    if (strstr(setting, "channel_trigger_threshold")!=NULL) {
      read = fscanf(f_ini, "%d", &parameter);
      if (ch == -1) {
	for(i=0; i<Nch; i++) {
	  WDcfg.thr[i] = parameter;
	  printf("%d(ch %d) ", WDcfg.thr[i], i);
	}
	printf("\n");
      } else {
	WDcfg.thr[ch] = parameter;
	printf("%d(ch %d)\n", WDcfg.thr[ch], ch);
      }
      continue;
    }

    // channel trigger mask (yes/no)
    if (strstr(setting, "channel_enable_trigger")!=NULL) {
      read = fscanf(f_ini, "%s", option);
      for(i=0; i<strlen(option); i++) option[i] = tolower(option[i]);
      if (strcmp(option, "yes")==0) {
	if (ch == -1) for (i=0; i<4; i++) WDcfg.trgMask |= (1 << i);
	else WDcfg.trgMask |= (1 << ch);
      } else if (strcmp(option, "no")==0) {
	if (ch == -1) for (i=0; i<4; i++) WDcfg.trgMask &= ~(1 << i);
	else WDcfg.trgMask &= ~(1 << ch);
      } else {
	printf("%s: invalid option to enable channel trigger\n", option);
      }
      for (i=32; i-->0;) {
	if ((i+1)%4==0) printf(" ");
	printf("%d",WDcfg.trgMask>>i&1);
      }
      printf("\n");
      continue;
    }

    // channel trigger polarity
    if (strstr(setting, "channel_trigger_polarity")!=NULL) {
      read = fscanf(f_ini, "%s", option);
      for(i=0; i<strlen(option); i++) option[i] = tolower(option[i]);
      if (strcmp(option, "negative")==0) {
	if (ch==-1) WDcfg.polarity = 0xf;
	else WDcfg.polarity |= (1<<ch);
      } else if (strcmp(option, "positive")==0) {
	if (ch==-1) WDcfg.polarity = 0x0;
	else WDcfg.polarity &= ~(1<<ch);
      } else {
	printf("%s: invalid trigger polarity\n", option);
	return 1;
      }
      for (i=Nch; i-->0;) printf("%d",WDcfg.polarity>>i&1); printf("\n");
      continue;
    }

    // channel recording mask (yes/no)
    if (strstr(setting, "channel_enable_recording")!=NULL) {
      read = fscanf(f_ini, "%s", option);
      for(i=0; i<strlen(option); i++) option[i] = tolower(option[i]);
      if (strcmp(option, "yes")==0) {
	if (ch == -1) WDcfg.mask = 0xf;
	else WDcfg.mask |= (1 << ch);
      } else if (strcmp(option, "no")==0) {
	if (ch == -1) WDcfg.mask = 0x0;
	else WDcfg.mask &= ~(1 << ch);
      } else {
	printf("%s: invalid option to enable channel recording\n", option);
	return 1;
      }
      for (i=Nch; i-->0;) printf("%d",WDcfg.mask>>i&1); printf("\n");
      continue;
    }

    // coincidence window
    if (strstr(setting, "coincidence_window")!=NULL) {
      read = fscanf(f_ini, "%d", &parameter);
      if (parameter < 16) WDcfg.trgMask |= (parameter << 20);
      else printf("%d: invalid coincidence window\n",parameter);
      if (parameter==0) printf("1 ns\n");
      else printf("%d ns\n",parameter*16);
      continue;
    }

    // coincidence level
    if (strstr(setting, "coincidence_level")!=NULL) {
      read = fscanf(f_ini, "%d", &parameter);
      if (parameter < 16) WDcfg.trgMask |= (parameter << 24);
      else printf("%d: invalid coincidence level\n",parameter);
      printf("more than %d channels\n",parameter);
      continue;
    }

    printf("%s: invalid setting\n", setting);
    return 1;
  }
  return 0;
}

int init_DT5751(int handle)
{
  // get board info
  CAEN_DGTZ_BoardInfo_t board;
  int err = CAEN_DGTZ_GetInfo(handle, &board);
  if (err) { printf("Can't get board info!\n"); 
      return 1; }
  printf("Initialization of DT5751\n");
  printf("Connected to %s\n", board.ModelName);
  printf("ROC FPGA Release: %s\n", board.ROC_FirmwareRel);
  printf("AMC FPGA Release: %s\n", board.AMC_FirmwareRel);
  
  // calibrate board
  err = CAEN_DGTZ_Calibrate(handle);
  if (err) { printf("Can't calibrate board!\n"); 
      return 1; }

  // load configurations
  FILE *f_ini;

  
  printf("**************************************************************\n");
  printf("                        Initialise DT5751\n");
  printf("**************************************************************\n");
  
  /* *************************************************************************************** */
  /* Open and parse configuration file                                                       */
  /* *************************************************************************************** */
  char ConfigFileName[100];
  strcpy(ConfigFileName, DEFAULT_CONFIG_FILE);
  printf("Opening Configuration File %s\n", ConfigFileName);
  f_ini = fopen(ConfigFileName, "r");

  if (f_ini == NULL ) {
    err = ERR_CONF_FILE_NOT_FOUND;
    return err;
  }

  err = ParseConfigDT(f_ini);
  fclose(f_ini);
  
  if (err) return 1;
  else printf("Configuration of DT5751 completed.\n");
  
  // global settings
  uint16_t nEvtBLT=1; // number of events for each block transfer
  err |= CAEN_DGTZ_Reset(handle);
  err |= CAEN_DGTZ_SetRecordLength(handle,WDcfg.ns);
  err |= CAEN_DGTZ_SetPostTriggerSize(handle,WDcfg.post);
  err |= CAEN_DGTZ_SetMaxNumEventsBLT(handle,nEvtBLT);
  err |= CAEN_DGTZ_SetAcquisitionMode(handle,CAEN_DGTZ_SW_CONTROLLED);
  err |= CAEN_DGTZ_SetChannelEnableMask(handle,WDcfg.mask);
  err |= CAEN_DGTZ_SetIOLevel(handle,(CAEN_DGTZ_IOLevel_t)WDcfg.exTrgSrc);
  err |= CAEN_DGTZ_SetExtTriggerInputMode(handle,(CAEN_DGTZ_TriggerMode_t)WDcfg.exTrgMod);
  err |= CAEN_DGTZ_SetSWTriggerMode(handle, (CAEN_DGTZ_TriggerMode_t)WDcfg.swTrgMod);
  // set up trigger coincidence among channels
  err |= CAEN_DGTZ_WriteRegister(handle,0x810c,WDcfg.trgMask);
  // take the right most 4 bits in WDcfg.trgMask to set trg mode
  err |= CAEN_DGTZ_SetChannelSelfTrigger(handle,(CAEN_DGTZ_TriggerMode_t)WDcfg.chTrgMod,WDcfg.trgMask & 0xf);
  // configure individual channels
  for (int ich=0; ich<Nch; ich++) {
    err |= CAEN_DGTZ_SetChannelDCOffset(handle,ich,WDcfg.offset[ich]);
    err |= CAEN_DGTZ_SetChannelTriggerThreshold(handle,ich,WDcfg.thr[ich]);
    err |= CAEN_DGTZ_SetTriggerPolarity(handle,ich,(CAEN_DGTZ_TriggerPolarity_t)(WDcfg.polarity>>ich&1));
  }
  if (err) { printf("Board configure error: %d\n", err); 
      return 1; }
  sleep(1); // wait till baseline get stable
  
  return 0;
}

int read_DT5751(int handle, unsigned int nevents, std::vector<DT5751_Event_t>& events, bool swtrigger)
{
  /* printf("Start read\n"); */
  CAEN_DGTZ_ErrorCode ret=CAEN_DGTZ_Success;
  ERROR_CODES ErrCode= ERR_NONE;


  int i;
  //, Nb=0, Ne=0;
  uint32_t BufferSize, NumEvents,Nb=0,Ne=0;

  CAEN_DGTZ_EventInfo_t       EventInfo;
  
  char *dt5751_buffer=NULL;
  char *dt5751_eventPtr;
  
  uint32_t AllocatedSize;

  CAEN_DGTZ_UINT16_EVENT_t    *Event16=NULL; /* generic event struct with 16 bit data (10, 12, 14 and 16 bit digitizers */

  
  /* Read data from the board */
  // Allocate memory for the event data and readout buffer

  ret = CAEN_DGTZ_AllocateEvent(handle, (void**)&Event16);

  if (ret != CAEN_DGTZ_Success) {
    ErrCode = ERR_MALLOC;
    return ErrCode;
  }

  /* printf("allocated event %d\n",ret); */

  ret = CAEN_DGTZ_MallocReadoutBuffer(handle, &dt5751_buffer, &AllocatedSize); /* WARNING: This malloc must be done after the digitizer programming */
  if (ret) { printf("Can't allocate memory! Quit.\n"); return ret; }
  ret = CAEN_DGTZ_AllocateEvent(handle, (void**)&Event16);
  /* printf("malloc %d %d\n",AllocatedSize,ret); */
  if (ret) {
    ErrCode = ERR_MALLOC;
    return ErrCode;
  }

  BufferSize = 0;
  NumEvents = 0;

  ret = CAEN_DGTZ_SWStartAcquisition(handle);
  if (ret)
  {
      ErrCode = ERR_INTERRUPT;
      return ErrCode;
  }
  printf("nevents:%d Number of events%d\n", nevents, NumEvents);
  // output loop
  int nEvtTot=0, nNeeded = 10, nEvtIn5sec=0; uint32_t nEvtOnBoard;
  uint32_t bsize, fsize=0;
  while (nEvtTot<nNeeded ) {
    if (WDcfg.swTrgMod!=CAEN_DGTZ_TRGMODE_DISABLED)
	if(swtrigger)
        CAEN_DGTZ_SendSWtrigger(handle);

    // read data from board
    CAEN_DGTZ_ReadMode_t mode=CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT;
    int err = CAEN_DGTZ_ReadData(handle,mode,dt5751_buffer,&bsize);
    if (err) { printf("Can't read data from board! Abort.\n"); break; }
    fsize += bsize;

    // write data to file
    err = CAEN_DGTZ_GetNumEvents(handle,dt5751_buffer,bsize,&nEvtOnBoard);
    printf("nEvtOnBoard: %d, err: %d\n", nEvtOnBoard, err); 
    if (err) { printf("nEvtOnBoard: %d, err: %d\n", nEvtOnBoard, err); break; }
    for (i=0; i<nEvtOnBoard; i++) {
      CAEN_DGTZ_EventInfo_t info; char *rawEvt = NULL;
      err = CAEN_DGTZ_GetEventInfo(handle,dt5751_buffer,bsize,i,&info,&rawEvt);
      err |= CAEN_DGTZ_DecodeEvent(handle, rawEvt, (void**)&Event16);
      events.push_back(DT5751_Event_t(EventInfo,*Event16));
      for(int ich=0; ich < Nch; ich++)
      {
          for(int ns = 0; ns < Event16->ChSize[ich]; ns++)
              printf("%d ", Event16->DataChannel[ich][ns]);
          printf("%d\n", ich);
      }
      //printf("Channel Size:%d", Event16->ChSize[ich]);
    }
  }

      
  /* //Freeing DT5751 memory  after read */
  free(dt5751_buffer);
  //  free(dt5751_eventPtr);
  delete(Event16);

  return 0;
  
}

int stop_DT5751(int handle)
{
  /* stop the acquisition */
  CAEN_DGTZ_SWStopAcquisition(handle);
  
  return 0;
}
