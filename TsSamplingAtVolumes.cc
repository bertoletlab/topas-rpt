// Extra Class for TsRadioactiveTimeGeneratorBind
//
// Created by Daniel Suarez on 20/03/24.
//

#include "TsSamplingAtVolumes.hh"

// CYLINDER
G4ThreeVector TsSamplingAtVolumes::SampleAtCylindersVolume(G4double rMaxCylinder, G4double hMaxCylinder, G4double rMinCylinder, G4double hMinCylinder){
    G4double volume_top = CLHEP::pi * rMinCylinder * rMinCylinder * (hMaxCylinder - hMinCylinder)/2; // Volume bottom is the same
    G4double volume_ring = CLHEP::pi * (rMaxCylinder * rMaxCylinder - rMinCylinder * rMinCylinder) * hMaxCylinder;
    G4double volume_total = 2 * volume_top + volume_ring;
    G4double selVolume = G4UniformRand();
    if (selVolume <= volume_ring / volume_total){
        G4double r = rMinCylinder + sqrt(G4UniformRand()) * (rMaxCylinder - rMinCylinder);
        G4double phi = 4 * CLHEP::pi * G4UniformRand() - 2 * CLHEP::pi;
        G4double xpos = r * std::sin(phi);
        G4double ypos = r * std::cos(phi);
        G4double zpos = -0.5 * hMaxCylinder + G4UniformRand() * hMaxCylinder;
        return G4ThreeVector(xpos, ypos, zpos);
    } else if (selVolume <= (volume_ring + volume_top) / volume_total){
        G4double r = sqrt(G4UniformRand()) * rMaxCylinder;
        G4double phi = 4 * CLHEP::pi * G4UniformRand() - 2 * CLHEP::pi;
        G4double xpos = r * std::sin(phi);
        G4double ypos = r * std::cos(phi);
        G4double zpos = 0.5 * (hMinCylinder + G4UniformRand() * (hMaxCylinder - hMinCylinder)/2);
        return G4ThreeVector(xpos, ypos, zpos);
    } else {
        G4double r = sqrt(G4UniformRand()) * rMaxCylinder;
        G4double phi = 4 * CLHEP::pi * G4UniformRand() - 2 * CLHEP::pi;
        G4double xpos = r * std::sin(phi);
        G4double ypos = r * std::cos(phi);
        G4double zpos = - 0.5 * (hMinCylinder + G4UniformRand() * (hMaxCylinder - hMinCylinder)/2);
        return G4ThreeVector(xpos, ypos, zpos);
    }
}

G4ThreeVector TsSamplingAtVolumes::SampleAtCylinderSurface(G4double rCylinder, G4double hCylinder){
    G4double lateralSurface = 2 * CLHEP::pi * rCylinder * hCylinder;
    G4double topBottomSurface = CLHEP::pi * rCylinder * rCylinder;
    G4double totalSurface = 2 * topBottomSurface + lateralSurface;
    G4double selSurface = G4UniformRand();
    if (selSurface <= lateralSurface / totalSurface){
        G4double phi = 4 * CLHEP::pi * G4UniformRand() - 2 * CLHEP::pi;
        G4double xpos = rCylinder * std::sin(phi);
        G4double ypos = rCylinder * std::cos(phi);
        G4double zpos = -0.5 * hCylinder + G4UniformRand() * hCylinder;
        return G4ThreeVector(xpos, ypos, zpos);
    } else if (selSurface <= (lateralSurface + topBottomSurface) / totalSurface){
        G4double radius = sqrt(G4UniformRand()) * rCylinder;
        G4double phi = 4 * CLHEP::pi * G4UniformRand() - 2 * CLHEP::pi;
        G4double xpos = radius * std::sin(phi);
        G4double ypos = radius * std::cos(phi);
        G4double zpos = hCylinder / 2;
        return G4ThreeVector(xpos, ypos, zpos);
    } else {
        G4double radius = sqrt(G4UniformRand()) * rCylinder;
        G4double phi = 4 * CLHEP::pi * G4UniformRand() - 2 * CLHEP::pi;
        G4double xpos = radius * std::sin(phi);
        G4double ypos = radius * std::cos(phi);
        G4double zpos = -hCylinder / 2;
        return G4ThreeVector(xpos, ypos, zpos);
    }
}

// BOX
G4ThreeVector TsSamplingAtVolumes::SampleAtBoxVolume(G4double xBox, G4double yBox, G4double zBox){
    G4double x = xBox * (G4UniformRand() - 0.5);
    G4double y = yBox * (G4UniformRand() - 0.5);
    G4double z = zBox * (G4UniformRand() - 0.5);
    return G4ThreeVector(x, y, z);
}

// SPHERE
G4ThreeVector TsSamplingAtVolumes::SampleAtSphereVolume(G4double rSphere, G4double rSphereMin){
    G4double r = rSphereMin + cbrt(G4UniformRand()) * (rSphere - rSphereMin);
    G4double phi = 4 * CLHEP::pi * G4UniformRand() - 2 * CLHEP::pi;
    G4double theta = std::acos(1 - 2 * G4UniformRand());
    G4double xpos = r * std::sin(theta) * std::cos(phi);
    G4double ypos = r * std::sin(theta) * std::sin(phi);
    G4double zpos = r * std::cos(theta);
    return G4ThreeVector(xpos, ypos, zpos);
}
G4ThreeVector TsSamplingAtVolumes::SampleAtSphereSurface(G4double rSphere){
    G4double r = rSphere;
    G4double phi = 4 * CLHEP::pi * G4UniformRand() - 2 * CLHEP::pi;
    G4double theta = std::acos(1 - 2 * G4UniformRand());
    G4double xpos = r * std::sin(theta) * std::cos(phi);
    G4double ypos = r * std::sin(theta) * std::sin(phi);
    G4double zpos = r * std::cos(theta);
    return G4ThreeVector(xpos, ypos, zpos);
}