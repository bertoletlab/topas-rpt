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

#ifndef TsCellMonolayer_hh
#define TsCellMonolayer_hh

#include "TsVGeometryComponent.hh"
#include "G4VPVParameterisation.hh"
#include "G4PVParameterised.hh"
#include "G4NistManager.hh"
#include "G4VSolid.hh"
#include "TsMonolayerParameterizations.hh"


class TsCellMonolayer : public TsVGeometryComponent
{    
public:
	TsCellMonolayer(TsParameterManager* pM, TsExtensionManager* eM, TsMaterialManager* mM, TsGeometryManager* gM,
				  TsVGeometryComponent* parentComponent, G4VPhysicalVolume* parentVolume, G4String& name);
	~TsCellMonolayer();
	
	G4VPhysicalVolume* Construct();
	void DefineMaxPositions(G4double MHLX, G4double MHLY, G4double MHLZ, G4double RCell, G4double HCell, G4double lCellHex);
    virtual void DefineDistributedPositions(G4double MHLX, G4double MHLY, G4double MHLZ, G4double RCell, G4double HCell, G4double lCellHex);
    virtual void DefinePositionsFromFile(G4String filename, G4double MHLZ, G4double HCell);

    G4int GetMaximumNumberOfCells(G4double MonolayerX, G4double MonolayerY, G4double RCell, G4double lCellHex);
    virtual void ResolveParameters();

    //Getters

    std::vector<G4ThreeVector> GetCellPositions(){return fCellPos;}; // Get cell positions

    std::vector<G4double> GetCellInfo(){ // Get geometry information of cell
        std::vector<G4double> fCellInfo;

        fCellInfo.push_back(fRCell);
        fCellInfo.push_back(fHCell);
        fCellInfo.push_back(fRNucleus);
        fCellInfo.push_back(fHNucleus);
        
        return fCellInfo;};


    std::vector<G4double> GetMonolayerInfo(){ // Get geometry information of monolayer
        std::vector<G4double> fMonolayerInfo;
        fMonolayerInfo.push_back(fMHLX);
        fMonolayerInfo.push_back(fMHLY);
        fMonolayerInfo.push_back(fMHLZ);
        return fMonolayerInfo;};


    G4int GetNoCopies(){return fNoCopies;};
    G4String GetCellGeometry(){return fCellGeometry;};

protected:

	// Defined by user:
	G4double fRCell;
	G4double fHCell;
	G4double flCellHex;
    G4double fRNucleus;
    G4double fHNucleus;
    G4double fMHLX;
    G4double fMHLY;
    G4double fMHLZ;

	G4int fNoCopies;
	G4int ncx;
	G4int ncy;
	G4String type;
    G4int fMaxCells;
    G4String fCellGeometry;
    G4String fPositionsFile;

	std::vector<G4double> fRotAng;
	std::vector<G4ThreeVector> fCellPos;
    std::vector<G4double> fCellInfo;
    std::vector<G4double> fMonolayerInfo;

	TsMonolayerParameterizations* param;




};



#endif
