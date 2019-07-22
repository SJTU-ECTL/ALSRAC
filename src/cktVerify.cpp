#include "cktVerify.h"


#define ABC_MAX_CUBES   100000
#define VERBOSEDEBUG
#define SAT_USE_ANALYZE_FINAL

// For derivation output (verbosity level 2)
#define L_IND    "%-*d"
#define L_ind    sat_solver_dl(s)*2+2,sat_solver_dl(s)
#define L_LIT    "%sx%d"
#define L_lit(p) lit_sign(p)?"~":"", (lit_var(p))


using namespace std;
using namespace abc;


static int nMuxes;
static const int var0  = 1;
static const int var1  = 0;
static const int varX  = 3;


static inline int     var_level     (sat_solver* s, int v)            { return s->levels[v];   }
static inline int     var_value     (sat_solver* s, int v)            { return s->assigns[v];  }
static inline int     var_polar     (sat_solver* s, int v)            { return s->polarity[v]; }

static inline void    var_set_level (sat_solver* s, int v, int lev)   { s->levels[v] = lev;    }
static inline void    var_set_value (sat_solver* s, int v, int val)   { s->assigns[v] = val;   }
static inline void    var_set_polar (sat_solver* s, int v, int pol)   { s->polarity[v] = pol;  }

static inline int      sat_solver_dl(sat_solver* s)                { return veci_size(&s->trail_lim); }
static inline veci*    sat_solver_read_wlist(sat_solver* s, lit l) { return &s->wlists[l];            }


static inline void order_assigned(sat_solver* s, int v)
{
}


// variable tags
static inline int     var_tag       (sat_solver* s, int v)            { return s->tags[v]; }
static inline void    var_set_tag   (sat_solver* s, int v, int tag)   {
    assert( tag > 0 && tag < 16 );
    if ( s->tags[v] == 0 )
        veci_push( &s->tagged, v );
    s->tags[v] = tag;
}
static inline void    var_add_tag   (sat_solver* s, int v, int tag)   {
    assert( tag > 0 && tag < 16 );
    if ( s->tags[v] == 0 )
        veci_push( &s->tagged, v );
    s->tags[v] |= tag;
}
static inline void    solver2_clear_tags(sat_solver* s, int start)    {
    int i, * tagged = veci_begin(&s->tagged);
    for (i = start; i < veci_size(&s->tagged); i++)
        s->tags[tagged[i]] = 0;
    veci_resize(&s->tagged,start);
}

static double sat_solver_progress(sat_solver* s)
{
    int     i;
    double  progress = 0;
    double  F        = 1.0 / s->size;
    for (i = 0; i < s->size; i++)
        if (var_value(s, i) != varX)
            progress += pow(F, var_level(s, i));
    return progress / s->size;
}
static inline void act_clause_rescale(sat_solver* s)
{
    if ( s->ClaActType == 0 )
    {
        unsigned* activity = (unsigned *)veci_begin(&s->act_clas);
        int i;
        for (i = 0; i < veci_size(&s->act_clas); i++)
            activity[i] >>= 14;
        s->cla_inc >>= 14;
        s->cla_inc = Abc_MaxInt( s->cla_inc, (1<<10) );
    }
    else
    {
        float* activity = (float *)veci_begin(&s->act_clas);
        int i;
        for (i = 0; i < veci_size(&s->act_clas); i++)
            activity[i] *= (float)1e-20;
        s->cla_inc *= (float)1e-20;
    }
}
static inline void act_clause_bump(sat_solver* s, clause *c)
{
    if ( s->ClaActType == 0 )
    {
        unsigned* act = (unsigned *)veci_begin(&s->act_clas) + c->lits[c->size];
        *act += s->cla_inc;
        if ( *act & 0x80000000 )
            act_clause_rescale(s);
    }
    else
    {
        float* act = (float *)veci_begin(&s->act_clas) + c->lits[c->size];
        *act += s->cla_inc;
        if (*act > 1e20)
            act_clause_rescale(s);
    }
}
static inline void act_clause_decay(sat_solver* s)
{
    if ( s->ClaActType == 0 )
        s->cla_inc += (s->cla_inc >> 10);
    else
        s->cla_inc *= s->cla_decay;
}

static inline int sat_clause_compute_lbd( sat_solver* s, clause* c )
{
    int i, lev, minl = 0, lbd = 0;
    for (i = 0; i < (int)c->size; i++)
    {
        lev = var_level(s, lit_var(c->lits[i]));
        if ( !(minl & (1 << (lev & 31))) )
        {
            minl |= 1 << (lev & 31);
            lbd++;
//            printf( "%d ", lev );
        }
    }
//    printf( " -> %d\n", lbd );
    return lbd;
}


static inline void order_update(sat_solver* s, int v) // updateorder
{
    int*    orderpos = s->orderpos;
    int*    heap     = veci_begin(&s->order);
    int     i        = orderpos[v];
    int     x        = heap[i];
    int     parent   = (i - 1) / 2;

    assert(s->orderpos[v] != -1);

    while (i != 0 && s->activity[x] > s->activity[heap[parent]]){
        heap[i]           = heap[parent];
        orderpos[heap[i]] = i;
        i                 = parent;
        parent            = (i - 1) / 2;
    }

    heap[i]     = x;
    orderpos[x] = i;
}



static inline void order_unassigned(sat_solver* s, int v) // undoorder
{
    int* orderpos = s->orderpos;
    if (orderpos[v] == -1){
        orderpos[v] = veci_size(&s->order);
        veci_push(&s->order,v);
        order_update(s,v);
//printf( "+%d ", v );
    }
}


static inline int sat_solver_enqueue(sat_solver* s, lit l, int from)
{
    int v  = lit_var(l);
    if ( s->pFreqs[v] == 0 )
//    {
        s->pFreqs[v] = 1;
//        s->nVarUsed++;
//    }

#ifdef VERBOSEDEBUG
    printf(L_IND"enqueue("L_LIT")\n", L_ind, L_lit(l));
#endif
    if (var_value(s, v) != varX)
        return var_value(s, v) == lit_sign(l);
    else{
/*
        if ( s->pCnfFunc )
        {
            if ( lit_sign(l) )
            {
                if ( (s->loads[v] & 1) == 0 )
                {
                    s->loads[v] ^= 1;
                    s->pCnfFunc( s->pCnfMan, l );
                }
            }
            else
            {
                if ( (s->loads[v] & 2) == 0 )
                {
                    s->loads[v] ^= 2;
                    s->pCnfFunc( s->pCnfMan, l );
                }
            }
        }
*/
        // New fact -- store it.
#ifdef VERBOSEDEBUG
        printf(L_IND"bind("L_LIT")\n", L_ind, L_lit(l));
#endif
        var_set_value(s, v, lit_sign(l));
        var_set_level(s, v, sat_solver_dl(s));
        s->reasons[v] = from;
        s->trail[s->qtail++] = l;
        order_assigned(s, v);
        return true;
    }
}


static inline int sat_solver_decision(sat_solver* s, lit l){
    assert(s->qtail == s->qhead);
    assert(var_value(s, lit_var(l)) == varX);
#ifdef VERBOSEDEBUG
    printf(L_IND"assume("L_LIT")  ", L_ind, L_lit(l));
    printf( "act = %.20f\n", s->activity[lit_var(l)] );
#endif
    veci_push(&s->trail_lim,s->qtail);
    return sat_solver_enqueue(s,l,0);
}


static void sat_solver_canceluntil(sat_solver* s, int level) {
    int      bound;
    int      lastLev;
    int      c;

    if (sat_solver_dl(s) <= level)
        return;

    assert( veci_size(&s->trail_lim) > 0 );
    bound   = (veci_begin(&s->trail_lim))[level];
    lastLev = (veci_begin(&s->trail_lim))[veci_size(&s->trail_lim)-1];

    ////////////////////////////////////////
    // added to cancel all assignments
//    if ( level == -1 )
//        bound = 0;
    ////////////////////////////////////////

    for (c = s->qtail-1; c >= bound; c--) {
        int     x  = lit_var(s->trail[c]);
        var_set_value(s, x, varX);
        s->reasons[x] = 0;
        if ( c < lastLev )
            var_set_polar( s, x, !lit_sign(s->trail[c]) );
    }
    //printf( "\n" );

    for (c = s->qhead-1; c >= bound; c--)
        order_unassigned(s,lit_var(s->trail[c]));

    s->qhead = s->qtail = bound;
    veci_resize(&s->trail_lim,level);
}


static inline void solver_init_activities(sat_solver* s)
{
    // variable activities
    s->VarActType             = 0;
    if ( s->VarActType == 0 )
    {
        s->var_inc            = (1 <<  5);
        s->var_decay          = -1;
    }
    else if ( s->VarActType == 1 )
    {
        s->var_inc            = Abc_Dbl2Word(1.0);
        s->var_decay          = Abc_Dbl2Word(1.0 / 0.95);
    }
    else if ( s->VarActType == 2 )
    {
        s->var_inc            = Xdbl_FromDouble(1.0);
        s->var_decay          = Xdbl_FromDouble(1.0 / 0.950);
    }
    else assert(0);

    // clause activities
    s->ClaActType             = 0;
    if ( s->ClaActType == 0 )
    {
        s->cla_inc            = (1 << 11);
        s->cla_decay          = -1;
    }
    else
    {
        s->cla_inc            = 1;
        s->cla_decay          = (float)(1 / 0.999);
    }
}


static inline void act_var_rescale(sat_solver* s)
{
    if ( s->VarActType == 0 )
    {
        word* activity = s->activity;
        int i;
        for (i = 0; i < s->size; i++)
            activity[i] >>= 19;
        s->var_inc >>= 19;
        s->var_inc = Abc_MaxInt( (unsigned)s->var_inc, (1<<4) );
    }
    else if ( s->VarActType == 1 )
    {
        double* activity = (double*)s->activity;
        int i;
        for (i = 0; i < s->size; i++)
            activity[i] *= 1e-100;
        s->var_inc = Abc_Dbl2Word( Abc_Word2Dbl(s->var_inc) * 1e-100 );
        //printf( "Rescaling var activity...\n" );
    }
    else if ( s->VarActType == 2 )
    {
        xdbl * activity = s->activity;
        int i;
        for (i = 0; i < s->size; i++)
            activity[i] = Xdbl_Div( activity[i], 200 ); // activity[i] / 2^200
        s->var_inc = Xdbl_Div( s->var_inc, 200 );
    }
    else assert(0);
}
static inline void act_var_bump(sat_solver* s, int v)
{
    if ( s->VarActType == 0 )
    {
        s->activity[v] += s->var_inc;
        if ((unsigned)s->activity[v] & 0x80000000)
            act_var_rescale(s);
        if (s->orderpos[v] != -1)
            order_update(s,v);
    }
    else if ( s->VarActType == 1 )
    {
        double act = Abc_Word2Dbl(s->activity[v]) + Abc_Word2Dbl(s->var_inc);
        s->activity[v] = Abc_Dbl2Word(act);
        if (act > 1e100)
            act_var_rescale(s);
        if (s->orderpos[v] != -1)
            order_update(s,v);
    }
    else if ( s->VarActType == 2 )
    {
        s->activity[v] = Xdbl_Add( s->activity[v], s->var_inc );
        if (s->activity[v] > ABC_CONST(0x014c924d692ca61b))
            act_var_rescale(s);
        if (s->orderpos[v] != -1)
            order_update(s,v);
    }
    else assert(0);
}
static inline void act_var_bump_global(sat_solver* s, int v)
{
    if ( !s->pGlobalVars || !s->pGlobalVars[v] )
        return;
    if ( s->VarActType == 0 )
    {
        s->activity[v] += (int)((unsigned)s->var_inc * 3);
        if (s->activity[v] & 0x80000000)
            act_var_rescale(s);
        if (s->orderpos[v] != -1)
            order_update(s,v);
    }
    else if ( s->VarActType == 1 )
    {
        double act = Abc_Word2Dbl(s->activity[v]) + Abc_Word2Dbl(s->var_inc) * 3.0;
        s->activity[v] = Abc_Dbl2Word(act);
        if ( act > 1e100)
            act_var_rescale(s);
        if (s->orderpos[v] != -1)
            order_update(s,v);
    }
    else if ( s->VarActType == 2 )
    {
        s->activity[v] = Xdbl_Add( s->activity[v], Xdbl_Mul(s->var_inc, Xdbl_FromDouble(3.0)) );
        if (s->activity[v] > ABC_CONST(0x014c924d692ca61b))
            act_var_rescale(s);
        if (s->orderpos[v] != -1)
            order_update(s,v);
    }
    else assert( 0 );
}
static inline void act_var_bump_factor(sat_solver* s, int v)
{
    if ( !s->factors )
        return;
    if ( s->VarActType == 0 )
    {
        s->activity[v] += (int)((unsigned)s->var_inc * (float)s->factors[v]);
        if (s->activity[v] & 0x80000000)
            act_var_rescale(s);
        if (s->orderpos[v] != -1)
            order_update(s,v);
    }
    else if ( s->VarActType == 1 )
    {
        double act = Abc_Word2Dbl(s->activity[v]) + Abc_Word2Dbl(s->var_inc) * s->factors[v];
        s->activity[v] = Abc_Dbl2Word(act);
        if ( act > 1e100)
            act_var_rescale(s);
        if (s->orderpos[v] != -1)
            order_update(s,v);
    }
    else if ( s->VarActType == 2 )
    {
        s->activity[v] = Xdbl_Add( s->activity[v], Xdbl_Mul(s->var_inc, Xdbl_FromDouble(s->factors[v])) );
        if (s->activity[v] > ABC_CONST(0x014c924d692ca61b))
            act_var_rescale(s);
        if (s->orderpos[v] != -1)
            order_update(s,v);
    }
    else assert( 0 );
}

