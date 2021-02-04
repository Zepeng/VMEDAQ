#include <vector>
#include <fstream>

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
