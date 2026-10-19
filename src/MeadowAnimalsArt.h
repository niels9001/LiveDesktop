#pragma once
#include "pch.h"

// Procedural vector art for the Meadow animals: species rigs (proportions in "U" units, feet at the
// origin, facing +x, y down) and Direct2D drawing of every rig part around its pivot.
namespace ld::animals
{
    using winrt::Windows::Foundation::Numerics::float2;

    enum Species { Bunny, Fox, Sheep, Deer, Hedgehog, Duck, Cat, Snail, Dog, Squirrel, SpeciesCount };
    const wchar_t* SpeciesName(int species);

    struct Rig
    {
        float height = 1, rx = 0.3f;        // top of the head/ears above the feet, half body width
        float2 body{}; float bx = 0.3f, by = 0.2f;
        int legs = 4;                       // 4: far back, far front, near back, near front; 2: far, near
        float legX[4]{};
        float legLen = 0.2f, legW = 0.07f, foot = 0.04f;
        float2 neck{};                      // head pivot (pose space)
        bool ears = true;
        float2 ear[2]{};                    // ear pivots (head space)
        float earLen = 0.2f, earW = 0.08f, earBase = 0, earFlutter = 0.1f, earDroop = 0;
        bool tail = true;
        float2 tailAt{}; float tailBase = 0;
        float2 eye{}; float eyeR = 0.03f;   // head space; eyeR 0 = no lid/glint parts
        float2 mouth{};                     // head space
        float graze = 0.6f;                 // head rotation (rad) to reach the grass
        float gait = 0.7f;                  // seconds per stride
        float speed = 2.0f;                 // U per second at scale 1
        float bob = 0.04f;                  // body bob while walking (U)
        float legAmp = 0.45f;
        bool hopper = false, waddle = false, stretch = false, wing = false;
    };
    Rig const& RigFor(int species);

    enum class PartKind { Leg, Tail, Body, Wing, Head, Ear, Lid, Glint, Carry };
    struct PartSpec
    {
        PartKind kind;
        int index = 0;
        bool inHead = false;   // child of the head container (pivot in head space)
        float2 pivot{};        // rotation pivot in parent space
        float2 min{}, max{};   // drawing bounds relative to the pivot
        bool haze = true;
    };
    std::vector<PartSpec> PartsFor(int species);

    // Draws one part in rig units around its pivot. `lit` maps a palette colour into the current light.
    void DrawPart(ID2D1DeviceContext* dc, int species, int variant, PartSpec const& part, std::function<uint32_t(uint32_t)> const& lit);

    uint32_t MixRgb(uint32_t a, uint32_t b, float t);
    uint32_t ScaleRgb(uint32_t a, float k);
    uint32_t WarmRgb(uint32_t a);
    float DiffRgb(uint32_t a, uint32_t b);
}