static inline void act_var_decay(sat_solver* s)
{
    if ( s->VarActType == 0 )
        s->var_inc += (s->var_inc >>  4);
    else if ( s->VarActType == 1 )
        s->var_inc = Abc_Dbl2Word( Abc_Word2Dbl(s->var_inc) * Abc_Word2Dbl(s->var_decay) );
    else if ( s->VarActType == 2 )
        s->var_inc = Xdbl_Mul(s->var_inc, s->var_decay);
    else assert(0);
}

static int sat_solver_lit_removable(sat_solver* s, int x, int minl)
{
    int      top     = veci_size(&s->tagged);

    assert(s->reasons[x] != 0);
    veci_resize(&s->stack,0);
    veci_push(&s->stack,x);

    while (veci_size(&s->stack)){
        int v = veci_pop(&s->stack);
        assert(s->reasons[v] != 0);
        if (clause_is_lit(s->reasons[v])){
            v = lit_var(clause_read_lit(s->reasons[v]));
            if (!var_tag(s,v) && var_level(s, v)){
                if (s->reasons[v] != 0 && ((1 << (var_level(s, v) & 31)) & minl)){
                    veci_push(&s->stack,v);
                    var_set_tag(s, v, 1);
                }else{
                    solver2_clear_tags(s, top);
                    return 0;
                }
            }
        }else{
            clause* c = clause_read(s, s->reasons[v]);
            lit* lits = clause_begin(c);
            int  i;
            for (i = 1; i < clause_size(c); i++){
                int v = lit_var(lits[i]);
                if (!var_tag(s,v) && var_level(s, v)){
                    if (s->reasons[v] != 0 && ((1 << (var_level(s, v) & 31)) & minl)){
                        veci_push(&s->stack,lit_var(lits[i]));
                        var_set_tag(s, v, 1);
                    }else{
                        solver2_clear_tags(s, top);
                        return 0;
                    }
                }
            }
        }
    }
    return 1;
}
static void sat_solver_analyze(sat_solver* s, int h, veci* learnt)
{
    lit*     trail   = s->trail;
    int      cnt     = 0;
    lit      p       = lit_Undef;
    int      ind     = s->qtail-1;
    lit*     lits;
    int      i, j, minl;
    veci_push(learnt,lit_Undef);
    do{
        assert(h != 0);
        if (clause_is_lit(h)){
            int x = lit_var(clause_read_lit(h));
            if (var_tag(s, x) == 0 && var_level(s, x) > 0){
                var_set_tag(s, x, 1);
                act_var_bump(s,x);
                if (var_level(s, x) == sat_solver_dl(s))
                    cnt++;
                else
                    veci_push(learnt,clause_read_lit(h));
            }
        }else{
            clause* c = clause_read(s, h);

            if (clause_learnt(c))
                act_clause_bump(s,c);
            lits = clause_begin(c);
            //printlits(lits,lits+clause_size(c)); printf("\n");
            for (j = (p == lit_Undef ? 0 : 1); j < clause_size(c); j++){
                int x = lit_var(lits[j]);
                if (var_tag(s, x) == 0 && var_level(s, x) > 0){
                    var_set_tag(s, x, 1);
                    act_var_bump(s,x);
                    // bump variables propaged by the LBD=2 clause
//                    if ( s->reasons[x] && clause_read(s, s->reasons[x])->lbd <= 2 )
//                        act_var_bump(s,x);
                    if (var_level(s,x) == sat_solver_dl(s))
                        cnt++;
                    else
                        veci_push(learnt,lits[j]);
                }
            }
        }

        while ( !var_tag(s, lit_var(trail[ind--])) );

        p = trail[ind+1];
        h = s->reasons[lit_var(p)];
        cnt--;

    }while (cnt > 0);

    *veci_begin(learnt) = lit_neg(p);

    lits = veci_begin(learnt);
    minl = 0;
    for (i = 1; i < veci_size(learnt); i++){
        int lev = var_level(s, lit_var(lits[i]));
        minl    |= 1 << (lev & 31);
    }

    // simplify (full)
    for (i = j = 1; i < veci_size(learnt); i++){
        if (s->reasons[lit_var(lits[i])] == 0 || !sat_solver_lit_removable(s,lit_var(lits[i]),minl))
            lits[j++] = lits[i];
    }

    // update size of learnt + statistics
    veci_resize(learnt,j);
    s->stats.tot_literals += j;


    // clear tags
    solver2_clear_tags(s,0);

#ifdef DEBUG
    for (i = 0; i < s->size; i++)
        assert(!var_tag(s, i));
#endif

#ifdef VERBOSEDEBUG
    printf(L_IND"Learnt {", L_ind);
    for (i = 0; i < veci_size(learnt); i++) printf(" "L_LIT, L_lit(lits[i]));
#endif
    if (veci_size(learnt) > 1){
        int max_i = 1;
        int max   = var_level(s, lit_var(lits[1]));
        lit tmp;

        for (i = 2; i < veci_size(learnt); i++)
            if (var_level(s, lit_var(lits[i])) > max){
                max   = var_level(s, lit_var(lits[i]));
                max_i = i;
            }

        tmp         = lits[1];
        lits[1]     = lits[max_i];
        lits[max_i] = tmp;
    }
#ifdef VERBOSEDEBUG
    {
        int lev = veci_size(learnt) > 1 ? var_level(s, lit_var(lits[1])) : 0;
        printf(" } at level %d\n", lev);
    }
#endif
}

static void sat_solver_analyze_final(sat_solver* s, int hConf, int skip_first)
{
    clause* conf = clause_read(s, hConf);
    int i, j, start;
    veci_resize(&s->conf_final,0);
    if ( s->root_level == 0 )
        return;
    assert( veci_size(&s->tagged) == 0 );
//    assert( s->tags[lit_var(p)] == l_Undef );
//    s->tags[lit_var(p)] = l_True;
    for (i = skip_first ? 1 : 0; i < clause_size(conf); i++)
    {
        int x = lit_var(clause_begin(conf)[i]);
        if (var_level(s, x) > 0)
            var_set_tag(s, x, 1);
    }

    start = (s->root_level >= veci_size(&s->trail_lim))? s->qtail-1 : (veci_begin(&s->trail_lim))[s->root_level];
    for (i = start; i >= (veci_begin(&s->trail_lim))[0]; i--){
        int x = lit_var(s->trail[i]);
        if (var_tag(s,x)){
            if (s->reasons[x] == 0){
                assert(var_level(s, x) > 0);
                veci_push(&s->conf_final,lit_neg(s->trail[i]));
            }else{
                if (clause_is_lit(s->reasons[x])){
                    lit q = clause_read_lit(s->reasons[x]);
                    assert(lit_var(q) >= 0 && lit_var(q) < s->size);
                    if (var_level(s, lit_var(q)) > 0)
                        var_set_tag(s, lit_var(q), 1);
                }
                else{
                    clause* c = clause_read(s, s->reasons[x]);
                    int* lits = clause_begin(c);
                    for (j = 1; j < clause_size(c); j++)
                        if (var_level(s, lit_var(lits[j])) > 0)
                            var_set_tag(s, lit_var(lits[j]), 1);
                }
            }
        }
    }
    solver2_clear_tags(s,0);
}

static void sat_solver_record(sat_solver* s, veci* cls)
{
    lit*    begin = veci_begin(cls);
    lit*    end   = begin + veci_size(cls);
    int     h     = (veci_size(cls) > 1) ? sat_solver_clause_new(s,begin,end,1) : 0;
    sat_solver_enqueue(s,*begin,h);
    assert(veci_size(cls) > 0);
    if ( h == 0 )
        veci_push( &s->unit_lits, *begin );

    ///////////////////////////////////
    // add clause to internal storage
    if ( s->pStore )
    {
        int RetValue = Sto_ManAddClause( (Sto_Man_t *)s->pStore, begin, end );
        assert( RetValue );
        (void) RetValue;
    }
    ///////////////////////////////////
/*
    if (h != 0) {
        act_clause_bump(s,clause_read(s, h));
        s->stats.learnts++;
        s->stats.learnts_literals += veci_size(cls);
    }
*/
}

// Returns a random float 0 <= x < 1. Seed must never be 0.
static inline double drand(double* seed) {
    int q;
    *seed *= 1389796;
    q = (int)(*seed / 2147483647);
    *seed -= (double)q * 2147483647;
    return *seed / 2147483647; }


// Returns a random integer 0 <= x < size. Seed must never be 0.
static inline int irand(double* seed, int size) {
    return (int)(drand(seed) * size); }

void sat_solver_reducedb(sat_solver* s)
{
    static abctime TimeTotal = 0;
    abctime clk = Abc_Clock();
    Sat_Mem_t * pMem = &s->Mem;
    int nLearnedOld = veci_size(&s->act_clas);
    int * act_clas = veci_begin(&s->act_clas);
    int * pPerm, * pArray, * pSortValues, nCutoffValue;
    int i, k, j, Id, Counter, CounterStart, nSelected;
    clause * c;

    assert( s->nLearntMax > 0 );
    assert( nLearnedOld == Sat_MemEntryNum(pMem, 1) );
    assert( nLearnedOld == (int)s->stats.learnts );

    s->nDBreduces++;

    //printf( "Calling reduceDB with %d learned clause limit.\n", s->nLearntMax );
    s->nLearntMax = s->nLearntStart + s->nLearntDelta * s->nDBreduces;
//    return;

    // create sorting values
    pSortValues = ABC_ALLOC( int, nLearnedOld );
    Sat_MemForEachLearned( pMem, c, i, k )
    {
        Id = clause_id(c);
//        pSortValues[Id] = act[Id];
        if ( s->ClaActType == 0 )
            pSortValues[Id] = ((7 - Abc_MinInt(c->lbd, 7)) << 28) | (act_clas[Id] >> 4);
        else
            pSortValues[Id] = ((7 - Abc_MinInt(c->lbd, 7)) << 28);// | (act_clas[Id] >> 4);
        assert( pSortValues[Id] >= 0 );
    }

    // preserve 1/20 of last clauses
    CounterStart  = nLearnedOld - (s->nLearntMax / 20);

    // preserve 3/4 of most active clauses
    nSelected = nLearnedOld*s->nLearntRatio/100;

    // find non-decreasing permutation
    pPerm = Abc_MergeSortCost( pSortValues, nLearnedOld );
    assert( pSortValues[pPerm[0]] <= pSortValues[pPerm[nLearnedOld-1]] );
    nCutoffValue = pSortValues[pPerm[nLearnedOld-nSelected]];
    ABC_FREE( pPerm );
//    ActCutOff = ABC_INFINITY;

    // mark learned clauses to remove
    Counter = j = 0;
    Sat_MemForEachLearned( pMem, c, i, k )
    {
        assert( c->mark == 0 );
        if ( Counter++ > CounterStart || clause_size(c) < 3 || pSortValues[clause_id(c)] > nCutoffValue || s->reasons[lit_var(c->lits[0])] == Sat_MemHand(pMem, i, k) )
            act_clas[j++] = act_clas[clause_id(c)];
        else // delete
        {
            c->mark = 1;
            s->stats.learnts_literals -= clause_size(c);
            s->stats.learnts--;
        }
    }
    assert( s->stats.learnts == (unsigned)j );
    assert( Counter == nLearnedOld );
    veci_resize(&s->act_clas,j);
    ABC_FREE( pSortValues );

    // update ID of each clause to be its new handle
    Counter = Sat_MemCompactLearned( pMem, 0 );
    assert( Counter == (int)s->stats.learnts );

    // update reasons
    for ( i = 0; i < s->size; i++ )
    {
        if ( !s->reasons[i] ) // no reason
            continue;
        if ( clause_is_lit(s->reasons[i]) ) // 2-lit clause
            continue;
        if ( !clause_learnt_h(pMem, s->reasons[i]) ) // problem clause
            continue;
        c = clause_read( s, s->reasons[i] );
        assert( c->mark == 0 );
        s->reasons[i] = clause_id(c); // updating handle here!!!
    }

    // update watches
    for ( i = 0; i < s->size*2; i++ )
    {
        pArray = veci_begin(&s->wlists[i]);
        for ( j = k = 0; k < veci_size(&s->wlists[i]); k++ )
        {
            if ( clause_is_lit(pArray[k]) ) // 2-lit clause
                pArray[j++] = pArray[k];
            else if ( !clause_learnt_h(pMem, pArray[k]) ) // problem clause
                pArray[j++] = pArray[k];
            else
            {
                c = clause_read(s, pArray[k]);
                if ( !c->mark ) // useful learned clause
                   pArray[j++] = clause_id(c); // updating handle here!!!
            }
        }
        veci_resize(&s->wlists[i],j);
    }

    // perform final move of the clauses
    Counter = Sat_MemCompactLearned( pMem, 1 );
    assert( Counter == (int)s->stats.learnts );

    // report the results
    TimeTotal += Abc_Clock() - clk;
    if ( s->fVerbose )
    {
    Abc_Print(1, "reduceDB: Keeping %7d out of %7d clauses (%5.2f %%)  ",
        s->stats.learnts, nLearnedOld, 100.0 * s->stats.learnts / nLearnedOld );
    Abc_PrintTime( 1, "Time", TimeTotal );
    }
}

