#include "cpu/o3/SpecCheck.hh"

#include <climits>

#include "debug/SpecCheck.hh"

namespace gem5 {

namespace o3 {

// STATS
int numFlushed = 0;
int numUniqFlushed = 0; // Non vulnerable
int numVulnerable = 0;
int numUniqVulnerable = 0;

// STATE
int currentFsmState = Q_INIT;
unsigned long long savedPC;
unsigned long long mainStart = ULLONG_MAX;
unsigned long long mainEnd = 0;
bool inMain = false;

// TABLES
std::vector<unsigned long long>flushed_pcs;
std::vector<unsigned long long>vuln_pcs;
std::vector<PhysRegIdPtr>taint_table;

int in_flushed(unsigned long long pc) {
    return (std::find(flushed_pcs.begin(), flushed_pcs.end(), pc)
             != flushed_pcs.end());
}

int in_vulnerable(unsigned long long pc) {
    return (std::find(vuln_pcs.begin(), vuln_pcs.end(), pc)
             != vuln_pcs.end());
}

int in_taint_table(PhysRegIdPtr reg) {
    return (std::find(taint_table.begin(), taint_table.end(), reg)
             != taint_table.end());
}

void set_taint(PhysRegIdPtr reg) {
    taint_table.push_back(reg);
}

void clear_taint_table() {
    taint_table.clear();
}

int is_load(StaticInstPtr staticInst) {
    return staticInst->isLoad();
}

void encountered_main(unsigned long long addr, size_t size) {
    mainStart = addr;
    mainEnd = mainStart + size;
    printf("Main found! Start: 0x%08llx, End: 0x%08llx\n", mainStart, mainEnd);
}

int is_micro_visible(StaticInstPtr staticInst) {
    return (staticInst->isLoad() ||
            staticInst->isStore() ||
            staticInst->isFloating() ||
            staticInst->isControl() ||
            staticInst->isCall() ||
            staticInst->isReturn() ||
            staticInst->isCondCtrl());
}

int consume_instruction(std::string inst,
            unsigned long long PC,
            bool commit,
            bool issue,
            bool complete,
            StaticInstPtr staticInst,
            DynInstPtr dynInst) {

    // Only run if we have started the main fn
    if (PC == mainStart) {
        inMain = true;
    }
    if (inMain && (PC == mainEnd || PC == mainEnd - 1)) {
        inMain = false;
    }
    if (!inMain || staticInst->isNop()) {
        return 0;
    }

    size_t numSrcs = dynInst->numSrcs();
    size_t numDsts = dynInst->numDests();

    PhysRegIdPtr dest = 0;
    PhysRegIdPtr src1 = 0;
    PhysRegIdPtr src2 = 0;

    if (numDsts > 0)
        dest = dynInst->renamedDestIdx(0);
    if (numSrcs > 0) {
        src1 = dynInst->renamedSrcIdx(0);
        if (numSrcs > 1) {
            src2 = dynInst->renamedSrcIdx(1);
        }
    }

    if (currentFsmState == Q_INIT) {
        clear_taint_table();
        savedPC = -1;

            // beginning of misspeculation window
            if (commit == 0) {

                savedPC = PC;
                numFlushed++;

                // check if flushed window is unique
                if (!in_flushed(savedPC)) {
                    flushed_pcs.push_back(savedPC);
                    numUniqFlushed = flushed_pcs.size();
                }

                // Completed memroy load
                if (is_load(staticInst) && complete != 0 && dest != 0) {
                        set_taint(dest);
                        currentFsmState = Q_2;
                }
                // Non completed memory load or non memory operation
                else {
                        currentFsmState = Q_1;
                }
            }

    }

    else if (currentFsmState == Q_1) {

        // Retired instruction
        // goto Q_INIT
        if (commit != 0) {
            currentFsmState = Q_INIT;
        }
        // flushed instruction
        else {
            // If completed memory inst:
            // add destination register to register array
            // goto Q_2
            if (is_load(staticInst) && complete != 0 && dest != 0) {
                set_taint(dest);
                currentFsmState = Q_2;
            }
            // Any other flushed instruction does not change state
        }
    }

    else if (currentFsmState == Q_2) {

        // Retired instruction
        if (commit != 0) {
            currentFsmState = Q_INIT;
        }
        // Flushed instruction
        else {
            // If instruction is micro visible and either of
            // its sources are in the taint table, goto accept
            if (issue!= 0 && is_micro_visible(staticInst) &&
               ((src1 != 0 && in_taint_table(src1)) ||
               (src2 != 0 && in_taint_table(src2)))) {
                currentFsmState = Q_ACC;
            }
            // If instruction is a load OR either of its
            // sources are in the taint table but the instruction
            // is not micro visible, add the destination to the
            // taint table
            else if (issue != 0 && (is_load(staticInst) ||
                    (src1 != 0 && in_taint_table(src1)) ||
                    (src2 != 0 && in_taint_table(src2)))) {
                set_taint(dest);
            }
        }
    }

    if (currentFsmState == Q_ACC) {
        numVulnerable++;
        // Check if misspeculated window is not already in list
        if (!in_vulnerable(savedPC)) {
            vuln_pcs.push_back(savedPC);
            numUniqVulnerable = vuln_pcs.size();
            printf("Potential vulnerable window found at: 0x%08llx\n",
                   savedPC);
        }
        currentFsmState = Q_INIT;
    }

    return 0;
}

} // namespace o3
} // namespace gem5
