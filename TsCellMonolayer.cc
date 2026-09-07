// Component for TsCellMonolayer
//
// Create by Daniel Suarez and Victor Valladolid on 2/2/24.
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

#include <iostream>
#include <fstream>
#include <cmath>
#include "TsCellMonolayer.hh"

#include "TsParameterManager.hh"
#include "G4VPhysicalVolume.hh"

#include "G4Orb.hh"
#include "G4ExtrudedSolid.hh"
#include "G4LogicalVolume.hh"
#include "G4SystemOfUnits.hh"
#include "G4PhysicalConstants.hh"
#include "Randomize.hh"

TsCellMonolayer::TsCellMonolayer(TsParameterManager* pM, TsExtensionManager* eM, TsMaterialManager* mM, 
                                     TsGeometryManager* gM, TsVGeometryComponent* parentComponent,
                                     G4VPhysicalVolume* parentVolume, G4String& name)
                                           :TsVGeometryComponent(pM, eM, mM, gM, parentComponent, parentVolume, name)
{
    G4cout << "TsCellMonolayer constructor" << G4endl;
    ResolveParameters();
}


TsCellMonolayer::~TsCellMonolayer()
{;}

void TsCellMonolayer::ResolveParameters() {

    // Getting the geometyr of the cell. Cylinders by default
    fCellGeometry = "Cylinder";
    //If the user has defined the geometry of the cell, read it
    if (fPm->ParameterExists(GetFullParmName("CellGeometry"))) {
        fCellGeometry = fPm->GetStringParameter(GetFullParmName("CellGeometry"));
        //Check if Cylinder or Sphere, if not exit
        if (fCellGeometry != "Cylinder" && fCellGeometry != "Sphere") {
            G4cerr << "Topas is exiting due to a serious error in source setup." << G4endl;
            G4cerr << "CellGeometry must be Cylinder or Sphere" << G4endl;
            exit(1);
        }
    }

    // Getting geometry parameters of the cell and medium
    if (fPm->ParameterExists(GetFullParmName("RCell")))
        fRCell = fPm->GetDoubleParameter(GetFullParmName("RCell"), "Length");
    fHCell = 2*fRCell; // Default value for spherical cells
    flCellHex = fRCell/(std::cos(30*deg));

    if (fPm->ParameterExists(GetFullParmName("RNucleus")))
        fRNucleus = fPm->GetDoubleParameter(GetFullParmName("RNucleus"), "Length");
    fHNucleus = 2*fRNucleus; // Default value for spherical nucleus


    if (fCellGeometry == "Cylinder"){ // Getting required parameters for cylinder cells
        if (fPm->ParameterExists(GetFullParmName("HCell"))){
            fHCell = fPm->GetDoubleParameter(GetFullParmName("HCell"), "Length");
        }else{
            G4cerr << "Topas is exiting due to a serious error in source setup." << G4endl;
            G4cerr << "For cylinder cells, HCell must be defined" << G4endl;
            exit(1);
        }

        fMHLZ = fHCell/2; // Default value for MonolayerHLZ
        
        if (fPm->ParameterExists(GetFullParmName("HNucleus"))){
            fHNucleus = fPm->GetDoubleParameter(GetFullParmName("HNucleus"), "Length");
        }else{
            G4cout << "HNucleus not defined, using HCell as HNucleus" << G4endl;
            fHNucleus = fHCell;
        }

    } else { // Required parameters for spherical cells
        fMHLZ = fRCell; // Default value for MonolayerHLZ
    }
    

    if (fPm->ParameterExists(GetFullParmName("MediumHLX")))
        fMHLX = fPm->GetDoubleParameter(GetFullParmName("MediumHLX"), "Length");
    if (fPm->ParameterExists(GetFullParmName("MediumHLY")))
        fMHLY = fPm->GetDoubleParameter(GetFullParmName("MediumHLY"), "Length");
    if (fPm->ParameterExists(GetFullParmName("MediumHLZ")))
        fMHLZ = fPm->GetDoubleParameter(GetFullParmName("MediumHLZ"), "Length");

    // Calculating the maximum number of cells that can fit in the monolayer
    fMaxCells = TsCellMonolayer::GetMaximumNumberOfCells(fMHLX, fMHLY, fRCell, flCellHex);

    fNoCopies = fMaxCells;
    if (fPm->ParameterExists(GetFullParmName("NumOfCells")))
        fNoCopies = fPm->GetIntegerParameter(GetFullParmName("NumOfCells"));
    G4cout << "Number of Cells set to " << fNoCopies << G4endl;

    // Setting the option of getting the positions cells from a file
    if (fPm->ParameterExists(GetFullParmName("PositionsFile")))
        fPositionsFile = fPm->GetStringParameter(GetFullParmName("PositionsFile"));


    // Defining the positions of the cells
    if (fCellPos.empty()){
        if (!fPositionsFile.empty()){
            //DefinePositionsFromFile(fPositionsFile, fMHLZ, fHCell);
        }else if (fNoCopies == fMaxCells){
            //DefineMaxPositions(fMHLX, fMHLY, fMHLZ, fRCell, fHCell, flCellHex);
        }else{
            //DefineDistributedPositions(fMHLX, fMHLY, fMHLZ, fRCell, fHCell, flCellHex);
        }

    }

}

