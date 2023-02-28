#ifndef CKT_UTIL_H
#define CKT_UTIL_H


#include "headers.h"
#include "abcApi.h"
#include "debugAssert.h"


// evaluate
void Ckt_EvalASIC(Abc_Ntk_t * pNtk, std::string fileName, double maxDelay, bool isOutput = false);
void Ckt_EvalFPGA(Abc_Ntk_t * pNtk, std::string fileName, std::string map = "strash;if -K 6 -a;");
float Ckt_GetArea(Abc_Ntk_t * pNtk);
float Ckt_GetDelay(Abc_Ntk_t * pNtk);

// timing
float Abc_GetArrivalTime(Abc_Ntk_t * pNtk);
void Abc_NtkTimePrepare(Abc_Ntk_t * pNtk);
void Abc_NodeDelayTraceArrival(Abc_Obj_t * pNode, Vec_Int_t * vSlacks);
void Abc_ManTimeExpand(Abc_ManTime_t * p, int nSize, int fProgressive);
Abc_ManTime_t * Abc_ManTimeStart(Abc_Ntk_t * pNtk);

// misc
void Ckt_NtkRename(Abc_Ntk_t * pNtk, const char * name);
Abc_Obj_t * Ckt_GetConst(Abc_Ntk_t * pNtk, bool isConst1 = true);
void Ckt_PrintNodeFunc(Abc_Obj_t * pNode);
void Ckt_PrintHopFunc(Hop_Obj_t * pHopObj, Vec_Ptr_t * vFanins);
void Ckt_WriteBlif(Abc_Ntk_t * pNtk, std::string fileName);
void Ckt_PrintSop(std::string sop);
void Ckt_PrintNodes(Vec_Ptr_t * vFanins);
void Ckt_PrintFanins(Abc_Obj_t * pObj);

#endif