static inline int  order_select(sat_solver* s, float random_var_freq) // selectvar
{
    int*      heap     = veci_begin(&s->order);
    int*      orderpos = s->orderpos;
    // Random decision:
    if (drand(&s->random_seed) < random_var_freq){
        int next = irand(&s->random_seed,s->size);
        assert(next >= 0 && next < s->size);
        if (var_value(s, next) == varX)
            return next;
    }
    // Activity based decision:
    while (veci_size(&s->order) > 0){
        int    next  = heap[0];
        int    size  = veci_size(&s->order)-1;
        int    x     = heap[size];
        veci_resize(&s->order,size);
        orderpos[next] = -1;
        if (size > 0){
            int    i     = 0;
            int    child = 1;
            while (child < size){

                if (child+1 < size && s->activity[heap[child]] < s->activity[heap[child+1]])
                    child++;
                assert(child < size);
                if (s->activity[x] >= s->activity[heap[child]])
                    break;

                heap[i]           = heap[child];
                orderpos[heap[i]] = i;
                i                 = child;
                child             = 2 * child + 1;
            }
            heap[i]           = x;
            orderpos[heap[i]] = i;
        }
        if (var_value(s, next) == varX)
            return next;
    }
    return var_Undef;
}

static lbool sat_solver_search(sat_solver* s, ABC_INT64_T nof_conflicts)
{
//    double  var_decay       = 0.95;
//    double  clause_decay    = 0.999;
    double  random_var_freq = s->fNotUseRandom ? 0.0 : 0.02;
    ABC_INT64_T  conflictC  = 0;
    veci    learnt_clause;
    int     i;

    assert(s->root_level == sat_solver_dl(s));

    s->nRestarts++;
    s->stats.starts++;
//    s->var_decay = (float)(1 / var_decay   );  // move this to sat_solver_new()
//    s->cla_decay = (float)(1 / clause_decay);  // move this to sat_solver_new()
//    veci_resize(&s->model,0);
    veci_new(&learnt_clause);

    // use activity factors in every even restart
    if ( (s->nRestarts & 1) && veci_size(&s->act_vars) > 0 )
//    if ( veci_size(&s->act_vars) > 0 )
        for ( i = 0; i < s->act_vars.size; i++ )
            act_var_bump_factor(s, s->act_vars.ptr[i]);

    // use activity factors in every restart
    if ( s->pGlobalVars && veci_size(&s->act_vars) > 0 )
        for ( i = 0; i < s->act_vars.size; i++ )
            act_var_bump_global(s, s->act_vars.ptr[i]);

    for (;;){
        int hConfl = sat_solver_propagate(s);
        if (hConfl != 0){
            // CONFLICT
            int blevel;

#ifdef VERBOSEDEBUG
            printf(L_IND"**CONFLICT**\n", L_ind);
#endif
            s->stats.conflicts++; conflictC++;
            if (sat_solver_dl(s) == s->root_level){
#ifdef SAT_USE_ANALYZE_FINAL
                sat_solver_analyze_final(s, hConfl, 0);
#endif
                veci_delete(&learnt_clause);
                return l_False;
            }

            veci_resize(&learnt_clause,0);
            sat_solver_analyze(s, hConfl, &learnt_clause);
            blevel = veci_size(&learnt_clause) > 1 ? var_level(s, lit_var(veci_begin(&learnt_clause)[1])) : s->root_level;
            blevel = s->root_level > blevel ? s->root_level : blevel;
            sat_solver_canceluntil(s,blevel);
            sat_solver_record(s,&learnt_clause);
#ifdef SAT_USE_ANALYZE_FINAL
//            if (learnt_clause.size() == 1) level[var(learnt_clause[0])] = 0;    // (this is ugly (but needed for 'analyzeFinal()') -- in future versions, we will backtrack past the 'root_level' and redo the assumptions)
            if ( learnt_clause.size == 1 )
                var_set_level(s, lit_var(learnt_clause.ptr[0]), 0);
#endif
            act_var_decay(s);
            act_clause_decay(s);

        }else{
            // NO CONFLICT
            int next;

            // Reached bound on number of conflicts:
            if ( (!s->fNoRestarts && nof_conflicts >= 0 && conflictC >= nof_conflicts) || (s->nRuntimeLimit && (s->stats.conflicts & 63) == 0 && Abc_Clock() > s->nRuntimeLimit)){
                s->progress_estimate = sat_solver_progress(s);
                sat_solver_canceluntil(s,s->root_level);
                veci_delete(&learnt_clause);
                return l_Undef; }

            // Reached bound on number of conflicts:
            if ( (s->nConfLimit && s->stats.conflicts > s->nConfLimit) ||
                 (s->nInsLimit  && s->stats.propagations > s->nInsLimit) )
            {
                s->progress_estimate = sat_solver_progress(s);
                sat_solver_canceluntil(s,s->root_level);
                veci_delete(&learnt_clause);
                return l_Undef;
            }

            // Simplify the set of problem clauses:
            if (sat_solver_dl(s) == 0 && !s->fSkipSimplify)
                sat_solver_simplify(s);

            // Reduce the set of learnt clauses:
//            if (s->nLearntMax && veci_size(&s->learned) - s->qtail >= s->nLearntMax)
            if (s->nLearntMax && veci_size(&s->act_clas) >= s->nLearntMax)
                sat_solver_reducedb(s);

            // New variable decision:
            s->stats.decisions++;
            next = order_select(s,(float)random_var_freq);

            if (next == var_Undef){
                // Model found:
                int i;
                for (i = 0; i < s->size; i++)
                    s->model[i] = (var_value(s,i)==var1 ? l_True : l_False);
                sat_solver_canceluntil(s,s->root_level);
                veci_delete(&learnt_clause);

                /*
                veci apa; veci_new(&apa);
                for (i = 0; i < s->size; i++)
                    veci_push(&apa,(int)(s->model.ptr[i] == l_True ? toLit(i) : lit_neg(toLit(i))));
                printf("model: "); printlits((lit*)apa.ptr, (lit*)apa.ptr + veci_size(&apa)); printf("\n");
                veci_delete(&apa);
                */

                return l_True;
            }

            if ( var_polar(s, next) ) // positive polarity
                sat_solver_decision(s,toLit(next));
            else
                sat_solver_decision(s,lit_neg(toLit(next)));
        }
    }

    return l_Undef; // cannot happen
}





int sat_solver_propagate(sat_solver* s)
{
    int     hConfl = 0;
    lit*    lits;
    lit false_lit;

    printf("sat_solver_propagate\n");
    while (hConfl == 0 && s->qtail - s->qhead > 0){
        lit p = s->trail[s->qhead++];

#ifdef TEST_CNF_LOAD
        int v = lit_var(p);
        if ( s->pCnfFunc )
        {
            if ( lit_sign(p) )
            {
                if ( (s->loads[v] & 1) == 0 )
                {
                    s->loads[v] ^= 1;
                    s->pCnfFunc( s->pCnfMan, p );
                }
            }
            else
            {
                if ( (s->loads[v] & 2) == 0 )
                {
                    s->loads[v] ^= 2;
                    s->pCnfFunc( s->pCnfMan, p );
                }
            }
        }
        {
#endif

        veci* ws    = sat_solver_read_wlist(s,p);
        int*  begin = veci_begin(ws);
        int*  end   = begin + veci_size(ws);
        int*i, *j;

        s->stats.propagations++;
//        s->simpdb_props--;

        //printf("checking lit %d: "L_LIT"\n", veci_size(ws), L_lit(p));
        for (i = j = begin; i < end; ){
            if (clause_is_lit(*i)){

                int Lit = clause_read_lit(*i);
                if (var_value(s, lit_var(Lit)) == lit_sign(Lit)){
                    *j++ = *i++;
                    continue;
                }

                *j++ = *i;
                if (!sat_solver_enqueue(s,clause_read_lit(*i),clause_from_lit(p))){
                    hConfl = s->hBinary;
                    (clause_begin(s->binary))[1] = lit_neg(p);
                    (clause_begin(s->binary))[0] = clause_read_lit(*i++);
                    // Copy the remaining watches:
                    while (i < end)
                        *j++ = *i++;
                }
            }else{

                clause* c = clause_read(s,*i);
                lits = clause_begin(c);

                // Make sure the false literal is data[1]:
                false_lit = lit_neg(p);
                if (lits[0] == false_lit){
                    lits[0] = lits[1];
                    lits[1] = false_lit;
                }
                assert(lits[1] == false_lit);

                // If 0th watch is true, then clause is already satisfied.
                if (var_value(s, lit_var(lits[0])) == lit_sign(lits[0]))
                    *j++ = *i;
                else{
                    // Look for new watch:
                    lit* stop = lits + clause_size(c);
                    lit* k;
                    for (k = lits + 2; k < stop; k++){
                        if (var_value(s, lit_var(*k)) != !lit_sign(*k)){
                            lits[1] = *k;
                            *k = false_lit;
                            veci_push(sat_solver_read_wlist(s,lit_neg(lits[1])),*i);
                            goto next; }
                    }

                    *j++ = *i;
                    // Clause is unit under assignment:
                    if ( c->lrn )
                        c->lbd = sat_clause_compute_lbd(s, c);
                    if (!sat_solver_enqueue(s,lits[0], *i)){
                        hConfl = *i++;
                        // Copy the remaining watches:
                        while (i < end)
                            *j++ = *i++;
                    }
                }
            }
        next:
            i++;
        }

        s->stats.inspects += j - veci_begin(ws);
        veci_resize(ws,j - veci_begin(ws));
#ifdef TEST_CNF_LOAD
        }
#endif
    }

    return hConfl;
}


void Abc_NtkCecSat( Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2, int nConfLimit, int nInsLimit )
{
    extern Abc_Ntk_t * Abc_NtkMulti( Abc_Ntk_t * pNtk, int nThresh, int nFaninMax, int fCnf, int fMulti, int fSimple, int fFactor );
    Abc_Ntk_t * pMiter;
    Abc_Ntk_t * pCnf;
    int RetValue;

    // get the miter of the two networks
    pMiter = Abc_NtkMiter( pNtk1, pNtk2, 1, 0, 0, 0 );
    if ( pMiter == NULL )
    {
        printf( "Miter computation has failed.\n" );
        return;
    }
    RetValue = Abc_NtkMiterIsConstant( pMiter );
    if ( RetValue == 0 )
    {
        printf( "Networks are NOT EQUIVALENT after structural hashing.\n" );
        // report the error
        pMiter->pModel = Abc_NtkVerifyGetCleanModel( pMiter, 1 );
        Abc_NtkVerifyReportError( pNtk1, pNtk2, pMiter->pModel );
        ABC_FREE( pMiter->pModel );
        Abc_NtkDelete( pMiter );
        return;
    }
    if ( RetValue == 1 )
    {
        Abc_NtkDelete( pMiter );
        printf( "Networks are equivalent after structural hashing.\n" );
        return;
    }

    // convert the miter into a CNF
    pCnf = Abc_NtkMulti( pMiter, 0, 100, 1, 0, 0, 0 );
    Abc_NtkDelete( pMiter );
    if ( pCnf == NULL )
    {
        printf( "Renoding for CNF has failed.\n" );
        return;
    }

    // solve the CNF using the SAT solver
    RetValue = Abc_NtkMiterSat_Test( pCnf, (ABC_INT64_T)nConfLimit, (ABC_INT64_T)nInsLimit, 1, NULL, NULL );
    if ( RetValue == -1 )
        printf( "Networks are undecided (SAT solver timed out).\n" );
    else if ( RetValue == 0 )
        printf( "Networks are NOT EQUIVALENT after SAT.\n" );
    else
        printf( "Networks are equivalent after SAT.\n" );
    if ( pCnf->pModel )
        Abc_NtkVerifyReportError( pNtk1, pNtk2, pCnf->pModel );
    ABC_FREE( pCnf->pModel );
    Abc_NtkDelete( pCnf );
}


void Abc_NtkVerifyReportError( Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2, int * pModel )
{
    Vec_Ptr_t * vNodes;
    Abc_Obj_t * pNode;
    int * pValues1, * pValues2;
    int nErrors, nPrinted, i, iNode = -1;

    assert( Abc_NtkCiNum(pNtk1) == Abc_NtkCiNum(pNtk2) );
    assert( Abc_NtkCoNum(pNtk1) == Abc_NtkCoNum(pNtk2) );
    // get the CO values under this model
    pValues1 = Abc_NtkVerifySimulatePattern( pNtk1, pModel );
    pValues2 = Abc_NtkVerifySimulatePattern( pNtk2, pModel );
    // count the mismatches
    nErrors = 0;
    for ( i = 0; i < Abc_NtkCoNum(pNtk1); i++ )
        nErrors += (int)( pValues1[i] != pValues2[i] );
    printf( "Verification failed for at least %d outputs: ", nErrors );
    // print the first 3 outputs
    nPrinted = 0;
    for ( i = 0; i < Abc_NtkCoNum(pNtk1); i++ )
        if ( pValues1[i] != pValues2[i] )
        {
            if ( iNode == -1 )
                iNode = i;
            printf( " %s", Abc_ObjName(Abc_NtkCo(pNtk1,i)) );
            if ( ++nPrinted == 3 )
                break;
        }
    if ( nPrinted != nErrors )
        printf( " ..." );
    printf( "\n" );
    // report mismatch for the first output
    if ( iNode >= 0 )
    {
        printf( "Output %s: Value in Network1 = %d. Value in Network2 = %d.\n",
            Abc_ObjName(Abc_NtkCo(pNtk1,iNode)), pValues1[iNode], pValues2[iNode] );
        printf( "Input pattern: " );
        // collect PIs in the cone
        pNode = Abc_NtkCo(pNtk1,iNode);
        vNodes = Abc_NtkNodeSupport( pNtk1, &pNode, 1 );
        // set the PI numbers
        Abc_NtkForEachCi( pNtk1, pNode, i )
            pNode->pCopy = (Abc_Obj_t *)(ABC_PTRINT_T)i;
        // print the model
        pNode = (Abc_Obj_t *)Vec_PtrEntry( vNodes, 0 );
        if ( Abc_ObjIsCi(pNode) )
        {
            Vec_PtrForEachEntry( Abc_Obj_t *, vNodes, pNode, i )
            {
                assert( Abc_ObjIsCi(pNode) );
                printf( " %s=%d", Abc_ObjName(pNode), pModel[(int)(ABC_PTRINT_T)pNode->pCopy] );
            }
        }
        printf( "\n" );
        Vec_PtrFree( vNodes );
    }
    ABC_FREE( pValues1 );
    ABC_FREE( pValues2 );
}


