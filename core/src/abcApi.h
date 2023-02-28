#ifndef ABC_API_H
#define ABC_API_H


#include "aig/aig/aig.h"
#include "aig/saig/saig.h"
#include "aig/gia/gia.h"
#include "aig/hop/hop.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "base/cmd/cmd.h"
#include "base/io/ioAbc.h"
#include "base/abc/abc.h"
#include "bool/bdc/bdc.h"
#include "bool/kit/kit.h"
#include "misc/util/abc_global.h"
#include "misc/util/util_hack.h"
#include "map/mio/mio.h"
#include "map/mio/mioInt.h"
#include "map/mapper/mapper.h"
#include "map/mapper/mapperInt.h"
#include "map/scl/sclCon.h"
#include "opt/sim/sim.h"
#include "opt/mfs/mfs.h"
#include "opt/mfs/mfsInt.h"
#include "proof/fraig/fraig.h"
#include "proof/ssw/ssw.h"


struct Abc_ManTime_t_
{
    Abc_Time_t     tArrDef;
    Abc_Time_t     tReqDef;
    Vec_Ptr_t  *   vArrs;
    Vec_Ptr_t  *   vReqs;
    Abc_Time_t     tInDriveDef;
    Abc_Time_t     tOutLoadDef;
    Abc_Time_t *   tInDrive;
    Abc_Time_t *   tOutLoad;
};
typedef struct Abc_ManTime_t_ Abc_ManTime_t;


#endif // ABC_API_H
