#ifndef CKT_INTER_H
#define CKT_INTER_H


#include "bits/stdc++.h"
#include "abcApi.h"
#include "debug_assert.hpp"


int Ckt_MfsTest(abc::Abc_Ntk_t * pNtk);
int Abc_NtkMfsResub( abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode );
void Ckt_SetMfsPars( abc::Mfs_Par_t * pPars );
abc::Aig_Man_t * Ckt_ConstructAppAig( abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode );
abc::Aig_Obj_t * Abc_NtkConstructAig_rec( abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode, abc::Aig_Man_t * pMan );
void Abc_MfsConvertHopToAig( abc::Abc_Obj_t * pObjOld, abc::Aig_Man_t * pMan );
void Abc_MfsConvertHopToAig_rec( abc::Hop_Obj_t * pObj, abc::Aig_Man_t * pMan );


#endif