Abc_Ntk_t * Abc_NtkMulti( Abc_Ntk_t * pNtk, int nThresh, int nFaninMax, int fCnf, int fMulti, int fSimple, int fFactor )
{
    Abc_Ntk_t * pNtkNew;

    assert( Abc_NtkIsStrash(pNtk) );
    assert( nThresh >= 0 );
    assert( nFaninMax > 1 );

    // print a warning about choice nodes
    if ( Abc_NtkGetChoiceNum( pNtk ) )
        printf( "Warning: The choice nodes in the AIG are removed by renoding.\n" );

    // define the boundary
    if ( fCnf )
        Abc_NtkMultiSetBoundsCnf( pNtk );
    else if ( fMulti )
        Abc_NtkMultiSetBoundsMulti( pNtk, nThresh );
    else if ( fSimple )
        Abc_NtkMultiSetBoundsSimple( pNtk );
    else if ( fFactor )
        Abc_NtkMultiSetBoundsFactor( pNtk );
    else
        Abc_NtkMultiSetBounds( pNtk, nThresh, nFaninMax );

    // perform renoding for this boundary
    pNtkNew = Abc_NtkStartFrom( pNtk, ABC_NTK_LOGIC, ABC_FUNC_BDD );
    Abc_NtkMultiInt( pNtk, pNtkNew );
    Abc_NtkFinalize( pNtk, pNtkNew );

    // make the network minimum base
    Abc_NtkMinimumBase( pNtkNew );

    // fix the problem with complemented and duplicated CO edges
    Abc_NtkLogicMakeSimpleCos( pNtkNew, 0 );

    // report the number of CNF objects
    if ( fCnf )
    {
//        int nClauses = Abc_NtkGetClauseNum(pNtkNew) + 2*Abc_NtkPoNum(pNtkNew) + 2*Abc_NtkLatchNum(pNtkNew);
//        printf( "CNF variables = %d. CNF clauses = %d.\n",  Abc_NtkNodeNum(pNtkNew), nClauses );
    }
//printf( "Maximum fanin = %d.\n", Abc_NtkGetFaninMax(pNtkNew) );

    if ( pNtk->pExdc )
        pNtkNew->pExdc = Abc_NtkDup( pNtk->pExdc );
    // make sure everything is okay
    if ( !Abc_NtkCheck( pNtkNew ) )
    {
        printf( "Abc_NtkMulti: The network check has failed.\n" );
        Abc_NtkDelete( pNtkNew );
        return NULL;
    }
    return pNtkNew;
}


void Abc_NtkMultiInt( Abc_Ntk_t * pNtk, Abc_Ntk_t * pNtkNew )
{
    ProgressBar * pProgress;
    Abc_Obj_t * pNode, * pConst1, * pNodeNew;
    int i;

    // set the constant node
    pConst1 = Abc_AigConst1(pNtk);
    if ( Abc_ObjFanoutNum(pConst1) > 0 )
    {
        pNodeNew = Abc_NtkCreateNode( pNtkNew );
        pNodeNew->pData = Cudd_ReadOne( (DdManager *)pNtkNew->pManFunc );   Cudd_Ref( (DdNode *)pNodeNew->pData );
        pConst1->pCopy = pNodeNew;
    }

    // perform renoding for POs
    pProgress = Extra_ProgressBarStart( stdout, Abc_NtkCoNum(pNtk) );
    Abc_NtkForEachCo( pNtk, pNode, i )
    {
        Extra_ProgressBarUpdate( pProgress, i, NULL );
        if ( Abc_ObjIsCi(Abc_ObjFanin0(pNode)) )
            continue;
        Abc_NtkMulti_rec( pNtkNew, Abc_ObjFanin0(pNode) );
    }
    Extra_ProgressBarStop( pProgress );

    // clean the boundaries and data field in the old network
    Abc_NtkForEachObj( pNtk, pNode, i )
    {
        pNode->fMarkA = 0;
        pNode->pData = NULL;
    }
}


void Abc_NtkMultiSetBoundsCnf( Abc_Ntk_t * pNtk )
{
    Abc_Obj_t * pNode;
    int i, nMuxes;

    // make sure the mark is not set
    Abc_NtkForEachObj( pNtk, pNode, i )
        assert( pNode->fMarkA == 0 );

    // mark the nodes where expansion stops using pNode->fMarkA
    Abc_NtkForEachNode( pNtk, pNode, i )
    {
        // skip PI/PO nodes
//        if ( Abc_NodeIsConst(pNode) )
//            continue;
        // mark the nodes with multiple fanouts
        if ( Abc_ObjFanoutNum(pNode) > 1 )
            pNode->fMarkA = 1;
        // mark the nodes that are roots of MUXes
        if ( Abc_NodeIsMuxType( pNode ) )
        {
            pNode->fMarkA = 1;
            Abc_ObjFanin0( Abc_ObjFanin0(pNode) )->fMarkA = 1;
            Abc_ObjFanin0( Abc_ObjFanin1(pNode) )->fMarkA = 1;
            Abc_ObjFanin1( Abc_ObjFanin0(pNode) )->fMarkA = 1;
            Abc_ObjFanin1( Abc_ObjFanin1(pNode) )->fMarkA = 1;
        }
        else  // mark the complemented edges
        {
            if ( Abc_ObjFaninC0(pNode) )
                Abc_ObjFanin0(pNode)->fMarkA = 1;
            if ( Abc_ObjFaninC1(pNode) )
                Abc_ObjFanin1(pNode)->fMarkA = 1;
        }
    }

    // mark the PO drivers
    Abc_NtkForEachCo( pNtk, pNode, i )
        Abc_ObjFanin0(pNode)->fMarkA = 1;

    // count the number of MUXes
    nMuxes = 0;
    Abc_NtkForEachNode( pNtk, pNode, i )
    {
        // skip PI/PO nodes
//        if ( Abc_NodeIsConst(pNode) )
//            continue;
        if ( Abc_NodeIsMuxType(pNode) &&
            Abc_ObjFanin0(pNode)->fMarkA == 0 &&
            Abc_ObjFanin1(pNode)->fMarkA == 0 )
            nMuxes++;
    }
//    printf( "The number of MUXes detected = %d (%5.2f %% of logic).\n", nMuxes, 300.0*nMuxes/Abc_NtkNodeNum(pNtk) );
}


void Abc_NtkMultiSetBoundsMulti( Abc_Ntk_t * pNtk, int nThresh )
{
    Abc_Obj_t * pNode;
    int i, nFanouts, nConeSize;

    // make sure the mark is not set
    Abc_NtkForEachObj( pNtk, pNode, i )
        assert( pNode->fMarkA == 0 );

    // mark the nodes where expansion stops using pNode->fMarkA
    Abc_NtkForEachNode( pNtk, pNode, i )
    {
        // skip PI/PO nodes
//        if ( Abc_NodeIsConst(pNode) )
//            continue;
        // mark the nodes with multiple fanouts
//        if ( Abc_ObjFanoutNum(pNode) > 1 )
//            pNode->fMarkA = 1;
        // mark the nodes with multiple fanouts
        nFanouts = Abc_ObjFanoutNum(pNode);
        nConeSize = Abc_NodeMffcSizeStop(pNode);
        if ( (nFanouts - 1) * nConeSize > nThresh )
            pNode->fMarkA = 1;
        // mark the children if they are pointed by the complemented edges
        if ( Abc_ObjFaninC0(pNode) )
            Abc_ObjFanin0(pNode)->fMarkA = 1;
        if ( Abc_ObjFaninC1(pNode) )
            Abc_ObjFanin1(pNode)->fMarkA = 1;
    }

    // mark the PO drivers
    Abc_NtkForEachCo( pNtk, pNode, i )
        Abc_ObjFanin0(pNode)->fMarkA = 1;
}


void Abc_NtkMultiSetBounds( Abc_Ntk_t * pNtk, int nThresh, int nFaninMax )
{
    Vec_Ptr_t * vCone = Vec_PtrAlloc(10);
    Abc_Obj_t * pNode;
    int i, nFanouts, nConeSize;

    // make sure the mark is not set
    Abc_NtkForEachObj( pNtk, pNode, i )
        assert( pNode->fMarkA == 0 );

    // mark the nodes where expansion stops using pNode->fMarkA
    Abc_NtkForEachNode( pNtk, pNode, i )
    {
        // skip PI/PO nodes
//        if ( Abc_NodeIsConst(pNode) )
//            continue;
        // mark the nodes with multiple fanouts
        nFanouts = Abc_ObjFanoutNum(pNode);
        nConeSize = Abc_NodeMffcSize(pNode);
        if ( (nFanouts - 1) * nConeSize > nThresh )
            pNode->fMarkA = 1;
    }

    // mark the PO drivers
    Abc_NtkForEachCo( pNtk, pNode, i )
        Abc_ObjFanin0(pNode)->fMarkA = 1;

    // make sure the fanin limit is met
    Abc_NtkForEachNode( pNtk, pNode, i )
    {
        // skip PI/PO nodes
//        if ( Abc_NodeIsConst(pNode) )
//            continue;
        if ( pNode->fMarkA == 0 )
            continue;
        // continue cutting branches until it meets the fanin limit
        while ( Abc_NtkMultiLimit(pNode, vCone, nFaninMax) );
        assert( vCone->nSize <= nFaninMax );
    }
    Vec_PtrFree(vCone);
/*
    // make sure the fanin limit is met
    Abc_NtkForEachNode( pNtk, pNode, i )
    {
        // skip PI/PO nodes
//        if ( Abc_NodeIsConst(pNode) )
//            continue;
        if ( pNode->fMarkA == 0 )
            continue;
        Abc_NtkMultiCone( pNode, vCone );
        assert( vCone->nSize <= nFaninMax );
    }
*/
}


void Abc_NtkMultiSetBoundsSimple( Abc_Ntk_t * pNtk )
{
    Abc_Obj_t * pNode;
    int i;
    // make sure the mark is not set
    Abc_NtkForEachObj( pNtk, pNode, i )
        assert( pNode->fMarkA == 0 );
    // mark the nodes where expansion stops using pNode->fMarkA
    Abc_NtkForEachNode( pNtk, pNode, i )
        pNode->fMarkA = 1;
}


void Abc_NtkMultiSetBoundsFactor( Abc_Ntk_t * pNtk )
{
    Abc_Obj_t * pNode;
    int i;
    // make sure the mark is not set
    Abc_NtkForEachObj( pNtk, pNode, i )
        assert( pNode->fMarkA == 0 );
    // mark the nodes where expansion stops using pNode->fMarkA
    Abc_NtkForEachNode( pNtk, pNode, i )
        pNode->fMarkA = (pNode->vFanouts.nSize > 1 && !Abc_NodeIsMuxControlType(pNode));
    // mark the PO drivers
    Abc_NtkForEachCo( pNtk, pNode, i )
        Abc_ObjFanin0(pNode)->fMarkA = 1;
}


Abc_Obj_t * Abc_NtkMulti_rec( Abc_Ntk_t * pNtkNew, Abc_Obj_t * pNodeOld )
{
    Vec_Ptr_t * vCone;
    Abc_Obj_t * pNodeNew;
    int i;

    assert( !Abc_ObjIsComplement(pNodeOld) );
    // return if the result if known
    if ( pNodeOld->pCopy )
        return pNodeOld->pCopy;
    assert( Abc_ObjIsNode(pNodeOld) );
    assert( !Abc_AigNodeIsConst(pNodeOld) );
    assert( pNodeOld->fMarkA );

//printf( "%d ", Abc_NodeMffcSizeSupp(pNodeOld) );

    // collect the renoding cone
    vCone = Vec_PtrAlloc( 10 );
    Abc_NtkMultiCone( pNodeOld, vCone );

    // create a new node
    pNodeNew = Abc_NtkCreateNode( pNtkNew );
    for ( i = 0; i < vCone->nSize; i++ )
        Abc_ObjAddFanin( pNodeNew, Abc_NtkMulti_rec(pNtkNew, (Abc_Obj_t *)vCone->pArray[i]) );

    // derive the function of this node
    pNodeNew->pData = Abc_NtkMultiDeriveBdd( (DdManager *)pNtkNew->pManFunc, pNodeOld, vCone );
    Cudd_Ref( (DdNode *)pNodeNew->pData );
    Vec_PtrFree( vCone );

    // remember the node
    pNodeOld->pCopy = pNodeNew;
    return pNodeOld->pCopy;
}


void Abc_NtkMultiCone( Abc_Obj_t * pNode, Vec_Ptr_t * vCone )
{
    assert( !Abc_ObjIsComplement(pNode) );
    assert( Abc_ObjIsNode(pNode) );
    vCone->nSize = 0;
    Abc_NtkMultiCone_rec( Abc_ObjFanin(pNode,0), vCone );
    Abc_NtkMultiCone_rec( Abc_ObjFanin(pNode,1), vCone );
}


