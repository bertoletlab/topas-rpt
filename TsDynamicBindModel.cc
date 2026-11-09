// Extra Class for TsRadioactiveTimeGeneratorBind
//
// Created by Daniel Suarez and Victor Valladolid on 2/2/24.
//

#include "TsDynamicBindModel.hh"

#include <algorithm>

TsDynamicBindModel::TsDynamicBindModel(G4double koff, G4double kon, G4double kint, G4double krec, G4double kefflux,
                        G4double Ri, G4double totalConcentrationInit, const std::vector<G4double>& washoutTimes) :
        fkoff(koff), fkon(kon), fkint(kint), fkrec(krec), fkefflux(kefflux), fRi(Ri), // Kinetic parameters
        fTotalConcentrationInit(totalConcentrationInit), fWashoutTimes(washoutTimes)
{
    std::sort(fWashoutTimes.begin(), fWashoutTimes.end());
    fWashoutTimes.erase(std::unique(fWashoutTimes.begin(), fWashoutTimes.end()), fWashoutTimes.end());
}

//Destructor
TsDynamicBindModel::~TsDynamicBindModel() {}


//Update method until the end of the time from 0
void TsDynamicBindModel::UpdateModel2compartmentsCell(G4double timeInit, G4double timeEnd, G4double Ei, G4double Ii,
                                                    G4bool IsWrite, G4String filename) {
    
    //If is write, open file to write
    if (IsWrite) {
        fFile.open(filename, std::ofstream::out | std::ofstream::trunc);
        // Write headers once at the beginning (place this in your initialization if needed)
        fFile << "Time\tE\tI\tFracRemaining" << G4endl;
    }

    //Setting parameter for iteration
    const G4double dtMax = 0.2; // Euler step in seconds; shortened to land exactly on washout times and on timeEnd
    G4double ttime = timeInit; // iterator for time
    fEconcentration = Ei; // Medium 
    fIconcentration = Ii; // Cytoplasm

    // Loop for differential equations calculations
    while (ttime < timeEnd) {

        const G4double tNext = NextTime(ttime, timeEnd, dtMax);
        const G4double dt = tNext - ttime;

        // Compute time differentials of E (medium) and I (cytoplasm), and update their values
        G4double dE = dt * (-fkon * fEconcentration * (fRi - fIconcentration) + fkoff * fIconcentration);
        G4double dI = dt * (fkon * fEconcentration * (fRi - fIconcentration) - fkoff * fIconcentration);  
        fEconcentration += dE;
        fIconcentration += dI;
        ttime = tNext;
        // A washout empties the medium at exactly its time (the step was cut to land on it). Applying it at
        // the end of a step gives each integration the interval (timeInit, timeEnd], so a washout on a step
        // boundary is already in the state returned for that boundary. No state is kept between calls.
        ApplyWashoutIfDue(ttime);

        // In case of washout, computing the fraction of isotopes remaining
        fFractionOfIsotopesRemaining = (fEconcentration + fIconcentration) / fTotalConcentrationInit;

        // Options to write the RPT distirbution in an output file 
        if (IsWrite) {
            // Write data values with tab separation
            fFile << ttime                                 << "\t"
                << fEconcentration / fTotalConcentrationInit     << "\t"
                << fIconcentration / fTotalConcentrationInit     << "\t"
                << fFractionOfIsotopesRemaining         << G4endl;
        }

    }

}

