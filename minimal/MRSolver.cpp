
#include <minisat/core/Dimacs.h>
#include "MRSolver.h"



bool MRSolver::solve() {
    if (compute_model_count>1){
        return result;
    }
    compute_model_count=1;
    Minisat::vec<Minisat::Lit> litsT;
    copyToClauses(this->clauses);
    while (solver.solve()){
        result =true;
        litsT.clear();
        model.clear();
        for (int i = 0; i < solver.nVars(); i++) {
            Minisat::lbool value =solver.model[i];
            model.push(value);
            if (value == Minisat::l_True) {
                litsT.push(~Minisat::mkLit(i));
            } else if (value == Minisat::l_False) {
                solver.addClause(~Minisat::mkLit(i));
            }
        }
        if (this->check()){
            break;
        }
        ++compute_model_count;
        solver.addClause(litsT);
    }
    return result;
}

bool MRSolver::check() {
    ++check_model_count;
    Minisat::vec<Minisat::lbool> model;
    int modeSize=0;
    for (int j = 0; j < solver.nVars(); ++j) {
        Minisat::lbool& value = solver.model[j];
        model.push(value);
        if (value==Minisat::l_True){
            ++modeSize;
        }
    }
    mr(this->clauses,model);
    Minisat::vec<Minisat::CRef> clauses;
    copyToClauses(this->clauses,clauses);
    StronglyConnectedGraph graph;
    createGraph(clauses,graph);
    int node= 0;
    int limit =solver.nVars();
    Minisat::vec<int> S;
    Minisat::vec<Minisat::CRef > ts;
    node = graph.getEmptyInDegree();
    while (node>-1){
        if (node>=limit){
            graph.remove(node);
            node = graph.getEmptyInDegree();
            continue;
        }
        computeS(graph,S);
        computeTS(clauses,graph.getComponent(node),ts);
        if (compute(ts,S)){
            break;
        }
        //M=M -S
        for (int i = 0; i < S.size(); ++i) {
            int key=S[i];
            if (model[key]==Minisat::l_True){
                --modeSize;
                model[key]=Minisat::l_False;
            }
        }
        reduce(clauses,S);
        graph.remove(node);
        node = graph.getEmptyInDegree();
    }

    for (int l = 0; l < clauses.size(); ++l) {
        ca.free(clauses[l]);
    }
    return modeSize==0;
}

void MRSolver::copyToClauses(Minisat::vec<Minisat::CRef> &clauses) {
    auto it = solver.clausesBegin();
    Minisat::vec<Minisat::Lit> list;
    while (it!=solver.clausesEnd()){
        list.clear();
        for (int i = 0; i < (*it).size(); ++i) {
            list.push((*it)[i]);
        }
        Minisat::sort(list);
        Minisat::CRef crf=ca.alloc(list);
        clauses.push(crf);
        ++it;
    }

}

bool MRSolver::mr(Minisat::vec<Minisat::CRef> &clauses,Minisat::vec<Minisat::lbool> &model) {
    int clauseSize=clauses.size();
    int clauseIndex=0;
    int clauseLastIndex=0;
    for (; clauseLastIndex < clauseSize; ++clauseLastIndex) {
        Minisat::CRef c = clauses[clauseLastIndex];
        clauses[clauseIndex] = c;
        Minisat::Clause *clause = ca.lea(c);
        int atomLastIndex = 0;
        int atomIndex = 0;
        int atomSize = clause->size();
        bool del = false;
        for (; atomLastIndex < atomSize; ++atomLastIndex) {
            auto d = (*clause)[atomLastIndex];
            (*clause)[atomIndex] = d;
            bool sign = Minisat::sign(d);
            int value = Minisat::var(d);
            if (sign && model[value] == Minisat::l_False) {
                del = true;
                break;
            }
            if (model[value] == Minisat::l_True) {
                ++atomIndex;
            }
        }
        if (del|| atomIndex==0) {
            ca.free(c);
            continue;
        }
        for (; atomIndex < atomSize; ++atomIndex) {
            clause->pop();
        }
        ++clauseIndex;

    }
    for (; clauseIndex < clauseSize;++clauseIndex) {
        clauses.pop();
    }
    return true;
}

bool MRSolver::createGraph(Minisat::vec<Minisat::CRef>& clauses,StronglyConnectedGraph &graph) {
    Graph gr;
    int base= solver.nVars();
    for (int i = 0; i < clauses.size(); ++i) {
        int key=base+i;
        Minisat::Clause * clause= ca.lea(clauses[i]);
        for (int j = 0; j < clause->size(); ++j) {
             Minisat::Lit& lit= clause->operator[](j);
             int value=Minisat::var(lit);
             if (Minisat::sign(lit)){
                 gr.addPoint(value,key);
             } else{
                 gr.addPoint(key,value);
             }
        }
    }
    gr.done();
    graph.compute(&gr);
    graph.done();
    return true;
}

