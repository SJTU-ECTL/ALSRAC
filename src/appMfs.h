#ifndef APP_MFS_H
#define APP_MFS_H


#include <bits/stdc++.h>
#include "abcApi.h"
#include "debug_assert.hpp"
#include "cktNtk.h"


int App_CommandMfs(abc::Abc_Ntk_t * pNtk);
void App_NtkMfsParsDefault(abc::Mfs_Par_t * pPars);
int App_NtkMfs(abc::Abc_Ntk_t * pNtk, abc::Mfs_Par_t * pPars);
int App_NtkMfsResub(abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode);
abc::Aig_Man_t * Abc_NtkToDar(abc::Abc_Ntk_t * pNtk, int fExors, int fRegisters);
int App_NtkMfsResubNode(abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode);
int App_NtkMfsSolveSatResub(abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode, int iFanin, int fOnlyRemove, int fSkipUpdate);
int Abc_NtkMfsTryResubOnce(abc::Mfs_Man_t * p, int * pCands, int nCands);
void Abc_NtkMfsUpdateNetwork(abc::Mfs_Man_t * p, abc::Abc_Obj_t * pObj, abc::Vec_Ptr_t * vMfsFanins, abc::Hop_Obj_t * pFunc);
abc::Vec_Ptr_t * App_FindLocalInput(abc::Abc_Obj_t * pNode, int nMax);
abc::Aig_Man_t * App_NtkConstructAig(abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode);
abc::Aig_Obj_t * Abc_NtkConstructAig_rec(abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode, abc::Aig_Man_t * pMan);
void Abc_MfsConvertHopToAig(abc::Abc_Obj_t * pObjOld, abc::Aig_Man_t * pMan);
void Abc_MfsConvertHopToAig_rec(abc::Hop_Obj_t * pObj, abc::Aig_Man_t * pMan);


#endif