int Abc_NtkMultiLimit( Abc_Obj_t * pNode, Vec_Ptr_t * vCone, int nFaninMax )
{
    vCone->nSize = 0;
    return Abc_NtkMultiLimit_rec( pNode, vCone, nFaninMax, 1, 1 );
}


void Abc_NtkMultiCone_rec( Abc_Obj_t * pNode, Vec_Ptr_t * vCone )
{
    assert( !Abc_ObjIsComplement(pNode) );
    if ( pNode->fMarkA || !Abc_ObjIsNode(pNode) )
    {
        Vec_PtrPushUnique( vCone, pNode );
        return;
    }
    Abc_NtkMultiCone_rec( Abc_ObjFanin(pNode,0), vCone );
    Abc_NtkMultiCone_rec( Abc_ObjFanin(pNode,1), vCone );
}


DdNode * Abc_NtkMultiDeriveBdd( DdManager * dd, Abc_Obj_t * pNodeOld, Vec_Ptr_t * vFaninsOld )
{
    Abc_Obj_t * pFaninOld;
    DdNode * bFunc;
    int i;
    assert( !Abc_AigNodeIsConst(pNodeOld) );
    assert( Abc_ObjIsNode(pNodeOld) );
    // set the elementary BDD variables for the input nodes
    for ( i = 0; i < vFaninsOld->nSize; i++ )
    {
        pFaninOld = (Abc_Obj_t *)vFaninsOld->pArray[i];
        pFaninOld->pData = Cudd_bddIthVar( dd, i );    Cudd_Ref( (DdNode *)pFaninOld->pData );
        pFaninOld->fMarkC = 1;
    }
    // call the recursive BDD computation
    bFunc = Abc_NtkMultiDeriveBdd_rec( dd, pNodeOld, vFaninsOld );  Cudd_Ref( bFunc );
    // dereference the intermediate nodes
    for ( i = 0; i < vFaninsOld->nSize; i++ )
    {
        pFaninOld = (Abc_Obj_t *)vFaninsOld->pArray[i];
        Cudd_RecursiveDeref( dd, (DdNode *)pFaninOld->pData );
        pFaninOld->fMarkC = 0;
    }
    Cudd_Deref( bFunc );
    return bFunc;
}


DdNode * Abc_NtkMultiDeriveBdd_rec( DdManager * dd, Abc_Obj_t * pNode, Vec_Ptr_t * vFanins )
{
    DdNode * bFunc, * bFunc0, * bFunc1;
    assert( !Abc_ObjIsComplement(pNode) );
    // if the result is available return
    if ( pNode->fMarkC )
    {
        assert( pNode->pData ); // network has a cycle
        return (DdNode *)pNode->pData;
    }
    // mark the node as visited
    pNode->fMarkC = 1;
    Vec_PtrPush( vFanins, pNode );
    // compute the result for both branches
    bFunc0 = Abc_NtkMultiDeriveBdd_rec( dd, Abc_ObjFanin(pNode,0), vFanins ); Cudd_Ref( bFunc0 );
    bFunc1 = Abc_NtkMultiDeriveBdd_rec( dd, Abc_ObjFanin(pNode,1), vFanins ); Cudd_Ref( bFunc1 );
    bFunc0 = Cudd_NotCond( bFunc0, (long)Abc_ObjFaninC0(pNode) );
    bFunc1 = Cudd_NotCond( bFunc1, (long)Abc_ObjFaninC1(pNode) );
    // get the final result
    bFunc = Cudd_bddAnd( dd, bFunc0, bFunc1 );   Cudd_Ref( bFunc );
    Cudd_RecursiveDeref( dd, bFunc0 );
    Cudd_RecursiveDeref( dd, bFunc1 );
    // set the result
    pNode->pData = bFunc;
    assert( pNode->pData );
    return bFunc;
}


int Abc_NtkMultiLimit_rec( Abc_Obj_t * pNode, Vec_Ptr_t * vCone, int nFaninMax, int fCanStop, int fFirst )
{
    int nNodes0, nNodes1;
    assert( !Abc_ObjIsComplement(pNode) );
    // check if the node should be added to the fanins
    if ( !fFirst && (pNode->fMarkA || !Abc_ObjIsNode(pNode)) )
    {
        Vec_PtrPushUnique( vCone, pNode );
        return 0;
    }
    // if we cannot stop in this branch, collect all nodes
    if ( !fCanStop )
    {
        Abc_NtkMultiLimit_rec( Abc_ObjFanin(pNode,0), vCone, nFaninMax, 0, 0 );
        Abc_NtkMultiLimit_rec( Abc_ObjFanin(pNode,1), vCone, nFaninMax, 0, 0 );
        return 0;
    }
    // if we can stop, try the left branch first, and return if we stopped
    assert( vCone->nSize == 0 );
    if ( Abc_NtkMultiLimit_rec( Abc_ObjFanin(pNode,0), vCone, nFaninMax, 1, 0 ) )
        return 1;
    // save the number of nodes in the left branch and call for the right branch
    nNodes0 = vCone->nSize;
    assert( nNodes0 <= nFaninMax );
    Abc_NtkMultiLimit_rec( Abc_ObjFanin(pNode,1), vCone, nFaninMax, 0, 0 );
    // check the number of nodes
    if ( vCone->nSize <= nFaninMax )
        return 0;
    // the number of nodes exceeds the limit

    // get the number of nodes in the right branch
    vCone->nSize = 0;
    Abc_NtkMultiLimit_rec( Abc_ObjFanin(pNode,1), vCone, nFaninMax, 0, 0 );
    // if this number exceeds the limit, solve the problem for this branch
    if ( vCone->nSize > nFaninMax )
    {
        int RetValue;
        vCone->nSize = 0;
        RetValue = Abc_NtkMultiLimit_rec( Abc_ObjFanin(pNode,1), vCone, nFaninMax, 1, 0 );
        assert( RetValue == 1 );
        return 1;
    }

    nNodes1 = vCone->nSize;
    assert( nNodes1 <= nFaninMax );
    if ( nNodes0 >= nNodes1 )
    { // the left branch is larger - cut it
        assert( Abc_ObjFanin(pNode,0)->fMarkA == 0 );
        Abc_ObjFanin(pNode,0)->fMarkA = 1;
    }
    else
    { // the right branch is larger - cut it
        assert( Abc_ObjFanin(pNode,1)->fMarkA == 0 );
        Abc_ObjFanin(pNode,1)->fMarkA = 1;
    }
    return 1;
}


int Abc_NtkMiterSat_Test( Abc_Ntk_t * pNtk, ABC_INT64_T nConfLimit, ABC_INT64_T nInsLimit, int fVerbose, ABC_INT64_T * pNumConfs, ABC_INT64_T * pNumInspects )
{
    sat_solver * pSat;
    lbool   status;
    int RetValue = 0;
    abctime clk;

    if ( pNumConfs )
        *pNumConfs = 0;
    if ( pNumInspects )
        *pNumInspects = 0;

    assert( Abc_NtkLatchNum(pNtk) == 0 );

//    if ( Abc_NtkPoNum(pNtk) > 1 )
//        fprintf( stdout, "Warning: The miter has %d outputs. SAT will try to prove all of them.\n", Abc_NtkPoNum(pNtk) );

    // load clauses into the sat_solver
    clk = Abc_Clock();
    pSat = (sat_solver *)Abc_NtkMiterSatCreate_Test( pNtk, 0 );
    if ( pSat == NULL )
        return 1;
//printf( "%d \n", pSat->clauses.size );
//sat_solver_delete( pSat );
//return 1;

//    printf( "Created SAT problem with %d variable and %d clauses. ", sat_solver_nvars(pSat), sat_solver_nclauses(pSat) );
//    ABC_PRT( "Time", Abc_Clock() - clk );

    // simplify the problem
    clk = Abc_Clock();
    status = sat_solver_simplify(pSat);
//    printf( "Simplified the problem to %d variables and %d clauses. ", sat_solver_nvars(pSat), sat_solver_nclauses(pSat) );
//    ABC_PRT( "Time", Abc_Clock() - clk );
    if ( status == 0 )
    {
        sat_solver_delete( pSat );
//        printf( "The problem is UNSATISFIABLE after simplification.\n" );
        return 1;
    }

    // solve the miter
    clk = Abc_Clock();
    if ( fVerbose )
        pSat->verbosity = 1;
    status = sat_solver_solve_Test( pSat, NULL, NULL, (ABC_INT64_T)nConfLimit, (ABC_INT64_T)nInsLimit, (ABC_INT64_T)0, (ABC_INT64_T)0 );
    if ( status == l_Undef )
    {
//        printf( "The problem timed out.\n" );
        RetValue = -1;
    }
    else if ( status == l_True )
    {
//        printf( "The problem is SATISFIABLE.\n" );
        RetValue = 0;
    }
    else if ( status == l_False )
    {
//        printf( "The problem is UNSATISFIABLE.\n" );
        RetValue = 1;
    }
    else
        assert( 0 );
//    ABC_PRT( "SAT sat_solver time", Abc_Clock() - clk );
//    printf( "The number of conflicts = %d.\n", (int)pSat->sat_solver_stats.conflicts );

    // if the problem is SAT, get the counterexample
    if ( status == l_True )
    {
//        Vec_Int_t * vCiIds = Abc_NtkGetCiIds( pNtk );
        Vec_Int_t * vCiIds = Abc_NtkGetCiSatVarNums( pNtk );
        pNtk->pModel = Sat_SolverGetModel( pSat, vCiIds->pArray, vCiIds->nSize );
        Vec_IntFree( vCiIds );
    }
    // free the sat_solver
    if ( fVerbose )
        Sat_SolverPrintStats( stdout, pSat );

    if ( pNumConfs )
        *pNumConfs = (int)pSat->stats.conflicts;
    if ( pNumInspects )
        *pNumInspects = (int)pSat->stats.inspects;

sat_solver_store_write( pSat, "trace.cnf" );
sat_solver_store_free( pSat );

    sat_solver_delete( pSat );
    return RetValue;
}


Vec_Int_t * Abc_NtkGetCiSatVarNums( Abc_Ntk_t * pNtk )
{
    Vec_Int_t * vCiIds;
    Abc_Obj_t * pObj;
    int i;
    vCiIds = Vec_IntAlloc( Abc_NtkCiNum(pNtk) );
    Abc_NtkForEachCi( pNtk, pObj, i )
        Vec_IntPush( vCiIds, (int)(ABC_PTRINT_T)pObj->pCopy );
    return vCiIds;
}


void * Abc_NtkMiterSatCreate_Test( Abc_Ntk_t * pNtk, int fAllPrimes )
{
    sat_solver * pSat;
    Abc_Obj_t * pNode;
    int RetValue, i; //, clk = Abc_Clock();

    assert( Abc_NtkIsStrash(pNtk) || Abc_NtkIsBddLogic(pNtk) );
    if ( Abc_NtkIsBddLogic(pNtk) )
        return Abc_NtkMiterSatCreateLogic_Test(pNtk, fAllPrimes);

    nMuxes = 0;
    pSat = sat_solver_new();
//sat_solver_store_alloc( pSat );
    RetValue = Abc_NtkMiterSatCreateInt( pSat, pNtk );
sat_solver_store_mark_roots( pSat );

    Abc_NtkForEachObj( pNtk, pNode, i )
        pNode->fMarkA = 0;
//    ASat_SolverWriteDimacs( pSat, "temp_sat.cnf", NULL, NULL, 1 );
    if ( RetValue == 0 )
    {
        sat_solver_delete(pSat);
        return NULL;
    }
//    printf( "Ands = %6d.  Muxes = %6d (%5.2f %%).  ", Abc_NtkNodeNum(pNtk), nMuxes, 300.0*nMuxes/Abc_NtkNodeNum(pNtk) );
//    ABC_PRT( "Creating sat_solver", Abc_Clock() - clk );
    return pSat;
}


sat_solver * Abc_NtkMiterSatCreateLogic_Test( Abc_Ntk_t * pNtk, int fAllPrimes )
{
    sat_solver * pSat;
    Mem_Flex_t * pMmFlex;
    Abc_Obj_t * pNode;
    Vec_Str_t * vCube;
    Vec_Int_t * vVars;
    char * pSop0, * pSop1;
    int i;

    assert( Abc_NtkIsBddLogic(pNtk) );


    Abc_NtkForEachObj(pNtk, pNode, i) {
        cout << pNode->Id << "(" << pNode->Type << ")" << ":";
        Abc_Obj_t * pFanin;
        int j;
        Abc_ObjForEachFanin(pNode, pFanin, j) {
            cout << pFanin->Id << ",";
        }
        cout << endl;
    }


    // transfer the IDs to the copy field
    Abc_NtkForEachPi( pNtk, pNode, i )
        pNode->pCopy = (Abc_Obj_t *)(ABC_PTRINT_T)pNode->Id;

    // start the data structures
    pSat    = sat_solver_new();
sat_solver_store_alloc( pSat );
    pSat->fPrintClause = 1;
    pSat->fVerbose = 1;
    pMmFlex = Mem_FlexStart();
    vCube   = Vec_StrAlloc( 100 );
    vVars   = Vec_IntAlloc( 100 );

    // add clauses for each internal nodes
    Abc_NtkForEachNode( pNtk, pNode, i )
    {
        // derive SOPs for both phases of the node
        Abc_NodeBddToCnf_Test( pNode, pMmFlex, vCube, fAllPrimes, &pSop0, &pSop1 );
        // add the clauses to the sat_solver
        if ( !Abc_NodeAddClauses( pSat, pSop0, pSop1, pNode, vVars ) )
        {
            sat_solver_delete( pSat );
            pSat = NULL;
            goto finish;
        }
    }
    // add clauses for each PO
    Abc_NtkForEachPo( pNtk, pNode, i )
    {
        if ( !Abc_NodeAddClausesTop( pSat, pNode, vVars ) )
        {
            sat_solver_delete( pSat );
            pSat = NULL;
            goto finish;
        }
    }
sat_solver_store_mark_roots( pSat );

finish:
    // delete
    Vec_StrFree( vCube );
    Vec_IntFree( vVars );
    Mem_FlexStop( pMmFlex, 0 );
    return pSat;
}


