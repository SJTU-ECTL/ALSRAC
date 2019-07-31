#ifndef APP_MFS_H
#define APP_MFS_H


#include <bits/stdc++.h>
#include "abcApi.h"

namespace abc{
struct Int_Man_t_
{
    // clauses of the problems
    Sto_Man_t *     pCnf;         // the set of CNF clauses for A and B
    int             pGloVars[16]; // global variables
    int             nGloVars;     // the number of global variables
    // various parameters
    int             fVerbose;     // verbosiness flag
    int             fProofVerif;  // verifies the proof
    int             fProofWrite;  // writes the proof file
    int             nVarsAlloc;   // the allocated size of var arrays
    int             nClosAlloc;   // the allocated size of clause arrays
    // internal BCP
    int             nRootSize;    // the number of root level assignments
    int             nTrailSize;   // the number of assignments made
    lit *           pTrail;       // chronological order of assignments (size nVars)
    lit *           pAssigns;     // assignments by variable (size nVars)
    char *          pSeens;       // temporary mark (size nVars)
    Sto_Cls_t **    pReasons;     // reasons for each assignment (size nVars)
    Sto_Cls_t **    pWatches;     // watched clauses for each literal (size 2*nVars)
    // interpolation data
    int             nVarsAB;      // the number of global variables
    int *           pVarTypes;    // variable type (size nVars) [1=A, 0=B, <0=AB]
    unsigned *      pInters;      // storage for interpolants as truth tables (size nClauses)
    int             nIntersAlloc; // the allocated size of truth table array
    int             nWords;       // the number of words in the truth table
    // proof recording
    int             Counter;      // counter of resolved clauses
    int *           pProofNums;   // the proof numbers for each clause (size nClauses)
    FILE *          pFile;        // the file for proof recording
    // internal verification
    lit *           pResLits;     // the literals of the resolvent
    int             nResLits;     // the number of literals of the resolvent
    int             nResLitsAlloc;// the number of literals of the resolvent
    // runtime stats
    abctime         timeBcp;      // the runtime for BCP
    abctime         timeTrace;    // the runtime of trace construction
    abctime         timeTotal;    // the total runtime of interpolation
};
}


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
abc::Hop_Obj_t * Abc_NtkMfsInterplate_Test(abc::Mfs_Man_t * p, int * pCands, int nCands);
abc::Hop_Obj_t * Kit_GraphToHop(abc::Hop_Man_t * pMan, abc::Kit_Graph_t * pGraph);
abc::Hop_Obj_t * Kit_GraphToHopInternal(abc::Hop_Man_t * pMan, abc::Kit_Graph_t * pGraph);
int Int_ManInterpolate_Test(abc::Int_Man_t * p, abc::Sto_Man_t * pCnf, int fVerbose, unsigned ** ppResult);
void Int_ManResize(abc::Int_Man_t * p);
void Int_ManPrepareInter(abc::Int_Man_t * p);
void Int_ManProofWriteOne(abc::Int_Man_t * p, abc::Sto_Cls_t * pClause );
int Int_ManGlobalVars(abc::Int_Man_t * p);
int Int_ManProcessRoots(abc::Int_Man_t * p);
int Int_ManProofRecordOne(abc::Int_Man_t * p, abc::Sto_Cls_t * pClause);
abc::Sto_Cls_t * Int_ManPropagate(abc::Int_Man_t * p, int Start);
int Int_ManProofTraceOne(abc::Int_Man_t * p, abc::Sto_Cls_t * pConflict, abc::Sto_Cls_t * pFinal);
void Int_ManPrintResolvent(abc::lit * pResLits, int nResLits);
void Int_ManPrintClause(abc::Int_Man_t * p, abc::Sto_Cls_t * pClause);


#endif