G4VPhysicalVolume* TsCellMonolayer::Construct()
{
    // Defining the positions of the cells
    if (fCellPos.empty()){
        if (!fPositionsFile.empty()){
            DefinePositionsFromFile(fPositionsFile, fMHLZ, fHCell);
        }else if (fNoCopies == fMaxCells){
            DefineMaxPositions(fMHLX, fMHLY, fMHLZ, fRCell, fHCell, flCellHex);
        }else{
            DefineDistributedPositions(fMHLX, fMHLY, fMHLZ, fRCell, fHCell, flCellHex);
        }

    }
	BeginConstruction();

    // Building the envelope, i.e. the culture medium
    G4Box* Medium = new G4Box("Medium", 
                                 fMHLX, 
                                 fMHLY, 
                                 fMHLZ);


    fEnvelopeLog = CreateLogicalVolume(Medium);
    fEnvelopePhys = CreatePhysicalVolume(fEnvelopeLog);

    // Building the cell and nucleus, as cylinders or spheres

    G4VSolid* Cell = nullptr;
    G4VSolid* Nucleus = nullptr;

    if (fCellGeometry == "Sphere"){ // Building the cells as spheres
        // Cell
        Cell = new G4Orb("Cell", fRCell);
        // Nucleus
        Nucleus = new G4Orb("Nucleus", fRNucleus);

    }else{ // Building the cells as cylinders
        // Cell
        Cell = new G4Tubs("Cell", 0, fRCell, fHCell/2, 0, 360);  // inner radius, outer radius, height, start angle, end angle
        // Nucleus
        Nucleus = new G4Tubs("Nucleus", 0, fRNucleus, fHNucleus/2, 0, 360);  // inner radius, outer radius, height, start angle, end angle
    }

    // Logical and physical volumes for the cell. Placing cells in their positions
    G4LogicalVolume* logCell = CreateLogicalVolume("Cell",  Cell);

    // Setting the parameterization of the cells
    param = new TsMonolayerParameterizations(fPm);
    param->SetPositions(fCellPos);
    param->SetRotation(fRotAng);


    CreatePhysicalVolume("Cell", logCell, fEnvelopePhys, kUndefined, fNoCopies, param);

    // Logical and physical volumes for the nucleus. Placing nucleus inside the cell
    G4LogicalVolume* logNucleus = CreateLogicalVolume("Nucleus",  Nucleus);

    CreatePhysicalVolume("Nucleus", 0, true, logNucleus, 0, new G4ThreeVector ((fRCell-fRNucleus)*0.,0.,0.), logCell);

    InstantiateChildren(fEnvelopePhys);

    return fEnvelopePhys;

}

