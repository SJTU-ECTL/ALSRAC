#ifndef CKT_SYNTHESIS_H
#define CKT_SYNTHESIS_H

#include <bits/stdc++.h>
#include "abcApi.h"
#include "cktTiming.h"
#include "debug_assert.hpp"


float Ckt_Synthesis(abc::Abc_Ntk_t * pNtk, std::string fileName);
float Ckt_GetArea(abc::Abc_Ntk_t * pNtk);


#endif
