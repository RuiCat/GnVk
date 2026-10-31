// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 3D 材质系统 实现                              ║
// ╚══════════════════════════════════════════════════════════╝
#include <GN.h>
#include "GN_3Dmaterial.h"

namespace GN3d {

using namespace GNg;

sMATERIAL::sMATERIAL() {
    SetDefault();
}

void sMATERIAL::SetDefault() {
    aDiffuse.x = 0.8f;
    aDiffuse.y = 0.8f;
    aDiffuse.z = 0.8f;
    aSpecular.x = 1.0f;
    aSpecular.y = 1.0f;
    aSpecular.z = 1.0f;
    aShininess = 32.0f;
    aEmissive.x = 0.0f;
    aEmissive.y = 0.0f;
    aEmissive.z = 0.0f;
    aRoughness = 0.5f;
    aMetallic = 0.0f;
    aAlphaMode = eALPHA_OPAQUE;
    aAlphaCutoff = 0.5f;
    aAlbedoTex = nullptr;
    aNormalTex = nullptr;
}

void sMATERIAL::SetWhite(float shininess) {
    aDiffuse.x = 0.9f;
    aDiffuse.y = 0.9f;
    aDiffuse.z = 0.9f;
    aSpecular.x = 1.0f;
    aSpecular.y = 1.0f;
    aSpecular.z = 1.0f;
    aShininess = shininess;
    aEmissive.x = 0.0f;
    aEmissive.y = 0.0f;
    aEmissive.z = 0.0f;
}

void sMATERIAL::SetRed(float shininess) {
    aDiffuse.x = 0.9f;
    aDiffuse.y = 0.2f;
    aDiffuse.z = 0.2f;
    aSpecular.x = 1.0f;
    aSpecular.y = 0.8f;
    aSpecular.z = 0.8f;
    aShininess = shininess;
    aEmissive.x = 0.0f;
    aEmissive.y = 0.0f;
    aEmissive.z = 0.0f;
}

void sMATERIAL::SetGreen(float shininess) {
    aDiffuse.x = 0.2f;
    aDiffuse.y = 0.9f;
    aDiffuse.z = 0.2f;
    aSpecular.x = 0.8f;
    aSpecular.y = 1.0f;
    aSpecular.z = 0.8f;
    aShininess = shininess;
    aEmissive.x = 0.0f;
    aEmissive.y = 0.0f;
    aEmissive.z = 0.0f;
}

void sMATERIAL::SetBlue(float shininess) {
    aDiffuse.x = 0.2f;
    aDiffuse.y = 0.3f;
    aDiffuse.z = 0.9f;
    aSpecular.x = 0.8f;
    aSpecular.y = 0.8f;
    aSpecular.z = 1.0f;
    aShininess = shininess;
    aEmissive.x = 0.0f;
    aEmissive.y = 0.0f;
    aEmissive.z = 0.0f;
}

void sMATERIAL::SetGold() {
    aDiffuse.x = 1.0f;
    aDiffuse.y = 0.84f;
    aDiffuse.z = 0.0f;
    aSpecular.x = 1.0f;
    aSpecular.y = 0.9f;
    aSpecular.z = 0.4f;
    aShininess = 128.0f;
    aRoughness = 0.3f;
    aMetallic = 1.0f;
}

void sMATERIAL::SetSilver() {
    aDiffuse.x = 0.75f;
    aDiffuse.y = 0.75f;
    aDiffuse.z = 0.75f;
    aSpecular.x = 1.0f;
    aSpecular.y = 1.0f;
    aSpecular.z = 1.0f;
    aShininess = 256.0f;
    aRoughness = 0.2f;
    aMetallic = 1.0f;
}

void sMATERIAL::SetPlastic(float r, float g, float b, float shininess) {
    aDiffuse.x = r;
    aDiffuse.y = g;
    aDiffuse.z = b;
    aSpecular.x = 1.0f;
    aSpecular.y = 1.0f;
    aSpecular.z = 1.0f;
    aShininess = shininess;
    aRoughness = 0.6f;
    aMetallic = 0.0f;
}

} // namespace GN3d
