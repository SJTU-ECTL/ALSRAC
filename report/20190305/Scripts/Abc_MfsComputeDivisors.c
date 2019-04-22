Vec_Ptr_t * Abc_MfsComputeDivisors(Mfs_Man_t * p, Abc_Obj_t * pNode, int nLevDivMax)
{
    // mark the TFI with the current trav ID
    Abc_NtkIncrementTravId(pNode->pNtk);
    vCone = Abc_MfsWinMarkTfi(pNode);
    // count the number of PIs
    Vec_PtrForEachEntry(Abc_Obj_t *, vCone, pObj, k)
        nTrueSupp += Abc_ObjIsCi(pObj);
    // mark with the current trav ID those nodes that should not be divisors:
    // (1) the node and its TFO
    // (2) the MFFC of the node
    // Why?
    // (3) the node's fanins (these are treated as a special case)
    Abc_NtkIncrementTravId(pNode->pNtk);
    Abc_MfsWinSweepLeafTfo_rec(pNode, pLeDivMax);
    Abc_ObjForEachFanin(pNode, pObj, k)
        Abc_Mfs_NodeSetTravIdCurrent(pObj);

    // at this point the nodes are marked with two trav IDs:
    // nodes to be collected as divisors are marked with previous trav ID
    // nodes to be avoided as divisors are marked with the current trav ID

    // start collecting the divisors
    Abc_PtrForEachEntry(Abc_Obj_t *, vCone, pObj, k) {
        if (!Abc_NodeIsTravIdPrevious(pObj))
            continue;
        if (pObj->pLevel > nLevDivMax)
            continue;
        Vec_PtrPush(vDivs, pObj);
        if (Vec_PtrSize(vDivs) >= p->pPars->nWinMax)
            break;
    }

    // explore the fanouts of already collected divisors
    if (Vec_PtrSize(vDivs) < p->pPars->nWinMax)
    Vec_PtrForEachEntry( Abc_Obj_t *, vDivs, pObj, k ) {
        // consider fanouts of this node
        Abc_ObjForEachFanout(pObj, pFanout, f) {
            // stop is there are too many fanouts
            if (p->pPars->nFanoutsMax && f > p->pPars->nFanoutsMax)
                break;
            // skip nodes that are already added
            if (Abc_NodeIsTravIdPrevious(pFanout))
                continue;
            // skip nodes in the TFO or in the MFFC of node
            // Why?
            if (Abc_NodeIsTravIdCurrent(pFanout))
                continue;
            // skip COs
            if (!Abc_ObjIsNode(pFanout))
                continue;
            // skip nodes with large level
            if (pFanout->Level > nLevDivMax)
                continue;
            // skip nodes whose fanins are not divisors
            // Why?
            Abc_ObjForEachFanin(pFanout, pFanin, m)
                if (!Abc_NodeIsTravIdPrevious(pFanout))
                    break;
            if (m < Abc_ObjFaninNum(pFanout))
                continue;
            // add the node to the divisors
            Vec_PtrPush(vDivs, pFanout);
            Vec_PtrPushUnique(p->vNodes, pFanout);
            if (Vec_PtrSize(vDivs) >= p->pPars->nWinMax)
                break;
        }
    }

    // sort the divisors by level in the increasing order
    Vec_PtrSort(vDivs, Abc_NodeCompareLevellsIncrease);

    // add the fanins of the node
    Abc_ObjForEachFanin(pNode, pFanin, k)
        Vec_PtrPush(vDivs, pFanin);

    return vDivs;
}
