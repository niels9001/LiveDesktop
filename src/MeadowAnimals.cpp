#include "pch.h"
#include "MeadowAnimals.h"
#include "MeadowAnimalsArt.h"
#include "Graphics.h"
#include "Visuals.h"
#include "Log.h"
#include <cstdarg>

namespace ld
{
    namespace art = ld::animals;
    using SteadyClock = std::chrono::steady_clock;

    namespace
    {
        constexpr float kTau = 6.2831853f;
        constexpr size_t kMaxAnimals = 10;
        constexpr float kLayerSplits[3] = { 1.0f, 1.45f, 1.8f };
        constexpr float kChildScale = 0.6f; // relative size of a sub-agent "child" animal

        std::wstring F(wchar_t const* fmt, ...)
        {
            wchar_t buf[2048];
            va_list args;
            va_start(args, fmt);
            vswprintf_s(buf, std::size(buf), fmt, args);
            va_end(args);
            return buf;
        }

        float Lerp(float a, float b, float t) { return a + (b - a) * t; }

        // Piecewise-linear lookup over (x, y) points sorted by x.
        float Curve(std::initializer_list<float2> pts, float x)
        {
            auto it = pts.begin();
            if (x <= it->x) return it->y;
            for (auto prev = it++; it != pts.end(); prev = it++)
                if (x <= it->x) return Lerp(prev->y, it->y, (x - prev->x) / std::max(1e-5f, it->x - prev->x));
            return (pts.end() - 1)->y;
        }

        const wchar_t* StatusWord(AgentStatus s)
        {
            switch (s)
            {
            case AgentStatus::Working: return L"working";
            case AgentStatus::Waiting: return L"needs you";
            case AgentStatus::Done: return L"done";
            default: return L"idle";
            }
        }
    }

    struct MeadowAnimals::Animal
    {
        AgentSession s;
        int species = 0, variant = 0;
        float seed = 0;
        art::Rig const* rig = nullptr;
        Mode mode = Mode::Busy;
        bool leaving = false, journeying = false, hovered = false, bubble = false;
        // Set when AgentSession::parentId pointed at a session that was alive at creation time; see
        // SessionsProvider::LinkChildren. Child animals render smaller and spawn/leave near their parent.
        bool isChild = false;
        uint64_t token = 0;
        int layer = -1, level = -1;
        float face = 1, slotX = 0;
        float x = 0, d = 0; // last explicit placement (monitor-local x, depth)

        wuc::ContainerVisual root{ nullptr }, facing{ nullptr }, bob{ nullptr }, jumper{ nullptr }, pose{ nullptr }, head{ nullptr };
        wuc::SpriteVisual shadow{ nullptr }, zz{ nullptr };
        wuc::CompositionPropertySet p{ nullptr };
        struct Part
        {
            art::PartSpec spec;
            wuc::SpriteVisual sprite{ nullptr };
            wuc::CompositionDrawingSurface surface{ nullptr };
            float2 px{};
        };
        std::vector<Part> parts;

        struct Motion
        {
            SteadyClock::time_point start{};
            float duration = 0;
            bool loop = false;
            std::vector<float3> samples; // (t, x, d)
        } motion;

        wuc::SpriteVisual label{ nullptr };
        wuc::CompositionDrawingSurface labelSurface{ nullptr };
        float2 labelSize{};
        std::wstring labelText;
        bool labelIsBubble = false;
    };

    // ------------------------------------------------------------------ setup

    MeadowAnimals::MeadowAnimals(SceneContext const& ctx, MeadowStage stage, wuc::CompositionPropertySet const& wind, MeadowLight const& light)
        : m_c(ctx.compositor), m_graphics(ctx.graphics), m_pointer(ctx.pointer), m_wind(wind), m_stage(std::move(stage)), m_lit(light), m_drawn(light)
    {
        m_queue = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
        m_W = static_cast<float>(m_stage.rc.right - m_stage.rc.left);
        m_H = static_cast<float>(m_stage.rc.bottom - m_stage.rc.top);
        m_U = m_H * 0.105f;
        float g = m_stage.groundY(m_W * 0.5f);
        float bottom = m_stage.visibleBottom > 0 ? m_stage.visibleBottom : m_H;
        m_tf = std::clamp((bottom - m_H * 0.04f - g) / std::max(1.0f, m_H * 1.012f - g), 0.45f, 0.85f);

        m_light = m_c.CreatePropertySet();
        m_light.InsertScalar(L"Night", light.night);
        m_hazeBrush = m_c.CreateColorBrush(Argb(255, light.haze));
        m_shadowBrush = GlowBrush(m_c, Argb(255, 0x1A1410), 0.35f, 0.55f);
        m_sparkle = GlowBrush(m_c, Argb(255, 0xFFF4C4), 0.22f, 0.55f);
        for (uint32_t c : { 0xF6B8C8u, 0xFFFFFFu, 0xF9D6A0u, 0xD8C4F2u })
            m_petals.push_back(GlowBrush(m_c, Argb(240, c), 0.5f, 0.85f));

        if (m_graphics)
        {
            m_zzSize = float2(Px(46), Px(40));
            auto surface = m_graphics->CreateSurface(m_zzSize);
            m_graphics->Draw(surface, m_zzSize, [&](ID2D1DeviceContext* dc) {
                winrt::com_ptr<ID2D1SolidColorBrush> brush;
                dc->CreateSolidColorBrush(D2DColor(0xFFFFFF, 0.9f), brush.put());
                auto zLo = m_graphics->Layout(L"z", m_graphics->Font(FontRole::UiSemibold, 15), Px(30), Px(30), false);
                auto big = m_graphics->Layout(L"Z", m_graphics->Font(FontRole::UiSemibold, 21), Px(30), Px(30), false);
                dc->DrawTextLayout(D2D1::Point2F(0, Px(16)), zLo.get(), brush.get());
                dc->DrawTextLayout(D2D1::Point2F(Px(16), 0), big.get(), brush.get());
            });
            m_zzBrush = m_c.CreateSurfaceBrush(surface);
            m_zzBrush.Stretch(wuc::CompositionStretch::Fill);
        }
        Log(L"Meadow animals: stage %.0fx%.0f, U %.1f, front depth %.2f", m_W, m_H, m_U, m_tf);
    }

    MeadowAnimals::~MeadowAnimals()
    {
        for (auto& t : m_timers) t.Stop();
        m_timers.clear();
        for (auto& [id, a] : m_animals)
        {
            if (a->layer >= 0)
                if (auto layer = LayerVisual(a->layer)) layer.Children().Remove(a->root);
            if (a->label && m_stage.top) m_stage.top.Children().Remove(a->label);
        }
    }

    float MeadowAnimals::Px(float dip) const { return m_graphics ? m_graphics->Px(dip) : dip; }

    // ------------------------------------------------------------------ geometry

    float MeadowAnimals::Y(float d, float x) const
    {
        float g = m_stage.groundY(x);
        if (d <= 1.0f) return Lerp(m_stage.backY(x), g + m_H * 0.05f, std::max(0.0f, d));
        float t = Curve({ { 1.0f, 0.0f }, { 1.3f, 0.14f }, { 1.6f, 0.36f }, { 2.0f, m_tf } }, d);
        return g + (m_H * 1.012f - g) * t;
    }

    float MeadowAnimals::ScaleAt(float d) const
    {
        if (d <= 1.0f) return 0.32f + 0.06f * std::max(0.0f, d);
        return Curve({ { 1.0f, 0.38f },  { 1.3f, 0.5f }, { 1.6f, 0.72f }, { 2.0f, 1.1f } }, d);
    }

    float MeadowAnimals::HazeAt(float d) const
    {
        if (d <= 1.0f) return 0.5f - 0.05f * std::max(0.0f, d);
        return Curve({ { 1.0f, 0.22f }, { 1.3f, 0.1f }, { 1.6f, 0.04f }, { 2.0f, 0.0f } }, d);
    }

    int MeadowAnimals::LayerOf(float d) const
    {
        if (d < kLayerSplits[0]) return 0;
        if (d < kLayerSplits[1]) return 1;
        if (d < kLayerSplits[2]) return 2;
        return 3;
    }

    float MeadowAnimals::DepthOf(Mode mode) const
    {
        switch (mode)
        {
        case Mode::Busy: return 0.0f;
        case Mode::Attention: return 2.0f;
        default: return 1.3f;
        }
    }

    wuc::ContainerVisual MeadowAnimals::LayerVisual(int layer) const
    {
        switch (layer)
        {
        case 0: return m_stage.back;
        case 1: return m_stage.rest;
        case 2: return m_stage.mid;
        case 3: return m_stage.front;
        default: return nullptr;
        }
    }

    MeadowAnimals::Mode MeadowAnimals::Desired(AgentSession const& s) const
    {
        switch (s.status)
        {
        case AgentStatus::Working: return Mode::Busy;
        case AgentStatus::Waiting: return Mode::Attention;
        case AgentStatus::Done: return Clock::now() - s.since > std::chrono::minutes(10) ? Mode::Sleeping : Mode::Resting;
        default: return Mode::Sleeping;
        }
    }

    int MeadowAnimals::Escalation(Animal const& a) const
    {
        auto waited = Clock::now() - a.s.since;
        if (waited < std::chrono::minutes(2)) return 0;
        if (waited < std::chrono::minutes(8)) return 1;
        return 2;
    }

