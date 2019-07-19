#ifndef APP_MFS_H
#define APP_MFS_H


#include <bits/stdc++.h>
#include "abcApi.h"


void App_DontCareSimpl(abc::Abc_Ntk_t * pNtk);
int Abc_CommandMfs_Test(abc::Abc_Ntk_t * pNtk);
void Abc_NtkMfsParsDefault_Test(abc::Mfs_Par_t * pPars);
int Abc_NtkMfs_Test(abc::Abc_Ntk_t * pNtk, abc::Mfs_Par_t * pPars);
void Abc_NtkMfsPowerResub(abc::Mfs_Man_t * p, abc::Mfs_Par_t * pPars);
int Abc_NtkMfsResub(abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode);
int Abc_NtkMfsNode(abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode);
int Abc_WinNode(abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode);
int Abc_NtkMfsSolveSatResub(abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode, int iFanin, int fOnlyRemove, int fSkipUpdate);
void Abc_NtkMfsUpdateNetwork(abc::Mfs_Man_t * p, abc::Abc_Obj_t * pObj, abc::Vec_Ptr_t * vMfsFanins, abc::Hop_Obj_t * pFunc);
int Abc_NtkMfsTryResubOnce(abc::Mfs_Man_t * p, int * pCands, int nCands);
abc::Aig_Man_t * Abc_NtkToDar(abc::Abc_Ntk_t * pNtk, int fExors, int fRegisters);
abc::Vec_Int_t * Abc_NtkPowerEstimate(abc::Abc_Ntk_t * pNtk, int fProbOne);
abc::Hop_Obj_t * Abc_NodeIfNodeResyn(abc::Bdc_Man_t * p, abc::Hop_Man_t * pHop, abc::Hop_Obj_t * pRoot, int nVars, abc::Vec_Int_t * vTruth, unsigned * puCare, float dProb);
abc::Vec_Int_t * Saig_ManComputeSwitchProbs(abc::Aig_Man_t * pAig, int nFrames, int nPref, int fProbOne);
abc::Gia_Man_t * Gia_ManFromAigSwitch(abc::Aig_Man_t * p);
void Gia_ManFromAig_rec(abc::Gia_Man_t * pNew, abc::Aig_Man_t * p, abc::Aig_Obj_t * pObj);
abc::Aig_Man_t * Abc_NtkConstructAig_Test(abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode);
abc::Aig_Obj_t * Abc_NtkConstructAig_rec(abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode, abc::Aig_Man_t * pMan);
void Abc_MfsConvertHopToAig(abc::Abc_Obj_t * pObjOld, abc::Aig_Man_t * pMan);
abc::Aig_Obj_t * Abc_NtkConstructCare_rec(abc::Aig_Man_t * pCare, abc::Aig_Obj_t * pObj, abc::Aig_Man_t * pMan);
void Abc_MfsConvertHopToAig_rec(abc::Hop_Obj_t * pObj, abc::Aig_Man_t * pMan);
abc::Cnf_Dat_t * Cnf_DeriveSimple_Test(abc::Aig_Man_t * p, int nOutputs);
abc::sat_solver * Abc_MfsCreateSolverResub_Test(abc::Mfs_Man_t * p, int * pCands, int nCands, int fInvert);
int Abc_MfsSatAddXor(abc::sat_solver * pSat, int iVarA, int iVarB, int iVarC);
int sat_solver_addclause_Test(abc::sat_solver* s, abc::lit* begin, abc::lit* end);
int Abc_NtkMfsResubNode_Test(abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode);


#endif