void TsDynamicBindModel::UpdateModel2compartmentsNucleus(G4double timeInit, G4double timeEnd, G4double Ei, G4double Ni,
                                                    G4bool IsWrite, G4String filename) {
    
    //If is write, open file to write
    if (IsWrite) {
        fFile.open(filename, std::ofstream::out | std::ofstream::trunc);
        // Write headers once at the beginning (place this in your initialization if needed)
        fFile << "Time\tE\tN\tFracRemaining" << G4endl;
    }

    //Setting parameter for iteration
    const G4double dtMax = 0.2; // Euler step in seconds; shortened to land exactly on washout times and on timeEnd
    G4double ttime = timeInit; // iterator for time
    fEconcentration = Ei; // Medium 
    fNconcentration = Ni; // Cytoplasm

    // Loop for differential equations calculations
    while (ttime < timeEnd) {

        const G4double tNext = NextTime(ttime, timeEnd, dtMax);
        const G4double dt = tNext - ttime;

        // Compute time differentials of E (medium) and I (cytoplasm), and update their values
        G4double dE = dt * (-fkon * fEconcentration * (fRi - fNconcentration) + fkoff * fNconcentration);
        G4double dN = dt * (fkon * fEconcentration * (fRi - fNconcentration) - fkoff * fNconcentration);  
        fEconcentration += dE;
        fNconcentration += dN;
        ttime = tNext;
        // A washout empties the medium at exactly its time (the step was cut to land on it). Applying it at
        // the end of a step gives each integration the interval (timeInit, timeEnd], so a washout on a step
        // boundary is already in the state returned for that boundary. No state is kept between calls.
        ApplyWashoutIfDue(ttime);

        // In case of washout, computing the fraction of isotopes remaining
        fFractionOfIsotopesRemaining = (fEconcentration + fNconcentration) / fTotalConcentrationInit;

        // Options to write the RPT distirbution in an output file 
        if (IsWrite) {
            // Write data values with tab separation
            fFile << ttime                                 << "\t"
                << fEconcentration / fTotalConcentrationInit     << "\t"
                << fNconcentration / fTotalConcentrationInit     << "\t"
                << fFractionOfIsotopesRemaining         << G4endl;
        }

    }

}


void TsDynamicBindModel::UpdateModel3compartments(G4double timeInit, G4double timeEnd, G4double Ei, G4double Mi, G4double Ii,
                                                    G4double Di, G4bool IsWrite, G4String filename = " ") {

    //If is write, open file to write
    if (IsWrite) {
        fFile.open(filename, std::ofstream::out | std::ofstream::trunc);
        // Write headers once at the beginning (place this in your initialization if needed)
        fFile << "Time\tE\tM\tI\tD\tFracRemaining" << G4endl;
    }

    //Setting parameter for iteration
    const G4double dtMax = 0.2; // Euler step in seconds; shortened to land exactly on washout times and on timeEnd
    G4double ttime = timeInit; // iterator for time
    fEconcentration = Ei; // Medium
    fMconcentration = Mi; // Membrane
    fIconcentration = Ii; // Cytoplasm
    fDconcentration = Di; // Effluxed activity, in the medium but no longer bindable

    // Loop for differential equations calculations
    while (ttime < timeEnd) {

        const G4double tNext = NextTime(ttime, timeEnd, dtMax);
        const G4double dt = tNext - ttime;

        // Compute time differentials of E (medium), M (membrane) and I (cytoplasm), and update their values
        // Receptors are occupied by membrane-bound and internalised ligand (as in the 4-compartment model);
        // unbinding releases membrane-bound ligand; recycling moves ligand from cytoplasm back to the membrane.
        // Effluxed activity stays in the well until a washout, as non-binding activity in the medium (D);
        // it used to leave the simulation, so its decays were never simulated.
        G4double freeReceptors = fRi - fMconcentration - fIconcentration;
        G4double dE = dt * (-fkon * fEconcentration * freeReceptors + fkoff * fMconcentration);
        G4double dM = dt * (fkon * fEconcentration * freeReceptors - fkoff * fMconcentration - fkint * fMconcentration + fkrec * fIconcentration);
        G4double dI = dt * (fkint * fMconcentration - fkrec * fIconcentration - fkefflux * fIconcentration);
        G4double dD = dt * (fkefflux * fIconcentration);
        fEconcentration += dE;
        fMconcentration += dM;
        fIconcentration += dI;
        fDconcentration += dD;

        ttime = tNext;
        // A washout empties the medium at exactly its time (the step was cut to land on it). Applying it at
        // the end of a step gives each integration the interval (timeInit, timeEnd], so a washout on a step
        // boundary is already in the state returned for that boundary. No state is kept between calls.
        ApplyWashoutIfDue(ttime);

        // In case of washout, computing the fraction of isotopes remaining
        fFractionOfIsotopesRemaining = (fEconcentration + fMconcentration + fIconcentration + fDconcentration) / fTotalConcentrationInit;

        // Options to write the RPT distirbution in an output file 
        if (IsWrite) {

        // Write data values with tab separation
        fFile << ttime                                 << "\t"
              << fEconcentration / fTotalConcentrationInit     << "\t"
              << fMconcentration / fTotalConcentrationInit     << "\t"
              << fIconcentration / fTotalConcentrationInit     << "\t"
              << fDconcentration / fTotalConcentrationInit     << "\t"
              << fFractionOfIsotopesRemaining         << G4endl;
        }
    }

}