    float MeadowAnimals::SlotFor(Animal const& a, Mode mode) const
    {
        float lo = 0.12f, hi = 0.92f, spacing = 0.07f;
        if (mode == Mode::Attention) { lo = 0.36f; hi = 0.78f; spacing = 0.14f; }
        else if (mode == Mode::Resting || mode == Mode::Sleeping) { lo = 0.32f; hi = 0.80f; spacing = 0.06f; }
        auto sameZone = [&](Mode m) {
            bool rest = mode == Mode::Resting || mode == Mode::Sleeping;
            return rest ? (m == Mode::Resting || m == Mode::Sleeping) : m == mode;
        };
        float preferred = lo + (hi - lo) * ((Hash(a.s.id) >> 4) % 1000) / 999.0f;
        for (int k = 0; k < 40; ++k)
        {
            float step = static_cast<float>((k + 1) / 2) * spacing * 0.5f * (k % 2 ? 1.0f : -1.0f);
            float cand = preferred + step;
            if (cand < lo || cand > hi) continue;
            bool free = true;
            for (auto const& [id, o] : m_animals)
                if (o.get() != &a && !o->leaving && sameZone(o->mode) && std::abs(o->slotX / m_W - cand) < spacing)
                {
                    free = false;
                    break;
                }
            if (free) return cand * m_W;
        }
        return preferred * m_W;
    }

    float2 MeadowAnimals::PoseAt(Animal const& a) const
    {
        auto const& m = a.motion;
        if (m.samples.empty()) return float2(a.x, a.d);
        float f = 1.0f;
        if (m.duration > 0)
        {
            float elapsed = std::chrono::duration<float>(SteadyClock::now() - m.start).count();
            if (elapsed < 0) f = 0;
            else f = m.loop ? std::fmod(elapsed / m.duration, 1.0f) : std::min(1.0f, elapsed / m.duration);
        }
        float3 prev = m.samples.front();
        if (f <= prev.x) return float2(prev.y, prev.z);
        for (auto const& s : m.samples)
        {
            if (f <= s.x)
            {
                float t = (f - prev.x) / std::max(1e-5f, s.x - prev.x);
                return float2(Lerp(prev.y, s.y, t), Lerp(prev.z, s.z, t));
            }
            prev = s;
        }
        return float2(m.samples.back().y, m.samples.back().z);
    }

    // ------------------------------------------------------------------ animation helpers

    wuc::ExpressionAnimation MeadowAnimals::Expr(Animal const& a, wchar_t const* text) const
    {
        auto e = m_c.CreateExpressionAnimation(text);
        e.SetReferenceParameter(L"p", a.p);
        e.SetReferenceParameter(L"wind", m_wind);
        e.SetReferenceParameter(L"light", m_light);
        return e;
    }

    void MeadowAnimals::To(wuc::CompositionObject const& target, wchar_t const* property, float value, float seconds) const
    {
        auto k = m_c.CreateScalarKeyFrameAnimation();
        k.InsertKeyFrame(1.0f, value, m_c.CreateCubicBezierEasingFunction(float2(0.35f, 0.0f), float2(0.25f, 1.0f)));
        k.Duration(Seconds(std::max(0.02f, seconds)));
        target.StartAnimation(property, k);
    }

    void MeadowAnimals::To(wuc::CompositionObject const& target, wchar_t const* property, float3 value, float seconds) const
    {
        auto k = m_c.CreateVector3KeyFrameAnimation();
        k.InsertKeyFrame(1.0f, value, m_c.CreateCubicBezierEasingFunction(float2(0.35f, 0.0f), float2(0.25f, 1.0f)));
        k.Duration(Seconds(std::max(0.02f, seconds)));
        target.StartAnimation(property, k);
    }

    void MeadowAnimals::Turn(Animal& a, float face, float seconds) const
    {
        float from = a.face;
        a.face = face;
        if (from == face)
        {
            // No actual direction change - just make sure the visual scale matches.
            To(a.facing, L"Scale", float3(face, 1, 1), seconds);
            return;
        }
        // A plain Scale-X animation through zero reads as a flat mirror flip. Instead squash
        // thin and a touch taller at the turn point, then settle into the new facing - like a
        // sprite pivoting on its heel rather than flipping like a card.
        auto squash = m_c.CreateCubicBezierEasingFunction(float2(0.3f, 0.0f), float2(0.15f, 1.0f));
        auto settle = m_c.CreateCubicBezierEasingFunction(float2(0.3f, 0.0f), float2(0.25f, 1.0f));
        auto k = m_c.CreateVector3KeyFrameAnimation();
        k.InsertKeyFrame(0.0f, float3(from, 1.0f, 1.0f));
        k.InsertKeyFrame(0.45f, float3(0.0f, 1.2f, 1.0f), squash);
        k.InsertKeyFrame(1.0f, float3(face, 1.0f, 1.0f), settle);
        k.Duration(Seconds(seconds));
        a.facing.StartAnimation(L"Scale", k);
    }

    void MeadowAnimals::Keys(wuc::CompositionObject const& target, wchar_t const* property, std::initializer_list<std::pair<float, float>> keys, float seconds, bool loop) const
    {
        auto k = m_c.CreateScalarKeyFrameAnimation();
        auto linear = m_c.CreateLinearEasingFunction();
        for (auto const& [t, v] : keys) k.InsertKeyFrame(t, v, linear);
        k.Duration(Seconds(seconds));
        if (loop) k.IterationBehavior(wuc::AnimationIterationBehavior::Forever);
        target.StartAnimation(property, k);
    }

    uint32_t MeadowAnimals::Lit(uint32_t rgb) const
    {
        uint32_t c = art::MixRgb(art::ScaleRgb(rgb, m_lit.bright), m_lit.tint, m_lit.tintAmount);
        if (m_lit.warmth > 0.001f) c = art::MixRgb(c, art::WarmRgb(c), m_lit.warmth);
        return c;
    }

    // ------------------------------------------------------------------ timers / step chains

    void MeadowAnimals::After(float seconds, std::function<void()> fn)
    {
        if (!m_queue) return;
        auto timer = m_queue.CreateTimer();
        timer.Interval(Seconds(std::max(0.016f, seconds)));
        timer.IsRepeating(false);
        std::weak_ptr<MeadowAnimals> weak = weak_from_this();
        timer.Tick([weak, fn](winrt::Windows::System::DispatcherQueueTimer const& sender, auto&&) {
            auto keep = sender;
            keep.Stop();
            auto self = weak.lock();
            if (!self) return;
            auto f = fn;
            std::erase_if(self->m_timers, [&](auto const& t) { return t == keep; });
            f();
        });
        m_timers.push_back(timer);
        timer.Start();
    }

    void MeadowAnimals::Run(Animal& a, std::vector<Step> steps)
    {
        a.token = m_nextToken++;
        Next(a.s.id, a.token, std::make_shared<std::vector<Step>>(std::move(steps)), 0);
    }

    void MeadowAnimals::Next(std::wstring const& id, uint64_t token, std::shared_ptr<std::vector<Step>> steps, size_t index)
    {
        for (; index < steps->size(); ++index)
        {
            auto it = m_animals.find(id);
            if (it == m_animals.end() || it->second->token != token) return;
            float wait = (*steps)[index](*it->second);
            if (wait > 0)
            {
                After(wait, [this, id, token, steps, index] { Next(id, token, steps, index + 1); });
                return;
            }
        }
    }

    // ------------------------------------------------------------------ building an animal

