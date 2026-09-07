//
// Created by Daniel Suarez on 20/03/24.
//

#ifndef TSRADIOACTIVETIMESOURCE_TSSAMPLINGATVOLUMES_HH
#define TSRADIOACTIVETIMESOURCE_TSSAMPLINGATVOLUMES_HH

#include "TsParameterManager.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"



class TsSamplingAtVolumes {
public:
    TsSamplingAtVolumes(){}; // Constructor
    ~TsSamplingAtVolumes(){}; // Destructor

    G4ThreeVector SampleAtCylindersVolume(G4double rMaxCylinder, G4double hMaxCylinder, G4double rMinCylinder = 0, G4double hMinCylinder = 0);
    G4ThreeVector SampleAtCylinderSurface(G4double rCylinder, G4double hCylinder);
    G4ThreeVector SampleAtSphereVolume(G4double rSphere, G4double rSphereMin); // To be defined for 3DGeometry
    G4ThreeVector SampleAtSphereSurface(G4double rSphere);  // To be defined for 3DGeometry
    G4ThreeVector SampleAtBoxVolume(G4double xBox, G4double yBox, G4double zBox);

};


#endif //TSRADIOACTIVETIMESOURCE_TSSAMPLINGATVOLUMES_HH
