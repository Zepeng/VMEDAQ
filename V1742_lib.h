#ifndef _WAVEDUMP_H_
#define _WAVEDUMP_H_

#include <vector>
#include <CAENDigitizer.h>

typedef struct V1742_Event_t
{
V1742_Event_t(const CAEN_DGTZ_EventInfo_t& ei, const CAEN_DGTZ_X742_EVENT_t& e): 
  eventInfo(ei), 
    event(e) 
  {
  };
  CAEN_DGTZ_EventInfo_t eventInfo;
  CAEN_DGTZ_X742_EVENT_t event;
} V1742_Event_t;

int init_V1742();
int read_V1742(unsigned int nevents, std::vector<V1742_Event_t>& events);
int stop_V1742();

#endif 