    MeadowAnimals::Animal& MeadowAnimals::Create(AgentSession const& s)
    {
        auto owned = std::make_unique<Animal>();
        Animal& a = *owned;
        a.s = s;
        uint32_t h = Hash(s.id);
        a.species = static_cast<int>(h % art::SpeciesCount);
        a.variant = static_cast<int>((h >> 8) % 4);
        a.seed = ((h >> 16) % 1000) / 1000.0f * kTau;
        a.rig = &art::RigFor(a.species);
        a.isChild = !s.parentId.empty();
        auto const& rig = *a.rig;
        float U = m_U;

        a.p = m_c.CreatePropertySet();
        for (auto name : { L"Walk", L"Gait", L"Graze", L"Nibble", L"Dig", L"Carry", L"Wave", L"Excite", L"Lie", L"Sleep", L"Look", L"Blink", L"Haze" })
            a.p.InsertScalar(name, 0.0f);
        a.p.InsertScalar(L"Seed", a.seed);
        Keys(a.p, L"Gait", { { 0.0f, 0.0f }, { 1.0f, kTau } }, rig.gait, true);
        if (a.species == art::Squirrel)
        {
            a.p.InsertScalar(L"Gesture", 0.0f);
            Keys(a.p, L"Gesture", { { 0.0f, 0.0f }, { 1.0f, kTau } }, 3.2f, true);
        }
        Keys(a.p, L"Blink", { { 0.0f, 0.0f }, { 0.93f, 0.0f }, { 0.96f, 1.0f }, { 0.99f, 0.0f }, { 1.0f, 0.0f } }, 3.5f + 2.5f * ((h >> 3) % 100) / 100.0f, true);

        a.root = m_c.CreateContainerVisual();
        a.root.Opacity(0);

        a.shadow = m_c.CreateSpriteVisual();
        float sw = 2.5f * rig.rx * U, sh = std::max(0.12f * U, 0.42f * rig.rx * U);
        a.shadow.Size(float2(sw, sh));
        a.shadow.Offset(float3(-sw * 0.5f, -sh * 0.5f, 0));
        a.shadow.Brush(m_shadowBrush);
        a.root.Children().InsertAtTop(a.shadow);

        a.facing = m_c.CreateContainerVisual();
        a.root.Children().InsertAtTop(a.facing);
        a.bob = m_c.CreateContainerVisual();
        a.facing.Children().InsertAtTop(a.bob);
        a.jumper = m_c.CreateContainerVisual();
        a.bob.Children().InsertAtTop(a.jumper);
        a.pose = m_c.CreateContainerVisual();
        a.jumper.Children().InsertAtTop(a.pose);

        // Squirrels already hop for attention; the extra gait-driven excitement bob looks like a tremor.
        float exciteBob = a.species == art::Squirrel ? 0.0f : 0.05f * U;
        a.bob.StartAnimation(L"Offset", Expr(a, F(L"Vector3(0, -p.Walk*%.3f*Abs(Sin(p.Gait)) - p.Excite*%.3f*Abs(Sin(p.Gait*2)), 0)", rig.bob * U, exciteBob).c_str()));
        a.pose.StartAnimation(L"Offset", Expr(a, F(L"Vector3(0, p.Lie*%.3f, 0)", rig.legLen * 0.8f * U).c_str()));
        a.pose.StartAnimation(L"Scale", Expr(a, F(L"Vector3(1 + %.3f*p.Walk*Sin(p.Gait), 1 + 0.022*Sin(wind.Phase*50 + p.Seed), 1)", rig.stretch ? 0.07f : 0.0f).c_str()));
        if (rig.waddle)
        {
            a.pose.CenterPoint(float3(0, -rig.legLen * U, 0));
            a.pose.StartAnimation(L"RotationAngle", Expr(a, L"p.Walk*0.09*Sin(p.Gait)"));
        }

        auto shadowOpacity = Expr(a, F(L"(0.30 - 0.12*light.Night)*(1 - p.Haze)*Clamp(1 + (J.Offset.Y + B.Offset.Y)/%.3f, 0.25, 1)", 1.4f * U).c_str());
        shadowOpacity.SetReferenceParameter(L"J", a.jumper);
        shadowOpacity.SetReferenceParameter(L"B", a.bob);
        a.shadow.StartAnimation(L"Opacity", shadowOpacity);

        float texK = U * 1.15f;
        for (auto const& spec : art::PartsFor(a.species))
        {
            if (spec.inHead && !a.head)
            {
                a.head = m_c.CreateContainerVisual();
                a.head.Offset(float3(rig.neck.x * U, rig.neck.y * U, 0));
                a.head.StartAnimation(L"RotationAngle", Expr(a, F(L"p.Graze*%.3f + p.Dig*0.35 + p.Nibble*(0.25 + 0.12*Sin(p.Gait*3)) + p.Sleep*0.3 - p.Excite*0.12 + p.Look + p.Walk*0.04*Sin(p.Gait*2)", rig.graze).c_str()));
                a.pose.Children().InsertAtTop(a.head);
            }
            Animal::Part part;
            part.spec = spec;
            float2 extent = spec.max - spec.min;
            part.px = float2(std::max(2.0f, std::ceil(extent.x * texK)), std::max(2.0f, std::ceil(extent.y * texK)));
            auto sprite = m_c.CreateSpriteVisual();
            sprite.Size(extent * U);
            sprite.Offset(float3((spec.pivot.x + spec.min.x) * U, (spec.pivot.y + spec.min.y) * U, 0));
            sprite.CenterPoint(float3(-spec.min.x * U, -spec.min.y * U, 0));
            if (m_graphics)
            {
                part.surface = m_graphics->CreateSurface(part.px);
                auto brush = m_c.CreateSurfaceBrush(part.surface);
                brush.Stretch(wuc::CompositionStretch::Fill);
                sprite.Brush(brush);
                if (spec.haze)
                {
                    auto haze = m_c.CreateSpriteVisual();
                    haze.Size(extent * U);
                    auto mask = m_c.CreateMaskBrush();
                    mask.Source(m_hazeBrush);
                    mask.Mask(brush);
                    haze.Brush(mask);
                    haze.StartAnimation(L"Opacity", Expr(a, L"p.Haze"));
                    sprite.Children().InsertAtTop(haze);
                }
            }
            part.sprite = sprite;

            switch (spec.kind)
            {
            case art::PartKind::Leg:
            {
                int i = spec.index;
                float ph = 0;
                bool front = false, wave = false;
                if (rig.legs == 4)
                {
                    front = i == 1 || i == 3;
                    wave = i == 3 && !rig.wing;
                    ph = rig.hopper ? (front ? 0.6f * 3.14159f : 0.0f) : ((i == 1 || i == 2) ? 3.14159f : 0.0f);
                }
                else ph = i == 1 ? 3.14159f : 0.0f;
                std::wstring text = F(L"p.Walk*%.3f*Sin(p.Gait + %.3f)", rig.legAmp, ph);
                if (a.species == art::Squirrel && front)
                    text = F(L"p.Walk*%.3f*(1 - p.Carry)*Sin(p.Gait + %.3f) - p.Carry*0.35", rig.legAmp, ph);
                if (front) text += F(L" + p.Dig*0.5*Sin(p.Gait*3 + %.3f)", ph);
                if (wave) text += a.species == art::Squirrel
                    ? L" - p.Wave*(1.5 + 0.12*Sin(p.Gesture))"
                    : L" - p.Wave*(1.5 + 0.3*Sin(p.Gait*4))";
                sprite.StartAnimation(L"RotationAngle", Expr(a, text.c_str()));
                sprite.StartAnimation(L"Scale", Expr(a, L"Vector3(1, 1 - 0.72*p.Lie, 1)"));
                break;
            }
            case art::PartKind::Ear:
                sprite.StartAnimation(L"RotationAngle", Expr(a, F(L"%.3f*(1 - 0.5*p.Excite) + p.Walk*0.12*Sin(p.Gait + 1) + %.3f*wind.Strength*(0.6*Sin(wind.Phase*61 + p.Seed + %d) + 0.4*Sin(wind.Phase*137 + 2*p.Seed)) + %.3f*Max(p.Sleep, p.Lie*0.5)",
                    rig.earBase + (spec.index ? 0.08f : 0.0f), rig.earFlutter, spec.index * 2, rig.earDroop).c_str()));
                break;
            case art::PartKind::Tail:
                sprite.StartAnimation(L"RotationAngle", Expr(a, F(a.species == art::Squirrel
                    ? L"%.3f + (0.04 + p.Walk*0.04 + p.Excite*0.04)*Sin(p.Gesture + p.Seed) - p.Lie*0.25"
                    : L"%.3f + p.Walk*0.25*Sin(p.Gait*2) + (1 - p.Walk)*0.12*Sin(wind.Phase*40 + p.Seed) + p.Excite*0.3*Sin(p.Gait*4) - p.Lie*0.25", rig.tailBase).c_str()));
                break;
            case art::PartKind::Wing:
                sprite.StartAnimation(L"RotationAngle", Expr(a, L"-p.Wave*(0.7 + 0.35*Sin(p.Gait*2)) - p.Excite*0.15*Abs(Sin(p.Gait*3))"));
                break;
            case art::PartKind::Lid:
                sprite.StartAnimation(L"Opacity", Expr(a, L"Max(p.Sleep, p.Blink)"));
                break;
            case art::PartKind::Glint:
                sprite.StartAnimation(L"Opacity", Expr(a, L"light.Night*0.9*(1 - Max(p.Sleep, p.Blink))"));
                break;
            case art::PartKind::Carry:
                sprite.StartAnimation(L"Opacity", Expr(a, L"p.Carry"));
                break;
            default: break;
            }
            (spec.inHead ? a.head : a.pose).Children().InsertAtTop(sprite);
            a.parts.push_back(std::move(part));
        }

        if (m_zzBrush)
        {
            a.zz = m_c.CreateSpriteVisual();
            float zw = 0.62f * U, zh = zw * m_zzSize.y / std::max(1.0f, m_zzSize.x);
            a.zz.Size(float2(zw, zh));
            a.zz.Brush(m_zzBrush);
            a.zz.Opacity(0);
            float bx = rig.neck.x * U * 0.5f, by = -rig.height * U * 0.55f - zh;
            a.zz.StartAnimation(L"Offset", Expr(a, F(L"Vector3(%.2f + %.2f*Sin(wind.Phase*25 + p.Seed), %.2f - %.2f*Abs(Sin(wind.Phase*30 + p.Seed)), 0)", bx, 0.06f * U, by, 0.12f * U).c_str()));
            a.root.Children().InsertAtTop(a.zz);
        }

        if (m_graphics && m_stage.top)
        {
            a.label = m_c.CreateSpriteVisual();
            a.labelSurface = m_graphics->CreateSurface(float2(4, 4));
            auto brush = m_c.CreateSurfaceBrush(a.labelSurface);
            brush.Stretch(wuc::CompositionStretch::Fill);
            a.label.Brush(brush);
            a.label.Opacity(0);
            m_stage.top.Children().InsertAtTop(a.label);
        }

        DrawParts(a);
        auto [it, _] = m_animals.emplace(s.id, std::move(owned));
        return *it->second;
    }