void TsDynamicBindModel::UpdateModel4compartments(G4double timeInit, G4double timeEnd, G4double Ei, G4double Mi, G4double Ii, G4double Di,
                                                    G4bool IsWrite, G4String filename = " ") {

    //If is write, open file to write
    if (IsWrite) {
        fFile.open(filename, std::ofstream::out | std::ofstream::trunc);
        // Write headers once at the beginning (place this in your initialization if needed)
        fFile << "Time\tE\tM\tI\tD\tFracRemaining" << G4endl;
    }

    //Setting parameter for iteration
    const G4double dtMax = 0.2; // Euler step in seconds; shortened to land exactly on washout times and on timeEnd
    G4double ttime = timeInit; // iterator for time
    fEconcentration = Ei; // Medium
    fMconcentration = Mi; // Membrane
    fIconcentration = Ii; // Cytoplasm
    fDconcentration = Di; // Degraded

    // Loop for differential equations calculations
    while (ttime < timeEnd) {

        const G4double tNext = NextTime(ttime, timeEnd, dtMax);
        const G4double dt = tNext - ttime;

        // Compute time differentials of E (medium), M (membrane), I (cytoplasm) and D (degraded), and update their values
        G4double dE = dt * (-fkon * fEconcentration * (fRi - fMconcentration - fIconcentration - fDconcentration) + fkoff * fMconcentration);
        G4double dM = dt * (fkon * fEconcentration * (fRi - fMconcentration - fIconcentration - fDconcentration) - fkoff * fMconcentration - fkint * fMconcentration + fkrec * fIconcentration);
        G4double dI = dt * (fkint * fMconcentration - fkrec * fIconcentration - fkefflux * fIconcentration);
        G4double dD = dt * (fkefflux * fIconcentration);
        fEconcentration += dE;
        fMconcentration += dM;
        fIconcentration += dI;
        fDconcentration += dD;

        ttime = tNext;
        // A washout empties the medium at exactly its time (the step was cut to land on it). Applying it at
        // the end of a step gives each integration the interval (timeInit, timeEnd], so a washout on a step
        // boundary is already in the state returned for that boundary. No state is kept between calls.
        ApplyWashoutIfDue(ttime);

        // In case of washout, computing the fraction of isotopes remaining
        fFractionOfIsotopesRemaining = (fEconcentration + fMconcentration + fIconcentration + fDconcentration) / fTotalConcentrationInit;

        // Options to write the RPT distirbution in an output file 
        if (IsWrite) {
        // Write data values with tab separation
        fFile << ttime                                 << "\t"
              << fEconcentration / fTotalConcentrationInit     << "\t"
              << fMconcentration / fTotalConcentrationInit     << "\t"
              << fIconcentration / fTotalConcentrationInit     << "\t"
              << fDconcentration / fTotalConcentrationInit     << "\t"
              << fFractionOfIsotopesRemaining         << G4endl;
        }

    }

}

void TsDynamicBindModel::UpdateModelManual(G4double timeInit, G4double timeEnd, G4double Ei, G4double Mi, G4double Ii, G4double Ni, G4double Di) {


    // Setting probabilities
    fEconcentration = Ei;
    fMconcentration = Mi;
    fIconcentration = Ii;
    fNconcentration = Ni;
    fDconcentration = Di;

    // A washout inside (timeInit, timeEnd] empties the medium, as in the integrated models.
    for (G4double tw : fWashoutTimes) {
        if (tw > timeInit && tw <= timeEnd) {
            fEconcentration = 0;
            fDconcentration = 0;
        }
    }

    fFractionOfIsotopesRemaining = (fEconcentration + fMconcentration + fIconcentration + fNconcentration + fDconcentration) / fTotalConcentrationInit;

}


G4double TsDynamicBindModel::NextTime(G4double ttime, G4double timeEnd, G4double dtMax) const {
    G4double tNext = ttime + dtMax;
    // First washout strictly after ttime (washout times are sorted).
    auto it = std::upper_bound(fWashoutTimes.begin(), fWashoutTimes.end(), ttime);
    if (it != fWashoutTimes.end() && *it < tNext) tNext = *it;
    if (timeEnd < tNext) tNext = timeEnd;
    return tNext;
}


void TsDynamicBindModel::ApplyWashoutIfDue(G4double ttime) {
    if (std::binary_search(fWashoutTimes.begin(), fWashoutTimes.end(), ttime)) {
        fEconcentration = 0;
        fDconcentration = 0;
    }
}
