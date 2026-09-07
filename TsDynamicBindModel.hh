//
// Created by Daniel Suarez and Victor Valladolid on 2/2/24.
//

#ifndef TSRADIOACTIVETIMESOURCE_TSDYNAMICBINDMODEL_H
#define TSRADIOACTIVETIMESOURCE_TSDYNAMICBINDMODEL_H

#include <cmath>
#include <fstream>
#include "TsParameterManager.hh"
#include "TsSource.hh"
#include "TsGeometryManager.hh"

class TsDynamicBindModel {
public:
    //Constructor
    TsDynamicBindModel(G4double koff, G4double kon, G4double kint, G4double krec, G4double kefflux,
                        G4double Ri, G4double totalConcentrationInit, G4double washoutTime);

    ~TsDynamicBindModel();

    // Methods to calculate the probabilties with each binding kinetic models
    void UpdateModel2compartmentsCell(G4double timeInit, G4double timeEnd, G4double Ei, G4double Ii, G4bool IsWrite, G4String filename);
    void UpdateModel2compartmentsNucleus(G4double timeInit, G4double timeEnd, G4double Ei, G4double Ii, G4bool IsWrite, G4String filename);
    void UpdateModel3compartments(G4double timeInit, G4double timeEnd, G4double Ei, G4double Mi, G4double Ii, G4bool IsWrite, G4String filename);
    void UpdateModel4compartments(G4double timeInit, G4double timeEnd, G4double Ei, G4double Mi, G4double Ii, G4double Di, G4bool IsWrite, G4String filename);
    void UpdateModelManual(G4double timeInit, G4double timeEnd, G4double Ei, G4double Mi, G4double Ii, G4double Ni, G4double Di);

    std::vector<G4double> GetModelResults() const{
        std::vector<G4double > results;
        results.push_back(fEconcentration);
        results.push_back(fMconcentration);
        results.push_back(fIconcentration);
        results.push_back(fNconcentration);
        results.push_back(fDconcentration);
        results.push_back(fFractionOfIsotopesRemaining);
        return results;
    };

protected:

    //File to write
    std::ofstream fFile;

    // Concentration in each compartment of the models
    G4double fEconcentration = 0; // Medium
    G4double fMconcentration = 0; // Membrane
    G4double fIconcentration = 0; // Cytoplasm
    G4double fNconcentration = 0; // Nucleus
    G4double fDconcentration = 0; // Degraded

    // Binding Kinetic parameters
    G4double fkoff; // From membrane to medium
    G4double fkon; // From medium to membrane
    G4double fkint; // From membrane to cytoplasm
    G4double fkrec; // From cytoplasm to membrane
    G4double fkefflux; // From cytoplasm to medium (or degraded)
    G4double fRi; // Concentration of receptors
    G4double fKd; // Dissociation constant

    G4double fTotalConcentrationInit; // Total concentration of isotopes at the beginning
    G4double fFractionOfIsotopesRemaining; // Fraction of isotopes remaining

    //Time parameters
    G4double fTini;
    G4double fTend;
    G4double fWashoutTime;
    G4double fIsWashoutDone;



};


#endif //TSRADIOACTIVETIMESOURCE_TSDYNAMICBINDMODEL_H