    void MeadowAnimals::DrawParts(Animal& a)
    {
        if (!m_graphics) return;
        float texK = m_U * 1.15f;
        auto lit = [this](uint32_t c) { return Lit(c); };
        for (auto& part : a.parts)
        {
            if (!part.surface) continue;
            m_graphics->Draw(part.surface, part.px, [&](ID2D1DeviceContext* dc) {
                D2D1_MATRIX_3X2_F t;
                dc->GetTransform(&t);
                dc->SetTransform(D2D1::Matrix3x2F::Translation(-part.spec.min.x, -part.spec.min.y) * D2D1::Matrix3x2F::Scale(texK, texK) * t);
                art::DrawPart(dc, a.species, a.variant, part.spec, lit);
                dc->SetTransform(t);
            });
        }
    }

    // ------------------------------------------------------------------ placement

    void MeadowAnimals::Reparent(Animal& a, int layer)
    {
        if (a.layer == layer) return;
        if (a.layer >= 0)
            if (auto old = LayerVisual(a.layer)) old.Children().Remove(a.root);
        a.layer = layer;
        if (auto now = LayerVisual(layer)) now.Children().InsertAtTop(a.root);
        FollowLayer(a);
    }

    void MeadowAnimals::Place(Animal& a, float x, float d)
    {
        a.root.StopAnimation(L"Offset");
        a.root.StopAnimation(L"Scale");
        a.p.StopAnimation(L"Haze");
        float s = ScaleAt(d) * (a.isChild ? kChildScale : 1.0f);
        a.root.Offset(float3(x, Y(d, x), 0));
        a.root.Scale(float3(s, s, 1));
        a.p.InsertScalar(L"Haze", HazeAt(d));
        Reparent(a, LayerOf(d));
        a.x = x;
        a.d = d;
        a.motion = {};
        a.motion.samples = { float3(0, x, d) };
    }

    void MeadowAnimals::Freeze(Animal& a)
    {
        float2 at = PoseAt(a);
        Place(a, at.x, at.y);
        a.root.StopAnimation(L"Opacity");
        a.root.Opacity(a.leaving ? 0.0f : 1.0f);
        a.facing.StopAnimation(L"Scale");
        a.facing.Scale(float3(a.face, 1, 1));
    }

    void MeadowAnimals::FollowLayer(Animal& a)
    {
        if (!a.label || a.layer < 0) return;
        auto layer = LayerVisual(a.layer);
        if (!layer) return;
        float gap = Px(6);
        auto e = m_c.CreateExpressionAnimation(L"L.Offset + R.Offset + Vector3(-w*0.5, (J.Offset.Y - top)*R.Scale.Y - h - gap, 0)");
        e.SetReferenceParameter(L"L", layer);
        e.SetReferenceParameter(L"R", a.root);
        e.SetReferenceParameter(L"J", a.jumper);
        e.SetScalarParameter(L"w", a.labelSize.x);
        e.SetScalarParameter(L"h", a.labelSize.y);
        e.SetScalarParameter(L"top", a.rig->height * m_U);
        e.SetScalarParameter(L"gap", gap);
        a.label.StartAnimation(L"Offset", e);
    }

    // ------------------------------------------------------------------ labels

    void MeadowAnimals::DrawLabel(Animal& a)
    {
        if (!a.label || !m_graphics) return;
        bool bubble = a.mode == Mode::Attention && !a.journeying;
        auto now = Clock::now();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - a.s.since);
        std::wstring title = Truncate(a.s.title.empty() ? a.s.repo : a.s.title, 38);
        std::wstring second;
        if (bubble)
        {
            std::wstring ask = a.s.detail.empty() ? std::wstring(L"Needs your input") : Truncate(a.s.detail, 46);
            second = ask + L" \u00B7 " + FormatAge(age);
        }
        else
            second = std::wstring(StatusWord(a.s.status)) + L" \u00B7 " + FormatAge(age) + L"  \u00B7  " + art::SpeciesName(a.species);
        std::wstring text = title + L"\n" + second;
        if (text == a.labelText && bubble == a.labelIsBubble) return;
        a.labelText = text;
        a.labelIsBubble = bubble;

        auto* titleFont = m_graphics->Font(FontRole::UiSemibold, bubble ? 13.0f : 12.0f);
        auto* bodyFont = m_graphics->Font(FontRole::Ui, bubble ? 12.0f : 11.0f);
        float maxW = Px(bubble ? 300.0f : 260.0f);
        auto t1 = m_graphics->Layout(title, titleFont, maxW, Px(40));
        auto t2 = m_graphics->Layout(second, bodyFont, maxW, Px(40));
        float2 m1 = m_graphics->Measure(t1.get()), m2 = m_graphics->Measure(t2.get());
        float padX = Px(bubble ? 12.0f : 10.0f), padY = Px(bubble ? 9.0f : 6.0f);
        float badge = bubble ? Px(22) : 0.0f, badgeGap = bubble ? Px(10) : 0.0f;
        float textW = std::ceil(std::max(m1.x, m2.x));
        float boxW = padX * 2 + badge + badgeGap + textW;
        float boxH = std::ceil(padY * 2 + m1.y + m2.y + Px(1));
        float tail = bubble ? Px(9) : 0.0f;
        float2 size(std::ceil(boxW + Px(4)), std::ceil(boxH + tail + Px(4)));
        a.labelSize = size;
        a.label.Size(size);

