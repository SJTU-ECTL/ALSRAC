int Abc_NtkMfsResub(Mfs_Man_t * p, Abc_Obj_t * pNode)
{
    // compute window roots, window support, and window nodes
    p->vRoots = Abc_MfsComputeRoots( pNode, p->pPars->nWinTfoLevs, p->pPars->nFanoutsMax );
    p->vSupp  = Abc_NtkNodeSupport( p->pNtk, (Abc_Obj_t **)Vec_PtrArray(p->vRoots), Vec_PtrSize(p->vRoots) );
    p->vNodes = Abc_NtkDfsNodes( p->pNtk, (Abc_Obj_t **)Vec_PtrArray(p->vRoots), Vec_PtrSize(p->vRoots) );
    // compute the divisors of the window
    p->vDivs = Abc_MfsComputeDivisors(p, pNode, Abc_ObjRequiredLevel(pNode) - 1);
    // construct AIG for the window, a miter in figure 4.2.1
    p->pAigWin = Abc_NtkConstructAig(p, pNode);
    // translate it into CNF
    p->pCnf = Cnf_DeriveSimple(p->pAigWin, 1+ Vec_PtrSize(p->vDivs));
    // create the SAT problem
    p->pSat = Abc_MfsCreateSovlverResub(p, NULL, 0, 0);
    // solve the SAT problem
    Abc_NtkMfsResubNode(p, pNode);
}
