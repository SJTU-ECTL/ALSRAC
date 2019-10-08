#ifndef CKT_UTIL_H
#define CKT_UTIL_H


#include "headers.h"
#include "abcApi.h"
#include "debugAssert.h"


// evaluate asic
void Ckt_EvalASIC(Abc_Ntk_t * pNtk, std::string fileName, double maxDelay);
void Ckt_EvalFPGA(Abc_Ntk_t * pNtk, std::string fileName);
float Ckt_GetArea(Abc_Ntk_t * pNtk);
float Ckt_GetDelay(Abc_Ntk_t * pNtk);

// timing
float Abc_GetArrivalTime(Abc_Ntk_t * pNtk);
void Abc_NtkTimePrepare(Abc_Ntk_t * pNtk);
void Abc_NodeDelayTraceArrival(Abc_Obj_t * pNode, Vec_Int_t * vSlacks);
void Abc_ManTimeExpand(Abc_ManTime_t * p, int nSize, int fProgressive);
Abc_ManTime_t * Abc_ManTimeStart(Abc_Ntk_t * pNtk);


#endif
