//
// activity_map voxel-weighted position sampler
//

#ifndef TsActivityMapPositionSampler_hh
#define TsActivityMapPositionSampler_hh

#include <cstddef>
#include <vector>

#include "Randomize.hh"

#include "G4ThreeVector.hh"

#include "ITsPositionSampler.hh"

class TsActivityMapPositionSampler : public ITsPositionSampler
{
public:
    struct Config
    {
        const std::vector<G4ThreeVector>* positions;
        const std::vector<G4double>* accumulatedCounts;
        G4double voxelSizeX;
        G4double voxelSizeY;
        G4double voxelSizeZ;
    };

    explicit TsActivityMapPositionSampler(const Config& cfg) : fConfig(cfg), fLastSelectedIndex(static_cast<size_t>(-1)) {}

    G4ThreeVector SamplePosition(const TsEventContext&) override
    {
        const std::vector<G4double>& accumCounts = *fConfig.accumulatedCounts;
        const std::vector<G4ThreeVector>& positions = *fConfig.positions;

        G4double random = G4UniformRand();
        size_t selectedIndex = 0;
        for (size_t i = 0; i < accumCounts.size(); i++) {
            if (random < accumCounts[i]) {
                selectedIndex = i;
                break;
            }
        }
        fLastSelectedIndex = selectedIndex;

        const G4ThreeVector& voxelCenter = positions[selectedIndex];
        G4double x = G4RandFlat::shoot(-1., 1.) * fConfig.voxelSizeX / 2.0;
        G4double y = G4RandFlat::shoot(-1., 1.) * fConfig.voxelSizeY / 2.0;
        G4double z = G4RandFlat::shoot(-1., 1.) * fConfig.voxelSizeZ / 2.0;
        return G4ThreeVector(voxelCenter.x() + x, voxelCenter.y() + y, voxelCenter.z() + z);
    }

    size_t GetLastSelectedIndex() const
    {
        return fLastSelectedIndex;
    }

private:
    Config fConfig;
    size_t fLastSelectedIndex;
};

#endif