void MRSolver::copyToClauses(Minisat::vec<Minisat::CRef> &source,
        Minisat::vec<Minisat::CRef> &dist) {
    for (int i = 0; i < source.size(); ++i) {
         Minisat::Clause &clause= ca[source[i]];
         Minisat::CRef  c = ca.alloc(clause);
         dist.push(c);
    }
}

bool MRSolver::compute(Minisat::vec<Minisat::CRef> &ts, Minisat::vec<int> &S) {
    if (ts.size() == 0) {
        return S.size() != 0;
    }
    if (S.size() == 1) {
        for (int i = 0; i < ts.size(); ++i) {
            Minisat::Clause *clause = ca.lea(ts[i]);
            for (int j = 0; j < clause->size(); ++j) {
                if (Minisat::sign((*clause)[j])) {
                    return true;
                }
            }
        }
        return false;
    }
    Minisat::Solver solver;
    Minisat::vec<Minisat::Lit> lits;
    for (int i = 0; i < this->solver.nVars(); i++) {
        solver.newVar();
    }

    for (int i = 0; i < ts.size(); ++i) {
        lits.clear();
        Minisat::Clause &clause = ca[ts[i]];
        if (clause.size() == 0)continue;
        for (int j = 0; j < clause.size(); ++j) {
            Minisat::Lit lit = clause[j];
            lits.push(lit);
        }
        solver.addClause(lits);
    }
    lits.clear();
    for (int k = 0; k < S.size(); ++k) {
        lits.push(~Minisat::mkLit(S[k]));
    }
    solver.addClause(lits);
    ++compute_mini_model_count;
    return solver.solve();
}

bool MRSolver::isIntersection(Minisat::vec<int> &S, Minisat::Clause &clause) {
    int clauseSize=clause.size();
    int clauseIndex=0;
    int sIndex=0;
    int sSize=S.size();
    int s;
    Minisat::Lit lis;
    int var;
    while (sIndex<sSize&&clauseIndex<clauseSize){
        lis=(clause)[clauseIndex];
        var= Minisat::var(lis);
        s=S[sIndex];
        if (Minisat::sign(lis)){
            ++clauseIndex;
            continue;
        }
        if (var==s){
            return true;
        }
        if(var>s){
            ++sIndex;
        } else{
            ++clauseIndex;
        }
    }
    return false;
}

void MRSolver::clear(Minisat::Clause &clause, Minisat::vec<int> &S) {
    int clauseSize=clause.size();
    int clauseIndex=0;
    int clauseLastIndex=0;
    int sIndex=0;
    int sSize=S.size();
    int s;

    while (sIndex<sSize&&clauseLastIndex<clauseSize){
        Minisat::Lit lis=(clause)[clauseLastIndex];
        (clause)[clauseIndex]=lis;
        s=S[sIndex];
        int var= Minisat::var(lis);
        if (var==s) {
            // 删除
            ++clauseLastIndex;
            ++sIndex;
            continue;
        }
        if(var>s){
            ++sIndex;
        } else{
            ++clauseLastIndex;
            ++clauseIndex;
        }
    }
    for (; clauseLastIndex <clauseSize ; ++clauseIndex,++clauseLastIndex) {
        (clause)[clauseIndex]= (clause)[clauseLastIndex];
    }
    for ( ;clauseIndex <clauseSize ; ++clauseIndex) {
        clause.pop();
    }
}

void MRSolver::reduce(Minisat::vec<Minisat::CRef> &clauses, Minisat::vec<int> &S) {
    int clauseSize = clauses.size();
    int clauseIndex=0;
    int clauseLastIndex=0;
    for (; clauseLastIndex < clauseSize;++clauseLastIndex) {
        Minisat::CRef crf = clauses[clauseLastIndex];
        clauses[clauseIndex]=crf;
        Minisat::Clause &clause = ca[crf];
        if (isIntersection(S, clause)) {
            // 删除
            ca.free(crf);
            continue;
        }
        clear(clause,S);
        if (clause.size()==0){
            ca.free(crf);
            continue;
        }
        ++clauseIndex;
    }
    for(;clauseIndex<clauseSize;++clauseIndex){
        clauses.pop();
    }

}

void
MRSolver::computeTS(Minisat::vec<Minisat::CRef> &clauses, std::vector<int> &source, Minisat::vec<Minisat::CRef> &ts) {
    ts.clear();
    for (int i = 0; i < clauses.size(); ++i) {
        Minisat::Clause* clause= ca.lea(clauses[i]);
        if (isSub(source,clause,solver.nVars())){
            ts.push(clauses[i]);
        }
    }
}

void MRSolver::printStats() {
    BaseSolver::_printStats();
    printf("ComputeModelCount     : %d \n", compute_model_count);
    printf("CheckModelCount       : %d \n", check_model_count);
    printf("ComputeMIniModelCount : %d \n", compute_mini_model_count);
}