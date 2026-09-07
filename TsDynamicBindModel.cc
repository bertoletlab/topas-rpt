// Extra Class for TsRadioactiveTimeGeneratorBind
//
// Created by Daniel Suarez and Victor Valladolid on 2/2/24.
//

#include "TsDynamicBindModel.hh"

TsDynamicBindModel::TsDynamicBindModel(G4double koff, G4double kon, G4double kint, G4double krec, G4double kefflux,
                        G4double Ri, G4double totalConcentrationInit, G4double washoutTime) :
        fkoff(koff), fkon(kon), fkint(kint), fkrec(krec), fkefflux(kefflux), fRi(Ri), // Kinetic parameters
        fTotalConcentrationInit(totalConcentrationInit), fWashoutTime(washoutTime), fIsWashoutDone(false)

{}

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
    G4double dt = 0.2; // parameters in seconds. Step _ 0.2 seconds
    G4double ttime = timeInit; // iterator for time
    fEconcentration = Ei; // Medium 
    fIconcentration = Ii; // Cytoplasm

    // Loop for differential equations calculations
    while (ttime < timeEnd) {

        // If washout has to be done (time > timeWashout and washout is not already done), concentration at medium is set to 0
        if (!fIsWashoutDone && ttime > fWashoutTime) {
            fEconcentration = 0;
            fIsWashoutDone = true;
        }

        // Compute time differentials of E (medium) and I (cytoplasm), and update their values
        G4double dE = dt * (-fkon * fEconcentration * (fRi - fIconcentration) + fkoff * fIconcentration);
        G4double dI = dt * (fkon * fEconcentration * (fRi - fIconcentration) - fkoff * fIconcentration);  
        fEconcentration += dE;
        fIconcentration += dI;
        // In case of washout, computing the fraction of isotopes remaining
        fFractionOfIsotopesRemaining = (fEconcentration + fIconcentration) / fTotalConcentrationInit;
        ttime += dt;

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
    G4double dt = 0.2; // parameters in seconds. Step _ 0.2 seconds
    G4double ttime = timeInit; // iterator for time
    fEconcentration = Ei; // Medium 
    fNconcentration = Ni; // Cytoplasm

    // Loop for differential equations calculations
    while (ttime < timeEnd) {

        // If washout has to be done (time > timeWashout and washout is not already done), concentration at medium is set to 0
        if (!fIsWashoutDone && ttime > fWashoutTime) {
            fEconcentration = 0;
            fIsWashoutDone = true;
        }

        // Compute time differentials of E (medium) and I (cytoplasm), and update their values
        G4double dE = dt * (-fkon * fEconcentration * (fRi - fNconcentration) + fkoff * fNconcentration);
        G4double dN = dt * (fkon * fEconcentration * (fRi - fNconcentration) - fkoff * fNconcentration);  
        fEconcentration += dE;
        fNconcentration += dN;
        // In case of washout, computing the fraction of isotopes remaining
        fFractionOfIsotopesRemaining = (fEconcentration + fNconcentration) / fTotalConcentrationInit;
        ttime += dt;

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
                                                    G4bool IsWrite, G4String filename = " ") {

    //If is write, open file to write
    if (IsWrite) {
        fFile.open(filename, std::ofstream::out | std::ofstream::trunc);
        // Write headers once at the beginning (place this in your initialization if needed)
        fFile << "Time\tE\tM\tI\tFracRemaining" << G4endl;
    }

    //Setting parameter for iteration
    G4double dt = 0.2; // parameters in seconds. Step = 0.2 seconds
    G4double ttime = timeInit; // iterator for time
    fEconcentration = Ei; // Medium
    fMconcentration = Mi; // Membrane
    fIconcentration = Ii; // Cytoplasm

    // Loop for differential equations calculations
    while (ttime < timeEnd) {

        // If washout has to be done (time > timeWashout and washout is not already done), concentration at medium is set to 0
        if (!fIsWashoutDone && ttime > fWashoutTime) {
            fEconcentration = 0;
            fIsWashoutDone = true;
        }

        // Compute time differentials of E (medium), M (membrane) and I (cytoplasm), and update their values
        G4double dE = dt * (-fkon * fEconcentration * (fRi - fIconcentration) + fkoff * fIconcentration);
        G4double dM = dt * (fkon * fEconcentration * (fRi - fIconcentration) - fkoff * fIconcentration - fkint * fMconcentration);
        G4double dI = dt * (fkint * fMconcentration - fkefflux * fIconcentration + fkrec * fIconcentration);
        fEconcentration += dE;
        fMconcentration += dM;
        fIconcentration += dI;

        // In case of washout, computing the fraction of isotopes remaining
        fFractionOfIsotopesRemaining = (fEconcentration + fMconcentration + fIconcentration) / fTotalConcentrationInit;
        ttime += dt;

        // Options to write the RPT distirbution in an output file 
        if (IsWrite) {

        // Write data values with tab separation
        fFile << ttime                                 << "\t"
              << fEconcentration / fTotalConcentrationInit     << "\t"
              << fMconcentration / fTotalConcentrationInit     << "\t"
              << fIconcentration / fTotalConcentrationInit     << "\t"
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
    G4double dt = 0.2; // parameters in seconds. Step = 0.2 seconds
    G4double ttime = timeInit; // iterator for time
    fEconcentration = Ei; // Medium
    fMconcentration = Mi; // Membrane
    fIconcentration = Ii; // Cytoplasm
    fDconcentration = Di; // Degraded

    // Loop for differential equations calculations
    while (ttime < timeEnd) {

        // If washout has to be done (time > timeWashout and washout is not already done), concentration at medium is set to 0
        if (!fIsWashoutDone && ttime > fWashoutTime) {
            fEconcentration = 0;
            fDconcentration = 0;
            fIsWashoutDone = true;
        }

        // Compute time differentials of E (medium), M (membrane), I (cytoplasm) and D (degraded), and update their values
        G4double dE = dt * (-fkon * fEconcentration * (fRi - fMconcentration - fIconcentration - fDconcentration) + fkoff * fMconcentration);
        G4double dM = dt * (fkon * fEconcentration * (fRi - fMconcentration - fIconcentration - fDconcentration) - fkoff * fMconcentration - fkint * fMconcentration + fkrec * fIconcentration);
        G4double dI = dt * (fkint * fMconcentration - fkrec * fIconcentration - fkefflux * fIconcentration);
        G4double dD = dt * (fkefflux * fIconcentration);
        fEconcentration += dE;
        fMconcentration += dM;
        fIconcentration += dI;
        fDconcentration += dD;

        // In case of washout, computing the fraction of isotopes remaining
        fFractionOfIsotopesRemaining = (fEconcentration + fMconcentration + fIconcentration + fDconcentration) / fTotalConcentrationInit;
        ttime += dt;

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

void TsDynamicBindModel::UpdateModelManual([[maybe_unused]] G4double timeInit, G4double timeEnd, G4double Ei, G4double Mi, G4double Ii, G4double Ni, G4double Di) {


    // Setting probabilities
    fEconcentration = Ei;
    fMconcentration = Mi;
    fIconcentration = Ii;
    fNconcentration = Ni;
    fDconcentration = Di;

    // Checking if the washout time is reached
    if (!fIsWashoutDone && timeEnd > fWashoutTime) {
        fEconcentration = 0;
        fDconcentration = 0;
        fIsWashoutDone = true;
    }

    fFractionOfIsotopesRemaining = (fEconcentration + fMconcentration + fIconcentration + fNconcentration + fDconcentration) / fTotalConcentrationInit;

}