int Abc_NtkMiterSatCreateInt( sat_solver * pSat, Abc_Ntk_t * pNtk )
{
    Abc_Obj_t * pNode, * pFanin, * pNodeC, * pNodeT, * pNodeE;
    Vec_Ptr_t * vNodes, * vSuper;
    Vec_Int_t * vVars;
    int i, k, fUseMuxes = 1;
//    int fOrderCiVarsFirst = 0;
    int RetValue = 0;

    assert( Abc_NtkIsStrash(pNtk) );

    // clean the CI node pointers
    Abc_NtkForEachCi( pNtk, pNode, i )
        pNode->pCopy = NULL;

    // start the data structures
    vNodes  = Vec_PtrAlloc( 1000 );   // the nodes corresponding to vars in the sat_solver
    vSuper  = Vec_PtrAlloc( 100 );    // the nodes belonging to the given implication supergate
    vVars   = Vec_IntAlloc( 100 );    // the temporary array for variables in the clause

    // add the clause for the constant node
    pNode = Abc_AigConst1(pNtk);
    pNode->fMarkA = 1;
    pNode->pCopy = (Abc_Obj_t *)(ABC_PTRINT_T)vNodes->nSize;
    Vec_PtrPush( vNodes, pNode );
    Abc_NtkClauseTriv( pSat, pNode, vVars );
/*
    // add the PI variables first
    Abc_NtkForEachCi( pNtk, pNode, i )
    {
        pNode->fMarkA = 1;
        pNode->pCopy = (Abc_Obj_t *)vNodes->nSize;
        Vec_PtrPush( vNodes, pNode );
    }
*/
    // collect the nodes that need clauses and top-level assignments
    Vec_PtrClear( vSuper );
    Abc_NtkForEachCo( pNtk, pNode, i )
    {
        // get the fanin
        pFanin = Abc_ObjFanin0(pNode);
        // create the node's variable
        if ( pFanin->fMarkA == 0 )
        {
            pFanin->fMarkA = 1;
            pFanin->pCopy = (Abc_Obj_t *)(ABC_PTRINT_T)vNodes->nSize;
            Vec_PtrPush( vNodes, pFanin );
        }
        // add the trivial clause
        Vec_PtrPush( vSuper, Abc_ObjChild0(pNode) );
    }
    if ( !Abc_NtkClauseTop( pSat, vSuper, vVars ) )
        goto Quits;


    // add the clauses
    Vec_PtrForEachEntry( Abc_Obj_t *, vNodes, pNode, i )
    {
        assert( !Abc_ObjIsComplement(pNode) );
        if ( !Abc_AigNodeIsAnd(pNode) )
            continue;
//printf( "%d ", pNode->Id );

        // add the clauses
        if ( fUseMuxes && Abc_NodeIsMuxType(pNode) )
        {
            nMuxes++;

            pNodeC = Abc_NodeRecognizeMux( pNode, &pNodeT, &pNodeE );
            Vec_PtrClear( vSuper );
            Vec_PtrPush( vSuper, pNodeC );
            Vec_PtrPush( vSuper, pNodeT );
            Vec_PtrPush( vSuper, pNodeE );
            // add the fanin nodes to explore
            Vec_PtrForEachEntry( Abc_Obj_t *, vSuper, pFanin, k )
            {
                pFanin = Abc_ObjRegular(pFanin);
                if ( pFanin->fMarkA == 0 )
                {
                    pFanin->fMarkA = 1;
                    pFanin->pCopy = (Abc_Obj_t *)(ABC_PTRINT_T)vNodes->nSize;
                    Vec_PtrPush( vNodes, pFanin );
                }
            }
            // add the clauses
            if ( !Abc_NtkClauseMux( pSat, pNode, pNodeC, pNodeT, pNodeE, vVars ) )
                goto Quits;
        }
        else
        {
            // get the supergate
            Abc_NtkCollectSupergate( pNode, fUseMuxes, vSuper );
            // add the fanin nodes to explore
            Vec_PtrForEachEntry( Abc_Obj_t *, vSuper, pFanin, k )
            {
                pFanin = Abc_ObjRegular(pFanin);
                if ( pFanin->fMarkA == 0 )
                {
                    pFanin->fMarkA = 1;
                    pFanin->pCopy = (Abc_Obj_t *)(ABC_PTRINT_T)vNodes->nSize;
                    Vec_PtrPush( vNodes, pFanin );
                }
            }
            // add the clauses
            if ( vSuper->nSize == 0 )
            {
                if ( !Abc_NtkClauseTriv( pSat, Abc_ObjNot(pNode), vVars ) )
//                if ( !Abc_NtkClauseTriv( pSat, pNode, vVars ) )
                    goto Quits;
            }
            else
            {
                if ( !Abc_NtkClauseAnd( pSat, pNode, vSuper, vVars ) )
                    goto Quits;
            }
        }
    }
/*
    // set preferred variables
    if ( fOrderCiVarsFirst )
    {
        int * pPrefVars = ABC_ALLOC( int, Abc_NtkCiNum(pNtk) );
        int nVars = 0;
        Abc_NtkForEachCi( pNtk, pNode, i )
        {
            if ( pNode->fMarkA == 0 )
                continue;
            pPrefVars[nVars++] = (int)pNode->pCopy;
        }
        nVars = Abc_MinInt( nVars, 10 );
        ASat_SolverSetPrefVars( pSat, pPrefVars, nVars );
    }
*/
/*
    Abc_NtkForEachObj( pNtk, pNode, i )
    {
        if ( !pNode->fMarkA )
            continue;
        printf( "%10s : ", Abc_ObjName(pNode) );
        printf( "%3d\n", (int)pNode->pCopy );
    }
    printf( "\n" );
*/
    RetValue = 1;
Quits :
    // delete
    Vec_IntFree( vVars );
    Vec_PtrFree( vNodes );
    Vec_PtrFree( vSuper );
    return RetValue;
}


int Abc_NodeAddClauses( sat_solver * pSat, char * pSop0, char * pSop1, Abc_Obj_t * pNode, Vec_Int_t * vVars )
{
    cout << "add clauses " <<pNode->Id << endl;

    Abc_Obj_t * pFanin;
    int i, c, nFanins;
    int RetValue = 0;
    char * pCube;

    nFanins = Abc_ObjFaninNum( pNode );
    assert( nFanins == Abc_SopGetVarNum( pSop0 ) );

//    if ( nFanins == 0 )
    if ( Cudd_Regular((Abc_Obj_t *)pNode->pData) == Cudd_ReadOne((DdManager *)pNode->pNtk->pManFunc) )
    {
        vVars->nSize = 0;
//        if ( Abc_SopIsConst1(pSop1) )
        if ( !Cudd_IsComplement(pNode->pData) )
            Vec_IntPush( vVars, toLit(pNode->Id) );
        else
            Vec_IntPush( vVars, lit_neg(toLit(pNode->Id)) );

        RetValue = sat_solver_addclause_Test2( pSat, vVars->pArray, vVars->pArray + vVars->nSize );
        if ( !RetValue )
        {
            printf( "The CNF is trivially UNSAT.\n" );
            return 0;
        }
        return 1;
    }

    // add clauses for the negative phase
    cout << "negative" << endl;
    for ( c = 0; ; c++ )
    {
        // get the cube
        pCube = pSop0 + c * (nFanins + 3);
        if ( *pCube == 0 )
            break;
        // add the clause
        vVars->nSize = 0;
        Abc_ObjForEachFanin( pNode, pFanin, i )
        {
            if ( pCube[i] == '0' )
                Vec_IntPush( vVars, toLit(pFanin->Id) );
            else if ( pCube[i] == '1' )
                Vec_IntPush( vVars, lit_neg(toLit(pFanin->Id)) );
        }
        Vec_IntPush( vVars, lit_neg(toLit(pNode->Id)) );

        RetValue = sat_solver_addclause_Test2( pSat, vVars->pArray, vVars->pArray + vVars->nSize );
        if ( !RetValue )
        {
            printf( "The CNF is trivially UNSAT.\n" );
            return 0;
        }
    }

    // add clauses for the positive phase
    cout << "positive" << endl;
    for ( c = 0; ; c++ )
    {
        // get the cube
        pCube = pSop1 + c * (nFanins + 3);
        if ( *pCube == 0 )
            break;
        // add the clause
        vVars->nSize = 0;
        Abc_ObjForEachFanin( pNode, pFanin, i )
        {
            if ( pCube[i] == '0' )
                Vec_IntPush( vVars, toLit(pFanin->Id) );
            else if ( pCube[i] == '1' )
                Vec_IntPush( vVars, lit_neg(toLit(pFanin->Id)) );
        }
        Vec_IntPush( vVars, toLit(pNode->Id) );

        RetValue = sat_solver_addclause_Test2( pSat, vVars->pArray, vVars->pArray + vVars->nSize );
        if ( !RetValue )
        {
            printf( "The CNF is trivially UNSAT.\n" );
            return 0;
        }
    }
    return 1;
}


int Abc_NodeAddClausesTop( sat_solver * pSat, Abc_Obj_t * pNode, Vec_Int_t * vVars )
{
    Abc_Obj_t * pFanin;
    int RetValue = 0;

    pFanin = Abc_ObjFanin0(pNode);
    if ( Abc_ObjFaninC0(pNode) )
    {
        vVars->nSize = 0;
        Vec_IntPush( vVars, toLit(pFanin->Id) );
        Vec_IntPush( vVars, toLit(pNode->Id) );
        RetValue = sat_solver_addclause_Test2( pSat, vVars->pArray, vVars->pArray + vVars->nSize );
        if ( !RetValue )
        {
            printf( "The CNF is trivially UNSAT.\n" );
            return 0;
        }

        vVars->nSize = 0;
        Vec_IntPush( vVars, lit_neg(toLit(pFanin->Id)) );
        Vec_IntPush( vVars, lit_neg(toLit(pNode->Id)) );
        RetValue = sat_solver_addclause_Test2( pSat, vVars->pArray, vVars->pArray + vVars->nSize );
        if ( !RetValue )
        {
            printf( "The CNF is trivially UNSAT.\n" );
            return 0;
        }
    }
    else
    {
        vVars->nSize = 0;
        Vec_IntPush( vVars, lit_neg(toLit(pFanin->Id)) );
        Vec_IntPush( vVars, toLit(pNode->Id) );
        cout << "?" << endl;
        RetValue = sat_solver_addclause_Test2( pSat, vVars->pArray, vVars->pArray + vVars->nSize );
        if ( !RetValue )
        {
            printf( "The CNF is trivially UNSAT.\n" );
            return 0;
        }

        vVars->nSize = 0;
        Vec_IntPush( vVars, toLit(pFanin->Id) );
        Vec_IntPush( vVars, lit_neg(toLit(pNode->Id)) );
        cout << "!" << endl;
        RetValue = sat_solver_addclause_Test2( pSat, vVars->pArray, vVars->pArray + vVars->nSize );
        if ( !RetValue )
        {
            printf( "The CNF is trivially UNSAT.\n" );
            return 0;
        }
    }

    cout << "node itself" << endl;
    vVars->nSize = 0;
    Vec_IntPush( vVars, toLit(pNode->Id) );
    RetValue = sat_solver_addclause_Test2( pSat, vVars->pArray, vVars->pArray + vVars->nSize );
    if ( !RetValue )
    {
        printf( "The CNF is trivially UNSAT.\n" );
        return 0;
    }
    return 1;
}


int Abc_NtkClauseTriv( sat_solver * pSat, Abc_Obj_t * pNode, Vec_Int_t * vVars )
{
//printf( "Adding triv %d.         %d\n", Abc_ObjRegular(pNode)->Id, (int)pSat->sat_solver_stats.clauses );
    vVars->nSize = 0;
    Vec_IntPush( vVars, toLitCond( (int)(ABC_PTRINT_T)Abc_ObjRegular(pNode)->pCopy, Abc_ObjIsComplement(pNode) ) );
//    Vec_IntPush( vVars, toLitCond( (int)Abc_ObjRegular(pNode)->Id, Abc_ObjIsComplement(pNode) ) );
    return sat_solver_addclause_Test2( pSat, vVars->pArray, vVars->pArray + vVars->nSize );
}


int Abc_NtkClauseTop( sat_solver * pSat, Vec_Ptr_t * vNodes, Vec_Int_t * vVars )
{
    Abc_Obj_t * pNode;
    int i;
//printf( "Adding triv %d.         %d\n", Abc_ObjRegular(pNode)->Id, (int)pSat->sat_solver_stats.clauses );
    vVars->nSize = 0;
    Vec_PtrForEachEntry( Abc_Obj_t *, vNodes, pNode, i )
        Vec_IntPush( vVars, toLitCond( (int)(ABC_PTRINT_T)Abc_ObjRegular(pNode)->pCopy, Abc_ObjIsComplement(pNode) ) );
//    Vec_IntPush( vVars, toLitCond( (int)Abc_ObjRegular(pNode)->Id, Abc_ObjIsComplement(pNode) ) );
    return sat_solver_addclause_Test2( pSat, vVars->pArray, vVars->pArray + vVars->nSize );
}


