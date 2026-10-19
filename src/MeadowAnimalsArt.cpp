#include "pch.h"
#include "MeadowAnimalsArt.h"
#include "Graphics.h"
#include <array>

namespace ld::animals
{
    using winrt::com_ptr;

    // ---------------------------------------------------------------- colour

    namespace
    {
        struct Rgbf { float r, g, b; };
        Rgbf Unpack(uint32_t c) { return { ((c >> 16) & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, (c & 255) / 255.0f }; }
        uint32_t Pack(Rgbf c)
        {
            auto ch = [](float v) { return static_cast<uint32_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); };
            return (ch(c.r) << 16) | (ch(c.g) << 8) | ch(c.b);
        }
    }

    uint32_t MixRgb(uint32_t a, uint32_t b, float t)
    {
        Rgbf x = Unpack(a), y = Unpack(b);
        return Pack({ x.r + (y.r - x.r) * t, x.g + (y.g - x.g) * t, x.b + (y.b - x.b) * t });
    }
    uint32_t ScaleRgb(uint32_t a, float k)
    {
        Rgbf x = Unpack(a);
        return Pack({ x.r * k, x.g * k, x.b * k });
    }
    uint32_t WarmRgb(uint32_t a)
    {
        Rgbf x = Unpack(a);
        return Pack({ x.r * 1.25f + 0.04f, x.g * 1.03f + 0.01f, x.b * 0.75f });
    }
    float DiffRgb(uint32_t a, uint32_t b)
    {
        Rgbf x = Unpack(a), y = Unpack(b);
        return 255.0f * std::max({ std::abs(x.r - y.r), std::abs(x.g - y.g), std::abs(x.b - y.b) });
    }

    const wchar_t* SpeciesName(int species)
    {
        static const wchar_t* names[] = { L"bunny", L"fox", L"sheep", L"deer", L"hedgehog", L"duck", L"cat", L"snail", L"dog", L"squirrel" };
        static_assert(std::size(names) == SpeciesCount);
        return species >= 0 && species < SpeciesCount ? names[species] : L"?";
    }

    // ---------------------------------------------------------------- rigs

    namespace
    {
        std::array<Rig, SpeciesCount> MakeRigs()
        {
            std::array<Rig, SpeciesCount> rigs{};
            {
                Rig& r = rigs[Bunny];
                r.height = 0.92f; r.rx = 0.30f; r.body = { -0.02f, -0.27f }; r.bx = 0.27f; r.by = 0.21f;
                r.legs = 4; r.legX[0] = -0.13f; r.legX[1] = 0.14f; r.legX[2] = -0.09f; r.legX[3] = 0.18f;
                r.legLen = 0.15f; r.legW = 0.085f; r.foot = 0.05f;
                r.neck = { 0.17f, -0.36f };
                r.ear[0] = { -0.01f, -0.19f }; r.ear[1] = { 0.05f, -0.2f }; r.earLen = 0.36f; r.earW = 0.085f;
                r.earBase = -0.25f; r.earFlutter = 0.12f; r.earDroop = -0.9f;
                r.tailAt = { -0.28f, -0.31f };
                r.eye = { 0.12f, -0.11f }; r.eyeR = 0.035f; r.mouth = { 0.22f, -0.01f };
                r.graze = 0.55f; r.gait = 0.9f; r.speed = 2.6f; r.bob = 0.2f; r.legAmp = 0.45f; r.hopper = true;
            }
            {
                Rig& r = rigs[Fox];
                r.height = 0.80f; r.rx = 0.36f; r.body = { 0, -0.40f }; r.bx = 0.33f; r.by = 0.15f;
                r.legX[0] = -0.22f; r.legX[1] = 0.19f; r.legX[2] = -0.17f; r.legX[3] = 0.24f;
                r.legLen = 0.30f; r.legW = 0.07f; r.foot = 0.05f;
                r.neck = { 0.27f, -0.47f };
                r.ear[0] = { -0.01f, -0.12f }; r.ear[1] = { 0.07f, -0.13f }; r.earLen = 0.17f; r.earW = 0.12f;
                r.earBase = -0.08f; r.earFlutter = 0.06f; r.earDroop = -0.35f;
                r.tailAt = { -0.30f, -0.42f }; r.tailBase = -0.12f;
                r.eye = { 0.11f, -0.08f }; r.eyeR = 0.028f; r.mouth = { 0.27f, 0.03f };
                r.graze = 0.8f; r.gait = 0.6f; r.speed = 2.2f; r.bob = 0.05f; r.legAmp = 0.5f;
            }
            {
                Rig& r = rigs[Sheep];
                r.height = 0.76f; r.rx = 0.38f; r.body = { 0, -0.44f }; r.bx = 0.35f; r.by = 0.25f;
                r.legX[0] = -0.2f; r.legX[1] = 0.17f; r.legX[2] = -0.15f; r.legX[3] = 0.22f;
                r.legLen = 0.24f; r.legW = 0.075f; r.foot = 0.02f;
                r.neck = { 0.28f, -0.50f };
                r.ear[0] = { 0.0f, -0.07f }; r.ear[1] = { 0.04f, -0.07f }; r.earLen = 0.14f; r.earW = 0.05f;
                r.earBase = -1.5f; r.earFlutter = 0.1f; r.earDroop = -0.25f;
                r.tailAt = { -0.34f, -0.50f };
                r.eye = { 0.12f, -0.03f }; r.eyeR = 0.028f; r.mouth = { 0.17f, 0.11f };
                r.graze = 0.95f; r.gait = 0.8f; r.speed = 1.9f; r.bob = 0.04f; r.legAmp = 0.4f;
            }
            {
                Rig& r = rigs[Deer];
                r.height = 1.22f; r.rx = 0.36f; r.body = { 0, -0.62f }; r.bx = 0.32f; r.by = 0.17f;
                r.legX[0] = -0.22f; r.legX[1] = 0.18f; r.legX[2] = -0.17f; r.legX[3] = 0.23f;
                r.legLen = 0.50f; r.legW = 0.06f; r.foot = 0.02f;
                r.neck = { 0.24f, -0.70f };
                r.ear[0] = { 0.06f, -0.41f }; r.ear[1] = { 0.11f, -0.42f }; r.earLen = 0.15f; r.earW = 0.07f;
                r.earBase = -0.9f; r.earFlutter = 0.1f; r.earDroop = -0.3f;
                r.tailAt = { -0.31f, -0.68f };
                r.eye = { 0.16f, -0.38f }; r.eyeR = 0.026f; r.mouth = { 0.3f, -0.29f };
                r.graze = 1.2f; r.gait = 0.7f; r.speed = 3.0f; r.bob = 0.05f; r.legAmp = 0.45f;
            }
            {
                Rig& r = rigs[Hedgehog];
                r.height = 0.38f; r.rx = 0.30f; r.body = { 0, -0.16f }; r.bx = 0.27f; r.by = 0.15f;
                r.legX[0] = -0.14f; r.legX[1] = 0.12f; r.legX[2] = -0.1f; r.legX[3] = 0.16f;
                r.legLen = 0.07f; r.legW = 0.06f; r.foot = 0.03f;
                r.neck = { 0.2f, -0.13f };
                r.ear[0] = { 0.0f, -0.07f }; r.ear[1] = { 0.04f, -0.07f }; r.earLen = 0.06f; r.earW = 0.04f;
                r.earFlutter = 0.03f;
                r.tail = false;
                r.eye = { 0.08f, -0.02f }; r.eyeR = 0.022f; r.mouth = { 0.2f, 0.05f };
                r.graze = 0.45f; r.gait = 0.35f; r.speed = 1.6f; r.bob = 0.02f; r.legAmp = 0.6f;
            }
            {
                Rig& r = rigs[Duck];
                r.height = 0.70f; r.rx = 0.30f; r.body = { 0, -0.28f }; r.bx = 0.25f; r.by = 0.16f;
                r.legs = 2; r.legX[0] = -0.02f; r.legX[1] = 0.05f;
                r.legLen = 0.15f; r.legW = 0.045f; r.foot = 0.07f;
                r.neck = { 0.15f, -0.36f };
                r.ears = false; r.tail = false; r.wing = true; r.waddle = true;
                r.eye = { 0.08f, -0.23f }; r.eyeR = 0.022f; r.mouth = { 0.25f, -0.18f };
                r.graze = 1.0f; r.gait = 0.5f; r.speed = 1.6f; r.bob = 0.03f; r.legAmp = 0.5f;
            }
            {
                Rig& r = rigs[Cat];
                r.height = 0.82f; r.rx = 0.32f; r.body = { 0, -0.33f }; r.bx = 0.28f; r.by = 0.14f;
                r.legX[0] = -0.19f; r.legX[1] = 0.15f; r.legX[2] = -0.15f; r.legX[3] = 0.19f;
                r.legLen = 0.24f; r.legW = 0.07f; r.foot = 0.04f;
                r.neck = { 0.24f, -0.42f };
                r.ear[0] = { -0.04f, -0.14f }; r.ear[1] = { 0.08f, -0.15f }; r.earLen = 0.13f; r.earW = 0.11f;
                r.earBase = 0.05f; r.earFlutter = 0.06f; r.earDroop = -0.3f;
                r.tailAt = { -0.27f, -0.37f };
                r.eye = { 0.11f, -0.08f }; r.eyeR = 0.03f; r.mouth = { 0.18f, 0.01f };
                r.graze = 0.6f; r.gait = 0.65f; r.speed = 2.2f; r.bob = 0.04f; r.legAmp = 0.45f;
            }
            {
                Rig& r = rigs[Snail];
                r.height = 0.50f; r.rx = 0.36f; r.body = { 0, -0.06f }; r.bx = 0.32f; r.by = 0.06f;
                r.legs = 0;
                r.neck = { 0.27f, -0.11f };
                r.ear[0] = { 0.0f, -0.05f }; r.ear[1] = { 0.045f, -0.05f }; r.earLen = 0.17f; r.earW = 0.035f;
                r.earBase = 0.25f; r.earFlutter = 0.08f; r.earDroop = 0.9f;
                r.tail = false;
                r.eyeR = 0; r.mouth = { 0.1f, 0.0f };
                r.graze = 0.3f; r.gait = 1.6f; r.speed = 0.6f; r.bob = 0; r.legAmp = 0; r.stretch = true;
            }
            {
                Rig& r = rigs[Dog];
                r.height = 0.86f; r.rx = 0.34f; r.body = { 0, -0.36f }; r.bx = 0.31f; r.by = 0.17f;
                r.legX[0] = -0.21f; r.legX[1] = 0.18f; r.legX[2] = -0.16f; r.legX[3] = 0.22f;
                r.legLen = 0.27f; r.legW = 0.08f; r.foot = 0.045f;
                r.neck = { 0.26f, -0.43f };
                // Floppy, hanging ears (shared rig across the Rottweiler/Golden Doodle/White Doodle
                // coat variants) rather than the pointy Fox/Cat ear shape.
                r.ear[0] = { -0.02f, -0.1f }; r.ear[1] = { 0.06f, -0.11f }; r.earLen = 0.19f; r.earW = 0.1f;
                r.earBase = -0.15f; r.earFlutter = 0.08f; r.earDroop = -0.65f;
                r.tailAt = { -0.29f, -0.4f };
                r.eye = { 0.13f, -0.07f }; r.eyeR = 0.03f; r.mouth = { 0.26f, 0.02f };
                r.graze = 0.65f; r.gait = 0.6f; r.speed = 2.4f; r.bob = 0.05f; r.legAmp = 0.5f;
            }
            {
                Rig& r = rigs[Squirrel];
                r.height = 1.08f; r.rx = 0.48f; r.body = { -0.015f, -0.27f }; r.bx = 0.25f; r.by = 0.24f;
                r.legX[0] = 0.015f; r.legX[1] = 0.08f; r.legX[2] = 0.085f; r.legX[3] = 0.13f;
                r.legLen = 0.09f; r.legW = 0.065f; r.foot = 0.055f;
                r.neck = { 0.025f, -0.50f };
                r.ear[0] = { -0.045f, -0.19f }; r.ear[1] = { 0.045f, -0.205f }; r.earLen = 0.12f; r.earW = 0.06f;
                r.earBase = -0.06f; r.earFlutter = 0.07f; r.earDroop = -0.35f;
                r.tailAt = { -0.18f, -0.10f }; r.tailBase = 0.06f;
                r.eye = { 0.095f, -0.152f }; r.eyeR = 0.05f; r.mouth = { 0.25f, 0.015f };
                r.graze = 0.65f; r.gait = 0.45f; r.speed = 2.9f; r.bob = 0.10f; r.legAmp = 0.55f; r.hopper = true;
            }
            return rigs;
        }
    }

