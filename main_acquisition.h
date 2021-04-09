#include <vector>
#include <fstream>
#include "H5Cpp.h"

/* GENERAL SETTING  */
#define V1718 1
#define DIG1742 1
#define DIG5751 1
#define debug 1

/* 1 modulo attivo*/

#define HEADER_SIZE 7
#define EVT_SIZE HEADER_SIZE
short TOTAL_value[EVT_SIZE];

unsigned short writeFastEvent(std::vector<int> wriD, std::ofstream *Fouf);
unsigned short writeFastEvent(int event_id, std::vector<int> v1751, std::vector<int> v1742, H5::H5File* h5file);
