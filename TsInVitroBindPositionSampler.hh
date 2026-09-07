//
// invitro_bind compartment position sampler
//

#ifndef TsInVitroBindPositionSampler_hh
#define TsInVitroBindPositionSampler_hh

#include <cmath>
#include <functional>
#include <vector>

#include "Randomize.hh"

#include "G4TransportationManager.hh"
#include "G4VPhysicalVolume.hh"

#include "ITsPositionSampler.hh"
#include "TsSamplingAtVolumes.hh"

class TsInVitroBindPositionSampler : public ITsPositionSampler
{
public:
    struct Config
    {
        TsSamplingAtVolumes* samplingPosition;
        const std::vector<G4double>* cellInfo;
        const std::vector<G4double>* monolayerInfo;
        const std::vector<G4ThreeVector>* cellPositions;
        const std::vector<G4VPhysicalVolume*>* physicalVolumes;
        G4ThreeVector componentCenter;
        const TsCompartmentProbabilities* probabilities;
        std::function<G4int()> sampleMembraneCellId;
        G4String worldName;
        G4bool mediumIsSphere;
        G4double mediumRadius;
        G4bool cellIsSphere;
        G4bool cellIsCylinder;
    };

    explicit TsInVitroBindPositionSampler(const Config& cfg) : fConfig(cfg), fHistorySuppressed(false) {}

    G4ThreeVector SamplePosition(const TsEventContext&) override
    {
        fHistorySuppressed = false;

        G4TransportationManager* transportationManager = G4TransportationManager::GetTransportationManager();
        G4Navigator* navigator =
                transportationManager->GetNavigator(transportationManager->GetParallelWorld(fConfig.worldName));

        const TsCompartmentProbabilities& p = *fConfig.probabilities;
        G4double randBind = G4UniformRand();

        if (randBind <= p.medium + p.degraded) {
            return SampleMediumPosition(navigator);
        } else if (randBind <= p.medium + p.degraded + p.membrane) {
            G4int cellId = fConfig.sampleMembraneCellId();
            return SampleMembranePosition(cellId);
        } else if (randBind <= p.medium + p.degraded + p.membrane + p.cytoplasm) {
            G4int cellId = std::floor(G4UniformRand() * fConfig.cellPositions->size());
            return SampleCytoplasmPosition(cellId);
        } else if (randBind <= p.medium + p.degraded + p.membrane + p.cytoplasm + p.nucleus) {
            G4int cellId = std::floor(G4UniformRand() * fConfig.cellPositions->size());
            return SampleNucleusPosition(cellId);
        }

        fHistorySuppressed = true;
        return G4ThreeVector(0., 0., 0.);
    }

    bool IsHistorySuppressed(const TsEventContext&) override { return fHistorySuppressed; }

private:
    G4ThreeVector SampleMediumPosition(G4Navigator* navigator) const
    {
        G4VPhysicalVolume* foundVolume = nullptr;
        G4bool isPointInMedium = false;
        G4ThreeVector sampled(0., 0., 0.);
        while (!isPointInMedium) {
            sampled = fConfig.samplingPosition->SampleAtBoxVolume((*fConfig.monolayerInfo)[0] * 2,
                                                                  (*fConfig.monolayerInfo)[1] * 2,
                                                                  (*fConfig.monolayerInfo)[2] * 2);
            if (fConfig.mediumIsSphere)
                sampled = fConfig.samplingPosition->SampleAtSphereVolume(fConfig.mediumRadius, 0.0);
            G4ThreeVector globalPosition = sampled + fConfig.componentCenter;
            foundVolume = navigator->LocateGlobalPointAndSetup(globalPosition);
            if (foundVolume) {
                isPointInMedium = (foundVolume != (*fConfig.physicalVolumes)[1] &&
                                   foundVolume != (*fConfig.physicalVolumes)[2]);
            }
        }
        return sampled;
    }

    G4ThreeVector SampleMembranePosition(G4int cellId) const
    {
        G4ThreeVector sampled(0., 0., 0.);
        if (fConfig.cellIsSphere) {
            sampled = fConfig.samplingPosition->SampleAtSphereSurface((*fConfig.cellInfo)[0]);
        } else if (fConfig.cellIsCylinder) {
            sampled =
                    fConfig.samplingPosition->SampleAtCylinderSurface((*fConfig.cellInfo)[0], (*fConfig.cellInfo)[1]);
        }
        sampled += (*fConfig.cellPositions)[cellId];
        return sampled;
    }

    G4ThreeVector SampleCytoplasmPosition(G4int cellId) const
    {
        G4ThreeVector sampled(0., 0., 0.);
        if (fConfig.cellIsSphere) {
            sampled = fConfig.samplingPosition->SampleAtSphereVolume((*fConfig.cellInfo)[0], (*fConfig.cellInfo)[2]);
        } else if (fConfig.cellIsCylinder) {
            sampled = fConfig.samplingPosition->SampleAtCylindersVolume((*fConfig.cellInfo)[0], (*fConfig.cellInfo)[1],
                                                                        (*fConfig.cellInfo)[2], (*fConfig.cellInfo)[3]);
        }
        sampled += (*fConfig.cellPositions)[cellId];
        return sampled;
    }

    G4ThreeVector SampleNucleusPosition(G4int cellId) const
    {
        G4ThreeVector sampled(0., 0., 0.);
        if (fConfig.cellIsSphere) {
            sampled = fConfig.samplingPosition->SampleAtSphereVolume((*fConfig.cellInfo)[2], 0.0);
        } else if (fConfig.cellIsCylinder) {
            sampled = fConfig.samplingPosition->SampleAtCylindersVolume((*fConfig.cellInfo)[2], (*fConfig.cellInfo)[3],
                                                                        0, 0);
        }
        sampled += (*fConfig.cellPositions)[cellId];
        return sampled;
    }

    Config fConfig;
    G4bool fHistorySuppressed;
};

#endif