    Rig const& RigFor(int species)
    {
        static const auto rigs = MakeRigs();
        return rigs[std::clamp(species, 0, SpeciesCount - 1)];
    }

    std::vector<PartSpec> PartsFor(int species)
    {
        Rig const& r = RigFor(species);
        std::vector<PartSpec> v;
        auto leg = [&](int i) {
            if (species == Squirrel && (i == 1 || i == 3))
            {
                // Forepaws pivot at the shoulders for holding, digging and waving.
                v.push_back({ PartKind::Leg, i, false, { r.legX[i], i == 1 ? -0.405f : -0.38f }, { -0.06f, -0.06f }, { 0.20f, 0.14f } });
                return;
            }
            if (species == Squirrel)
            {
                // Pivot at the haunch baseline so the soles stay aligned when the feet flatten to rest.
                float soleY = r.body.y + 0.09f + 0.16f;
                v.push_back({ PartKind::Leg, i, false, { r.legX[i], soleY }, { -0.075f, -0.075f }, { 0.145f, 0.01f } });
                return;
            }
            v.push_back({ PartKind::Leg, i, false, { r.legX[i], -r.legLen }, { -r.legW * 0.6f - 0.04f, -r.legW * 0.6f - 0.01f },
                { r.legW * 0.6f + r.foot + 0.1f, r.legLen + r.legW * 0.7f + 0.02f } });
        };
        if (r.legs == 4)
        {
            if (species != Squirrel) leg(0);
            leg(1);
        }
        if (r.legs == 2) leg(0);
        if (r.tail) v.push_back({ PartKind::Tail, 0, false, r.tailAt, { species == Squirrel ? -0.80f : -0.62f, species == Squirrel ? -1.0f : -0.6f }, { 0.2f, 0.25f } });
        float2 bmin(r.body.x - r.bx - 0.14f, r.body.y - r.by - 0.32f), bmax(r.body.x + r.bx + 0.16f, 0.06f);
        if (species == Snail) { bmin = { -0.44f, -0.5f }; bmax = { 0.44f, 0.05f }; }
        v.push_back({ PartKind::Body, 0, false, { 0, 0 }, bmin, bmax });
        if (r.wing) v.push_back({ PartKind::Wing, 0, false, { -0.02f, -0.33f }, { -0.3f, -0.16f }, { 0.14f, 0.16f } });
        if (species == Squirrel)
            v.push_back({ PartKind::Carry, 0, false, { 0.30f, -0.39f }, { -0.09f, -0.10f }, { 0.09f, 0.13f } });
        if (r.legs == 4)
        {
            if (species == Squirrel) leg(0);
            leg(2);
            if (species != Squirrel) leg(3);
        }
        if (r.legs == 2) leg(1);
        float hw = std::max(r.earW * 0.7f + 0.05f, 0.09f);
        if (r.ears) v.push_back({ PartKind::Ear, 0, true, r.ear[0], { -hw, -r.earLen - 0.07f }, { hw, 0.05f } });
        float2 hmin(-0.25f, -0.45f), hmax(0.42f, 0.22f);
        if (species == Deer) { hmin = { -0.2f, -0.8f }; hmax = { 0.45f, 0.15f }; }
        if (species == Duck) { hmin = { -0.14f, -0.36f }; hmax = { 0.34f, 0.08f }; }
        v.push_back({ PartKind::Head, 0, true, { 0, 0 }, hmin, hmax });
        if (r.ears) v.push_back({ PartKind::Ear, 1, true, r.ear[1], { -hw, -r.earLen - 0.07f }, { hw, 0.05f } });
        if (r.eyeR > 0)
        {
            float e = r.eyeR * 1.7f;
            v.push_back({ PartKind::Lid, 0, true, r.eye, { -e, -e }, { e, e } });
            float g = r.eyeR * 2.2f;
            v.push_back({ PartKind::Glint, 0, true, r.eye, { -g, -g }, { g, g }, false });
        }
        // Keep the squirrel's near forepaw visible over its cheek when waving.
        if (species == Squirrel) leg(3);
        else
            v.push_back({ PartKind::Carry, 0, true, r.mouth, { -0.14f, -0.11f }, { 0.14f, 0.13f } });
        return v;
    }

    // ---------------------------------------------------------------- palettes

    namespace
    {
        struct Pal
        {
            uint32_t body, belly, dark, accent, extra, head, iris;
        };