G4int TsCellMonolayer::GetMaximumNumberOfCells(G4double MHLX, G4double MHLY, G4double RCell, G4double lCellHex) {
    // GetMaximumNumberOfCells is used to:
    // 1) Check if the monolayer is big enough to fit at least one cell,
    // 2) Calculate the number of columns in the monolayer, an return the number of cells that can fit in the monolayer.

    if(MHLX<RCell || 2*MHLY<2*RCell){
        G4cerr << "Topas is exiting due to a serious error in source setup." << G4endl;
        G4cerr << "The monolayer is too small to fit a cell." << G4endl;
        G4cerr << "Please check the parameter file." << G4endl;
        exit(1);
    }

    // Calculating the number of columns in the cell monolayer.
    ncx=floor((2*MHLX-2*RCell)/(0.5*lCellHex+RCell+(lCellHex-RCell)))+1;
    ncy=floor(2*MHLY/(2*RCell));


    return ncx*ncy;
    }

void TsCellMonolayer::DefineMaxPositions([[maybe_unused]] G4double MHLX, [[maybe_unused]] G4double MHLY, [[maybe_unused]] G4double MHLZ, G4double RCell, [[maybe_unused]] G4double HCell, G4double lCellHex){

    fCellPos.clear();
    
    // Deciding the type of geometry (if the last column is same as the first one or not)
    if((2*MHLY-2*RCell*ncy)>RCell){type="NS";}else{type="YS";};
    

    // Building the positions of the cells depending on the type
    if(type=="NS"){
        fNoCopies=ncx*ncy;
        for(int nx=0; nx<ncx; nx++){
            for(int ny=0; ny<ncy; ny++){
                fRotAng.push_back(G4UniformRand()*360*deg);
                if(nx%2==0){
                    fCellPos.push_back(G4ThreeVector(3*lCellHex*nx/2-(ncx-1)*3*lCellHex/4,
                                                        2*ny*RCell-RCell*(ncy-1./2),
                                                        -MHLZ+0.5*HCell));
                }else{
                    fCellPos.push_back(G4ThreeVector(3*lCellHex*nx/2-(ncx-1)*3*lCellHex/4,
                                                        (2*ny+1)*RCell-RCell*(ncy-1./2),
                                                        -MHLZ+0.5*HCell));
                }

            }}
    }else{
        fNoCopies=ncx*ncy-floor(ncx/2);
        for(int nx=0; nx<ncx; nx++){
            if(nx%2==0){
                for(int ny=0; ny<ncy; ny++){
                    fRotAng.push_back(G4UniformRand()*360*deg);
                    fCellPos.push_back(G4ThreeVector(3*lCellHex*nx/2-(ncx-1)*3*lCellHex/4,
                                                        2*ny*RCell-RCell*(ncy-1),
                                                        -MHLZ+0.5*HCell));}
            }else{
                for(int ny=0; ny<ncy-1; ny++){
                    fRotAng.push_back(G4UniformRand()*360*deg);
                    fCellPos.push_back(G4ThreeVector(3*lCellHex*nx/2-(ncx-1)*3*lCellHex/4,
                                                    (2*ny+1)*RCell-RCell*(ncy-1),
                                                    -MHLZ+0.5*HCell));}
            }

        }

    }
    G4cout << "  " << G4endl;
    G4cout << "Monolayer information:" << G4endl;
    G4cout << "The rows number is: " << ncx << G4endl;
    G4cout << "The columns number is: " << ncy << G4endl;
    G4cout << fCellPos.size() << " cells have been created." << G4endl;
    G4cout << "  " << G4endl;

} // End function

