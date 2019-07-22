#ifndef ABC_VERIFY_H
#define ABC_VERIFY_H


#include <bits/stdc++.h>
#include "abcApi.h"


void Abc_NtkCecSat(abc::Abc_Ntk_t * pNtk1, abc::Abc_Ntk_t * pNtk2, int nConfLimit, int nInsLimit);
void Abc_NtkVerifyReportError(abc::Abc_Ntk_t * pNtk1, abc::Abc_Ntk_t * pNtk2, int * pModel);
abc::Abc_Ntk_t * Abc_NtkMulti(abc::Abc_Ntk_t * pNtk, int nThresh, int nFaninMax, int fCnf, int fMulti, int fSimple, int fFactor );
void Abc_NtkMultiInt(abc::Abc_Ntk_t * pNtk, abc::Abc_Ntk_t * pNtkNew);
void Abc_NtkMultiSetBoundsCnf(abc::Abc_Ntk_t * pNtk);
void Abc_NtkMultiSetBoundsMulti(abc::Abc_Ntk_t * pNtk, int nThresh);
void Abc_NtkMultiSetBounds(abc::Abc_Ntk_t * pNtk, int nThresh, int nFaninMax);
void Abc_NtkMultiSetBoundsSimple(abc::Abc_Ntk_t * pNtk);
void Abc_NtkMultiSetBoundsFactor(abc::Abc_Ntk_t * pNtk);
abc::Abc_Obj_t * Abc_NtkMulti_rec(abc::Abc_Ntk_t * pNtkNew, abc::Abc_Obj_t * pNodeOld);
void Abc_NtkMultiCone(abc::Abc_Obj_t * pNode, abc::Vec_Ptr_t * vCone);
int Abc_NtkMultiLimit(abc::Abc_Obj_t * pNode, abc::Vec_Ptr_t * vCone, int nFaninMax);
int Abc_NtkMultiLimit_rec(abc::Abc_Obj_t * pNode, abc::Vec_Ptr_t * vCone, int nFaninMax, int fCanStop, int fFirst);
void Abc_NtkMultiCone_rec(abc::Abc_Obj_t * pNode, abc::Vec_Ptr_t * vCone);
abc::DdNode * Abc_NtkMultiDeriveBdd(abc::DdManager * dd, abc::Abc_Obj_t * pNodeOld, abc::Vec_Ptr_t * vFaninsOld);
abc::DdNode * Abc_NtkMultiDeriveBdd_rec(abc::DdManager * dd, abc::Abc_Obj_t * pNode, abc::Vec_Ptr_t * vFanins);
int Abc_NtkMiterSat_Test(abc::Abc_Ntk_t * pNtk, abc::ABC_INT64_T nConfLimit, abc::ABC_INT64_T nInsLimit, int fVerbose, abc::ABC_INT64_T * pNumConfs, abc::ABC_INT64_T * pNumInspects);
abc::Vec_Int_t * Abc_NtkGetCiSatVarNums(abc::Abc_Ntk_t * pNtk);
void * Abc_NtkMiterSatCreate_Test(abc::Abc_Ntk_t * pNtk, int fAllPrimes);
abc::sat_solver * Abc_NtkMiterSatCreateLogic_Test(abc::Abc_Ntk_t * pNtk, int fAllPrimes);
int Abc_NtkMiterSatCreateInt(abc::sat_solver * pSat, abc::Abc_Ntk_t * pNtk);
int Abc_NodeAddClauses(abc::sat_solver * pSat, char * pSop0, char * pSop1, abc::Abc_Obj_t * pNode, abc::Vec_Int_t * vVars);
int Abc_NodeAddClausesTop(abc::sat_solver * pSat, abc::Abc_Obj_t * pNode, abc::Vec_Int_t * vVars);
int Abc_NtkClauseTriv(abc::sat_solver * pSat, abc::Abc_Obj_t * pNode, abc::Vec_Int_t * vVars);
int Abc_NtkClauseTop(abc::sat_solver * pSat, abc::Vec_Ptr_t * vNodes, abc::Vec_Int_t * vVars);
int Abc_NtkClauseMux(abc::sat_solver * pSat, abc::Abc_Obj_t * pNode, abc::Abc_Obj_t * pNodeC, abc::Abc_Obj_t * pNodeT, abc::Abc_Obj_t * pNodeE, abc::Vec_Int_t * vVars);
void Abc_NtkCollectSupergate(abc::Abc_Obj_t * pNode, int fStopAtMux, abc::Vec_Ptr_t * vNodes);
int Abc_NtkClauseAnd(abc::sat_solver * pSat, abc::Abc_Obj_t * pNode, abc::Vec_Ptr_t * vSuper, abc::Vec_Int_t * vVars);
int Abc_NtkCollectSupergate_rec(abc::Abc_Obj_t * pNode, abc::Vec_Ptr_t * vSuper, int fFirst, int fStopAtMux);
void Abc_NodeBddToCnf_Test(abc::Abc_Obj_t * pNode, abc::Mem_Flex_t * pMmMan, abc::Vec_Str_t * vCube, int fAllPrimes, char ** ppSop0, char ** ppSop1);
char * Abc_ConvertBddToSop(abc::Mem_Flex_t * pMan, abc::DdManager * dd, abc::DdNode * bFuncOn, abc::DdNode * bFuncOnDc, int nFanins, int fAllPrimes, abc::Vec_Str_t * vCube, int fMode);
int Abc_ConvertZddToSop(abc::DdManager * dd, abc::DdNode * zCover, char * pSop, int nFanins, abc::Vec_Str_t * vCube, int fPhase);
void Abc_ConvertZddToSop_rec(abc::DdManager * dd, abc::DdNode * zCover, char * pSop, int nFanins, abc::Vec_Str_t * vCube, int fPhase, int * pnCubes);
int Abc_CountZddCubes(abc::DdManager * dd, abc::DdNode * zCover);
void Abc_CountZddCubes_rec(abc::DdManager * dd, abc::DdNode * zCover, int * pnCubes);
abc::DdNode * Abc_ConvertSopToBdd(abc::DdManager * dd, char * pSop, abc::DdNode ** pbVars);
int sat_solver_addclause_Test2(abc::sat_solver* s, abc::lit* begin, abc::lit* end);
int sat_solver_solve_Test(abc::sat_solver* s, abc::lit* begin, abc::lit* end, abc::ABC_INT64_T nConfLimit, abc::ABC_INT64_T nInsLimit, abc::ABC_INT64_T nConfLimitGlobal, abc::ABC_INT64_T nInsLimitGlobal);
int sat_solver_solve_internal_Test(abc::sat_solver* s);
double luby(double y, int x);
int sat_solver_propagate(abc::sat_solver* s);


#endif
