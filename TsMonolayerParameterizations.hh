//
// ********************************************************************
// *                                                                  *
// * This file is part of the TOPAS-nBio extensions to the            *
// *   TOPAS Simulation Toolkit.                                      *
// * The TOPAS-nBio extensions are freely available under the license *
// *   agreement set forth at: https://topas-nbio.readthedocs.io/     *
// *                                                                  *
// ********************************************************************
//

#ifndef TsSpheresParameterizations_hh
#define TsSpheresParameterizations_hh

#include "G4VPVParameterisation.hh"
#include "G4ThreeVector.hh"
#include "G4UnitsTable.hh"
#include "G4SystemOfUnits.hh"
#include "G4VPhysicalVolume.hh"
#include "G4LogicalVolume.hh"
#include "G4VSolid.hh"
#include "G4VisAttributes.hh"
#include "G4ExtrudedSolid.hh"
//#include "TsVGeometryComponent.hh"
#include "TsParameterManager.hh"

#include "TsMonolayerParameterizations.hh"

class TsMonolayerParameterizations : public G4VPVParameterisation
{
public:
    TsMonolayerParameterizations(TsParameterManager*);
    virtual ~TsMonolayerParameterizations();
    virtual void ComputeTransformation(const G4int copyNo, G4VPhysicalVolume* physVol) const;
    // virtual G4Material* ComputeMaterial (const G4int repNo, G4VPhysicalVolume *currentVol, const G4VTouchable *parentTouch=0) const; 
    
    //void SetCellParameters(G4double flCell, G4double fzCell, G4double MonolayerX, G4double MonolayerY); // G4double Rmin, G4double Rmax, G4double Phi, G4double Theta);
    
    void SetPositions(std::vector<G4ThreeVector> PosCell);
    void SetRotation(std::vector<G4double> RotAng);
    //void SetMaterial(G4Material* mat);
    //void ComputeDimensions(TsExtrudedSolid & , const G4int, const G4VPhysicalVolume*) const {};

    G4String fMaterialSubSphere;

    std::vector<G4double> fPosX;
    std::vector<G4double> fPosY;
    std::vector<G4double> fPosZ;
    std::vector<G4ThreeVector> fPosCell;
    std::vector<G4double> fRotAng;

protected:
    std::vector<G4double> fPositions;

private:
    G4RotationMatrix* fRot;
};

#endif 