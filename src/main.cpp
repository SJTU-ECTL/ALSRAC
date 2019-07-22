#include <bits/stdc++.h>
#include "cmdline.h"
#include "abcApi.h"
#include "appMfs.h"
#include "cktVerify.h"


using namespace std;
using namespace abc;
using namespace cmdline;


parser Cmdline_Parser(int argc, char * argv[])
{
    parser option;
    option.add <string>    ("input",   'i', "Original Circuit file",    true);
    option.add <string>    ("output",  'o', "Approximate Circuit file", false);
    option.add <string>    ("genlib",  'g', "Map libarary file",        false, "data/genlib/mcnc.genlib");
    option.add <int>       ("nFrame",  'f', "Simulation Frame number",  false, 10240, range(1, INT_MAX));
    option.add <int>       ("level",   'l', "TFO level",                false, 3,     range(0, INT_MAX));
    option.add <int>       ("nLocalPI",'m', "Local PI number",          false, 10,    range(1, INT_MAX));
    option.parse_check(argc, argv);
    return option;
}


int main(int argc, char * argv[])
{
    parser option = Cmdline_Parser(argc, argv);
    string input = option.get <string> ("input");

    Abc_Start();
    Abc_Ntk_t * pNtk = Io_Read(const_cast <char *>(input.c_str()), IO_FILE_BLIF, 1, 0);
    Abc_Ntk_t * pNtkCare = Io_Read("./data/test/care.blif", IO_FILE_BLIF, 1, 0);
    pNtk->pExcare = pNtkCare;
    Abc_CommandMfs_Test(pNtk);
    Io_Write(pNtk, "test.blif", IO_FILE_BLIF);
    Abc_NtkDelete(pNtk);

    // int nConfLimit = 10000;
    // int nInsLimit  = 0;
    // Abc_Ntk_t * pNtk1 = Io_Read("./data/test/xor1.blif", IO_FILE_BLIF, 1, 0);
    // Abc_Ntk_t * pNtk2 = Io_Read("./data/test/xor2.blif", IO_FILE_BLIF, 1, 0);
    // pNtk1 = Abc_NtkStrash(pNtk1, 0, 1, 0);
    // pNtk2 = Abc_NtkStrash(pNtk2, 0, 1, 0);
    // Abc_NtkCecSat(pNtk1, pNtk2, nConfLimit, nInsLimit);
    // Abc_NtkDelete(pNtk1);
    // Abc_NtkDelete(pNtk2);

    Abc_Stop();

    return 0;
}
