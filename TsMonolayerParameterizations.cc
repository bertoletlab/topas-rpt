// Extra Class for TsCellMonolayer
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
// Created by Daniel Suarez and Victor Valladolid on 2/2/24.

#include "TsMonolayerParameterizations.hh"
#include "G4SystemOfUnits.hh"

#include "G4NistManager.hh"
#include "G4Material.hh"
#include "G4Box.hh"
#include "G4Orb.hh"
#include "TsParameterManager.hh"
#include "G4PVPlacement.hh"
#include "G4LogicalVolume.hh"

TsMonolayerParameterizations::TsMonolayerParameterizations(TsParameterManager*)
    : G4VPVParameterisation()
{
    fRot = new G4RotationMatrix();
}

TsMonolayerParameterizations::~TsMonolayerParameterizations()
{}

void TsMonolayerParameterizations::ComputeTransformation(const G4int copyNo, G4VPhysicalVolume* physVol) const
{
  G4ThreeVector trans = fPosCell[copyNo];
  fRot->set(0,0,0);
  fRot->rotateZ(fRotAng[copyNo]);
  physVol->SetTranslation(trans);
  physVol->SetRotation(fRot);
}

void TsMonolayerParameterizations::SetPositions(std::vector<G4ThreeVector> PosCell)
{
  fPosCell = PosCell;
}


void TsMonolayerParameterizations::SetRotation(std::vector<G4double> RotAng)
{
  fRotAng = RotAng;
}