void TsCellMonolayer::DefineDistributedPositions(G4double MHLX, G4double MHLY, G4double MHLZ, G4double RCell, G4double HCell, G4double lCellHex) {

    if (fNoCopies > 0.4 * fMaxCells){

        G4cout << " " << G4endl;
        G4cout << "WARNING: " << G4endl;
        G4cout << "Too many cells to allocate, the program may break." << G4endl;
        G4cout << "It is not recomended to use as input more than 40% of maximum cells" << G4endl;
        G4cout << "Maximum cells: " << fMaxCells << G4endl;
        G4cout << "Input cells: " << fNoCopies << G4endl;


    }

    //First Cell
    G4double posX = (2.0 * G4UniformRand() - 1.0) * (MHLX-RCell);
    G4double posY = (2.0 * G4UniformRand() - 1.0) * (MHLY-RCell);
    fRotAng.push_back(G4UniformRand()*360*deg);
    fCellPos.push_back(G4ThreeVector(posX,
                                     posY,
                                     -MHLZ+0.5*HCell));

    // Sampling the position of the cells in the monolayer
    for (int nc = 1; nc < fNoCopies; ++nc) {
        G4cout << "\r" << "Allocating cells: " << nc << "/" << fNoCopies << std::flush;

        bool overlap = true;
        G4int iterations = 0;


        while(overlap){
            posX = (2.0 * G4UniformRand() - 1.0) * (MHLX-RCell);
            posY = (2.0 * G4UniformRand() - 1.0) * (MHLY-RCell);
            G4bool checkCells = true;
            for (size_t i = 0; i < fCellPos.size(); ++i) {
                G4double distance = (G4ThreeVector(posX, posY, -MHLZ+0.5*HCell) - fCellPos[i]).mag();
                if (distance < 2.0 * lCellHex) {
                    checkCells = false;
                    break;  // No need to check further, overlap already detected
                }
            }
            if(checkCells){
                overlap = false;
            }
            ++iterations;
            // If too many iterations, exit the program
            if (iterations > 100000) {
                G4cerr << "Topas is exiting due to a serious error in geometry setup." << G4endl;
                G4cerr << "Too many iterations to find a free position for cell " << nc << std::endl;
                exit(1); 
            }
        }

        // If no overlap, add the cell position
        if(!overlap){
            fRotAng.push_back(G4UniformRand()*360*deg);
            fCellPos.push_back(G4ThreeVector(posX,
                                             posY,
                                             -MHLZ+0.5*HCell));
        }


    }

    G4cout << "  " << G4endl;
    G4cout << "Monolayer information:" << G4endl;
    G4cout << "The rows number is: " << ncx << G4endl;
    G4cout << "The columns number is: " << ncy << G4endl;
    G4cout << fCellPos.size() << " cells have been created." << G4endl;
    G4cout << "  " << G4endl;

}


void TsCellMonolayer::DefinePositionsFromFile(G4String filename, G4double MHLZ, G4double HCell) {
    fCellPos.clear();
    std::ifstream file(filename);
    // Check if the file is open
    if (!file.is_open()) {
        G4cerr << "Topas is exiting due to a serious error in geometry setup." << G4endl;
        G4cerr << "The file " << filename << " to read cell positions does not exist." << G4endl;
        exit(1);
    }

    G4cout << "Remember that the positions have to be specified in millimeters. Each cell position has to be in a different line like: nCell xPos yPos" << G4endl;
    //Read file line by line
    G4float xPos, yPos;
    G4int nCell;
    while (file >> nCell >> xPos >> yPos) {
        fRotAng.push_back(G4UniformRand()*360*deg);
        fCellPos.push_back(G4ThreeVector(xPos, yPos, -MHLZ + 0.5 * HCell));

    }

    // Updating the number of cells with the number of lines read from the file
    fNoCopies = fCellPos.size();

    G4cout << "  " << G4endl;
    G4cout << "Monolayer information:" << G4endl;
    G4cout << "Read from file: " << filename << G4endl;
    G4cout << fCellPos.size() << " cells have been created." << G4endl;
    G4cout << "  " << G4endl;
}

