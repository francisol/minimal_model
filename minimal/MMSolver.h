

#ifndef MINISAT_MMSOLVER_H
#define MINISAT_MMSOLVER_H

#include "SpaceDefine.h"
#include "BaseSolver.h"
namespace Minimal {
    class MMSolver : public BaseSolver {

    public:
        bool solve();

        void printStats();

        SOlVER_NAMESPACE::vec<SOlVER_NAMESPACE::lbool> &getModel() {
            return model;
        }

        inline void addClause(SOlVER_NAMESPACE::vec<SOlVER_NAMESPACE::Lit> &lits) {solver.addClause_(lits); }

    private:
        int compute_model_count = 0;
        bool result = false;
        SOlVER_NAMESPACE::vec<SOlVER_NAMESPACE::lbool> model;
    };
}

#endif //MINISAT_MMSOLVER_H