int Abc_NtkClauseMux( sat_solver * pSat, Abc_Obj_t * pNode, Abc_Obj_t * pNodeC, Abc_Obj_t * pNodeT, Abc_Obj_t * pNodeE, Vec_Int_t * vVars )
{
    int VarF, VarI, VarT, VarE, fCompT, fCompE;
//printf( "Adding mux %d.         %d\n", pNode->Id, (int)pSat->sat_solver_stats.clauses );

    assert( !Abc_ObjIsComplement( pNode ) );
    assert( Abc_NodeIsMuxType( pNode ) );
    // get the variable numbers
    VarF = (int)(ABC_PTRINT_T)pNode->pCopy;
    VarI = (int)(ABC_PTRINT_T)pNodeC->pCopy;
    VarT = (int)(ABC_PTRINT_T)Abc_ObjRegular(pNodeT)->pCopy;
    VarE = (int)(ABC_PTRINT_T)Abc_ObjRegular(pNodeE)->pCopy;
//    VarF = (int)pNode->Id;
//    VarI = (int)pNodeC->Id;
//    VarT = (int)Abc_ObjRegular(pNodeT)->Id;
//    VarE = (int)Abc_ObjRegular(pNodeE)->Id;

    // get the complementation flags
    fCompT = Abc_ObjIsComplement(pNodeT);
    fCompE = Abc_ObjIsComplement(pNodeE);

    // f = ITE(i, t, e)
    // i' + t' + f
    // i' + t  + f'
    // i  + e' + f
    // i  + e  + f'
    // create four clauses
    vVars->nSize = 0;
    Vec_IntPush( vVars, toLitCond(VarI,  1) );
    Vec_IntPush( vVars, toLitCond(VarT,  1^fCompT) );
    Vec_IntPush( vVars, toLitCond(VarF,  0) );
    if ( !sat_solver_addclause_Test2( pSat, vVars->pArray, vVars->pArray + vVars->nSize ) )
        return 0;
    vVars->nSize = 0;
    Vec_IntPush( vVars, toLitCond(VarI,  1) );
    Vec_IntPush( vVars, toLitCond(VarT,  0^fCompT) );
    Vec_IntPush( vVars, toLitCond(VarF,  1) );
    if ( !sat_solver_addclause_Test2( pSat, vVars->pArray, vVars->pArray + vVars->nSize ) )
        return 0;
    vVars->nSize = 0;
    Vec_IntPush( vVars, toLitCond(VarI,  0) );
    Vec_IntPush( vVars, toLitCond(VarE,  1^fCompE) );
    Vec_IntPush( vVars, toLitCond(VarF,  0) );
    if ( !sat_solver_addclause_Test2( pSat, vVars->pArray, vVars->pArray + vVars->nSize ) )
        return 0;
    vVars->nSize = 0;
    Vec_IntPush( vVars, toLitCond(VarI,  0) );
    Vec_IntPush( vVars, toLitCond(VarE,  0^fCompE) );
    Vec_IntPush( vVars, toLitCond(VarF,  1) );
    if ( !sat_solver_addclause_Test2( pSat, vVars->pArray, vVars->pArray + vVars->nSize ) )
        return 0;

    if ( VarT == VarE )
    {
//        assert( fCompT == !fCompE );
        return 1;
    }

    // two additional clauses
    // t' & e' -> f'       t  + e   + f'
    // t  & e  -> f        t' + e'  + f
    vVars->nSize = 0;
    Vec_IntPush( vVars, toLitCond(VarT,  0^fCompT) );
    Vec_IntPush( vVars, toLitCond(VarE,  0^fCompE) );
    Vec_IntPush( vVars, toLitCond(VarF,  1) );
    if ( !sat_solver_addclause_Test2( pSat, vVars->pArray, vVars->pArray + vVars->nSize ) )
        return 0;
    vVars->nSize = 0;
    Vec_IntPush( vVars, toLitCond(VarT,  1^fCompT) );
    Vec_IntPush( vVars, toLitCond(VarE,  1^fCompE) );
    Vec_IntPush( vVars, toLitCond(VarF,  0) );
    return sat_solver_addclause_Test2( pSat, vVars->pArray, vVars->pArray + vVars->nSize );
}


void Abc_NtkCollectSupergate( Abc_Obj_t * pNode, int fStopAtMux, Vec_Ptr_t * vNodes )
{
    int RetValue, i;
    assert( !Abc_ObjIsComplement(pNode) );
    // collect the nodes in the implication supergate
    Vec_PtrClear( vNodes );
    RetValue = Abc_NtkCollectSupergate_rec( pNode, vNodes, 1, fStopAtMux );
    assert( vNodes->nSize > 1 );
    // unmark the visited nodes
    for ( i = 0; i < vNodes->nSize; i++ )
        Abc_ObjRegular((Abc_Obj_t *)vNodes->pArray[i])->fMarkB = 0;
    // if we found the node and its complement in the same implication supergate,
    // return empty set of nodes (meaning that we should use constant-0 node)
    if ( RetValue == -1 )
        vNodes->nSize = 0;
}


int Abc_NtkClauseAnd( sat_solver * pSat, Abc_Obj_t * pNode, Vec_Ptr_t * vSuper, Vec_Int_t * vVars )
{
    int fComp1, Var, Var1, i;
//printf( "Adding AND %d.  (%d)    %d\n", pNode->Id, vSuper->nSize+1, (int)pSat->sat_solver_stats.clauses );

    assert( !Abc_ObjIsComplement( pNode ) );
    assert( Abc_ObjIsNode( pNode ) );

//    nVars = sat_solver_nvars(pSat);
    Var = (int)(ABC_PTRINT_T)pNode->pCopy;
//    Var = pNode->Id;

//    assert( Var  < nVars );
    for ( i = 0; i < vSuper->nSize; i++ )
    {
        // get the predecessor nodes
        // get the complemented attributes of the nodes
        fComp1 = Abc_ObjIsComplement((Abc_Obj_t *)vSuper->pArray[i]);
        // determine the variable numbers
        Var1 = (int)(ABC_PTRINT_T)Abc_ObjRegular((Abc_Obj_t *)vSuper->pArray[i])->pCopy;
//        Var1 = (int)Abc_ObjRegular(vSuper->pArray[i])->Id;

        // check that the variables are in the SAT manager
//        assert( Var1 < nVars );

        // suppose the AND-gate is A * B = C
        // add !A => !C   or   A + !C
    //  fprintf( pFile, "%d %d 0%c", Var1, -Var, 10 );
        vVars->nSize = 0;
        Vec_IntPush( vVars, toLitCond(Var1, fComp1) );
        Vec_IntPush( vVars, toLitCond(Var,  1     ) );
        if ( !sat_solver_addclause_Test2( pSat, vVars->pArray, vVars->pArray + vVars->nSize ) )
            return 0;
    }

    // add A & B => C   or   !A + !B + C
//  fprintf( pFile, "%d %d %d 0%c", -Var1, -Var2, Var, 10 );
    vVars->nSize = 0;
    for ( i = 0; i < vSuper->nSize; i++ )
    {
        // get the predecessor nodes
        // get the complemented attributes of the nodes
        fComp1 = Abc_ObjIsComplement((Abc_Obj_t *)vSuper->pArray[i]);
        // determine the variable numbers
        Var1 = (int)(ABC_PTRINT_T)Abc_ObjRegular((Abc_Obj_t *)vSuper->pArray[i])->pCopy;
//        Var1 = (int)Abc_ObjRegular(vSuper->pArray[i])->Id;
        // add this variable to the array
        Vec_IntPush( vVars, toLitCond(Var1, !fComp1) );
    }
    Vec_IntPush( vVars, toLitCond(Var, 0) );
    return sat_solver_addclause_Test2( pSat, vVars->pArray, vVars->pArray + vVars->nSize );
}


int Abc_NtkCollectSupergate_rec( Abc_Obj_t * pNode, Vec_Ptr_t * vSuper, int fFirst, int fStopAtMux )
{
    int RetValue1, RetValue2, i;
    // check if the node is visited
    if ( Abc_ObjRegular(pNode)->fMarkB )
    {
        // check if the node occurs in the same polarity
        for ( i = 0; i < vSuper->nSize; i++ )
            if ( vSuper->pArray[i] == pNode )
                return 1;
        // check if the node is present in the opposite polarity
        for ( i = 0; i < vSuper->nSize; i++ )
            if ( vSuper->pArray[i] == Abc_ObjNot(pNode) )
                return -1;
        assert( 0 );
        return 0;
    }
    // if the new node is complemented or a PI, another gate begins
    if ( !fFirst )
        if ( Abc_ObjIsComplement(pNode) || !Abc_ObjIsNode(pNode) || Abc_ObjFanoutNum(pNode) > 1 || (fStopAtMux && Abc_NodeIsMuxType(pNode)) )
        {
            Vec_PtrPush( vSuper, pNode );
            Abc_ObjRegular(pNode)->fMarkB = 1;
            return 0;
        }
    assert( !Abc_ObjIsComplement(pNode) );
    assert( Abc_ObjIsNode(pNode) );
    // go through the branches
    RetValue1 = Abc_NtkCollectSupergate_rec( Abc_ObjChild0(pNode), vSuper, 0, fStopAtMux );
    RetValue2 = Abc_NtkCollectSupergate_rec( Abc_ObjChild1(pNode), vSuper, 0, fStopAtMux );
    if ( RetValue1 == -1 || RetValue2 == -1 )
        return -1;
    // return 1 if at least one branch has a duplicate
    return RetValue1 || RetValue2;
}


void Abc_NodeBddToCnf_Test(Abc_Obj_t * pNode, Mem_Flex_t * pMmMan, Vec_Str_t * vCube, int fAllPrimes, char ** ppSop0, char ** ppSop1)
{
    assert( Abc_NtkHasBdd(pNode->pNtk) );
    *ppSop0 = Abc_ConvertBddToSop( pMmMan, (DdManager *)pNode->pNtk->pManFunc, (DdNode *)pNode->pData, (DdNode *)pNode->pData, Abc_ObjFaninNum(pNode), fAllPrimes, vCube, 0 );
    *ppSop1 = Abc_ConvertBddToSop( pMmMan, (DdManager *)pNode->pNtk->pManFunc, (DdNode *)pNode->pData, (DdNode *)pNode->pData, Abc_ObjFaninNum(pNode), fAllPrimes, vCube, 1 );
}


char * Abc_ConvertBddToSop( Mem_Flex_t * pMan, DdManager * dd, DdNode * bFuncOn, DdNode * bFuncOnDc, int nFanins, int fAllPrimes, Vec_Str_t * vCube, int fMode )
{
    int fVerify = 0;
    char * pSop;
    DdNode * bFuncNew, * bCover, * zCover, * zCover0, * zCover1;
    int nCubes = 0, nCubes0, nCubes1, fPhase = 0;

    assert( bFuncOn == bFuncOnDc || Cudd_bddLeq( dd, bFuncOn, bFuncOnDc ) );
    if ( Cudd_IsConstant(bFuncOn) || Cudd_IsConstant(bFuncOnDc) )
    {
        if ( pMan )
            pSop = Mem_FlexEntryFetch( pMan, nFanins + 4 );
        else
            pSop = ABC_ALLOC( char, nFanins + 4 );
        pSop[0] = ' ';
        pSop[1] = '0' + (int)(bFuncOn == Cudd_ReadOne(dd));
        pSop[2] = '\n';
        pSop[3] = '\0';
        return pSop;
    }

    if ( fMode == -1 )
    { // try both phases
        assert( fAllPrimes == 0 );

        // get the ZDD of the negative polarity
        bCover = Cudd_zddIsop( dd, Cudd_Not(bFuncOnDc), Cudd_Not(bFuncOn), &zCover0 );
        Cudd_Ref( zCover0 );
        Cudd_Ref( bCover );
        Cudd_RecursiveDeref( dd, bCover );
        nCubes0 = Abc_CountZddCubes( dd, zCover0 );

        // get the ZDD of the positive polarity
        bCover = Cudd_zddIsop( dd, bFuncOn, bFuncOnDc, &zCover1 );
        Cudd_Ref( zCover1 );
        Cudd_Ref( bCover );
        Cudd_RecursiveDeref( dd, bCover );
        nCubes1 = Abc_CountZddCubes( dd, zCover1 );

        // compare the number of cubes
        if ( nCubes1 <= nCubes0 )
        { // use positive polarity
            nCubes = nCubes1;
            zCover = zCover1;
            Cudd_RecursiveDerefZdd( dd, zCover0 );
            fPhase = 1;
        }
        else
        { // use negative polarity
            nCubes = nCubes0;
            zCover = zCover0;
            Cudd_RecursiveDerefZdd( dd, zCover1 );
            fPhase = 0;
        }
    }
    else if ( fMode == 0 )
    {
        // get the ZDD of the negative polarity
        if ( fAllPrimes )
        {
            zCover = Extra_zddPrimes( dd, Cudd_Not(bFuncOnDc) );
            Cudd_Ref( zCover );
        }
        else
        {
            bCover = Cudd_zddIsop( dd, Cudd_Not(bFuncOnDc), Cudd_Not(bFuncOn), &zCover );
            Cudd_Ref( zCover );
            Cudd_Ref( bCover );
            Cudd_RecursiveDeref( dd, bCover );
        }
        nCubes = Abc_CountZddCubes( dd, zCover );
        fPhase = 0;
    }
    else if ( fMode == 1 )
    {
        // get the ZDD of the positive polarity
        if ( fAllPrimes )
        {
            zCover = Extra_zddPrimes( dd, bFuncOnDc );
            Cudd_Ref( zCover );
        }
        else
        {
            bCover = Cudd_zddIsop( dd, bFuncOn, bFuncOnDc, &zCover );
            Cudd_Ref( zCover );
            Cudd_Ref( bCover );
            Cudd_RecursiveDeref( dd, bCover );
        }
        nCubes = Abc_CountZddCubes( dd, zCover );
        fPhase = 1;
    }
    else
    {
        assert( 0 );
    }

    if ( nCubes > ABC_MAX_CUBES )
    {
        Cudd_RecursiveDerefZdd( dd, zCover );
        printf( "The number of cubes exceeded the predefined limit (%d).\n", ABC_MAX_CUBES );
        return NULL;
    }

    // allocate memory for the cover
    if ( pMan )
        pSop = Mem_FlexEntryFetch( pMan, (nFanins + 3) * nCubes + 1 );
    else
        pSop = ABC_ALLOC( char, (nFanins + 3) * nCubes + 1 );
    pSop[(nFanins + 3) * nCubes] = 0;
    // create the SOP
    Vec_StrFill( vCube, nFanins, '-' );
    Vec_StrPush( vCube, '\0' );
    Abc_ConvertZddToSop( dd, zCover, pSop, nFanins, vCube, fPhase );
    Cudd_RecursiveDerefZdd( dd, zCover );

    // verify
    if ( fVerify )
    {
        bFuncNew = Abc_ConvertSopToBdd( dd, pSop, NULL );  Cudd_Ref( bFuncNew );
        if ( bFuncOn == bFuncOnDc )
        {
            if ( bFuncNew != bFuncOn )
                printf( "Verification failed.\n" );
        }
        else
        {
            if ( !Cudd_bddLeq(dd, bFuncOn, bFuncNew) || !Cudd_bddLeq(dd, bFuncNew, bFuncOnDc) )
                printf( "Verification failed.\n" );
        }
        Cudd_RecursiveDeref( dd, bFuncNew );
    }
    return pSop;
}


