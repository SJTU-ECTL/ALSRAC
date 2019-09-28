#include "cktBlif.h"


using namespace std;
using namespace abc;


void Ckt_WriteBlif(Abc_Ntk_t * pAbcNtk, const string & fileName)
{
    Abc_Ntk_t * pNtkTemp = Abc_NtkDup(pAbcNtk);
    pNtkTemp = Abc_NtkToNetlist(pNtkTemp);
    Io_WriteBlif(pNtkTemp, const_cast<char *>(fileName.c_str()), 1, 1, 1);
    Abc_NtkDelete(pNtkTemp);
}