        Pal PalFor(int species, int v)
        {
            v &= 3;
            switch (species)
            {
            case Bunny:
            {
                const uint32_t body[] = { 0xEADCC8, 0xB08A66, 0x9D9BA6, 0xF5F1EA };
                return { body[v], 0xFBF6EE, 0x2E2426, 0xF2AAB2, 0, body[v], 0x2E2426 };
            }
            case Fox:
            {
                const uint32_t body[] = { 0xE47E38, 0xCF6428, 0xD1A472, 0xA3A2AE };
                return { body[v], 0xFBF4EA, 0x3A2A27, 0x2E2422, 0, body[v], 0x2E2422 };
            }
            case Sheep:
            {
                const uint32_t wool[] = { 0xF7F2E8, 0xEFE4CF, 0xDCD5CC, 0x5E5653 };
                const uint32_t face[] = { 0x3B3331, 0x4E4039, 0x302B2B, 0x221E1E };
                return { wool[v], wool[v], face[v], 0xE7A7A7, 0, face[v], 0x1C1818 };
            }
            case Deer:
            {
                const uint32_t body[] = { 0xC38853, 0xAE7543, 0xCB9C69, 0xB98153 };
                return { body[v], 0xF4E8D6, 0x372A25, 0xE8C6A4, 0xE9DCC2, body[v], 0x241C1A };
            }
            case Hedgehog:
            {
                const uint32_t spines[] = { 0x6E523F, 0x8A6A50, 0x5F5550, 0x9A7656 };
                const uint32_t face[] = { 0xECD6B8, 0xF1DDC2, 0xE4D2BF, 0xF0D8B4 };
                return { spines[v], face[v], 0x241C1C, 0xEFA0A0, 0, face[v], 0x241C1C };
            }
            case Duck:
            {
                const uint32_t body[] = { 0xF8F6F0, 0xF6D35A, 0xA88F74, 0x9C978E };
                const uint32_t head[] = { 0xF8F6F0, 0xF6D35A, 0x9A8268, 0x2E7A55 };
                const uint32_t bill[] = { 0xF0973A, 0xE88A2E, 0xE2A044, 0xE8C040 };
                const uint32_t wing[] = { 0xE6E2D8, 0xEEC447, 0x8A735A, 0x7E786F };
                return { body[v], MixRgb(body[v], 0xFFFFFF, 0.35f), 0x221C1A, bill[v], wing[v], head[v], 0x221C1A };
            }
            case Cat:
            {
                const uint32_t body[] = { 0xE9A158, 0x8E909A, 0x3E3B3B, 0xF1E3CC };
                const uint32_t stripe[] = { 0xC97E3C, 0x6C6E78, 0x2C2A2A, 0xD8A266 };
                return { body[v], 0xFAF5EE, 0x1E1A18, 0xE8999C, stripe[v], body[v], v == 2 ? 0xE2C552u : 0x8DB24Cu };
            }
            case Squirrel:
            {
                // Red, grey, black and golden coats, each with a pale chest and a fuller tail.
                const uint32_t body[] = { 0xCD783F, 0x9C9892, 0x4B4341, 0xDCA65F };
                const uint32_t belly[] = { 0xFFF0D2, 0xF5EEE3, 0xC3B6A2, 0xFFF0D0 };
                const uint32_t tail[] = { 0xBF6837, 0x9D968E, 0x554945, 0xC58B46 };
                return { body[v], belly[v], 0x2A211D, 0xE99A87, tail[v], body[v], 0x2F221D };
            }
            case Dog:
            {
                // 0 Rottweiler (black/tan), 1 Golden Doodle (golden curly), 2 White Doodle (white
                // curly), 3 a bonus chocolate-lab coat rounding out the 4-slot variant palette.
                const uint32_t body[] = { 0x1E1A18, 0xE8C27A, 0xF5F1E8, 0x6B4226 };
                const uint32_t belly[] = { 0xB8763A, 0xF6E4B8, 0xFFFFFF, 0x8A5A36 };
                const uint32_t dark[] = { 0x120F0E, 0x8A6534, 0x3A3430, 0x3A2414 };
                const uint32_t mark[] = { 0x8A5A2A, 0xD9A94F, 0xE3DCC9, 0x5A3A20 };
                return { body[v], belly[v], dark[v], 0x3A2420, mark[v], body[v], v == 0 || v == 3 ? 0x2A1A10u : 0x5E4028u };
            }
            default:
            {
                const uint32_t body[] = { 0xCDBB9F, 0xBEAE96, 0xD4C3A9, 0xB9A58C };
                const uint32_t shell[] = { 0xD38A4E, 0xB77392, 0x86A070, 0xD2A85A };
                return { body[v], MixRgb(body[v], 0xFFFFFF, 0.3f), 0x2A2220, 0xEFA0A0, shell[v], body[v], 0x2A2220 };
            }
            }
        }

        // ------------------------------------------------------------ pen

        struct Pen
        {
            ID2D1DeviceContext* dc;
            std::function<uint32_t(uint32_t)> const& lit;
            com_ptr<ID2D1Factory> f;
            com_ptr<ID2D1StrokeStyle> round;