int Abc_ConvertZddToSop( DdManager * dd, DdNode * zCover, char * pSop, int nFanins, Vec_Str_t * vCube, int fPhase )
{
    int nCubes = 0;
    Abc_ConvertZddToSop_rec( dd, zCover, pSop, nFanins, vCube, fPhase, &nCubes );
    return nCubes;
}


void Abc_ConvertZddToSop_rec( DdManager * dd, DdNode * zCover, char * pSop, int nFanins, Vec_Str_t * vCube, int fPhase, int * pnCubes )
{
    DdNode * zC0, * zC1, * zC2;
    int Index;

    if ( zCover == dd->zero )
        return;
    if ( zCover == dd->one )
    {
        char * pCube;
        pCube = pSop + (*pnCubes) * (nFanins + 3);
        sprintf( pCube, "%s %d\n", vCube->pArray, fPhase );
        (*pnCubes)++;
        return;
    }
    Index = zCover->index/2;
    assert( Index < nFanins );
    extraDecomposeCover( dd, zCover, &zC0, &zC1, &zC2 );
    vCube->pArray[Index] = '0';
    Abc_ConvertZddToSop_rec( dd, zC0, pSop, nFanins, vCube, fPhase, pnCubes );
    vCube->pArray[Index] = '1';
    Abc_ConvertZddToSop_rec( dd, zC1, pSop, nFanins, vCube, fPhase, pnCubes );
    vCube->pArray[Index] = '-';
    Abc_ConvertZddToSop_rec( dd, zC2, pSop, nFanins, vCube, fPhase, pnCubes );
}


int Abc_CountZddCubes( DdManager * dd, DdNode * zCover )
{
    int nCubes = 0;
    Abc_CountZddCubes_rec( dd, zCover, &nCubes );
    return nCubes;
}


void Abc_CountZddCubes_rec( DdManager * dd, DdNode * zCover, int * pnCubes )
{
    DdNode * zC0, * zC1, * zC2;
    if ( zCover == dd->zero )
        return;
    if ( zCover == dd->one )
    {
        (*pnCubes)++;
        return;
    }
    if ( (*pnCubes) > ABC_MAX_CUBES )
        return;
    extraDecomposeCover( dd, zCover, &zC0, &zC1, &zC2 );
    Abc_CountZddCubes_rec( dd, zC0, pnCubes );
    Abc_CountZddCubes_rec( dd, zC1, pnCubes );
    Abc_CountZddCubes_rec( dd, zC2, pnCubes );
}


DdNode * Abc_ConvertSopToBdd( DdManager * dd, char * pSop, DdNode ** pbVars )
{
    DdNode * bSum, * bCube, * bTemp, * bVar;
    char * pCube;
    int nVars, Value, v;

    // start the cover
    nVars = Abc_SopGetVarNum(pSop);
    bSum = Cudd_ReadLogicZero(dd);   Cudd_Ref( bSum );
    if ( Abc_SopIsExorType(pSop) )
    {
        for ( v = 0; v < nVars; v++ )
        {
            bSum  = Cudd_bddXor( dd, bTemp = bSum, pbVars? pbVars[v] : Cudd_bddIthVar(dd, v) );   Cudd_Ref( bSum );
            Cudd_RecursiveDeref( dd, bTemp );
        }
    }
    else
    {
        // check the logic function of the node
        Abc_SopForEachCube( pSop, nVars, pCube )
        {
            bCube = Cudd_ReadOne(dd);   Cudd_Ref( bCube );
            Abc_CubeForEachVar( pCube, Value, v )
            {
                if ( Value == '0' )
                    bVar = Cudd_Not( pbVars? pbVars[v] : Cudd_bddIthVar( dd, v ) );
                else if ( Value == '1' )
                    bVar = pbVars? pbVars[v] : Cudd_bddIthVar( dd, v );
                else
                    continue;
                bCube  = Cudd_bddAnd( dd, bTemp = bCube, bVar );   Cudd_Ref( bCube );
                Cudd_RecursiveDeref( dd, bTemp );
            }
            bSum = Cudd_bddOr( dd, bTemp = bSum, bCube );
            Cudd_Ref( bSum );
            Cudd_RecursiveDeref( dd, bTemp );
            Cudd_RecursiveDeref( dd, bCube );
        }
    }
    // complement the result if necessary
    bSum = Cudd_NotCond( bSum, !Abc_SopGetPhase(pSop) );
    Cudd_Deref( bSum );
    return bSum;
}


int sat_solver_addclause_Test2(sat_solver* s, lit* begin, lit* end)
{
    lit *i,*j;
    int maxvar;
    lit last;
    assert( begin < end );
    if ( s->fPrintClause )
    {
        for ( i = begin; i < end; i++ )
            printf( "%s%d ", (*i)&1 ? "!":"", (*i)>>1 );
        printf( "\n" );
    }

    veci_resize( &s->temp_clause, 0 );
    for ( i = begin; i < end; i++ )
        veci_push( &s->temp_clause, *i );
    begin = veci_begin( &s->temp_clause );
    end = begin + veci_size( &s->temp_clause );

    // insertion sort
    maxvar = lit_var(*begin);
    for (i = begin + 1; i < end; i++){
        lit l = *i;
        maxvar = lit_var(l) > maxvar ? lit_var(l) : maxvar;
        for (j = i; j > begin && *(j-1) > l; j--)
            *j = *(j-1);
        *j = l;
    }
    sat_solver_setnvars(s,maxvar+1);

    ///////////////////////////////////
    // add clause to internal storage
    if ( s->pStore )
    {
        int RetValue = Sto_ManAddClause( (Sto_Man_t *)s->pStore, begin, end );
        assert( RetValue );
        (void) RetValue;
    }
    ///////////////////////////////////

    // delete duplicates
    last = lit_Undef;
    for (i = j = begin; i < end; i++){
        //printf("lit: "L_LIT", value = %d\n", L_lit(*i), (lit_sign(*i) ? -s->assignss[lit_var(*i)] : s->assignss[lit_var(*i)]));
        if (*i == lit_neg(last) || var_value(s, lit_var(*i)) == lit_sign(*i))
            return true;   // tautology
        else if (*i != last && var_value(s, lit_var(*i)) == varX)
            last = *j++ = *i;
    }
//    j = i;

    if (j == begin)          // empty clause
        return false;

    if (j - begin == 1) // unit clause
        return sat_solver_enqueue(s,*begin,0);

    // create new clause
    sat_solver_clause_new(s,begin,j,0);
    return true;
}


int sat_solver_solve_Test(sat_solver* s, lit* begin, lit* end, ABC_INT64_T nConfLimit, ABC_INT64_T nInsLimit, ABC_INT64_T nConfLimitGlobal, ABC_INT64_T nInsLimitGlobal)
{
    lbool status;
    lit * i;
    ////////////////////////////////////////////////
    if ( s->fSolved )
    {
        if ( s->pStore )
        {
            int RetValue = Sto_ManAddClause( (Sto_Man_t *)s->pStore, NULL, NULL );
            assert( RetValue );
            (void) RetValue;
        }
        return l_False;
    }
    ////////////////////////////////////////////////

    if ( s->fVerbose )
        printf( "Running SAT solver with parameters %d and %d and %d.\n", s->nLearntStart, s->nLearntDelta, s->nLearntRatio );

    sat_solver_set_resource_limits( s, nConfLimit, nInsLimit, nConfLimitGlobal, nInsLimitGlobal );

#ifdef SAT_USE_ANALYZE_FINAL
    // Perform assumptions:
    s->root_level = 0;
    for ( i = begin; i < end; i++ )
        if ( !sat_solver_push(s, *i) )
        {
            sat_solver_canceluntil(s,0);
            s->root_level = 0;
            return l_False;
        }
    assert(s->root_level == sat_solver_dl(s));
#else
    //printf("solve: "); printlits(begin, end); printf("\n");
    for (i = begin; i < end; i++){
//        switch (lit_sign(*i) ? -s->assignss[lit_var(*i)] : s->assignss[lit_var(*i)]){
        switch (var_value(s, *i)) {
        case var1: // l_True:
            break;
        case varX: // l_Undef
            sat_solver_decision(s, *i);
            if (sat_solver_propagate(s) == 0)
                break;
            // fallthrough
        case var0: // l_False
            sat_solver_canceluntil(s, 0);
            return l_False;
        }
    }
    s->root_level = sat_solver_dl(s);
#endif

    status = sat_solver_solve_internal_Test(s);

    sat_solver_canceluntil(s,0);
    s->root_level = 0;

    ////////////////////////////////////////////////
    if ( status == l_False && s->pStore )
    {
        int RetValue = Sto_ManAddClause( (Sto_Man_t *)s->pStore, NULL, NULL );
        assert( RetValue );
        (void) RetValue;
    }
    ////////////////////////////////////////////////
    return status;
}


int sat_solver_solve_internal_Test(sat_solver* s)
{
    lbool status = l_Undef;
    int restart_iter = 0;
    veci_resize(&s->unit_lits, 0);
    s->nCalls++;

    if (s->verbosity >= 1){
        printf("==================================[MINISAT]===================================\n");
        printf("| Conflicts |     ORIGINAL     |              LEARNT              | Progress |\n");
        printf("|           | Clauses Literals |   Limit Clauses Literals  Lit/Cl |          |\n");
        printf("==============================================================================\n");
    }

    while (status == l_Undef){
        ABC_INT64_T nof_conflicts;
        double Ratio = (s->stats.learnts == 0)? 0.0 :
            s->stats.learnts_literals / (double)s->stats.learnts;
        if ( s->nRuntimeLimit && Abc_Clock() > s->nRuntimeLimit )
            break;
        if (s->verbosity >= 1)
        {
            printf("| %9.0f | %7.0f %8.0f | %7.0f %7.0f %8.0f %7.1f | %6.3f %% |\n",
                (double)s->stats.conflicts,
                (double)s->stats.clauses,
                (double)s->stats.clauses_literals,
                (double)0,
                (double)s->stats.learnts,
                (double)s->stats.learnts_literals,
                Ratio,
                s->progress_estimate*100);
            fflush(stdout);
        }
        nof_conflicts = (ABC_INT64_T)( 100 * luby(2, restart_iter++) );
        status = sat_solver_search(s, nof_conflicts);
        // quit the loop if reached an external limit
        if ( s->nConfLimit && s->stats.conflicts > s->nConfLimit )
            break;
        if ( s->nInsLimit  && s->stats.propagations > s->nInsLimit )
            break;
        if ( s->nRuntimeLimit && Abc_Clock() > s->nRuntimeLimit )
            break;
        if ( s->pFuncStop && s->pFuncStop(s->RunId) )
            break;
    }
    if (s->verbosity >= 1)
        printf("==============================================================================\n");

    sat_solver_canceluntil(s,s->root_level);
    // save variable values
    if ( status == l_True && s->user_vars.size )
    {
        int v;
        for ( v = 0; v < s->user_vars.size; v++ )
            veci_push(&s->user_values, sat_solver_var_value(s, s->user_vars.ptr[v]));
    }
    return status;
}


double luby(double y, int x)
{
    int size, seq;
    for (size = 1, seq = 0; size < x+1; seq++, size = 2*size + 1);
    while (size-1 != x){
        size = (size-1) >> 1;
        seq--;
        x = x % size;
    }
    return pow(y, (double)seq);
}
