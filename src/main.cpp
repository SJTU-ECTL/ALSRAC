#include <bits/stdc++.h>
#include "cmdline.h"
#include "cktFullSimplify.h"

using namespace std;
using namespace abc;
using namespace cmdline;


int main(int argc, char * argv[])
{
    Ckt_FullSimplifyTest(argc, argv);
    // parser option = Cmdline_Parser(argc, argv);
    // string input = option.get <string> ("input");
    // string output = option.get <string> ("output");

    // Abc_Start();
    // Abc_Ntk_t * pNtk = Io_Read(const_cast <char *>(input.c_str()), IO_FILE_BLIF, 1, 0);
    // shared_ptr <Ckt_Ntk_t> pCktNtk = make_shared <Ckt_Ntk_t> (pNtk);
    // pCktNtk->Init(1024);
    // Abc_NtkDelete(pNtk);
    // Abc_Stop();

    return 0;
}