            Pen(ID2D1DeviceContext* d, std::function<uint32_t(uint32_t)> const& l) : dc(d), lit(l)
            {
                dc->GetFactory(f.put());
                f->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
                    D2D1_LINE_JOIN_ROUND), nullptr, 0, round.put());
            }
            D2D1_COLOR_F Col(uint32_t c, float a, bool raw) const { return D2DColor(raw ? c : lit(c), a); }
            com_ptr<ID2D1SolidColorBrush> Solid(uint32_t c, float a = 1, bool raw = false)
            {
                com_ptr<ID2D1SolidColorBrush> b;
                dc->CreateSolidColorBrush(Col(c, a, raw), b.put());
                return b;
            }
            com_ptr<ID2D1LinearGradientBrush> Linear(float2 p0, float2 p1, uint32_t c0, uint32_t c1, float a0 = 1, float a1 = 1)
            {
                D2D1_GRADIENT_STOP stops[2] = { { 0.0f, Col(c0, a0, false) }, { 1.0f, Col(c1, a1, false) } };
                com_ptr<ID2D1GradientStopCollection> coll;
                com_ptr<ID2D1LinearGradientBrush> b;
                if (SUCCEEDED(dc->CreateGradientStopCollection(stops, 2, coll.put())))
                    dc->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties({ p0.x, p0.y }, { p1.x, p1.y }), coll.get(), b.put());
                return b;
            }
            com_ptr<ID2D1LinearGradientBrush> Vert(float y0, float y1, uint32_t c0, uint32_t c1) { return Linear({ 0, y0 }, { 0, y1 }, c0, c1); }
            com_ptr<ID2D1RadialGradientBrush> Radial(float2 c, float rx, float ry, uint32_t c0, float a0, uint32_t c1, float a1, bool raw = false)
            {
                D2D1_GRADIENT_STOP stops[2] = { { 0.0f, Col(c0, a0, raw) }, { 1.0f, Col(c1, a1, raw) } };
                com_ptr<ID2D1GradientStopCollection> coll;
                com_ptr<ID2D1RadialGradientBrush> b;
                if (SUCCEEDED(dc->CreateGradientStopCollection(stops, 2, coll.put())))
                    dc->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties({ c.x, c.y }, { 0, 0 }, rx, ry), coll.get(), b.put());
                return b;
            }
            com_ptr<ID2D1Geometry> Ell(float2 c, float rx, float ry, float deg = 0)
            {
                com_ptr<ID2D1EllipseGeometry> e;
                f->CreateEllipseGeometry(D2D1::Ellipse({ c.x, c.y }, rx, ry), e.put());
                if (deg == 0 || !e) return e;
                com_ptr<ID2D1TransformedGeometry> t;
                f->CreateTransformedGeometry(e.get(), D2D1::Matrix3x2F::Rotation(deg, { c.x, c.y }), t.put());
                return t;
            }
            com_ptr<ID2D1Geometry> Smooth(std::vector<float2> const& p, bool closed)
            {
                com_ptr<ID2D1PathGeometry> path;
                com_ptr<ID2D1GeometrySink> sink;
                if (p.size() < 2 || FAILED(f->CreatePathGeometry(path.put())) || FAILED(path->Open(sink.put()))) return nullptr;
                size_t n = p.size();
                auto at = [&](ptrdiff_t i) {
                    if (closed) return p[(i % static_cast<ptrdiff_t>(n) + n) % n];
                    return p[std::clamp<ptrdiff_t>(i, 0, n - 1)];
                };
                sink->BeginFigure({ p[0].x, p[0].y }, closed ? D2D1_FIGURE_BEGIN_FILLED : D2D1_FIGURE_BEGIN_HOLLOW);
                size_t segs = closed ? n : n - 1;
                for (size_t i = 0; i < segs; ++i)
                {
                    float2 p0 = at(i - 1), p1 = at(i), p2 = at(i + 1), p3 = at(i + 2);
                    float2 c1 = p1 + (p2 - p0) / 6.0f, c2 = p2 - (p3 - p1) / 6.0f;
                    sink->AddBezier(D2D1::BezierSegment({ c1.x, c1.y }, { c2.x, c2.y }, { p2.x, p2.y }));
                }
                sink->EndFigure(closed ? D2D1_FIGURE_END_CLOSED : D2D1_FIGURE_END_OPEN);
                sink->Close();
                return path;
            }
            com_ptr<ID2D1Geometry> Bezier(float2 start, std::initializer_list<D2D1_BEZIER_SEGMENT> segments)
            {
                com_ptr<ID2D1PathGeometry> path;
                com_ptr<ID2D1GeometrySink> sink;
                if (FAILED(f->CreatePathGeometry(path.put())) || FAILED(path->Open(sink.put()))) return nullptr;
                sink->BeginFigure({ start.x, start.y }, D2D1_FIGURE_BEGIN_FILLED);
                sink->AddBeziers(segments.begin(), static_cast<UINT32>(segments.size()));
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                sink->Close();
                return path;
            }
            com_ptr<ID2D1Geometry> Poly(std::vector<float2> const& p)
            {
                com_ptr<ID2D1PathGeometry> path;
                com_ptr<ID2D1GeometrySink> sink;
                if (p.size() < 3 || FAILED(f->CreatePathGeometry(path.put())) || FAILED(path->Open(sink.put()))) return nullptr;
                sink->BeginFigure({ p[0].x, p[0].y }, D2D1_FIGURE_BEGIN_FILLED);
                for (size_t i = 1; i < p.size(); ++i) sink->AddLine({ p[i].x, p[i].y });
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                sink->Close();
                return path;
            }
            com_ptr<ID2D1Geometry> Combine(ID2D1Geometry* a, ID2D1Geometry* b, D2D1_COMBINE_MODE mode)
            {
                com_ptr<ID2D1PathGeometry> path;
                com_ptr<ID2D1GeometrySink> sink;
                if (!a || !b || FAILED(f->CreatePathGeometry(path.put())) || FAILED(path->Open(sink.put()))) return nullptr;
                a->CombineWithGeometry(b, mode, nullptr, 0.0015f, sink.get());
                sink->Close();
                return path;
            }
            com_ptr<ID2D1Geometry> Intersect(ID2D1Geometry* a, ID2D1Geometry* b) { return Combine(a, b, D2D1_COMBINE_MODE_INTERSECT); }
            com_ptr<ID2D1Geometry> Union(ID2D1Geometry* a, ID2D1Geometry* b) { return Combine(a, b, D2D1_COMBINE_MODE_UNION); }
            void Fill(ID2D1Geometry* g, ID2D1Brush* b) { if (g && b) dc->FillGeometry(g, b); }
            void Fill(com_ptr<ID2D1Geometry> const& g, ID2D1Brush* b) { Fill(g.get(), b); }
            void Stroke(com_ptr<ID2D1Geometry> const& g, ID2D1Brush* b, float w) { if (g && b) dc->DrawGeometry(g.get(), b, w, round.get()); }
            void Dot(float2 c, float rx, float ry, ID2D1Brush* b) { if (b) dc->FillEllipse(D2D1::Ellipse({ c.x, c.y }, rx, ry), b); }
            void Capsule(float x0, float y0, float x1, float y1, float r, ID2D1Brush* b)
            {
                if (b) dc->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x0, y0, x1, y1), r, r), b);
            }
            // Soft volume: gradient fill, lighter top highlight clipped to the shape.
            void Volume(com_ptr<ID2D1Geometry> const& g, float top, float bottom, uint32_t c, float hlAlpha = 0.18f)
            {
                if (!g) return;
                Fill(g, Vert(top, bottom, MixRgb(c, 0xFFFFFF, 0.12f), ScaleRgb(c, 0.8f)).get());
                D2D1_RECT_F bounds{};
                g->GetBounds(nullptr, &bounds);
                float cx = (bounds.left + bounds.right) * 0.5f, w = bounds.right - bounds.left, h = bounds.bottom - bounds.top;
                auto hl = Intersect(g.get(), Ell({ cx - w * 0.08f, bounds.top + h * 0.12f }, w * 0.36f, h * 0.22f).get());
                Fill(hl, Radial({ cx - w * 0.08f, bounds.top + h * 0.12f }, w * 0.36f, h * 0.22f, 0xFFFFFF, hlAlpha, 0xFFFFFF, 0).get());
            }
            void Eye(float2 c, float r, uint32_t iris, bool cat)
            {
                if (cat)
                {
                    Dot(c, r * 0.95f, r, Solid(iris).get());
                    Dot(c, r * 0.32f, r * 0.82f, Solid(0x141010).get());
                }
                else
                {
                    Dot(c, r * 0.88f, r, Solid(iris).get());
                }
                Dot({ c.x + r * 0.28f, c.y - r * 0.38f }, r * 0.36f, r * 0.36f, Solid(0xFFFFFF, 0.92f).get());
                Dot({ c.x - r * 0.3f, c.y + r * 0.35f }, r * 0.16f, r * 0.16f, Solid(0xFFFFFF, 0.6f).get());
            }
            void Blush(float2 c, float r, uint32_t col, float a = 0.45f)
            {
                Dot(c, r, r * 0.7f, Radial(c, r, r * 0.7f, col, a, col, 0).get());
            }
        };

        // ------------------------------------------------------------ parts

        void DrawLeg(Pen& p, int sp, int v, Pal const& c, Rig const& r, int idx)
        {
            bool back = r.legs == 4 && (idx == 0 || idx == 2);
            if (sp == Squirrel)
            {
                if (back)
                {
                    p.Dot({ 0.035f, -0.034f }, 0.10f, 0.034f,
                        p.Vert(-0.068f, 0.0f, ScaleRgb(c.body, 0.85f), ScaleRgb(c.body, 0.68f)).get());
                    return;
                }
                auto paw = p.Smooth({ { -0.025f, 0.01f }, { 0.005f, -0.025f }, { 0.06f, 0.015f },
                    { 0.13f, 0.035f }, { 0.16f, 0.06f }, { 0.13f, 0.09f }, { 0.045f, 0.085f }, { -0.015f, 0.045f } }, true);
                p.Volume(paw, -0.025f, 0.09f, c.body, 0.12f);
                p.Stroke(p.Smooth({ { 0.12f, 0.065f }, { 0.14f, 0.068f } }, false), p.Solid(c.dark, 0.25f).get(), 0.007f);
                return;
            }
            float w = r.legW, len = r.legLen;
            uint32_t top = c.body, bottom = ScaleRgb(c.body, 0.92f), foot = ScaleRgb(c.body, 0.95f);
            switch (sp)
            {
            case Bunny: foot = c.belly; break;
            case Fox: bottom = c.dark; foot = c.dark; break;
            case Sheep: top = bottom = foot = c.dark; break;
            case Deer: foot = c.dark; break;
            case Hedgehog: top = bottom = foot = ScaleRgb(c.belly, 0.78f); break;
            case Duck: top = bottom = foot = c.accent; break;
            case Cat: if (v >= 2) foot = c.belly; break;
            case Dog: foot = c.belly; if (v == 0) bottom = c.extra; break;
            }
            if (sp == Duck)
            {
                p.Capsule(-w * 0.35f, -w * 0.3f, w * 0.35f, len, w * 0.35f, p.Solid(top).get());
                p.Fill(p.Smooth({ { -0.02f, len - 0.02f }, { 0.1f, len - 0.015f }, { 0.11f, len + 0.02f }, { -0.03f, len + 0.02f } }, true), p.Solid(foot).get());
                return;
            }
            p.Capsule(-w * 0.5f, -w * 0.5f, w * 0.5f, len, w * 0.5f, p.Vert(0, len, top, bottom).get());
            if (sp == Bunny && back)
                p.Dot({ 0.035f, len - 0.02f }, 0.1f, 0.042f, p.Vert(len - 0.06f, len + 0.02f, foot, ScaleRgb(foot, 0.85f)).get());
            else if (sp == Deer)
                p.Capsule(-w * 0.5f, len - w * 0.7f, w * 0.5f, len + w * 0.1f, w * 0.3f, p.Solid(foot).get());
            else
                p.Dot({ r.foot * 0.45f, len - w * 0.15f }, w * 0.55f + r.foot * 0.5f, w * 0.42f, p.Solid(foot).get());
        }

        void DrawBody(Pen& p, int sp, int v, Pal const& c, Rig const& r)
        {
            float2 b = r.body;
            switch (sp)
            {
            case Squirrel:
            {
                auto main = p.Smooth({ { b.x - 0.21f, b.y + 0.17f }, { b.x - 0.25f, b.y - 0.02f },
                    { b.x - 0.13f, b.y - 0.22f }, { b.x + 0.02f, b.y - 0.25f }, { b.x + 0.15f, b.y - 0.16f },
                    { b.x + 0.23f, b.y + 0.04f }, { b.x + 0.18f, b.y + 0.20f }, { b.x + 0.02f, b.y + 0.23f } }, true);
                auto haunch = p.Ell({ b.x - 0.08f, b.y + 0.09f }, 0.17f, 0.16f);
                auto body = p.Union(main.get(), haunch.get());
                p.Volume(body, b.y - r.by, b.y + r.by, c.body);
                p.Fill(p.Intersect(body.get(), p.Ell({ b.x + 0.12f, b.y + 0.015f }, 0.12f, 0.22f, -12.0f).get()), p.Solid(c.belly).get());
                p.Stroke(p.Smooth({ { b.x - 0.13f, b.y + 0.01f }, { b.x - 0.05f, b.y + 0.05f }, { b.x - 0.04f, b.y + 0.13f } }, false),
                    p.Solid(ScaleRgb(c.body, 0.75f), 0.25f).get(), 0.01f);
                break;
            }
            case Bunny:
            {
                auto main = p.Ell(b, r.bx, r.by);
                auto haunch = p.Ell({ b.x - 0.1f, b.y + 0.06f }, 0.17f, 0.16f);
                p.Volume(main, b.y - r.by, b.y + r.by, c.body);
                p.Volume(haunch, b.y - 0.1f, b.y + 0.22f, c.body, 0.12f);
                p.Fill(p.Intersect(main.get(), p.Ell({ b.x + 0.12f, b.y + 0.13f }, 0.15f, 0.1f).get()), p.Solid(c.belly, 0.9f).get());
                break;
            }
            case Fox:
            {
                auto main = p.Ell(b, r.bx, r.by);
                p.Volume(main, b.y - r.by, b.y + r.by, c.body);
                p.Fill(p.Intersect(main.get(), p.Ell({ b.x + 0.26f, b.y + 0.07f }, 0.14f, 0.13f).get()), p.Solid(c.belly).get());
                p.Fill(p.Intersect(main.get(), p.Ell({ b.x, b.y + 0.16f }, 0.26f, 0.06f).get()), p.Solid(c.belly, 0.8f).get());
                break;
            }
            case Sheep:
            {
                constexpr int n = 14;
                auto shade = p.Solid(ScaleRgb(c.body, 0.82f));
                for (int i = 0; i < n; ++i)
                {
                    float a = 6.2831853f * i / n;
                    p.Dot({ b.x + std::cos(a) * r.bx * 0.86f, b.y + std::sin(a) * r.by * 0.82f + 0.02f }, 0.105f, 0.105f, shade.get());
                }
                p.Dot(b, r.bx * 0.86f, r.by * 0.84f, shade.get());
                for (int i = 0; i < n; ++i)
                {
                    float a = 6.2831853f * i / n;
                    float2 at(b.x + std::cos(a) * r.bx * 0.86f, b.y + std::sin(a) * r.by * 0.82f);
                    float lift = 0.5f - 0.5f * std::sin(a);
                    uint32_t col = MixRgb(ScaleRgb(c.body, 0.86f), MixRgb(c.body, 0xFFFFFF, 0.15f), lift);
                    p.Dot(at, 0.1f, 0.1f, p.Radial({ at.x - 0.02f, at.y - 0.03f }, 0.12f, 0.12f, MixRgb(col, 0xFFFFFF, 0.12f), 1, ScaleRgb(col, 0.9f), 1).get());
                }
                p.Dot({ b.x, b.y - 0.02f }, r.bx * 0.8f, r.by * 0.74f, p.Vert(b.y - r.by, b.y + r.by, MixRgb(c.body, 0xFFFFFF, 0.1f), ScaleRgb(c.body, 0.9f)).get());
                break;
            }
            case Deer:
            {
                auto main = p.Ell(b, r.bx, r.by);
                p.Volume(main, b.y - r.by, b.y + r.by, c.body);
                p.Fill(p.Intersect(main.get(), p.Ell({ b.x + 0.02f, b.y + 0.15f }, 0.28f, 0.07f).get()), p.Solid(c.belly, 0.9f).get());
                if (v >= 2)
                {
                    auto spot = p.Solid(0xFFF8EC, 0.75f);
                    const float2 spots[] = { { -0.16f, -0.08f }, { -0.05f, -0.11f }, { 0.07f, -0.09f }, { -0.1f, -0.02f }, { 0.02f, -0.03f }, { 0.14f, -0.04f } };
                    for (auto s : spots) p.Dot({ b.x + s.x, b.y + s.y }, 0.022f, 0.016f, spot.get());
                }
                break;
            }
            case Hedgehog:
            {
                std::vector<float2> pts;
                constexpr int n = 26;
                for (int i = 0; i <= n; ++i)
                {
                    float a = 3.14159265f + 3.14159265f * i / n;
                    float k = (i % 2) ? 1.0f : 0.0f;
                    pts.push_back({ b.x + std::cos(a) * (r.bx + 0.07f * k), b.y + std::sin(a) * (r.by + 0.075f * k) });
                }
                pts.push_back({ b.x + r.bx * 0.95f, b.y + 0.1f });
                pts.push_back({ b.x - r.bx * 0.95f, b.y + 0.1f });
                auto spikes = p.Poly(pts);
                p.Fill(spikes, p.Vert(b.y - r.by - 0.07f, b.y + 0.1f, MixRgb(c.body, 0xFFFFFF, 0.08f), ScaleRgb(c.body, 0.7f)).get());
                auto inner = p.Ell({ b.x - 0.01f, b.y + 0.01f }, r.bx * 0.8f, r.by * 0.78f);
                p.Fill(inner, p.Radial({ b.x - 0.05f, b.y - 0.08f }, r.bx * 0.9f, r.by, MixRgb(c.body, 0xF4E2C8, 0.35f), 1, c.body, 1).get());
                auto tips = p.Solid(MixRgb(c.body, 0xFFF4E0, 0.55f), 0.7f);
                for (int i = 1; i < n; i += 2)
                {
                    float a = 3.14159265f + 3.14159265f * i / n;
                    p.Dot({ b.x + std::cos(a) * (r.bx + 0.05f), b.y + std::sin(a) * (r.by + 0.055f) }, 0.012f, 0.012f, tips.get());
                }
                p.Dot({ b.x + 0.04f, b.y + 0.1f }, r.bx * 0.82f, 0.05f, p.Solid(ScaleRgb(c.belly, 0.9f)).get());
                break;
            }
            case Duck:
            {
                auto g = p.Smooth({ { -0.34f, -0.45f }, { -0.25f, -0.34f }, { -0.23f, -0.2f }, { -0.08f, -0.13f }, { 0.1f, -0.13f },
                    { 0.23f, -0.2f }, { 0.25f, -0.3f }, { 0.15f, -0.4f }, { -0.02f, -0.42f }, { -0.2f, -0.39f } }, true);
                p.Volume(g, -0.45f, -0.13f, c.body);
                p.Fill(p.Intersect(g.get(), p.Ell({ 0.06f, -0.15f }, 0.2f, 0.06f).get()), p.Solid(c.belly, 0.8f).get());
                break;
            }
            case Cat:
            {
                auto main = p.Ell(b, r.bx, r.by);
                p.Volume(main, b.y - r.by, b.y + r.by, c.body);
                if (v != 3)
                {
                    auto stripe = p.Solid(c.extra, v == 2 ? 0.35f : 0.8f);
                    for (int i = 0; i < 3; ++i)
                    {
                        float x = b.x - 0.14f + 0.1f * i;
                        p.Fill(p.Intersect(main.get(), p.Ell({ x, b.y - r.by + 0.02f }, 0.022f, 0.08f).get()), stripe.get());
                    }
                }
                if (v >= 1) p.Fill(p.Intersect(main.get(), p.Ell({ b.x + 0.18f, b.y + 0.08f }, 0.14f, 0.09f).get()), p.Solid(c.belly).get());
                break;
            }
            case Dog:
            {
                // The Rottweiler is stockier/more muscular than the shared Dog rig's default
                // lean build — broaden and deepen the chest/body ellipse for just that variant.
                float bx = v == 0 ? r.bx * 1.16f : r.bx, by = v == 0 ? r.by * 1.12f : r.by;
                auto main = p.Ell(b, bx, by);
                p.Volume(main, b.y - by, b.y + by, c.body);
                // Chest/belly patch: the Rottweiler's tan bib, or a lighter undercoat on the others.
                p.Fill(p.Intersect(main.get(), p.Ell({ b.x + 0.2f, b.y + 0.1f }, v == 0 ? 0.18f : 0.16f, v == 0 ? 0.115f : 0.1f).get()), p.Solid(c.belly, v == 0 ? 1.0f : 0.85f).get());
                if (v == 1 || v == 2)
                {
                    // Curly-coat texture: a scatter of little loop strokes over the body for the doodles.
                    auto curl = p.Solid(c.extra, 0.6f);
                    const float2 curls[] = { { -0.16f, -0.08f }, { -0.04f, -0.14f }, { 0.08f, -0.1f }, { -0.1f, 0.02f }, { 0.04f, 0.04f }, { 0.16f, -0.02f } };
                    for (auto const& at : curls)
                        p.Stroke(p.Smooth({ { b.x + at.x - 0.028f, b.y + at.y }, { b.x + at.x, b.y + at.y - 0.03f }, { b.x + at.x + 0.028f, b.y + at.y } }, false), curl.get(), 0.02f);
                }
                break;
            }
            default: // snail
            {
                auto foot = p.Smooth({ { -0.37f, -0.01f }, { -0.1f, 0.0f }, { 0.18f, 0.0f }, { 0.34f, -0.02f }, { 0.36f, -0.08f },
                    { 0.29f, -0.13f }, { 0.18f, -0.08f }, { -0.1f, -0.07f }, { -0.3f, -0.05f } }, true);
                p.Volume(foot, -0.13f, 0.0f, c.body, 0.25f);
                float2 s(-0.04f, -0.25f);
                float rs = 0.19f;
                p.Dot({ s.x + 0.02f, s.y + 0.02f }, rs, rs, p.Solid(ScaleRgb(c.extra, 0.6f), 0.5f).get());
                p.Dot(s, rs, rs, p.Radial({ s.x - 0.06f, s.y - 0.07f }, rs * 1.35f, rs * 1.35f, MixRgb(c.extra, 0xFFFFFF, 0.25f), 1, ScaleRgb(c.extra, 0.72f), 1).get());
                std::vector<float2> spiral;
                for (int i = 0; i <= 36; ++i)
                {
                    float t = i / 36.0f, a = t * 3.2f * 3.14159265f;
                    float rr = rs * 0.82f * (1.0f - t * 0.85f);
                    spiral.push_back({ s.x + std::cos(a) * rr, s.y + std::sin(a) * rr });
                }
                p.Stroke(p.Smooth(spiral, false), p.Solid(ScaleRgb(c.extra, 0.62f), 0.8f).get(), 0.022f);
                p.Dot({ s.x - 0.07f, s.y - 0.1f }, 0.055f, 0.03f, p.Solid(0xFFFFFF, 0.3f).get());
                break;
            }
            }
        }

        void DrawHead(Pen& p, int sp, int v, Pal const& c, Rig const& r)
        {
            switch (sp)
            {
            case Squirrel:
            {
                auto head = p.Bezier({ 0.025f, -0.2493f }, {
                    { { 0.1241f, -0.2493f }, { 0.2694f, -0.162f }, { 0.2694f, -0.0603f } },
                    { { 0.2694f, 0.0065f }, { 0.2067f, 0.0631f }, { 0.1355f, 0.094f } },
                    { { 0.0984f, 0.1101f }, { 0.059f, 0.1193f }, { 0.025f, 0.1193f } },
                    { { -0.0741f, 0.1193f }, { -0.1546f, 0.0367f }, { -0.1546f, -0.065f } },
                    { { -0.1546f, -0.1667f }, { -0.0741f, -0.2493f }, { 0.025f, -0.2493f } } });
                auto muzzle = p.Bezier({ 0.0013f, 0.1177f }, {
                    { { -0.0123f, 0.0761f }, { -0.0663f, -0.0544f }, { 0.021f, -0.0913f } },
                    { { 0.071f, -0.1125f }, { 0.232f, -0.0525f }, { 0.2687f, -0.0485f } },
                    { { 0.2581f, 0.0474f }, { 0.1202f, 0.1193f }, { 0.025f, 0.1193f } },
                    { { 0.017f, 0.1193f }, { 0.0091f, 0.1187f }, { 0.0013f, 0.1177f } } });
                p.Volume(head, -0.2493f, 0.1193f, c.body);
                p.Fill(p.Intersect(head.get(), muzzle.get()), p.Solid(c.belly).get());
                p.Dot({ 0.26f, -0.048f }, 0.025f, 0.022f, p.Solid(c.dark).get());
                p.Eye(r.eye, r.eyeR, c.iris, false);
                p.Stroke(p.Smooth({ { 0.095f, 0.018f }, { 0.155f, 0.048f }, { 0.215f, 0.01f } }, false), p.Solid(c.dark, 0.6f).get(), 0.009f);
                p.Blush({ 0.045f, -0.035f }, 0.045f, c.accent, 0.65f);
                break;
            }
            case Bunny:
            {
                auto head = p.Ell({ 0.07f, -0.08f }, 0.17f, 0.15f);
                p.Volume(head, -0.23f, 0.07f, c.body);
                p.Fill(p.Intersect(head.get(), p.Ell({ 0.15f, -0.01f }, 0.1f, 0.075f).get()), p.Solid(c.belly, 0.85f).get());
                p.Dot({ 0.232f, -0.06f }, 0.024f, 0.019f, p.Solid(c.accent).get());
                p.Eye(r.eye, r.eyeR, c.iris, false);
                p.Blush({ 0.13f, -0.03f }, 0.04f, c.accent);
                break;
            }
            case Fox:
            {
                auto skull = p.Ell({ 0.05f, -0.06f }, 0.14f, 0.12f);
                auto snout = p.Smooth({ { 0.07f, -0.12f }, { 0.2f, -0.06f }, { 0.31f, -0.015f }, { 0.2f, 0.045f }, { 0.05f, 0.05f } }, true);
                p.Volume(skull, -0.18f, 0.06f, c.body);
                p.Fill(snout, p.Vert(-0.1f, 0.05f, MixRgb(c.body, 0xFFFFFF, 0.08f), ScaleRgb(c.body, 0.9f)).get());
                auto cheek = p.Ell({ 0.13f, 0.045f }, 0.2f, 0.07f);
                auto white = p.Solid(c.belly);
                p.Fill(p.Intersect(skull.get(), cheek.get()), white.get());
                p.Fill(p.Intersect(snout.get(), cheek.get()), white.get());
                p.Dot({ 0.305f, -0.018f }, 0.026f, 0.022f, p.Solid(c.accent).get());
                p.Eye(r.eye, r.eyeR, c.iris, false);
                break;
            }
            case Sheep:
            {
                auto face = p.Ell({ 0.1f, 0.02f }, 0.095f, 0.15f, -38.0f);
                p.Fill(face, p.Vert(-0.12f, 0.15f, MixRgb(c.dark, 0xFFFFFF, 0.1f), ScaleRgb(c.dark, 0.85f)).get());
                for (auto [at, rr] : { std::pair{ float2(-0.04f, -0.07f), 0.06f }, { float2(0.0f, -0.12f), 0.065f }, { float2(0.07f, -0.13f), 0.055f } })
                    p.Dot(at, rr, rr, p.Radial({ at.x - 0.015f, at.y - 0.02f }, rr * 1.2f, rr * 1.2f, MixRgb(c.body, 0xFFFFFF, 0.15f), 1, ScaleRgb(c.body, 0.88f), 1).get());
                p.Dot(r.eye, r.eyeR * 1.35f, r.eyeR * 1.35f, p.Solid(0xF6F0E6).get());
                p.Eye(r.eye, r.eyeR * 0.85f, c.iris, false);
                p.Blush({ 0.15f, 0.06f }, 0.03f, c.accent, 0.35f);
                break;
            }
            case Deer:
            {
                auto neck = p.Smooth({ { -0.08f, 0.07f }, { -0.06f, -0.15f }, { 0.02f, -0.33f }, { 0.13f, -0.34f }, { 0.11f, -0.15f }, { 0.09f, 0.07f } }, true);
                p.Fill(neck, p.Linear({ -0.05f, 0 }, { 0.11f, 0 }, ScaleRgb(c.body, 0.92f), MixRgb(c.body, 0xFFFFFF, 0.06f)).get());
                p.Fill(p.Intersect(neck.get(), p.Ell({ 0.1f, -0.12f }, 0.05f, 0.14f).get()), p.Solid(c.belly, 0.55f).get());
                if (v < 2)
                {
                    auto antler = p.Solid(c.extra);
                    p.Stroke(p.Smooth({ { 0.08f, -0.43f }, { 0.05f, -0.55f }, { 0.0f, -0.66f } }, false), antler.get(), 0.022f);
                    p.Stroke(p.Smooth({ { 0.05f, -0.54f }, { -0.01f, -0.57f } }, false), antler.get(), 0.018f);
                    p.Stroke(p.Smooth({ { 0.14f, -0.44f }, { 0.15f, -0.56f }, { 0.2f, -0.66f } }, false), antler.get(), 0.022f);
                    p.Stroke(p.Smooth({ { 0.15f, -0.55f }, { 0.21f, -0.57f } }, false), antler.get(), 0.018f);
                }
                auto head = p.Ell({ 0.15f, -0.37f }, 0.12f, 0.085f);
                p.Volume(head, -0.45f, -0.29f, c.body);
                p.Dot({ 0.26f, -0.335f }, 0.075f, 0.05f, p.Solid(MixRgb(c.body, c.belly, 0.25f)).get());
                p.Dot({ 0.325f, -0.34f }, 0.022f, 0.019f, p.Solid(c.dark).get());
                p.Eye(r.eye, r.eyeR, c.iris, false);
                break;
            }
            case Hedgehog:
            {
                auto face = p.Smooth({ { -0.08f, -0.09f }, { 0.04f, -0.08f }, { 0.14f, -0.03f }, { 0.22f, 0.02f }, { 0.14f, 0.065f }, { 0.0f, 0.07f }, { -0.08f, 0.04f } }, true);
                p.Volume(face, -0.09f, 0.07f, c.belly, 0.25f);
                p.Dot({ 0.22f, 0.02f }, 0.026f, 0.022f, p.Solid(c.dark).get());
                p.Eye(r.eye, r.eyeR, c.iris, false);
                p.Blush({ 0.1f, 0.035f }, 0.028f, c.accent);
                break;
            }
            case Duck:
            {
                p.Capsule(-0.05f, -0.17f, 0.06f, 0.03f, 0.05f, p.Linear({ -0.05f, 0 }, { 0.06f, 0 }, ScaleRgb(c.head, 0.92f), c.head).get());
                if (v == 3) p.Capsule(-0.052f, -0.07f, 0.062f, -0.045f, 0.012f, p.Solid(0xF4F2EC).get());
                auto head = p.Ell({ 0.05f, -0.2f }, 0.11f, 0.105f);
                p.Volume(head, -0.31f, -0.1f, c.head, 0.22f);
                p.Fill(p.Smooth({ { 0.13f, -0.235f }, { 0.22f, -0.22f }, { 0.285f, -0.19f }, { 0.25f, -0.163f }, { 0.13f, -0.158f } }, true),
                    p.Vert(-0.23f, -0.16f, MixRgb(c.accent, 0xFFFFFF, 0.1f), ScaleRgb(c.accent, 0.85f)).get());
                p.Eye(r.eye, r.eyeR, c.iris, false);
                if (v == 1) p.Blush({ 0.09f, -0.17f }, 0.03f, 0xF59A6A);
                break;
            }
            case Cat:
            {
                auto head = p.Ell({ 0.06f, -0.06f }, 0.15f, 0.13f);
                p.Volume(head, -0.19f, 0.07f, c.body);
                if (v != 2) p.Fill(p.Intersect(head.get(), p.Ell({ 0.16f, 0.0f }, 0.07f, 0.05f).get()), p.Solid(v == 0 ? MixRgb(c.body, 0xFFFFFF, 0.5f) : c.belly).get());
                p.Fill(p.Poly({ { 0.185f, -0.045f }, { 0.215f, -0.045f }, { 0.2f, -0.025f } }), p.Solid(c.accent).get());
                p.Eye(r.eye, r.eyeR, c.iris, true);
                auto whisker = p.Solid(0xFFFFFF, 0.5f);
                p.Stroke(p.Smooth({ { 0.18f, -0.01f }, { 0.31f, -0.035f } }, false), whisker.get(), 0.007f);
                p.Stroke(p.Smooth({ { 0.18f, 0.0f }, { 0.3f, 0.025f } }, false), whisker.get(), 0.007f);
                p.Blush({ 0.12f, -0.01f }, 0.03f, c.accent, 0.3f);
                break;
            }
            case Dog:
            {
                // The Rottweiler reads as a slightly older, heavier-jowled dog: a touch more
                // skull/snout bulk than the doodles, so it's not just the same head recolored.
                float skullR = v == 0 ? 0.16f : 0.15f;
                auto skull = p.Ell({ 0.04f, -0.09f }, skullR, 0.135f);
                auto snout = p.Smooth(v == 0
                    ? std::vector<float2>{ { 0.07f, -0.12f }, { 0.19f, -0.065f }, { 0.28f, -0.005f }, { 0.22f, 0.07f }, { 0.05f, 0.075f } }
                    : std::vector<float2>{ { 0.08f, -0.12f }, { 0.2f, -0.07f }, { 0.29f, -0.01f }, { 0.2f, 0.06f }, { 0.06f, 0.065f } }, true);
                p.Volume(skull, -0.21f, 0.07f, c.body);
                if (v == 0)
                {
                    // Black mask on top of the muzzle with a tan lower jaw/chin and tan cheek
                    // patch — the breed's signature two-tone face, not an all-tan snout.
                    p.Fill(snout, p.Vert(-0.1f, 0.075f, ScaleRgb(c.body, 1.05f), c.dark).get());
                    p.Fill(p.Intersect(snout.get(), p.Ell({ 0.16f, 0.045f }, 0.15f, 0.045f).get()), p.Solid(c.belly).get());
                    p.Fill(p.Intersect(skull.get(), p.Ell({ -0.07f, -0.02f }, 0.055f, 0.04f).get()), p.Solid(c.belly, 0.85f).get());
                }
                else
                {
                    p.Fill(snout, p.Vert(-0.1f, 0.065f, MixRgb(c.belly, 0xFFFFFF, 0.08f), ScaleRgb(c.belly, 0.95f)).get());
                }
                p.Dot({ v == 0 ? 0.275f : 0.285f, -0.015f }, 0.026f, 0.021f, p.Solid(c.dark).get());
                p.Eye(r.eye, r.eyeR, c.iris, false);
                if (v == 0 || v == 3)
                {
                    // Rottweiler/chocolate-lab tan eyebrow dots above the eyes — bigger and set a
                    // touch higher on the Rottweiler, its classic "worried" eyebrow marking.
                    auto brow = p.Solid(c.belly);
                    p.Dot({ 0.055f, -0.15f }, v == 0 ? 0.021f : 0.018f, v == 0 ? 0.015f : 0.014f, brow.get());
                    p.Dot({ 0.17f, -0.145f }, v == 0 ? 0.021f : 0.018f, v == 0 ? 0.015f : 0.014f, brow.get());
                }
                else
                {
                    // Doodle curly cheek fluff.
                    auto curl = p.Solid(c.extra, 0.6f);
                    for (auto at : { float2(-0.06f, -0.1f), float2(-0.02f, -0.16f), float2(0.02f, -0.19f) })
                        p.Dot({ at.x, at.y }, 0.026f, 0.026f, curl.get());
                }
                if (v == 0)
                {
                    // A dusting of grizzled grey around the muzzle and chin so it reads as an
                    // older dog rather than a puppy.
                    auto grey = p.Solid(0xBDB6AD, 0.55f);
                    p.Dot({ 0.1f, 0.045f }, 0.022f, 0.015f, grey.get());
                    p.Dot({ 0.17f, 0.04f }, 0.018f, 0.013f, grey.get());
                    p.Dot({ -0.01f, -0.02f }, 0.016f, 0.012f, grey.get());
                    // A soft jowl fold line under the muzzle for a looser, older look.
                    p.Stroke(p.Smooth({ { 0.1f, 0.055f }, { 0.19f, 0.058f } }, false), p.Solid(ScaleRgb(c.dark, 1.3f), 0.5f).get(), 0.008f);
                }
                p.Blush({ 0.14f, 0.02f }, 0.028f, c.accent, 0.3f);
                break;
            }
            default: // snail
            {
                auto head = p.Ell({ 0.04f, -0.035f }, 0.085f, 0.07f);
                p.Volume(head, -0.1f, 0.03f, c.body, 0.25f);
                p.Stroke(p.Smooth({ { 0.06f, -0.01f }, { 0.085f, 0.0f }, { 0.105f, -0.012f } }, false), p.Solid(c.dark, 0.6f).get(), 0.01f);
                p.Blush({ 0.07f, -0.005f }, 0.022f, c.accent);
                break;
            }
            }
        }

        void DrawEar(Pen& p, int sp, int v, Pal const& c, Rig const& r)
        {
            float L = r.earLen, w = r.earW;
            switch (sp)
            {
            case Squirrel:
            {
                auto ear = p.Smooth({ { -w * 0.7f, 0.01f }, { -w, -L * 0.5f }, { -w * 0.5f, -L * 0.85f },
                    { 0, -L }, { w * 0.7f, -L * 0.65f }, { w * 0.7f, -L * 0.2f }, { w * 0.5f, 0.01f } }, true);
                p.Fill(ear, p.Vert(-L, 0.01f, c.extra, c.body).get());
                p.Dot({ 0, -L * 0.43f }, w * 0.42f, L * 0.3f, p.Solid(c.accent, 0.8f).get());
                if (v == 0 || v == 3)
                {
                    auto tuft = p.Solid(c.extra);
                    p.Fill(p.Smooth({ { -w * 0.55f, -L * 0.75f }, { -w * 0.25f, -L - 0.02f },
                        { w * 0.12f, -L * 0.9f }, { w * 0.4f, -L * 0.65f } }, true), tuft.get());
                }
                break;
            }
            case Bunny:
            case Deer:
            {
                auto ear = p.Ell({ 0, -L * 0.5f }, w, L * 0.5f);
                p.Fill(ear, p.Linear({ -w, 0 }, { w, 0 }, ScaleRgb(c.body, 0.9f), MixRgb(c.body, 0xFFFFFF, 0.08f)).get());
                p.Dot({ 0.005f, -L * 0.5f + 0.015f }, w * 0.5f, L * 0.36f, p.Solid(c.accent, 0.85f).get());
                break;
            }
            case Fox:
            case Cat:
            {
                auto ear = p.Smooth({ { -w * 0.5f, 0.01f }, { -w * 0.2f, -L * 0.6f }, { 0, -L }, { w * 0.25f, -L * 0.55f }, { w * 0.5f, 0.01f } }, true);
                p.Fill(ear, p.Solid(c.body).get());
                p.Fill(p.Poly({ { -w * 0.24f, -0.01f }, { 0.0f, -L * 0.72f }, { w * 0.24f, -0.01f } }), p.Solid(sp == Fox ? c.belly : c.accent, 0.85f).get());
                if (sp == Fox) p.Fill(p.Intersect(ear.get(), p.Ell({ 0, -L }, 0.07f, 0.06f).get()), p.Solid(c.accent).get());
                break;
            }
            case Sheep:
                p.Dot({ 0, -L * 0.5f }, w, L * 0.5f, p.Linear({ -w, 0 }, { w, 0 }, ScaleRgb(c.dark, 0.85f), MixRgb(c.dark, 0xFFFFFF, 0.1f)).get());
                break;
            case Hedgehog:
                p.Dot({ 0, -L * 0.5f }, w, w, p.Solid(ScaleRgb(c.belly, 0.82f)).get());
                break;
            case Snail:
                p.Stroke(p.Smooth({ { 0, 0.0f }, { 0.01f, -L * 0.5f }, { 0, -L } }, false), p.Solid(c.body).get(), 0.03f);
                p.Eye({ 0, -L }, 0.034f, c.iris, false);
                break;
            case Dog:
            {
                if (v == 0)
                {
                    // Rottweiler: small triangular ear held close to the skull, folded forward
                    // and down — not the long hanging floppy ear the doodle coats use.
                    //
                    // A same-color fill with only a hairline stroke proved invisible once
                    // anti-aliasing/haze/distance blur softened it away (confirmed by rendered
                    // snapshot inspection) — the ear sat entirely *inside* the skull's own
                    // silhouette, so there was never a hard, geometry-level edge to read, only a
                    // subtle value change. Extending the tip so it visibly breaches above the
                    // skull's own outline creates a real silhouette notch against the sky/grass
                    // behind it, which survives distance and blur far better than a thin stroke.
                    float eL = L * 0.78f, eW = w * 1.3f;
                    auto ear = p.Smooth({ { -eW * 0.5f, 0.03f }, { -eW * 0.1f, -eL * 0.95f },
                        { eW * 0.58f, -eL * 0.42f }, { eW * 0.44f, 0.06f } }, true);
                    p.Fill(ear, p.Vert(-eL, 0.06f, ScaleRgb(c.body, 1.1f), ScaleRgb(c.body, 0.78f)).get());
                    // Bold dark contour + a brighter fold-crease highlight — thick/opaque enough
                    // to still read once scaled down for a distant, hazier instance.
                    p.Stroke(ear, p.Solid(ScaleRgb(c.body, 0.3f), 1.0f).get(), 0.022f);
                    p.Stroke(p.Smooth({ { -eW * 0.4f, 0.0f }, { -eW * 0.08f, -eL * 0.8f } }, false),
                        p.Solid(MixRgb(c.body, 0xFFFFFF, 0.42f), 0.75f).get(), 0.014f);
                    break;
                }
                // Floppy, hanging ear: wider near the head, rounding to a soft tip, shaded toward
                // the tip since it hangs in its own shadow.
                auto ear = p.Smooth({ { -w * 0.55f, 0.0f }, { -w * 0.65f, -L * 0.45f }, { -w * 0.2f, -L * 0.85f },
                    { w * 0.15f, -L * 0.7f }, { w * 0.5f, -L * 0.25f }, { w * 0.35f, 0.02f } }, true);
                p.Fill(ear, p.Vert(-L, 0.02f, ScaleRgb(c.body, 0.95f), ScaleRgb(c.body, 0.76f)).get());
                p.Fill(p.Intersect(ear.get(), p.Ell({ -w * 0.1f, -L * 0.45f }, w * 0.35f, L * 0.3f).get()), p.Solid(c.belly, 0.4f).get());
                break;
            }
            }
        }

        void DrawTail(Pen& p, int sp, int v, Pal const& c)
        {
            switch (sp)
            {
            case Squirrel:
            {
                // The tail rises from the lower haunch and curls away from the head.
                auto tail = p.Smooth({ { 0.085f, 0.03f }, { 0.015f, -0.04f }, { -0.055f, -0.18f }, { -0.045f, -0.40f },
                    { 0.005f, -0.62f }, { -0.02f, -0.78f }, { -0.16f, -0.90f }, { -0.34f, -0.93f },
                    { -0.53f, -0.88f }, { -0.66f, -0.80f }, { -0.75f, -0.64f }, { -0.67f, -0.49f },
                    { -0.53f, -0.445f }, { -0.425f, -0.51f }, { -0.445f, -0.60f }, { -0.32f, -0.58f },
                    { -0.32f, -0.40f }, { -0.30f, -0.20f }, { -0.22f, -0.045f }, { -0.075f, 0.025f } }, true);
                p.Volume(tail, -0.93f, 0.03f, c.extra, 0.16f);
                p.Fill(p.Intersect(tail.get(), p.Ell({ -0.16f, -0.57f }, 0.18f, 0.33f, -15.0f).get()),
                    p.Radial({ -0.12f, -0.64f }, 0.28f, 0.36f, c.belly, 0.3f, c.belly, 0.0f).get());
                p.Stroke(p.Smooth({ { -0.14f, -0.81f }, { -0.31f, -0.86f }, { -0.50f, -0.82f } }, false),
                    p.Solid(MixRgb(c.extra, c.belly, 0.35f), 0.5f).get(), 0.018f);
                break;
            }
            case Bunny:
            case Sheep:
                p.Dot({ -0.02f, 0 }, 0.075f, 0.072f, p.Radial({ -0.035f, -0.02f }, 0.09f, 0.09f, MixRgb(sp == Bunny ? c.belly : c.body, 0xFFFFFF, 0.2f), 1,
                    ScaleRgb(sp == Bunny ? c.belly : c.body, 0.86f), 1).get());
                break;
            case Fox:
            {
                auto tail = p.Smooth({ { 0.02f, -0.06f }, { -0.2f, -0.1f }, { -0.42f, -0.04f }, { -0.5f, 0.04f }, { -0.38f, 0.1f }, { -0.16f, 0.08f }, { 0.02f, 0.05f } }, true);
                p.Fill(tail, p.Vert(-0.1f, 0.1f, MixRgb(c.body, 0xFFFFFF, 0.1f), ScaleRgb(c.body, 0.85f)).get());
                p.Fill(p.Intersect(tail.get(), p.Ell({ -0.5f, 0.04f }, 0.12f, 0.12f).get()), p.Solid(c.belly).get());
                break;
            }
            case Deer:
                p.Dot({ -0.02f, -0.05f }, 0.035f, 0.07f, p.Solid(c.belly).get());
                break;
            case Cat:
                p.Stroke(p.Smooth({ { 0.02f, 0.0f }, { -0.08f, -0.1f }, { -0.1f, -0.25f }, { -0.04f, -0.37f }, { 0.03f, -0.39f } }, false),
                    p.Solid(ScaleRgb(c.body, 0.95f)).get(), 0.06f);
                break;
            case Dog:
                if (v == 0)
                {
                    // Rottweiler: short, stubby docked-looking tail — a tapered stub, not a long
                    // sweeping one. A round dot read as a button stuck to the rump rather than a
                    // tail segment once rendered (confirmed via snapshot), so this is shaped as a
                    // short wedge: wide where it joins the body (and hidden under it, since Tail
                    // z-orders behind Body) tapering to a rounded tip that clears the Rottweiler's
                    // extra-broad rump (see DrawBody) by a comfortable margin.
                    auto tail = p.Smooth({ { 0.015f, -0.05f }, { -0.07f, -0.065f }, { -0.14f, -0.03f },
                        { -0.15f, 0.015f }, { -0.1f, 0.04f }, { -0.015f, 0.028f } }, true);
                    p.Fill(tail, p.Vert(-0.065f, 0.04f, ScaleRgb(c.body, 1.08f), ScaleRgb(c.body, 0.8f)).get());
                    p.Stroke(tail, p.Solid(ScaleRgb(c.body, 0.3f), 0.9f).get(), 0.016f);
                }
                else
                    p.Stroke(p.Smooth({ { 0.02f, 0.0f }, { -0.1f, -0.08f }, { -0.16f, -0.2f }, { -0.1f, -0.32f }, { -0.01f, -0.36f } }, false),
                        p.Solid(c.body).get(), 0.065f);
                break;
            }
        }

        void DrawCarry(Pen& p, int sp)
        {
            switch (sp)
            {
            case Squirrel:
            {
                auto nut = p.Smooth({ { -0.058f, -0.01f }, { 0.058f, -0.01f }, { 0.05f, 0.05f },
                    { 0.0f, 0.095f }, { -0.05f, 0.05f } }, true);
                p.Volume(nut, -0.01f, 0.095f, 0xBD8546, 0.25f);
                p.Dot({ 0, -0.012f }, 0.067f, 0.033f, p.Vert(-0.045f, 0.021f, 0x8A6035, 0x614126).get());
                p.Stroke(p.Smooth({ { 0, -0.04f }, { 0.008f, -0.073f }, { 0.026f, -0.08f } }, false), p.Solid(0x614126).get(), 0.014f);
                p.Stroke(p.Smooth({ { -0.026f, 0.025f }, { -0.02f, 0.052f } }, false), p.Solid(0xE8BD78, 0.7f).get(), 0.012f);
                break;
            }
            case Bunny:
                p.Stroke(p.Smooth({ { -0.06f, 0.0f }, { -0.11f, -0.05f } }, false), p.Solid(0x6BAA48).get(), 0.018f);
                p.Stroke(p.Smooth({ { -0.06f, 0.0f }, { -0.11f, 0.03f } }, false), p.Solid(0x5C9A3E).get(), 0.018f);
                p.Fill(p.Smooth({ { -0.07f, -0.022f }, { 0.04f, -0.012f }, { 0.08f, 0.004f }, { 0.04f, 0.016f }, { -0.07f, 0.022f } }, true), p.Solid(0xEE8A2E).get());
                break;
            case Sheep:
            case Deer:
            {
                p.Stroke(p.Smooth({ { 0, 0 }, { -0.01f, 0.05f }, { -0.03f, 0.09f } }, false), p.Solid(0x5C9A3E).get(), 0.012f);
                auto petal = p.Solid(0xFFFFFF);
                for (int i = 0; i < 7; ++i)
                {
                    float a = 6.2831853f * i / 7;
                    p.Dot({ std::cos(a) * 0.032f, std::sin(a) * 0.032f - 0.01f }, 0.02f, 0.02f, petal.get());
                }
                p.Dot({ 0, -0.01f }, 0.02f, 0.02f, p.Solid(0xF7C531).get());
                break;
            }
            case Hedgehog:
                p.Dot({ 0, 0 }, 0.045f, 0.042f, p.Radial({ -0.015f, -0.015f }, 0.06f, 0.06f, 0xF0645A, 1, 0xB8322C, 1).get());
                p.Stroke(p.Smooth({ { 0, -0.04f }, { 0.01f, -0.07f } }, false), p.Solid(0x6B4A2A).get(), 0.01f);
                break;
            case Fox:
            case Cat:
                p.Dot({ 0, 0.01f }, 0.034f, 0.044f, p.Solid(0xB0743A).get());
                p.Dot({ 0, -0.026f }, 0.04f, 0.022f, p.Solid(0x7A5532).get());
                break;
            case Dog:
            {
                auto bone = p.Solid(0xEDE3CC);
                p.Capsule(-0.09f, -0.016f, 0.09f, 0.016f, 0.016f, bone.get());
                p.Dot({ -0.09f, -0.025f }, 0.022f, 0.022f, bone.get());
                p.Dot({ -0.09f, 0.025f }, 0.022f, 0.022f, bone.get());
                p.Dot({ 0.09f, -0.025f }, 0.022f, 0.022f, bone.get());
                p.Dot({ 0.09f, 0.025f }, 0.022f, 0.022f, bone.get());
                break;
            }
            default:
                p.Fill(p.Smooth({ { -0.07f, 0.0f }, { 0.0f, -0.035f }, { 0.07f, 0.0f }, { 0.0f, 0.035f } }, true), p.Solid(0x78B04E).get());
                p.Stroke(p.Smooth({ { -0.06f, 0.0f }, { 0.06f, 0.0f } }, false), p.Solid(0x4E8A34).get(), 0.008f);
                break;
            }
        }
    }

    void DrawPart(ID2D1DeviceContext* dc, int species, int variant, PartSpec const& part, std::function<uint32_t(uint32_t)> const& lit)
    {
        Rig const& r = RigFor(species);
        Pal c = PalFor(species, variant);
        bool isFar = (part.kind == PartKind::Leg && (r.legs == 4 ? part.index < 2 : part.index == 0)) || (part.kind == PartKind::Ear && part.index == 0);
        std::function<uint32_t(uint32_t)> farLit = [&](uint32_t col) { return lit(ScaleRgb(col, 0.8f)); };
        Pen p(dc, isFar ? farLit : lit);
        switch (part.kind)
        {
        case PartKind::Leg: DrawLeg(p, species, variant, c, r, part.index); break;
        case PartKind::Tail: DrawTail(p, species, variant, c); break;
        case PartKind::Body: DrawBody(p, species, variant, c, r); break;
        case PartKind::Wing:
        {
            auto wing = p.Ell({ -0.07f, 0.03f }, 0.16f, 0.08f, -12.0f);
            p.Fill(wing, p.Vert(-0.05f, 0.11f, MixRgb(c.extra, 0xFFFFFF, 0.08f), ScaleRgb(c.extra, 0.85f)).get());
            if (variant >= 2) p.Fill(p.Intersect(wing.get(), p.Ell({ -0.16f, 0.06f }, 0.05f, 0.03f).get()), p.Solid(0x3E6CC4, 0.9f).get());
            break;
        }
        case PartKind::Head: DrawHead(p, species, variant, c, r); break;
        case PartKind::Ear: DrawEar(p, species, variant, c, r); break;
        case PartKind::Lid:
        {
            float e = r.eyeR;
            p.Dot({ 0, 0 }, e * 1.2f, e * 1.22f, p.Solid(c.head).get());
            p.Stroke(p.Smooth({ { -e * 0.85f, 0.0f }, { 0.0f, e * 0.38f }, { e * 0.85f, 0.0f } }, false), p.Solid(species == Sheep ? 0x0E0C0C : c.dark, 0.85f).get(), e * 0.28f);
            break;
        }
        case PartKind::Glint:
        {
            float e = r.eyeR;
            p.Dot({ 0, 0 }, e * 2.1f, e * 2.1f, p.Radial({ 0, 0 }, e * 2.1f, e * 2.1f, 0xE8F4C8, 0.55f, 0xE8F4C8, 0.0f, true).get());
            p.Dot({ 0, 0 }, e * 0.75f, e * 0.8f, p.Radial({ 0, 0 }, e * 0.8f, e * 0.8f, 0xFAFFE6, 1.0f, 0xCFE89A, 0.6f, true).get());
            break;
        }
        case PartKind::Carry: DrawCarry(p, species); break;
        }
    }
}
