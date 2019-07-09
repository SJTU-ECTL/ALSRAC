#ifndef CKT_FULL_SIMPLIFY_H
#define CKT_FULL_SIMPLIFY_H

#include "cktNtk.h"
#include "cktVisual.h"
#include "cmdline.h"

cmdline::parser Cmdline_Parser(int argc, char * argv[]);
void Ckt_FullSimplifyTest(int argc, char * argv[]);
void Ckt_ComputeRoot(std::shared_ptr <Ckt_Obj_t> pCktObj, std::vector < std::shared_ptr <Ckt_Obj_t> > & vRoots, int nTfoLevel = 2, int nMaxFO = 20);
void Ckt_ComputeRoot_Rec(std::shared_ptr <Ckt_Obj_t> pCktObj, std::vector < std::shared_ptr <Ckt_Obj_t> > & vRoots, int nTfoLevel, int nMaxFO);
bool Ckt_CheckRoot(std::shared_ptr <Ckt_Obj_t> pCktObj, int nTfoLevel, int nMaxFO);
void Ckt_ComputeSupport(std::vector < std::shared_ptr <Ckt_Obj_t> > & vRoots, std::vector < std::shared_ptr <Ckt_Obj_t> > & vSupp, int nLocalPI);
void Ckt_CollectNodes(std::vector < std::shared_ptr <Ckt_Obj_t> > & vRoots, std::vector < std::shared_ptr <Ckt_Obj_t> > & vSupp, std::vector < std::shared_ptr <Ckt_Obj_t> > & vNodes);
void Ckt_CollectNodes_Rec(std::shared_ptr <Ckt_Obj_t> pCktObj, std::set < std::shared_ptr <Ckt_Obj_t> > & vSuppSet, std::vector < std::shared_ptr <Ckt_Obj_t> > & vNodes);
void Ckt_GenerateNtk(std::vector < std::shared_ptr <Ckt_Obj_t> > & vRoots, std::vector < std::shared_ptr <Ckt_Obj_t> > & vSupp, std::vector < std::shared_ptr <Ckt_Obj_t> > & vNodes, std::string fileName);
void Ckt_SimplifyNtk(std::string fileName);
int Ckt_CollectMffc(std::vector < std::shared_ptr <Ckt_Obj_t> > & vNodes, std::vector < std::shared_ptr <Ckt_Obj_t> > & vMffc);
void Ckt_CollectMffc_Rec(std::shared_ptr <Ckt_Obj_t> pCktObj, std::vector < std::shared_ptr <Ckt_Obj_t> > & vMffc, bool isTop);
int Ckt_ReadNewNtk(std::string fileName);
void Ckt_ReplaceNtk(abc::Abc_Ntk_t * pOldNtk, std::string fileName);


#endif
