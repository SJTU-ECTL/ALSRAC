#ifndef CKT_WIN_DC_H
#define CKT_WIN_DC_H


#include <bits/stdc++.h>
#include <boost/random.hpp>
#include "abcApi.h"
#include "debug_assert.hpp"
#include "cktNtk.h"
#include "cktVisual.h"
#include "cktBlif.h"


int                                                 Ckt_WinMfsTest                  (abc::Abc_Ntk_t * pNtk, int nWinTfiLevs);
int                                                 Ckt_WinMfsResub                 (abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode, int nWinTfiLevs);
void                                                Ckt_WinSetMfsPars               (abc::Mfs_Par_t * pPars);
abc::Aig_Man_t *                                    Ckt_WinConstructAppAig          (abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode, abc::Vec_Ptr_t * vWinPIs, std::shared_ptr <Ckt_Ntk_t> pCktNtk);
abc::Aig_Obj_t *                                    Ckt_WinConstructAppAig_rec      (abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode, abc::Aig_Man_t * pMan, abc::Vec_Ptr_t * vWinPIs, std::shared_ptr <Ckt_Ntk_t> pCktNtk);
void                                                Ckt_WinMfsConvertHopToAig       (abc::Abc_Obj_t * pObjOld, abc::Aig_Man_t * pMan);
void                                                Ckt_WinMfsConvertHopToAig_rec   (abc::Hop_Obj_t * pObj, abc::Aig_Man_t * pMan);
abc::Vec_Ptr_t *                                    Ckt_WinNtkNodeSupport           (abc::Abc_Ntk_t * pNtk, abc::Abc_Obj_t ** ppNodes, int nNodes, int minLevel);
void                                                Ckt_WinNtkNodeSupport_rec       (abc::Abc_Obj_t * pNode, abc::Vec_Ptr_t * vNodes, int minLevel);
abc::Vec_Ptr_t *                                    Ckt_WinNtkDfsNodes              (abc::Abc_Ntk_t * pNtk, abc::Abc_Obj_t ** ppNodes, int nNodes, int minLevel);
void                                                Ckt_WinNtkDfs_rec               (abc::Abc_Obj_t * pNode, abc::Vec_Ptr_t * vNodes, int minLevel);


#endif