        m_graphics->Draw(a.labelSurface, size, [&](ID2D1DeviceContext* dc) {
            auto solid = [&](uint32_t rgb, float alpha) {
                winrt::com_ptr<ID2D1SolidColorBrush> b;
                dc->CreateSolidColorBrush(D2DColor(rgb, alpha), b.put());
                return b;
            };
            float ox = Px(2), oy = Px(1);
            float radius = bubble ? Px(10) : boxH * 0.5f;
            auto rect = D2D1::RectF(ox, oy, ox + boxW, oy + boxH);
            auto shadowRect = D2D1::RectF(ox, oy + Px(1.5f), ox + boxW, oy + boxH + Px(1.5f));
            dc->FillRoundedRectangle(D2D1::RoundedRect(shadowRect, radius, radius), solid(0x000000, bubble ? 0.16f : 0.12f).get());
            auto bg = bubble ? solid(0xFFFFFF, 0.94f) : solid(0x1E1C1A, 0.72f);
            dc->FillRoundedRectangle(D2D1::RoundedRect(rect, radius, radius), bg.get());
            if (bubble)
            {
                winrt::com_ptr<ID2D1Factory> factory;
                dc->GetFactory(factory.put());
                winrt::com_ptr<ID2D1PathGeometry> geo;
                factory->CreatePathGeometry(geo.put());
                winrt::com_ptr<ID2D1GeometrySink> sink;
                geo->Open(sink.put());
                float cx = ox + boxW * 0.5f;
                sink->BeginFigure(D2D1::Point2F(cx - Px(8), oy + boxH - Px(1)), D2D1_FIGURE_BEGIN_FILLED);
                sink->AddLine(D2D1::Point2F(cx, oy + boxH + tail));
                sink->AddLine(D2D1::Point2F(cx + Px(8), oy + boxH - Px(1)));
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                sink->Close();
                dc->FillGeometry(geo.get(), bg.get());
                float bcx = ox + padX + badge * 0.5f, bcy = oy + boxH * 0.5f;
                dc->FillEllipse(D2D1::Ellipse(D2D1::Point2F(bcx, bcy), badge * 0.5f, badge * 0.5f), solid(0xF5A524, 1.0f).get());
                auto bang = m_graphics->Layout(L"!", m_graphics->Font(FontRole::UiSemibold, 14, DWRITE_TEXT_ALIGNMENT_CENTER), badge, badge, false);
                float2 bm = m_graphics->Measure(bang.get());
                dc->DrawTextLayout(D2D1::Point2F(bcx - badge * 0.5f, bcy - bm.y * 0.5f), bang.get(), solid(0xFFFFFF, 1.0f).get());
            }
            float tx = ox + padX + badge + badgeGap, ty = oy + padY;
            dc->DrawTextLayout(D2D1::Point2F(tx, ty), t1.get(), solid(bubble ? 0x1B1A19 : 0xFFFFFF, bubble ? 1.0f : 0.95f).get());
            dc->DrawTextLayout(D2D1::Point2F(tx, ty + m1.y + Px(1)), t2.get(), solid(bubble ? 0x6A5A3A : 0xE8E2D8, bubble ? 1.0f : 0.8f).get());
        });
        FollowLayer(a);
    }

    void MeadowAnimals::ShowLabel(Animal& a)
    {
        if (!a.label) return;
        bool bubble = a.mode == Mode::Attention && !a.journeying;
        a.bubble = bubble;
        DrawLabel(a);
        bool show = !a.leaving && (bubble || a.hovered);
        To(a.label, L"Opacity", show ? 1.0f : 0.0f, show ? 0.35f : 0.25f);
    }

    void MeadowAnimals::ShowZz(Animal& a, bool show)
    {
        if (!a.zz) return;
        if (show)
        {
            auto k = m_c.CreateScalarKeyFrameAnimation();
            auto linear = m_c.CreateLinearEasingFunction();
            k.InsertKeyFrame(0.0f, 0.0f, linear);
            k.InsertKeyFrame(0.3f, 0.85f, linear);
            k.InsertKeyFrame(0.7f, 0.85f, linear);
            k.InsertKeyFrame(1.0f, 0.0f, linear);
            k.Duration(Seconds(4.5f));
            k.DelayTime(Seconds(0.8f));
            k.IterationBehavior(wuc::AnimationIterationBehavior::Forever);
            a.zz.StartAnimation(L"Opacity", k);
        }
        else
            To(a.zz, L"Opacity", 0.0f, 0.3f);
    }

    // ------------------------------------------------------------------ behaviours

    MeadowAnimals::Step MeadowAnimals::Prepare(bool lying)
    {
        return [this, lying](Animal& a) {
            for (auto name : { L"Walk", L"Graze", L"Nibble", L"Dig", L"Carry", L"Wave", L"Excite", L"Lie", L"Sleep", L"Look" })
                To(a.p, name, 0.0f, 0.4f);
            To(a.jumper, L"Offset", float3(0, 0, 0), 0.35f);
            To(a.facing, L"Scale", float3(a.face, 1, 1), 0.3f);
            ShowZz(a, false);
            ShowLabel(a);
            return lying ? 0.6f : 0.45f;
        };
    }

    MeadowAnimals::Step MeadowAnimals::Celebrate()
    {
        return [this](Animal& a) {
            Happy(a);
            return 1.5f;
        };
    }

    void MeadowAnimals::Happy(Animal& a)
    {
        float U = m_U;
        auto jump = m_c.CreateVector3KeyFrameAnimation();
        auto up = m_c.CreateCubicBezierEasingFunction(float2(0.2f, 0.7f), float2(0.4f, 1.0f));
        auto down = m_c.CreateCubicBezierEasingFunction(float2(0.6f, 0.0f), float2(0.8f, 0.4f));
        jump.InsertKeyFrame(0.3f, float3(0, -0.9f * U, 0), up);
        jump.InsertKeyFrame(0.6f, float3(0, 0, 0), down);
        jump.InsertKeyFrame(0.72f, float3(0, -0.2f * U, 0), up);
        jump.InsertKeyFrame(0.85f, float3(0, 0, 0), down);
        jump.Duration(Seconds(1.4f));
        a.jumper.StartAnimation(L"Offset", jump);

        auto spin = m_c.CreateVector3KeyFrameAnimation();
        auto linear = m_c.CreateLinearEasingFunction();
        float f = a.face;
        spin.InsertKeyFrame(0.12f, float3(f, 1, 1), linear);
        spin.InsertKeyFrame(0.3f, float3(-f, 1, 1), linear);
        spin.InsertKeyFrame(0.48f, float3(f, 1, 1), linear);
        spin.Duration(Seconds(1.4f));
        a.facing.StartAnimation(L"Scale", spin);

        Keys(a.p, L"Excite", { { 0.0f, 0.0f }, { 0.1f, 1.0f }, { 0.8f, 1.0f }, { 1.0f, 0.0f } }, 1.4f, false);
        Keys(a.p, L"Wave", { { 0.0f, 0.0f }, { 0.2f, 1.0f }, { 0.7f, 1.0f }, { 1.0f, 0.0f } }, 1.4f, false);
        Sparkles(a, 12, false);
        Sparkles(a, 8, true);
    }

    void MeadowAnimals::HopIn(Animal& a)
    {
        float U = m_U;
        auto up = m_c.CreateCubicBezierEasingFunction(float2(0.2f, 0.7f), float2(0.4f, 1.0f));
        auto down = m_c.CreateCubicBezierEasingFunction(float2(0.6f, 0.0f), float2(0.8f, 0.4f));
        auto hop = m_c.CreateVector3KeyFrameAnimation();
        hop.InsertKeyFrame(0.35f, float3(0, -0.6f * U, 0), up);
        hop.InsertKeyFrame(0.8f, float3(0, 0, 0), down);
        hop.Duration(Seconds(0.9f));
        a.jumper.StartAnimation(L"Offset", hop);
        Keys(a.p, L"Wave", { { 0.0f, 0.0f }, { 0.25f, 1.0f }, { 0.6f, 1.0f }, { 1.0f, 0.0f } }, 0.9f, false);
    }

    void MeadowAnimals::HopOut(Animal& a)
    {
        float U = m_U;
        auto up = m_c.CreateCubicBezierEasingFunction(float2(0.2f, 0.7f), float2(0.4f, 1.0f));
        auto down = m_c.CreateCubicBezierEasingFunction(float2(0.6f, 0.0f), float2(0.8f, 0.4f));
        auto hop = m_c.CreateVector3KeyFrameAnimation();
        hop.InsertKeyFrame(0.3f, float3(0, -0.45f * U, 0), up);
        hop.InsertKeyFrame(0.65f, float3(0, 0, 0), down);
        hop.Duration(Seconds(0.8f));
        a.jumper.StartAnimation(L"Offset", hop);
        Keys(a.p, L"Wave", { { 0.0f, 0.0f }, { 0.2f, 1.0f }, { 0.75f, 1.0f }, { 1.0f, 0.0f } }, 0.8f, false);
    }

    void MeadowAnimals::Sparkles(Animal& a, int count, bool petals)
    {
        if (!m_stage.top) return;
        float2 at = PoseAt(a);
        float s = ScaleAt(at.y) * (a.isChild ? kChildScale : 1.0f);
        float3 c(at.x, Y(at.y, at.x) - a.rig->height * m_U * s * 0.8f - 0.45f * m_U * s, 0);
        auto batch = m_c.CreateScopedBatch(wuc::CompositionBatchTypes::Animation);
        std::vector<wuc::Visual> made;
        for (int i = 0; i < count; ++i)
        {
            auto v = m_c.CreateSpriteVisual();
            float size = (petals ? m_rng.Next(0.07f, 0.11f) : m_rng.Next(0.05f, 0.09f)) * m_U * std::max(0.45f, s);
            v.Size(float2(size, petals ? size * 0.62f : size));
            v.Brush(petals ? m_petals[m_rng.Index(m_petals.size())] : m_sparkle);
            float ang = kTau * (i + m_rng.Next(0.0f, 0.8f)) / count;
            float r = m_U * std::max(0.5f, s) * (petals ? m_rng.Next(0.5f, 0.9f) : m_rng.Next(0.35f, 0.7f));
            float3 start = c - float3(size * 0.5f, size * 0.5f, 0);
            v.Offset(start);
            v.Opacity(0);
            v.CenterPoint(float3(size * 0.5f, size * 0.3f, 0));
            float secs = petals ? m_rng.Next(2.2f, 3.0f) : m_rng.Next(1.0f, 1.5f);
            auto path = m_c.CreateVector3KeyFrameAnimation();
            float3 burst = start + float3(std::cos(ang) * r, std::sin(ang) * r * 0.7f - r * 0.3f, 0);
            path.InsertKeyFrame(0.35f, burst, m_c.CreateCubicBezierEasingFunction(float2(0.1f, 0.8f), float2(0.3f, 1.0f)));
            if (petals) path.InsertKeyFrame(1.0f, burst + float3(m_rng.Next(-0.3f, 0.5f) * m_U * s, m_U * s * m_rng.Next(0.5f, 0.9f), 0));
            else path.InsertKeyFrame(1.0f, burst + float3(0, m_U * s * 0.15f, 0));
            path.Duration(Seconds(secs));
            path.DelayTime(Seconds(0.3f));
            v.StartAnimation(L"Offset", path);
            auto fade = m_c.CreateScalarKeyFrameAnimation();
            fade.InsertKeyFrame(0.1f, 1.0f);
            fade.InsertKeyFrame(0.6f, petals ? 0.9f : 0.8f);
            fade.InsertKeyFrame(1.0f, 0.0f);
            fade.Duration(Seconds(secs));
            fade.DelayTime(Seconds(0.3f));
            v.StartAnimation(L"Opacity", fade);
            if (petals)
            {
                auto turn = m_c.CreateScalarKeyFrameAnimation();
                turn.InsertKeyFrame(1.0f, m_rng.Next(-4.0f, 4.0f));
                turn.Duration(Seconds(secs));
                turn.DelayTime(Seconds(0.3f));
                v.StartAnimation(L"RotationAngle", turn);
            }
            m_stage.top.Children().InsertAtTop(v);
            made.push_back(v);
        }
        batch.End();
        auto top = m_stage.top;
        batch.Completed([top, made](auto&&, auto&&) { for (auto const& v : made) top.Children().Remove(v); });
    }

    void MeadowAnimals::Journey(Animal& a, float x0, float d0, float x1, float d1, float speedK, std::vector<Step>& steps)
    {
        auto const& rig = *a.rig;
        // Cut the path where it crosses a layer boundary so the root can be reparented between grass rows.
        std::vector<float> cuts{ 0.0f };
        for (float split : kLayerSplits)
            if ((d0 - split) * (d1 - split) < 0) cuts.push_back((split - d0) / (d1 - d0));
        cuts.push_back(1.0f);
        std::sort(cuts.begin(), cuts.end());

        struct Segment { std::vector<float3> pts; float duration; int layer; float op0, op1; };
        std::vector<Segment> segs;
        float total = 0;
        for (size_t i = 0; i + 1 < cuts.size(); ++i)
        {
            float u0 = cuts[i], u1 = cuts[i + 1];
            if (u1 - u0 < 1e-4f) continue;
            Segment seg;
            constexpr int n = 14;
            float len = 0, scale = 0;
            float3 prev{};
            for (int k = 0; k <= n; ++k)
            {
                float u = Lerp(u0, u1, k / static_cast<float>(n));
                float x = Lerp(x0, x1, u), d = Lerp(d0, d1, u);
                float3 pos(x, Y(d, x), d);
                if (k > 0) len += std::hypot(pos.x - prev.x, pos.y - prev.y);
                scale += ScaleAt(d);
                prev = pos;
                seg.pts.push_back(pos);
            }
            scale /= (n + 1);
            float dm = Lerp(d0, d1, (u0 + u1) * 0.5f);
            seg.layer = LayerOf(dm);
            float speed = std::max(0.2f, rig.speed) * speedK * m_U * scale;
            seg.duration = std::max({ len / speed, std::abs(d1 - d0) * (u1 - u0) * 1.5f, 0.4f });
            auto atBoundary = [](float d) { return std::abs(d - kLayerSplits[0]) < 1e-3f; };
            seg.op0 = (i > 0 && atBoundary(seg.pts.front().z)) ? 0.0f : 1.0f;
            seg.op1 = (i + 2 < cuts.size() && atBoundary(seg.pts.back().z)) ? 0.0f : 1.0f;
            total += seg.duration;
            segs.push_back(std::move(seg));
        }
        if (segs.empty()) return;
        float k = total > 9.0f ? 9.0f / total : 1.0f;
        float face = x1 - x0 > 1.0f ? 1.0f : (x1 - x0 < -1.0f ? -1.0f : a.face);

        steps.push_back([this, face](Animal& an) {
            an.journeying = true;
            Turn(an, face, 0.25f);
            To(an.p, L"Walk", 1.0f, 0.3f);
            ShowLabel(an);
            return 0.0f;
        });
        for (auto& seg : segs)
        {
            seg.duration *= k;
            steps.push_back([this, seg](Animal& an) {
                Reparent(an, seg.layer);
                auto linear = m_c.CreateLinearEasingFunction();
                auto off = m_c.CreateVector3KeyFrameAnimation();
                auto sc = m_c.CreateVector3KeyFrameAnimation();
                auto hz = m_c.CreateScalarKeyFrameAnimation();
                size_t n = seg.pts.size() - 1;
                an.motion = {};
                an.motion.start = SteadyClock::now();
                an.motion.duration = seg.duration;
                for (size_t i = 1; i <= n; ++i)
                {
                    float t = i / static_cast<float>(n);
                    auto const& q = seg.pts[i];
                    float s = ScaleAt(q.z);
                    off.InsertKeyFrame(t, float3(q.x, q.y, 0), linear);
                    sc.InsertKeyFrame(t, float3(s, s, 1), linear);
                    hz.InsertKeyFrame(t, HazeAt(q.z), linear);
                }
                for (size_t i = 0; i <= n; ++i) an.motion.samples.push_back(float3(i / static_cast<float>(n), seg.pts[i].x, seg.pts[i].z));
                off.Duration(Seconds(seg.duration));
                sc.Duration(Seconds(seg.duration));
                hz.Duration(Seconds(seg.duration));
                an.root.StartAnimation(L"Offset", off);
                an.root.StartAnimation(L"Scale", sc);
                an.p.StartAnimation(L"Haze", hz);
                an.root.StopAnimation(L"Opacity");
                an.root.Opacity(seg.op0);
                if (seg.op0 < 1 || seg.op1 < 1)
                {
                    auto op = m_c.CreateScalarKeyFrameAnimation();
                    op.InsertKeyFrame(0.0f, seg.op0, linear);
                    op.InsertKeyFrame(0.3f, 1.0f, linear);
                    op.InsertKeyFrame(0.7f, 1.0f, linear);
                    op.InsertKeyFrame(1.0f, seg.op1, linear);
                    op.Duration(Seconds(seg.duration));
                    an.root.StartAnimation(L"Opacity", op);
                }
                an.x = seg.pts.back().x;
                an.d = seg.pts.back().z;
                return seg.duration;
            });
        }
        steps.push_back([this](Animal& an) {
            Place(an, an.x, an.d);
            an.root.StopAnimation(L"Opacity");
            an.root.Opacity(an.leaving ? 0.0f : 1.0f);
            To(an.p, L"Walk", 0.0f, 0.3f);
            an.journeying = false;
            return 0.3f;
        });
    }

    void MeadowAnimals::StartLoop(Animal& a)
    {
        a.level = -1;
        switch (a.mode)
        {
        case Mode::Busy: BusyLoop(a); break;
        case Mode::Attention: AttentionLoop(a); break;
        case Mode::Resting: RestLoop(a, false); break;
        case Mode::Sleeping: RestLoop(a, true); break;
        }
        ShowLabel(a);
    }

    void MeadowAnimals::BusyLoop(Animal& a)
    {
        auto const& rig = *a.rig;
        float x0 = a.x, d = a.d;
        float s = ScaleAt(d);
        float U = m_U;
        float P = 16.0f + 6.0f * m_rng.Next(0.0f, 1.0f);
        float delay = m_rng.Next(0.0f, 4.0f);
        float dir = (x0 < m_W * 0.5f) ? 1.0f : -1.0f;
        if (m_rng.Next(0.0f, 1.0f) < 0.3f) dir = -dir;
        bool snail = a.species == art::Snail;
        float dx = dir * m_rng.Next(2.2f, 3.6f) * U * s * (snail ? 0.35f : 1.0f);
        float x1 = std::clamp(x0 + dx, m_W * 0.04f, m_W * 0.96f);
        a.face = dir;

        auto linear = m_c.CreateLinearEasingFunction();
        auto at = [&](float x) { return float3(x, Y(d, x), 0); };
        // Keep the whole period in one looping animation per channel: the compositor runs it with no CPU.
        std::vector<float2> path = { { 0.0f, x0 }, { 0.22f, x0 } };
        for (int i = 1; i <= 4; ++i) path.push_back({ 0.22f + 0.04f * i, Lerp(x0, x1, i / 4.0f) });
        path.push_back({ 0.62f, x1 });
        for (int i = 1; i <= 4; ++i) path.push_back({ 0.62f + 0.04f * i, Lerp(x1, x0, i / 4.0f) });
        path.push_back({ 1.0f, x0 });

        auto off = m_c.CreateVector3KeyFrameAnimation();
        for (auto const& k : path) off.InsertKeyFrame(k.x, at(k.y), linear);
        off.Duration(Seconds(P));
        off.DelayTime(Seconds(delay));
        off.IterationBehavior(wuc::AnimationIterationBehavior::Forever);
        a.root.StartAnimation(L"Offset", off);

        auto squash = m_c.CreateCubicBezierEasingFunction(float2(0.3f, 0.0f), float2(0.15f, 1.0f));
        auto settle = m_c.CreateCubicBezierEasingFunction(float2(0.3f, 0.0f), float2(0.25f, 1.0f));
        auto face = m_c.CreateVector3KeyFrameAnimation();
        // Each turn-around squashes thin/tall at its midpoint instead of mirroring flat, matching Turn().
        face.InsertKeyFrame(0.0f, float3(dir, 1, 1), linear);
        face.InsertKeyFrame(0.59f, float3(dir, 1, 1), linear);
        face.InsertKeyFrame(0.6075f, float3(0.0f, 1.2f, 1), squash);
        face.InsertKeyFrame(0.625f, float3(-dir, 1, 1), settle);
        face.InsertKeyFrame(0.94f, float3(-dir, 1, 1), linear);
        face.InsertKeyFrame(0.9575f, float3(0.0f, 1.2f, 1), squash);
        face.InsertKeyFrame(0.975f, float3(dir, 1, 1), settle);
        face.InsertKeyFrame(1.0f, float3(dir, 1, 1), linear);
        face.Duration(Seconds(P));
        face.DelayTime(Seconds(delay));
        face.IterationBehavior(wuc::AnimationIterationBehavior::Forever);
        a.facing.StartAnimation(L"Scale", face);

        auto loop = [&](wchar_t const* name, std::initializer_list<std::pair<float, float>> keys) {
            auto k = m_c.CreateScalarKeyFrameAnimation();
            for (auto const& [t, v] : keys) k.InsertKeyFrame(t, v, linear);
            k.Duration(Seconds(P));
            k.DelayTime(Seconds(delay));
            k.IterationBehavior(wuc::AnimationIterationBehavior::Forever);
            a.p.StartAnimation(name, k);
        };
        loop(L"Walk", { { 0.0f, 0.0f }, { 0.21f, 0.0f }, { 0.23f, 1.0f }, { 0.37f, 1.0f }, { 0.39f, 0.0f }, { 0.61f, 0.0f }, { 0.63f, 1.0f }, { 0.77f, 1.0f }, { 0.79f, 0.0f }, { 1.0f, 0.0f } });
        loop(L"Graze", { { 0.0f, 0.0f }, { 0.03f, 1.0f }, { 0.18f, 1.0f }, { 0.21f, 0.0f }, { 0.92f, 0.0f }, { 0.95f, 0.6f }, { 1.0f, 0.0f } });
        loop(L"Nibble", { { 0.0f, 0.0f }, { 0.79f, 0.0f }, { 0.81f, 1.0f }, { 0.88f, 1.0f }, { 0.9f, 0.0f }, { 1.0f, 0.0f } });
        bool carries = std::any_of(a.parts.begin(), a.parts.end(), [](auto const& p) { return p.spec.kind == art::PartKind::Carry; });
        if (a.species == art::Squirrel)
            loop(L"Carry", { { 0.0f, 0.0f }, { 0.2f, 0.0f }, { 0.215f, 1.0f }, { 0.42f, 1.0f }, { 0.435f, 0.0f },
                { 0.79f, 0.0f }, { 0.81f, 1.0f }, { 0.88f, 1.0f }, { 0.9f, 0.0f }, { 1.0f, 0.0f } });
        else if (carries && (Hash(a.s.id) & 1))
            loop(L"Carry", { { 0.0f, 0.0f }, { 0.2f, 0.0f }, { 0.215f, 1.0f }, { 0.42f, 1.0f }, { 0.435f, 0.0f }, { 1.0f, 0.0f } });
        bool hopWork = a.species == art::Bunny || a.species == art::Deer || a.species == art::Sheep;
        if (!snail && !hopWork)
            loop(L"Dig", { { 0.0f, 0.0f }, { 0.39f, 0.0f }, { 0.41f, 1.0f }, { 0.59f, 1.0f }, { 0.61f, 0.0f }, { 1.0f, 0.0f } });
        if (hopWork)
        {
            auto hop = m_c.CreateVector3KeyFrameAnimation();
            float h = -0.28f * U;
            hop.InsertKeyFrame(0.0f, float3(0, 0, 0), linear);
            hop.InsertKeyFrame(0.40f, float3(0, 0, 0), linear);
            auto up = m_c.CreateCubicBezierEasingFunction(float2(0.2f, 0.7f), float2(0.4f, 1.0f));
            auto down = m_c.CreateCubicBezierEasingFunction(float2(0.6f, 0.0f), float2(0.8f, 0.4f));
            for (int i = 0; i < 3; ++i)
            {
                float t0 = 0.42f + 0.055f * i;
                hop.InsertKeyFrame(t0 + 0.0275f, float3(0, h, 0), up);
                hop.InsertKeyFrame(t0 + 0.055f, float3(0, 0, 0), down);
            }
            hop.InsertKeyFrame(1.0f, float3(0, 0, 0), linear);
            hop.Duration(Seconds(P));
            hop.DelayTime(Seconds(delay));
            hop.IterationBehavior(wuc::AnimationIterationBehavior::Forever);
            a.jumper.StartAnimation(L"Offset", hop);
        }
        (void)rig;

        a.motion = {};
        a.motion.start = SteadyClock::now() + std::chrono::milliseconds(static_cast<int>(delay * 1000));
        a.motion.duration = P;
        a.motion.loop = true;
        for (auto const& k : path) a.motion.samples.push_back(float3(k.x, k.y, d));
        a.slotX = x0;
    }

    void MeadowAnimals::AttentionLoop(Animal& a)
    {
        int level = Escalation(a);
        a.level = level;
        float U = m_U;
        Turn(a, a.x < m_W * 0.5f ? 1.0f : -1.0f, 0.3f);
        auto linear = m_c.CreateLinearEasingFunction();
        auto up = m_c.CreateCubicBezierEasingFunction(float2(0.2f, 0.7f), float2(0.4f, 1.0f));
        auto down = m_c.CreateCubicBezierEasingFunction(float2(0.6f, 0.0f), float2(0.8f, 0.4f));
        auto ease = m_c.CreateCubicBezierEasingFunction(float2(0.4f, 0.0f), float2(0.2f, 1.0f));
        float P = level == 0 ? 5.0f : (level == 1 ? 6.0f : 8.0f);

        auto hop = m_c.CreateVector3KeyFrameAnimation();
        hop.InsertKeyFrame(0.0f, float3(0, 0, 0), linear);
        if (level == 0)
        {
            hop.InsertKeyFrame(0.10f, float3(0, 0, 0), linear);
            hop.InsertKeyFrame(0.17f, float3(0, -0.12f * U, 0), up);
            hop.InsertKeyFrame(0.25f, float3(0, 0, 0), down);
        }
        else if (level == 1)
        {
            for (int i = 0; i < 3; ++i)
            {
                float t0 = 0.05f + 0.1f * i;
                hop.InsertKeyFrame(t0, float3(0, 0, 0), linear);
                hop.InsertKeyFrame(t0 + 0.05f, float3(0, -0.3f * U, 0), up);
                hop.InsertKeyFrame(t0 + 0.1f, float3(0, 0, 0), down);
            }
        }
        else
        {
            // Duck into the grass, then pop up to peek over it.
            hop.InsertKeyFrame(0.10f, float3(0, 0.5f * U, 0), ease);
            hop.InsertKeyFrame(0.40f, float3(0, 0.5f * U, 0), linear);
            hop.InsertKeyFrame(0.48f, float3(0, -0.4f * U, 0), up);
            hop.InsertKeyFrame(0.58f, float3(0, 0, 0), down);
        }
        hop.InsertKeyFrame(1.0f, float3(0, 0, 0), linear);
        hop.Duration(Seconds(P));
        hop.IterationBehavior(wuc::AnimationIterationBehavior::Forever);
        a.jumper.StartAnimation(L"Offset", hop);

        float w0 = level == 2 ? 0.6f : 0.45f, w1 = level == 2 ? 0.9f : 0.9f;
        float waveRamp = a.species == art::Squirrel ? 0.12f : 0.05f;
        Keys(a.p, L"Wave", { { 0.0f, 0.0f }, { w0, 0.0f }, { w0 + waveRamp, 1.0f }, { w1 - waveRamp, 1.0f }, { w1, 0.0f }, { 1.0f, 0.0f } }, P, true);
        To(a.p, L"Excite", level == 0 ? 0.35f : (level == 1 ? 0.6f : 0.8f), 0.5f);
        To(a.p, L"Walk", 0.0f, 0.3f);
        a.slotX = a.x;
    }

    void MeadowAnimals::RestLoop(Animal& a, bool sleep)
    {
        To(a.p, L"Lie", 1.0f, 0.9f);
        To(a.p, L"Sleep", sleep ? 1.0f : 0.0f, 1.2f);
        To(a.p, L"Walk", 0.0f, 0.3f);
        To(a.p, L"Excite", 0.0f, 0.3f);
        ShowZz(a, sleep);
        a.slotX = a.x;
    }

    // ------------------------------------------------------------------ state changes

    void MeadowAnimals::Transition(Animal& a, bool celebrate)
    {
        Mode target = Desired(a.s);
        Freeze(a);
        Mode from = a.mode;
        a.mode = target;
        bool restSwap = (from == Mode::Resting || from == Mode::Sleeping) && (target == Mode::Resting || target == Mode::Sleeping);
        if (restSwap && !celebrate)
        {
            a.token = m_nextToken++;
            StartLoop(a);
            return;
        }
        std::vector<Step> steps;
        a.journeying = true; // keeps the bubble hidden and Tick away until the new loop starts
        steps.push_back(Prepare(from == Mode::Resting || from == Mode::Sleeping));
        if (celebrate) steps.push_back(Celebrate());
        float x1 = SlotFor(a, target), d1 = DepthOf(target);
        a.slotX = x1;
        if (std::abs(x1 - a.x) > 2.0f || std::abs(d1 - a.d) > 0.01f)
            Journey(a, a.x, a.d, x1, d1, target == Mode::Attention ? 1.5f : 1.0f, steps);
        steps.push_back([this](Animal& an) {
            an.journeying = false;
            StartLoop(an);
            return 0.0f;
        });
        Log(L"Meadow animals: %s (%s) -> %s%s", Truncate(a.s.title, 30).c_str(), art::SpeciesName(a.species),
            target == Mode::Busy ? L"busy" : target == Mode::Attention ? L"attention" : target == Mode::Resting ? L"resting" : L"sleeping", celebrate ? L" (celebrate)" : L"");
        Run(a, std::move(steps));
    }

    void MeadowAnimals::Remove(std::wstring const& id)
    {
        auto it = m_animals.find(id);
        if (it == m_animals.end() || it->second->leaving) return;
        Animal& a = *it->second;
        a.leaving = true;
        a.token = m_nextToken++;
        if (a.isChild) HopOut(a);
        To(a.root, L"Opacity", 0.0f, 1.0f);
        if (a.label) To(a.label, L"Opacity", 0.0f, 0.5f);
        After(1.1f, [this, id] {
            auto found = m_animals.find(id);
            if (found == m_animals.end() || !found->second->leaving) return;
            Animal& gone = *found->second;
            if (gone.layer >= 0)
                if (auto layer = LayerVisual(gone.layer)) layer.Children().Remove(gone.root);
            if (gone.label && m_stage.top) m_stage.top.Children().Remove(gone.label);
            if (m_hovered == id) m_hovered.clear();
            m_animals.erase(found);
        });
    }

    void MeadowAnimals::Update(SessionsSnapshot const& snapshot)
    {
        std::vector<AgentSession const*> list;
        for (auto const& s : snapshot.sessions) list.push_back(&s);
        std::sort(list.begin(), list.end(), [](auto* x, auto* y) {
            bool ax = x->status == AgentStatus::Waiting || x->status == AgentStatus::Working;
            bool ay = y->status == AgentStatus::Waiting || y->status == AgentStatus::Working;
            if (ax != ay) return ax;
            return x->lastActivity > y->lastActivity;
        });
        if (list.size() > kMaxAnimals) list.resize(kMaxAnimals);

        std::set<std::wstring> keep;
        for (auto* s : list)
        {
            keep.insert(s->id);
            auto it = m_animals.find(s->id);
            if (it == m_animals.end())
            {
                Animal& a = Create(*s);
                a.mode = Desired(*s);
                float x = SlotFor(a, a.mode), d = DepthOf(a.mode);
                // Spawn near the parent instead of the usual slot queue, when we can find it.
                auto parentIt = a.isChild ? m_animals.find(s->parentId) : m_animals.end();
                if (parentIt != m_animals.end())
                {
                    float2 at = PoseAt(*parentIt->second);
                    x = at.x + ((Hash(s->id) & 1) ? 1.0f : -1.0f) * m_U * 1.1f;
                    d = std::min(1.0f, at.y + 0.02f);
                }
                a.face = (Hash(s->id) & 2) ? 1.0f : -1.0f;
                a.facing.Scale(float3(a.face, 1, 1));
                Place(a, x, d);
                a.slotX = x;
                To(a.root, L"Opacity", 1.0f, 1.2f);
                a.token = m_nextToken++;
                StartLoop(a);
                if (a.isChild) HopIn(a);
                Log(L"Meadow animals: + %s as %s/%d (%s)", Truncate(s->title, 30).c_str(), art::SpeciesName(a.species), a.variant, AgentStatusName(s->status));
                continue;
            }
            Animal& a = *it->second;
            AgentStatus was = a.s.status;
            bool returning = a.leaving;
            a.s = *s;
            if (returning)
            {
                a.leaving = false;
                To(a.root, L"Opacity", 1.0f, 0.6f);
                Transition(a, false);
                continue;
            }
            bool celebrate = s->status == AgentStatus::Done && (was == AgentStatus::Working || was == AgentStatus::Waiting);
            if (Desired(*s) != a.mode || celebrate)
                Transition(a, celebrate);
            else if (!a.journeying)
                ShowLabel(a);
        }
        std::vector<std::wstring> gone;
        for (auto const& [id, a] : m_animals)
            if (!keep.count(id)) gone.push_back(id);
        for (auto const& id : gone) Remove(id);
    }

    void MeadowAnimals::Tick()
    {
        for (auto& [id, ap] : m_animals)
        {
            Animal& a = *ap;
            if (a.leaving || a.journeying) continue;
            Mode want = Desired(a.s);
            if (want != a.mode)
            {
                Transition(a, false);
                continue;
            }
            if (a.mode == Mode::Attention && Escalation(a) != a.level)
                AttentionLoop(a);
            if (a.bubble || a.hovered) DrawLabel(a);
        }
    }

    // ------------------------------------------------------------------ light

    void MeadowAnimals::SetLight(MeadowLight const& light, float seconds)
    {
        m_lit = light;
        To(m_light, L"Night", light.night, seconds);
        auto color = m_c.CreateColorKeyFrameAnimation();
        color.InsertKeyFrame(1.0f, Argb(255, light.haze));
        color.Duration(Seconds(std::max(0.05f, seconds)));
        m_hazeBrush.StartAnimation(L"Color", color);
        bool redraw = art::DiffRgb(light.tint, m_drawn.tint) > 6.0f || art::DiffRgb(light.warm, m_drawn.warm) > 10.0f ||
            std::abs(light.bright - m_drawn.bright) > 0.03f || std::abs(light.warmth - m_drawn.warmth) > 0.03f ||
            std::abs(light.night - m_drawn.night) > 0.03f || std::abs(light.tintAmount - m_drawn.tintAmount) > 0.03f;
        if (!redraw) return;
        m_drawn = light;
        for (auto& [id, a] : m_animals) DrawParts(*a);
    }

    // ------------------------------------------------------------------ pointer

    std::optional<std::wstring> MeadowAnimals::Hit(float2 host) const
    {
        float strength = 1.0f;
        m_pointer.TryGetScalar(L"Strength", strength);
        float cx = (m_stage.rc.left + m_stage.rc.right) * 0.5f;
        float lx = host.x - m_stage.rc.left, ly = host.y - m_stage.rc.top;
        if (lx < 0 || ly < 0 || lx > m_W || ly > m_H) return std::nullopt;
        std::optional<std::wstring> best;
        float bestD = -1;
        for (auto const& [id, ap] : m_animals)
        {
            Animal const& a = *ap;
            if (a.leaving || a.layer < 0) continue;
            float2 at = PoseAt(a);
            if (at.y > 0.6f && at.y < 1.05f) continue; // behind the ground hill
            float s = ScaleAt(at.y) * (a.isChild ? kChildScale : 1.0f);
            float shift = (cx - host.x) * m_stage.parallax[std::clamp(LayerOf(at.y), 0, 3)] * strength;
            float fx = at.x + shift, fy = Y(at.y, at.x);
            // Cover the squirrel's full tail sweep on both sides so hover stays stable when it turns.
            float hitRadius = a.species == art::Squirrel ? 1.15f : 1.2f * a.rig->rx;
            float halfW = hitRadius * s * m_U + Px(4);
            float top = fy - a.rig->height * 1.05f * s * m_U - (a.mode == Mode::Attention ? 0.45f * m_U * s : 0.0f);
            bool inside = lx >= fx - halfW && lx <= fx + halfW && ly >= top && ly <= fy + 0.1f * m_U * s;
            if (!inside && a.bubble && a.labelSize.x > 0)
            {
                float by = top - a.labelSize.y;
                inside = lx >= fx - a.labelSize.x * 0.5f && lx <= fx + a.labelSize.x * 0.5f && ly >= by && ly <= top;
            }
            if (inside && at.y > bestD)
            {
                bestD = at.y;
                best = id;
            }
        }
        return best;
    }

    void MeadowAnimals::Hover(std::optional<float2> host)
    {
        m_pointerAt = host;
        std::wstring id;
        if (host)
            if (auto hit = Hit(*host)) id = *hit;
        if (id != m_hovered)
        {
            auto old = m_animals.find(m_hovered);
            if (old != m_animals.end())
            {
                Animal& a = *old->second;
                a.hovered = false;
                if (a.mode != Mode::Attention && !a.journeying) To(a.p, L"Excite", 0.0f, 0.4f);
                if (a.mode == Mode::Sleeping && !a.journeying) To(a.p, L"Sleep", 1.0f, 0.8f);
                a.p.StopAnimation(L"Look");
                To(a.p, L"Look", 0.0f, 0.5f);
                ShowLabel(a);
            }
            m_hovered = id;
            auto now = m_animals.find(id);
            if (now != m_animals.end())
            {
                Animal& a = *now->second;
                a.hovered = true;
                if (a.mode != Mode::Attention && !a.journeying) To(a.p, L"Excite", 0.4f, 0.3f);
                if (a.mode == Mode::Sleeping && !a.journeying) To(a.p, L"Sleep", 0.3f, 0.4f);
                ShowLabel(a);
            }
        }
        auto it = m_animals.find(m_hovered);
        if (it == m_animals.end() || !host) return;
        Animal& a = *it->second;
        float2 at = PoseAt(a);
        float s = ScaleAt(at.y) * (a.isChild ? kChildScale : 1.0f);
        float fx = at.x + m_stage.rc.left, fy = Y(at.y, at.x) + m_stage.rc.top;
        if (a.mode != Mode::Busy && !a.journeying)
        {
            float want = host->x >= fx ? 1.0f : -1.0f;
            if (want != a.face) Turn(a, want, 0.25f);
        }
        float headY = fy - a.rig->height * s * m_U * 0.8f;
        float look = std::clamp((host->y - headY) / (3.0f * m_U * s), -0.4f, 0.3f);
        a.p.StopAnimation(L"Look");
        a.p.InsertScalar(L"Look", look);
    }

    int MeadowAnimals::Activate(std::wstring const& id)
    {
        auto it = m_animals.find(id);
        if (it == m_animals.end()) return 0;
        Animal& a = *it->second;
        if (a.mode == Mode::Attention && !a.journeying)
        {
            Happy(a);
            Run(a, { [](Animal&) { return 1.5f; }, [this](Animal& an) {
                AttentionLoop(an);
                return 0.0f;
            } });
            return 650;
        }
        Keys(a.p, L"Excite", { { 0.0f, 0.0f }, { 0.25f, 1.0f }, { 1.0f, a.hovered ? 0.4f : 0.0f } }, 0.9f, false);
        return 300;
    }
}
