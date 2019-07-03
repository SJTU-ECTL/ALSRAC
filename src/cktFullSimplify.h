#ifndef CKT_FULL_SIMPLIFY_H
#define CKT_FULL_SIMPLIFY_H

#include "cktNtk.h"
#include "cmdline.h"

cmdline::parser Cmdline_Parser(int argc, char * argv[]);
void Ckt_FullSimplifyTest(int argc, char * argv[]);
void Ckt_ComputeRoot(std::shared_ptr <Ckt_Obj_t> pCktObj, std::vector < std::shared_ptr <Ckt_Obj_t> > & vRoots, int nTfoLevel = 2);
void Ckt_ComputeRoot_Rec(std::shared_ptr <Ckt_Obj_t> pCktObj, std::vector < std::shared_ptr <Ckt_Obj_t> > & vRoots, int nTfoLevel);
bool Ckt_CheckRoot(std::shared_ptr <Ckt_Obj_t> pCktObj, int nTfoLevel);

#endif
