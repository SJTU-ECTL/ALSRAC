sat_solver * Abc_MfsCreateSolverResub(Mfs_Man_t * p, int * pCands, int nCands, int fInvert)
{
    // get the literal for the output F
    // Why?
    pObjPo = Aig_ManCo(p->pAigWin, Aig_ManCoNum(p->pAigWin) - Vec_PtrSize(p->vDivs) - 1);
    Lits[0] = toLitCond(p->pCnf->pVarNums[pObjPo->Id], fInvert);
    // collect the outputs of the divisors
    // Why?
    Vec_PtrForEachEntryStart(Aig_Obj_t *, p->pAigWin->vCos, pObjPo, i, Aig_ManCoNum(p->pAigWin) - Vec_PtrSize(p->vDivs))
        Vec_IntPush( p->vProjVarsCnf, p->pCnf->pVarNums[pObjPo->Id]);
    // start the solver
    // Why?
    pSat = sat_solver_new();
    sat_solver_setnvars(pSat, 2 * p->pCnf->nVars + Vec_PtrSize(p->vDivs));
    // load the first copy of the clauses
    // Why?
    for (i = 0; i < p->pCnf->nClauses; i++)
        sat_solver_addclause(pSat, p->pCnf->pClauses[i], p->pCnf->pClauses[i+1]);
    sat_solver_addclause(pSat, Lits, Lits+1);
    // transform the literals
    // Why?
    for (i = 0; i < p->pCnf->nLiterals; i++)
        p->pCnf->pClauses[0][i] += 2 * p->pCnf->nVars;
    // load the second copy of the clauses
    // Why?
    for (i = 0; i < p->pCnf->nClauses; i++)
        sat_solver_addclause(pSat, p->pCnt->pClauses[i], p->pCnf->pClauses[i+1]);
    // transform the literals
    // Why?
    for (i = 0; i < p->pCnf->nLiterals; i++)
        p->pCnf->pClauses[0][i] -= 2 * p->pCnf->nVars;
    // add the clause for the second output of F
    // Why?
    Lits[0] = 2 * p->pCnf->nVars + lit_neg(Lits[0]);
    sat_solver_addclause(pSat, Lits, Lits+1);
    // add the clauses for the EXOR gates - and remember their outputs
    Vec_IntClear(p->vProjVarsSat);
    Vec_IntForEachEntry(p->vProjVarsCnf, iVar, i) {
        Abc_MfsSatAddXor(pSat, iVar, iVar + p->pCnf->nVars, 2 * p->pCnf->nVars + i);
        Vec_IntPush(p->ProjVarsSat, 2 * p->pCnf->nVars + i);
    }
    // simplify the solver
    sat_solver_simplify(pSat);
    return pSat;
}
