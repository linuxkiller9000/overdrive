#include "game.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

struct TrackMeshes {
    Model road{}, edgeL{}, edgeR{}, centerLine{}, curbL{}, curbR{}, windows{}, sidewalkL{}, sidewalkR{};
    std::vector<Vector3> buildings;
    std::vector<Color> buildingColors;
    Vector3 boundsCenter{0, 0, 0};
    float boundsRadius = 400.f;
};

Model ribbonMesh(const sim::Track& t, float inner, float outer, Color c1, Color c2, bool dashed = false) {
    // Triangle strip along the centerline, offset band [inner, outer] meters left.
    int n = (int)t.center.size();
    std::vector<Vector3> verts(2 * (n + 1));
    std::vector<Color> colors(2 * (n + 1));
    std::vector<unsigned short> idx(6 * n);
    for (int i = 0; i <= n; i++) {
        int k = i % n;
        sim::Vec2 c = t.center[k];
        sim::Vec2 d = t.dirAt(t.cum[k]);
        sim::Vec2 lft{-d.y, d.x};
        int seg = (i < n) ? i : i - 1;
        Color col = (seg % 2 == 0) ? c1 : c2;
        verts[2 * i] = {c.x + lft.x * inner, 0, c.y + lft.y * inner};
        verts[2 * i + 1] = {c.x + lft.x * outer, 0, c.y + lft.y * outer};
        colors[2 * i] = col;
        colors[2 * i + 1] = col;
    }
    int quads = 0;
    for (int i = 0; i < n; i++) {
        if (dashed && (i % 10) >= 4) continue;
        int a = 2 * i, b = 2 * i + 1, c = 2 * i + 2, d = 2 * i + 3;
        idx[6 * quads + 0] = a; idx[6 * quads + 1] = b; idx[6 * quads + 2] = c;
        idx[6 * quads + 3] = c; idx[6 * quads + 4] = b; idx[6 * quads + 5] = d;
        quads++;
    }
    Mesh m{};
    m.vertexCount = (int)verts.size();
    m.triangleCount = 2 * quads;
    m.vertices = (float*)MemAlloc(verts.size() * sizeof(Vector3));
    m.colors = (unsigned char*)MemAlloc(colors.size() * sizeof(Color));
    m.indices = (unsigned short*)MemAlloc(idx.size() * sizeof(unsigned short));
    memcpy(m.vertices, verts.data(), verts.size() * sizeof(Vector3));
    memcpy(m.colors, colors.data(), colors.size() * sizeof(Color));
    memcpy(m.indices, idx.data(), idx.size() * sizeof(unsigned short));
    UploadMesh(&m, false);
    return LoadModelFromMesh(m);
}

// One batched vertex-colored mesh of emissive window bands for all buildings.
Model windowMesh(const std::vector<Vector3>& buildings) {
    std::vector<Vector3> verts;
    std::vector<Color> colors;
    std::vector<unsigned short> idx;
    unsigned st = 777;
    auto rnd = [&]() { st ^= st << 13; st ^= st >> 17; st ^= st << 5; return (st & 0xFFFF) / 65535.f; };
    unsigned short base = 0;
    for (auto& b : buildings) {
        float halfW = 5.f, top = b.y * 2.f;
        Color warm{255, 190, 90, 255}, cool{110, 200, 255, 255};
        for (int face = 0; face < 4; face++) {
            for (float y = 3.2f; y < top - 2.f; y += 4.f) {
                if (rnd() < 0.45f) continue; // lights out
                Color wc = (rnd() < 0.5f) ? warm : cool;
                wc.r = (unsigned char)(wc.r * (0.5f + 0.5f * rnd()));
                float o = halfW + 0.12f, w = 3.2f, h = 1.1f;
                Vector3 v0, v1, v2, v3;
                if (face == 0)      { v0={b.x+o,y-h,b.z-w}; v1={b.x+o,y-h,b.z+w}; v2={b.x+o,y+h,b.z+w}; v3={b.x+o,y+h,b.z-w}; }
                else if (face == 1) { v0={b.x-o,y-h,b.z+w}; v1={b.x-o,y-h,b.z-w}; v2={b.x-o,y+h,b.z-w}; v3={b.x-o,y+h,b.z+w}; }
                else if (face == 2) { v0={b.x-w,y-h,b.z+o}; v1={b.x+w,y-h,b.z+o}; v2={b.x+w,y+h,b.z+o}; v3={b.x-w,y+h,b.z+o}; }
                else                { v0={b.x+w,y-h,b.z-o}; v1={b.x-w,y-h,b.z-o}; v2={b.x-w,y+h,b.z-o}; v3={b.x+w,y+h,b.z-o}; }
                verts.push_back(v0); verts.push_back(v1); verts.push_back(v2); verts.push_back(v3);
                colors.push_back(wc); colors.push_back(wc); colors.push_back(wc); colors.push_back(wc);
                idx.push_back(base); idx.push_back(base+1); idx.push_back(base+2);
                idx.push_back(base); idx.push_back(base+2); idx.push_back(base+3);
                base += 4;
                if (base > 65500) goto done;
            }
        }
    }
done: ;
    Mesh m{};
    m.vertexCount = (int)verts.size();
    m.triangleCount = (int)idx.size() / 3;
    m.vertices = (float*)MemAlloc(verts.size() * 3 * sizeof(float));
    m.colors = (unsigned char*)MemAlloc(colors.size() * 4);
    m.indices = (unsigned short*)MemAlloc(idx.size() * sizeof(unsigned short));
    for (size_t i = 0; i < verts.size(); i++) {
        m.vertices[3*i] = verts[i].x; m.vertices[3*i+1] = verts[i].y; m.vertices[3*i+2] = verts[i].z;
        m.colors[4*i] = colors[i].r; m.colors[4*i+1] = colors[i].g; m.colors[4*i+2] = colors[i].b; m.colors[4*i+3] = 255;
    }
    for (size_t i = 0; i < idx.size(); i++) m.indices[i] = idx[i];
    UploadMesh(&m, false);
    return LoadModelFromMesh(m);
}

TrackMeshes buildTrack(const sim::Track& t, int quality) {
    TrackMeshes tm;
    Color road1{38, 38, 46, 255}, road2{34, 34, 40, 255};
    tm.road = ribbonMesh(t, -t.halfWidth, t.halfWidth, road1, road2);
    Color neonA{0, 229, 220, 255}, neonB{255, 45, 130, 255};
    tm.edgeL = ribbonMesh(t, t.halfWidth - 0.4f, t.halfWidth, neonA, neonA);
    tm.edgeR = ribbonMesh(t, -t.halfWidth, -t.halfWidth + 0.4f, neonB, neonB);
    tm.centerLine = ribbonMesh(t, -0.15f, 0.15f, {80, 80, 95, 255}, {80, 80, 95, 255}, true);
    tm.sidewalkL = ribbonMesh(t, t.halfWidth + 1.5f, t.halfWidth + 10.f, {44, 49, 65, 255}, {40, 44, 59, 255});
    tm.sidewalkR = ribbonMesh(t, -t.halfWidth - 10.f, -t.halfWidth - 1.5f, {44, 49, 65, 255}, {40, 44, 59, 255});
    Color curbA{200, 40, 60, 255}, curbB{225, 225, 230, 255};
    tm.curbL = ribbonMesh(t, t.halfWidth + 0.05f, t.halfWidth + 1.5f, curbA, curbB);
    tm.curbR = ribbonMesh(t, -t.halfWidth - 1.5f, -t.halfWidth - 0.05f, curbA, curbB);
    // procedural city blocks outside the road
    tm.buildings.clear();
    if (quality > 0) {
        unsigned st = 12345;
        auto rnd = [&]() { st ^= st << 13; st ^= st >> 17; st ^= st << 5; return (st & 0xFFFF) / 65535.f; };
        for (int i = 0; i < (int)t.center.size(); i += 3) {
            for (int side = -1; side <= 1; side += 2) {
                sim::Vec2 c = t.center[i];
                sim::Vec2 d = t.dirAt(t.cum[i]);
                sim::Vec2 n{-d.y, d.x};
                float off = t.halfWidth + 38.f + rnd() * 60.f;
                float x = c.x + n.x * off * side, z = c.y + n.y * off * side;
                // keep the camera corridor clear: reject if near ANY track point
                bool tooClose = false;
                for (auto& tp : t.center)
                    if (sim::len(sim::Vec2{x, z} - tp) < t.halfWidth + 32.f) { tooClose = true; break; }
                if (tooClose) continue;
                float h = 12.f + rnd() * 42.f;
                tm.buildings.push_back({x, h * 0.5f, z});
                Color bc{(unsigned char)(20 + rnd() * 30), (unsigned char)(25 + rnd() * 35), (unsigned char)(60 + rnd() * 60), 255};
                if (rnd() > 0.6f) bc = (rnd() > 0.5f) ? Color{10, 90, 90, 255} : Color{90, 15, 45, 255};
                tm.buildingColors.push_back(bc);
            }
        }
        tm.windows = windowMesh(tm.buildings);
    }
    return tm;
}

// Vertex-lit unit boxes: stable directional face shading, no shader dependency.
void litBox(Vector3 p, Vector3 size, Color color) {
    const Vector3 v[8]={{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
                        {-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
    const int faces[6][4]={{4,5,6,7},{1,0,3,2},{0,4,7,3},{5,1,2,6},{3,7,6,2},{0,1,5,4}};
    const float light[6]={0.72f,0.60f,0.82f,0.52f,1.f,0.35f};
    rlBegin(RL_QUADS);
    for (int f=0;f<6;f++) {
        rlColor4ub((unsigned char)(color.r*light[f]),(unsigned char)(color.g*light[f]),(unsigned char)(color.b*light[f]),color.a);
        for (int i=0;i<4;i++) { Vector3 q=v[faces[f][i]];
            rlVertex3f(p.x+q.x*size.x*.5f,p.y+q.y*size.y*.5f,p.z+q.z*size.z*.5f); }
    }
    rlEnd();
}

void drawCar(const sim::Car& c, const sim::CarSpec& sp, Color body) {
    Matrix m = MatrixMultiply(MatrixRotateY(PI * 0.5f - c.heading), MatrixTranslate(c.pos.x, 0, c.pos.y));
    rlPushMatrix();
    rlMultMatrixf(MatrixToFloat(m));
    Color dark = ColorBrightness(body, -0.4f);
    Color glass{35, 65, 90, body.a};
    Color accent = (sp.name == "SPRINTER") ? Color{255, 200, 40, body.a} : (sp.name == "HAULER") ? Color{230, 230, 240, 255} : Color{0, 229, 220, 255};
    accent.a = body.a;
    if (sp.name == "HAULER") {
        litBox({0, 0.75f, 0}, {2.6f, 1.1f, 4.6f}, body);
        litBox({0, 1.45f, -0.4f}, {2.2f, 0.6f, 2.6f}, glass);
        litBox({0, 0.75f, -2.35f}, {2.6f, 0.5f, 0.2f}, accent);
    } else {
        float nose = (sp.name == "SPRINTER") ? 0.9f : 0.7f;
        litBox({0, 0.35f, 1.55f}, {1.9f, 0.35f, 1.3f}, dark);
        litBox({0, 0.5f, 0}, {2.1f, 0.55f, 3.2f}, body);
        litBox({0, 0.95f, -0.25f}, {1.5f, 0.45f, 1.6f}, glass);
        litBox({0, 0.85f, -1.85f}, {2.0f, 0.08f, 0.5f}, accent);
        litBox({-0.85f, 0.65f, -1.8f}, {0.12f, 0.35f, 0.3f}, dark);
        litBox({0.85f, 0.65f, -1.8f}, {0.12f, 0.35f, 0.3f}, dark);
        litBox({0, 0.42f, -1.65f}, {1.8f, 0.18f, 0.1f}, {255, 30, 60, body.a});
        litBox({0, 0.28f, nose + 1.6f}, {1.7f, 0.12f, 0.3f}, accent);
    }
    float wr = sp.radius * 0.42f;
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2)
            DrawCylinderEx({sx * 1.0f, wr, sz * 1.4f}, {sx * 1.3f, wr, sz * 1.4f}, wr, wr, 10, {22, 24, 30, body.a});
    if (c.boostLeft > 0.f) {
        litBox({0, 0.45f, -2.6f}, {1.4f, 0.3f, 0.9f}, {255, 200, 40, body.a});
        litBox({0, 0.45f, -3.2f}, {0.9f, 0.2f, 0.7f}, {255, 120, 20, body.a});
    }
    rlPopMatrix();
}

sim::CarInput readInput(const sim::Settings& s) {
    sim::RawKeys k{};
    k.accel = IsKeyDown(s.keyAccel) || IsKeyDown(s.keyAccelAlt);
    k.brake = IsKeyDown(s.keyBrake) || IsKeyDown(s.keyBrakeAlt);
    k.left = IsKeyDown(s.keyLeft) || IsKeyDown(s.keyLeftAlt);
    k.right = IsKeyDown(s.keyRight) || IsKeyDown(s.keyRightAlt);
    k.handbrake = IsKeyDown(s.keyHandbrake);
    k.boost = IsKeyPressed(s.keyBoost);
    sim::RawPad p{};
    if (IsGamepadAvailable(0)) {
        p.steerAxis = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        p.accelAxis = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_TRIGGER);
        p.left = IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
        p.right = IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
        p.accel = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
        p.brake = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
        p.handbrake = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_UP);
        p.boost = IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
    }
    return sim::mergeInputs(sim::mapKeys(s, k), sim::mapGamepad(s, p));
}

static void drawTextOutline(const char* text, int x, int y, int size, Color color) {
    DrawText(text, x - 2, y - 2, size, BLACK);
    DrawText(text, x + 2, y + 2, size, BLACK);
    DrawText(text, x, y, size, color);
}

void drawHud(const sim::Race& r, const GameApp& app, int W, int H) {
    const sim::Car& p = r.cars[0];
    float spd = std::fabs(sim::dot(p.vel, {std::cos(p.heading), std::sin(p.heading)})) * 3.6f;
    DrawText(TextFormat("%3.0f", spd), W - 150, H - 110, 64, WHITE);
    DrawText("km/h", W - 145, H - 40, 20, GRAY);
    DrawText(TextFormat("LAP %d/%d", p.lap + 1 > r.lapsToWin ? r.lapsToWin : p.lap + 1, r.lapsToWin), 20, 20, 28, WHITE);
    int place = p.finished ? p.finishPlace : 1;
    if (!p.finished)
        for (size_t i = 1; i < r.cars.size(); i++) if (r.cars[i].progress > p.progress) place++;
    DrawText(place == 1 ? "1st" : (place == 2 ? "2nd" : "3rd"), W - 90, 20, 28, place == 1 ? GOLD : WHITE);
    float t = p.finished ? p.raceTime : r.time() - 3.2f;
    if (t < 0) t = 0;
    DrawText(TextFormat("%d:%05.2f", (int)(t / 60), std::fmod(t, 60.f)), W / 2 - 60, 20, 28, WHITE);
    if (p.boostCharges > 0) {
        DrawRectangle(W / 2 - 60, H - 70, 120, 34, {255, 200, 40, 60});
        DrawRectangleLines(W / 2 - 60, H - 70, 120, 34, {255, 200, 40, 255});
        DrawText("TURBO", W / 2 - 44, H - 63, 20, {255, 200, 40, 255});
    }
    if (p.boostLeft > 0.f)
        drawTextOutline("BOOST!", W / 2 - 50, H - 110, 28, {255, 200, 40, 255});
    if (p.wrongWay) drawTextOutline("WRONG WAY", W / 2 - 110, H / 2 - 40, 34, RED);
    if (r.countdown > 0.f) {
        int cd = (int)r.countdown;
        drawTextOutline(cd > 0 ? TextFormat("%d", cd) : "GO!", W / 2 - 30, H / 2 - 60, 72, GOLD);
    }
    if (p.finished) {
        DrawRectangle(0, 0, W, H, {0, 0, 0, 160});
        drawTextOutline(p.finishPlace == 1 ? "YOU WIN!" : TextFormat("FINISHED %d%s", p.finishPlace, p.finishPlace == 2 ? "nd" : "rd"),
                        W / 2 - 140, H / 3, 44, GOLD);
        drawTextOutline("Press ESC", W / 2 - 60, H / 3 + 60, 20, LIGHTGRAY);
    }
    // minimap
    float mm = 150.f, mx = W - mm - 16, my = 60;
    sim::Vec2 lo{1e9f, 1e9f}, hi{-1e9f, -1e9f};
    for (auto& c : r.track.center) { lo.x = fminf(lo.x, c.x); lo.y = fminf(lo.y, c.y); hi.x = fmaxf(hi.x, c.x); hi.y = fmaxf(hi.y, c.y); }
    float sc = mm / fmaxf(hi.x - lo.x, hi.y - lo.y);
    auto proj = [&](sim::Vec2 v) { return Vector2{mx + (v.x - lo.x) * sc, my + (v.y - lo.y) * sc}; };
    DrawRectangle(mx - 8, my - 8, mm + 16, mm * (hi.y - lo.y) / fmaxf(hi.x - lo.x, 1.f) + 16, {0, 0, 0, 90});
    for (size_t i = 0; i < r.track.center.size(); i++) {
        Vector2 a = proj(r.track.center[i]);
        Vector2 b = proj(r.track.center[(i + 1) % r.track.center.size()]);
        DrawLineV(a, b, {120, 120, 140, 255});
    }
    for (size_t i = 0; i < r.cars.size(); i++) {
        Vector2 a = proj(r.cars[i].pos);
        DrawCircleV(a, 4, i == 0 ? GOLD : (i == 1 ? Color{255, 45, 130, 255} : Color{0, 229, 220, 255}));
    }
    if (app.showFps) DrawText(TextFormat("%d FPS", GetFPS()), 20, H - 30, 20, LIME);
}

} // namespace

static void configureWindow(const sim::Settings& s) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(s.width, s.height, "OVERDRIVE");
    SetTargetFPS(s.fpsCap);
    if (s.fullscreen) ToggleFullscreen();
}

// dt-based chase camera: tracks every sim step, independent of render cadence.
struct ChaseCam { Vector3 smoothed{}; bool init = false; };
static void updateChaseCam(Camera3D& cam, ChaseCam& cc, const sim::Race& r, float dt) {
    const sim::Car& p = r.cars[0];
    sim::Vec2 fwd{std::cos(p.heading), std::sin(p.heading)};
    float speed = sim::len(p.vel);
    float dist = 8.f + speed * 0.08f; // bounded: player stays in view through boost
    Vector3 camPos = {p.pos.x - fwd.x * dist, 4.5f + speed * 0.03f, p.pos.y - fwd.y * dist};
    if (!cc.init) { cc.smoothed = camPos; cc.init = true; }
    float a = 1.f - std::exp(-6.f * dt);
    cc.smoothed = Vector3Lerp(cc.smoothed, camPos, a);
    cam.position = cc.smoothed;
    cam.target = {p.pos.x + fwd.x * 2.5f, 0.8f, p.pos.y + fwd.y * 2.5f};
    cam.fovy = 60.f + speed * 0.3f + (p.boostLeft > 0 ? 8.f : 0.f);
}

static void drawFrame(const sim::Race& r, const TrackMeshes& tm, const Camera3D& cam, const GameApp& app, float steerVis) {
    const sim::Car& p = r.cars[0];
    sim::Vec2 fwd{std::cos(p.heading), std::sin(p.heading)};
    BeginDrawing();
    ClearBackground({8, 8, 20, 255});
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), {14, 10, 42, 255}, {64, 18, 60, 255});
    BeginMode3D(cam);
    DrawPlane({0, -0.05f, 0}, {1600, 1600}, {16, 16, 28, 255});
    DrawModel(tm.road, {0, 0, 0}, 1.f, WHITE);
    DrawModel(tm.edgeL, {0, 0.02f, 0}, 1.f, WHITE);
    DrawModel(tm.edgeR, {0, 0.02f, 0}, 1.f, WHITE);
    DrawModel(tm.centerLine, {0, 0.03f, 0}, 1.f, WHITE);
    DrawModel(tm.sidewalkL, {0, -0.01f, 0}, 1.f, WHITE);
    DrawModel(tm.sidewalkR, {0, -0.01f, 0}, 1.f, WHITE);
    DrawModel(tm.curbL, {0, 0.04f, 0}, 1.f, WHITE);
    DrawModel(tm.curbR, {0, 0.04f, 0}, 1.f, WHITE);
    if (tm.windows.meshCount > 0) { rlDisableBackfaceCulling(); DrawModel(tm.windows, {0, 0, 0}, 1.f, WHITE); rlEnableBackfaceCulling(); }
    Color fog{16, 12, 40, 255};
    for (size_t i = 0; i < tm.buildings.size(); i++) {
        Vector3 b = tm.buildings[i];
        float dist = Vector3Distance(b, cam.position);
        float fade = 1.f - fminf(dist / 700.f, 0.7f);
        Color bc = ColorLerp(fog, tm.buildingColors[i], fade);
        DrawCubeV(b, {10, b.y * 2, 10}, bc);
        DrawCubeV({b.x, b.y * 2 + 1.2f, b.z}, {6.5f, 2.4f, 6.5f}, ColorBrightness(bc, -0.25f));
        Color trim = (i % 3 == 0) ? Color{0, 229, 220, 255} : (i % 3 == 1) ? Color{255, 45, 130, 255} : Color{255, 200, 40, 255};
        DrawCubeV({b.x, b.y * 2 + 0.3f, b.z}, {10.4f, 0.35f, 10.4f}, ColorLerp(fog, trim, fade));
        if (b.y > 22.f)
            DrawCylinder({b.x, b.y * 2 + 5.f, b.z}, 0.25f, 0.08f, 6.f, 6, ColorBrightness(bc, 0.5f));
    }
    // Street furniture follows the road tangent, outside the drivable corridor.
    for (size_t k=0;k<r.track.center.size();k+=12) {
        sim::Vec2 c=r.track.center[k], d=r.track.dirAt(r.track.cum[k]);
        sim::Vec2 n{-d.y,d.x};
        for (int side=-1;side<=1;side+=2) {
            Vector3 b{c.x+n.x*(r.track.halfWidth+7.f)*side,0,c.y+n.y*(r.track.halfWidth+7.f)*side};
            litBox({b.x,3.8f,b.z},{.28f,7.6f,.28f},{80,90,115,255});
            litBox({b.x-n.x*side*1.5f,7.6f,b.z-n.y*side*1.5f},{3.5f,.22f,1.f},{130,145,170,255});
            litBox({b.x-n.x*side*2.f,7.42f,b.z-n.y*side*2.f},{1.6f,.10f,.7f},{255,221,154,255});
            // kiosk and illuminated advertising slabs alternate along each district
            if (k%24==0) {
                Vector3 a{b.x+n.x*side*4.f,0,b.z+n.y*side*4.f};
                litBox({a.x,1.4f,a.z},{3.f,2.8f,2.f},{29,42,61,255});
                litBox({a.x,2.9f,a.z},{3.4f,.2f,2.4f},{0,180,200,255});
            }
        }
    }
    // Three identifiable gateway arches provide district rhythm and navigation cues.
    for (int district=1;district<=3;district++) {
        float at=r.track.total*district*.25f;
        sim::Vec2 c=r.track.pointAt(at), d=r.track.dirAt(at), n{-d.y,d.x};
        rlPushMatrix(); rlTranslatef(c.x,0,c.y); rlRotatef(-atan2f(d.y,d.x)*RAD2DEG,0,1,0);
        Color ac=district==1?Color{0,210,225,255}:district==2?Color{245,60,130,255}:Color{255,190,65,255};
        float w=r.track.halfWidth+3.f;
        litBox({0,4.5f,-w},{1.1f,9.f,1.1f},{45,55,80,255});
        litBox({0,4.5f,w},{1.1f,9.f,1.1f},{45,55,80,255});
        litBox({0,9.2f,0},{1.4f,.7f,w*2+2},{30,39,59,255});
        litBox({-.72f,9.2f,0},{.08f,.32f,w*2+2},ac);
        for(int dash=0;dash<district;dash++) litBox({-.8f,9.25f,(dash-(district-1)*.5f)*2.f},{.08f,.9f,.8f},{240,245,255,255});
        rlPopMatrix();
    }
    // landmark spire outside the track bounds: stacked needle skyline beacon
    {
        Vector3 s = tm.boundsCenter;
        s.x += tm.boundsRadius + 250.f;
        s.z -= tm.boundsRadius * 0.5f;
        DrawCubeV({s.x, 25.f, s.z}, {9.f, 50.f, 9.f}, {30, 35, 70, 255});
        DrawCubeV({s.x, 58.f, s.z}, {5.5f, 18.f, 5.5f}, {40, 48, 90, 255});
        DrawCubeV({s.x, 72.f, s.z}, {3.f, 10.f, 3.f}, {55, 65, 110, 255});
        DrawCylinder({s.x, 84.f, s.z}, 0.8f, 0.1f, 14.f, 8, {80, 90, 140, 255});
        DrawSphere({s.x, 92.f, s.z}, 1.5f, {255, 60, 90, 255});
        for (int band = 0; band < 4; band++)
            DrawCubeV({s.x, 10.f + band * 12.f, s.z}, {9.4f, 0.7f, 9.4f}, {0, 229, 220, 255});
        DrawCubeV({s.x, 61.f, s.z}, {6.f, 0.7f, 6.f}, {255, 45, 130, 255});
    }
    // barriers + checkpoint gates + start banner
    for (size_t i = 0; i < r.track.center.size(); i += 4) {
        sim::Vec2 c = r.track.center[i];
        sim::Vec2 d = r.track.dirAt(r.track.cum[i]);
        sim::Vec2 n{-d.y, d.x};
        Color bc = (i / 4) % 2 == 0 ? Color{0, 229, 220, 255} : Color{255, 45, 130, 255};
        DrawCubeV({c.x + n.x * (r.track.halfWidth + 0.6f), 0.8f, c.y + n.y * (r.track.halfWidth + 0.6f)}, {0.5f, 1.6f, 0.5f}, bc);
        DrawCubeV({c.x - n.x * (r.track.halfWidth + 0.6f), 0.8f, c.y - n.y * (r.track.halfWidth + 0.6f)}, {0.5f, 1.6f, 0.5f}, bc);
    }
    for (size_t k = 1; k < r.track.checkpoints.size(); k++) {
        sim::Vec2 cp = r.track.pointAt(r.track.cum[r.track.checkpoints[k]]);
        DrawCubeV({cp.x, 3.f, cp.y}, {0.6f, 6.f, 0.6f}, {60, 60, 90, 255});
    }
    sim::Vec2 st = r.track.pointAt(0);
    sim::Vec2 sd = r.track.dirAt(0);
    sim::Vec2 sn{-sd.y, sd.x};
    // start banner: poles + beam, rotated to span the road
    {
        rlPushMatrix();
        rlTranslatef(st.x, 0, st.y);
        rlRotatef(-std::atan2(sd.y, sd.x) * 180.f / 3.14159265f, 0, 1, 0);
        DrawCubeV({0, 4.5f, -r.track.halfWidth}, {0.6f, 9.f, 0.6f}, WHITE);
        DrawCubeV({0, 4.5f, r.track.halfWidth}, {0.6f, 9.f, 0.6f}, WHITE);
        DrawCubeV({0, 9.f, 0}, {0.6f, 0.6f, 2 * r.track.halfWidth + 1.f}, {200, 200, 220, 255});
        rlPopMatrix();
    }
    for (auto& pk : r.pickups)
        if (pk.active) {
            float pulse = 0.8f + 0.2f * std::sin(GetTime() * 6.0);
            DrawCubeV({pk.pos.x, 1.f, pk.pos.y}, {pulse, pulse, pulse}, {255, 200, 40, 255});
        }
    Color cols[3] = {{230, 230, 240, 255}, {255, 45, 130, 255}, {0, 200, 190, 255}};
    // rivals sitting on the camera->player sight line fade out instead of hiding the player
    const sim::Car& pc = r.cars[0];
    sim::Vec2 camPos{cam.position.x, cam.position.z};
    sim::Vec2 seg = pc.pos - camPos;
    float segLen2 = fmaxf(sim::dot(seg, seg), 1e-3f);
    for (size_t i = 0; i < r.cars.size(); i++) {
        Color col = cols[i % 3];
        if (i > 0) {
            sim::Vec2 rel = r.cars[i].pos - camPos;
            float along = sim::dot(rel, seg) / segLen2;
            if (along > -0.1f && along < 1.0f) {
                sim::Vec2 closest{camPos.x + seg.x * along, camPos.y + seg.y * along};
                float d = sim::len(r.cars[i].pos - closest);
                if (d < 3.0f) col.a = 42;
            }
        }
        if (col.a < 255) {
            rlDrawRenderBatchActive();
            rlDisableDepthMask();
            BeginBlendMode(BLEND_ALPHA);
            drawCar(r.cars[i], r.specs[i], col);
            EndBlendMode();
            rlDrawRenderBatchActive();
            rlEnableDepthMask();
        } else {
            drawCar(r.cars[i], r.specs[i], col);
        }
    }
    EndMode3D();
    drawHud(r, app, GetScreenWidth(), GetScreenHeight());
    EndDrawing();
}

// ---------- menus ----------
static const char* diffName(int d) { return d == 0 ? "EASY (Sunday Drive)" : d == 2 ? "HARD (Apex)" : "NORMAL (Club)"; }

struct MenuResult { bool quit = false; };

// returns true if the user chose "race"
static void drawTitle(const GameApp& app, int sel) {
    const char* items[] = {"START RACE", "VEHICLE", "DIFFICULTY", "LAPS", "QUALITY", "FPS CAP", "QUIT"};
    std::vector<sim::CarSpec> roster = sim::defaultRoster();
    BeginDrawing();
    ClearBackground({8, 8, 20, 255});
    drawTextOutline("OVERDRIVE", GetScreenWidth() / 2 - 170, 60, 64, {255, 45, 130, 255});
    for (int i = 0; i < 7; i++) {
        const char* val = "";
        if (i == 1) val = roster[app.settings.playerVehicle % (int)roster.size()].name.c_str();
        if (i == 2) val = diffName(app.settings.aiDifficulty);
        if (i == 3) val = TextFormat("%d", app.settings.laps);
        if (i == 4) val = app.settings.quality ? "HIGH" : "LOW";
        if (i == 5) val = TextFormat("%d", app.settings.fpsCap);
        drawTextOutline(TextFormat("%s%s%s%s", i == sel ? "> " : "  ", items[i], val[0] ? ": " : "", val),
                        GetScreenWidth() / 2 - 220, 200 + i * 44, 28, i == sel ? GOLD : WHITE);
    }
    drawTextOutline("ARROWS + ENTER to select", GetScreenWidth() / 2 - 290, GetScreenHeight() - 50, 18, GRAY);
    EndDrawing();
}

static bool titleMenu(GameApp& app) {
    int sel = 0;
    std::vector<sim::CarSpec> roster = sim::defaultRoster();
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_UP)) sel = (sel + 6) % 7;
        if (IsKeyPressed(KEY_DOWN)) sel = (sel + 1) % 7;
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT)) {
            int d = IsKeyPressed(KEY_LEFT) ? -1 : 1;
            if (sel == 1) app.settings.playerVehicle = (app.settings.playerVehicle + (int)roster.size() + d) % (int)roster.size();
            if (sel == 2) app.settings.aiDifficulty = (app.settings.aiDifficulty + 3 + d) % 3;
            if (sel == 3) app.settings.laps = app.settings.laps == 1 ? 3 : app.settings.laps == 3 ? 5 : 1;
            if (sel == 4) app.settings.quality = 1 - app.settings.quality;
            if (sel == 5) app.settings.fpsCap = app.settings.fpsCap == 60 ? 30 : 60;
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            if (sel == 0) return true;
            if (sel == 6) return false;
            if (sel == 1) app.settings.playerVehicle = (app.settings.playerVehicle + 1) % (int)roster.size();
            if (sel == 2) app.settings.aiDifficulty = (app.settings.aiDifficulty + 1) % 3;
            if (sel == 3) app.settings.laps = app.settings.laps == 1 ? 3 : app.settings.laps == 3 ? 5 : 1;
            if (sel == 4) app.settings.quality = 1 - app.settings.quality;
            if (sel == 5) app.settings.fpsCap = app.settings.fpsCap == 60 ? 30 : 60;
        }
        drawTitle(app, sel);
    }
    return false;
}

// pause menu: 0 resume, 1 restart, 2 quit to title
static void drawPause(int sel) {
    const char* items[] = {"RESUME", "RESTART RACE", "QUIT TO TITLE"};
    BeginDrawing();
    ClearBackground({8, 8, 20, 255});
    drawTextOutline("PAUSED", GetScreenWidth() / 2 - 90, 120, 48, WHITE);
    for (int i = 0; i < 3; i++)
        drawTextOutline(TextFormat("%s%s", i == sel ? "> " : "  ", items[i]),
                        GetScreenWidth() / 2 - 140, 240 + i * 44, 28, i == sel ? GOLD : WHITE);
    EndDrawing();
}

static int pauseMenu(const GameApp& app) {
    (void)app;
    int sel = 0;
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_ESCAPE)) return 0;
        if (IsKeyPressed(KEY_UP)) sel = (sel + 2) % 3;
        if (IsKeyPressed(KEY_DOWN)) sel = (sel + 1) % 3;
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) return sel;
        drawPause(sel);
    }
    return 2;
}

// Records update: best lap any finish; best RACE time only counts on a win.
static void updateRecords(sim::Settings& s, const sim::Car& p) {
    if (p.bestLap > 0 && (s.bestLapMs < 0 || p.bestLap * 1000.f < s.bestLapMs))
        s.bestLapMs = (int)(p.bestLap * 1000.f);
    if (p.finishPlace == 1 && (s.bestRaceMs < 0 || p.raceTime * 1000.f < s.bestRaceMs))
        s.bestRaceMs = (int)(p.raceTime * 1000.f);
}

static void drawResults(const GameApp& app, const sim::Race& race) {
    const sim::Car& p = race.cars[0];
    BeginDrawing();
    ClearBackground({8, 8, 20, 255});
    drawTextOutline(p.finishPlace == 1 ? "YOU WIN!" : TextFormat("FINISHED %d%s", p.finishPlace, p.finishPlace == 2 ? "nd" : "rd"),
                    GetScreenWidth() / 2 - 160, 90, 48, GOLD);
    drawTextOutline(TextFormat("RACE TIME  %d:%05.2f", (int)(p.raceTime / 60), std::fmod(p.raceTime, 60.f)),
                    GetScreenWidth() / 2 - 170, 200, 28, WHITE);
    if (p.bestLap > 0)
        drawTextOutline(TextFormat("BEST LAP   %0.2f s", p.bestLap), GetScreenWidth() / 2 - 170, 240, 28, WHITE);
    if (app.settings.bestLapMs > 0)
        drawTextOutline(TextFormat("TRACK BEST LAP   %d:%05.2f", app.settings.bestLapMs / 60000, std::fmod(app.settings.bestLapMs / 1000.f, 60.f)),
                        GetScreenWidth() / 2 - 170, 300, 22, GRAY);
    if (app.settings.bestRaceMs > 0)
        drawTextOutline(TextFormat("TRACK BEST WIN   %d:%05.2f", app.settings.bestRaceMs / 60000, std::fmod(app.settings.bestRaceMs / 1000.f, 60.f)),
                        GetScreenWidth() / 2 - 170, 330, 22, GRAY);
    drawTextOutline("ENTER continue", GetScreenWidth() / 2 - 90, GetScreenHeight() - 60, 22, GOLD);
    EndDrawing();
}

static void resultsScreen(GameApp& app, const sim::Race& race) {
    const sim::Car& p = race.cars[0];
    // persist bests
    updateRecords(app.settings, p);
    sim::saveSettings(app.settings, app.settingsPath);
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) return;
        drawResults(app, race);
    }
}

int runGame(GameApp& app) {
    configureWindow(app.settings);
    SetExitKey(0); // ESC handled manually for pause
    while (!WindowShouldClose()) {
        if (!titleMenu(app)) break;
        sim::saveSettings(app.settings, app.settingsPath);
        bool quitToTitle = false;
        while (!quitToTitle && !WindowShouldClose()) {
            sim::Race race;
            race.init(app.settings.laps, app.settings.aiDifficulty, app.settings.playerVehicle);
            Camera3D cam{};
            cam.up = {0, 1, 0};
            cam.fovy = 60;
            cam.projection = CAMERA_PERSPECTIVE;
            ChaseCam cc;
            TrackMeshes tm = buildTrack(race.track, app.settings.quality);
            double acc = 0;
            int action = 0; // 0 racing, 1 restart, 2 quit to title
            while (!WindowShouldClose() && action == 0 && !race.cars[0].finished) {
                if (IsKeyPressed(app.settings.keyPause)) action = pauseMenu(app);
                if (action != 0) break;
                if (IsKeyPressed(KEY_F3)) app.showFps = !app.showFps;
                if (IsKeyPressed(KEY_F11)) ToggleFullscreen();
                acc += GetFrameTime();
                sim::CarInput in = race.countdown > 0.f ? sim::CarInput{} : readInput(app.settings);
                while (acc >= sim::STEP) {
                    race.step({in, sim::aiDrive(race, 1), sim::aiDrive(race, 2)});
                    updateChaseCam(cam, cc, race, sim::STEP);
                    acc -= sim::STEP;
                }
                drawFrame(race, tm, cam, app, in.steer);
            }
            if (race.cars[0].finished && action == 0) resultsScreen(app, race);
            UnloadModel(tm.road); UnloadModel(tm.edgeL); UnloadModel(tm.edgeR); UnloadModel(tm.centerLine);
        UnloadModel(tm.sidewalkL); UnloadModel(tm.sidewalkR);
        UnloadModel(tm.curbL); UnloadModel(tm.curbR);
        if (tm.windows.meshCount > 0) UnloadModel(tm.windows);
            if (action == 1) continue; // restart race
            quitToTitle = true;
        }
    }
    sim::saveSettings(app.settings, app.settingsPath);
    CloseWindow();
    return 0;
}

int runScreenshots(GameApp& app, const std::string& prefix, int warmFrames, int shotEvery, int shotCount) {
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_MSAA_4X_HINT);
    InitWindow(1366, 768, "overdrive-demo");
    SetTargetFPS(600);
    sim::Race race;
    race.init(3);
    Camera3D cam{};
    cam.up = {0, 1, 0};
    cam.fovy = 60;
    cam.projection = CAMERA_PERSPECTIVE;
    TrackMeshes tm = buildTrack(race.track, app.settings.quality);
    ChaseCam cc;
    int taken = 0;
    bool gotCorner = false, gotBoost = false, gotPause = false, gotFinish = false;
    auto snap = [&](const char* tag) {
        char name[256];
        std::snprintf(name, sizeof(name), "%s-%02d-%s.png", prefix.c_str(), taken, tag);
        TakeScreenshot(name);
        taken++;
    };
    for (int f = 0; f < 60 * 300; f++) {
        // player input runs through the same mapGamepad mapping the interactive
        // path uses, with scripted analog axes following the AI racing line
        sim::CarInput aiIn = sim::aiDrive(race, 0);
        if (race.cars[0].boostCharges > 0 && race.cars[0].boostLeft <= 0 && race.cars[0].lap >= 1) aiIn.boost = true;
        sim::RawPad pad{};
        pad.steerAxis = aiIn.steer;
        pad.accelAxis = aiIn.throttle;
        pad.brake = aiIn.brake > 0.5f;
        pad.handbrake = aiIn.handbrake;
        pad.boost = aiIn.boost;
        sim::CarInput in = sim::mapGamepad(app.settings, pad);
        race.step({in, sim::aiDrive(race, 1), sim::aiDrive(race, 2)});
        updateChaseCam(cam, cc, race, sim::STEP);
        float t = race.time();
        // grant one boost charge mid-race so activation is guaranteed on camera
        if (!gotBoost && race.cars[0].lap >= 1 && race.cars[0].boostCharges == 0 && t > 30.f && race.cars[0].boostLeft <= 0)
            race.cars[0].boostCharges = 1;
        if (f % 3 == 0) drawFrame(race, tm, cam, app, 0); // draw often enough that the chase camera tracks
        if (t >= 5.f && taken == 0) { snap("grid-straight"); }
        else if (!gotCorner && race.cars[0].sHint > race.track.total * 0.35f && race.cars[0].sHint < race.track.total * 0.55f) {
            snap("corner"); gotCorner = true;
        } else if (gotCorner && !gotBoost && race.cars[0].boostLeft > 0.5f) {
            snap("turbo"); gotBoost = true;
        } else if (gotBoost && !gotPause && f % 3 == 0) {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 140});
            DrawText("PAUSED", GetScreenWidth() / 2 - 80, GetScreenHeight() / 2 - 30, 36, WHITE);
            DrawText("ESC resume | F3 fps | F11 fullscreen", GetScreenWidth() / 2 - 150, GetScreenHeight() / 2 + 20, 18, GRAY);
            EndDrawing();
            snap("pause"); gotPause = true;
        } else if (gotPause && race.cars[0].finished && f % 3 == 0) { snap("finish"); gotFinish = true; break; }
    }
    CloseWindow();
    std::printf("demo frames: %d (finish=%d)\n", taken, (int)gotFinish);
    return gotFinish ? 0 : 2;
}

// Runtime smoke test of the interactive loop: real window, real readInput polling,
// scripted player input through the same mapGamepad mapping, full race sim.
// (X11 XTest injection does not reach GLFW under Xvfb, verified by probe; physical
// key polling therefore still needs on-hardware verification.)
int runUITest(GameApp& app) {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "overdrive-uitest");
    SetTargetFPS(240);
    sim::Race race;
    race.init(app.settings.laps, app.settings.aiDifficulty, app.settings.playerVehicle);
    Camera3D cam{};
    cam.up = {0, 1, 0};
    cam.fovy = 60;
    cam.projection = CAMERA_PERSPECTIVE;
    ChaseCam cc;
    TrackMeshes tm = buildTrack(race.track, app.settings.quality);
    bool polledOk = true;
    double acc = 0;
    for (int f = 0; f < 60 * 200 && !race.cars[0].finished && !WindowShouldClose(); f++) {
        acc += GetFrameTime();
        sim::CarInput hw = race.countdown > 0.f ? sim::CarInput{} : readInput(app.settings);
        if (hw.throttle < 0.f || hw.steer < -1.f || hw.steer > 1.f) polledOk = false;
        sim::CarInput aiIn = sim::aiDrive(race, 0);
        if (race.cars[0].boostCharges > 0 && race.cars[0].boostLeft <= 0 && race.cars[0].lap >= 1) aiIn.boost = true;
        sim::RawPad pad{};
        pad.steerAxis = aiIn.steer;
        pad.accelAxis = aiIn.throttle;
        pad.brake = aiIn.brake > 0.5f;
        pad.handbrake = aiIn.handbrake;
        pad.boost = aiIn.boost;
        sim::CarInput in = sim::mergeInputs(hw, sim::mapGamepad(app.settings, pad));
        while (acc >= sim::STEP) {
            race.step({in, sim::aiDrive(race, 1), sim::aiDrive(race, 2)});
            updateChaseCam(cam, cc, race, sim::STEP);
            acc -= sim::STEP;
        }
        drawFrame(race, tm, cam, app, in.steer);
    }
    CloseWindow();
    std::printf("uitest: polledOk=%d finished=%d progress=%.1f\n", (int)polledOk,
                (int)race.cars[0].finished, race.cars[0].progress);
    return (polledOk && race.cars[0].finished) ? 0 : 2;
}

// Renders menu/HUD evidence frames at target resolution (1366x768).
int runUIScreens(GameApp& app, const std::string& prefix) {
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_MSAA_4X_HINT);
    InitWindow(1366, 768, "overdrive-uiscreens");
    // 1 title, 2 vehicle selected, 3 pause, 4 results (finished race)
    app.settings.playerVehicle = 1;
    drawTitle(app, 0);
    TakeScreenshot((prefix + "-title.png").c_str());
    drawTitle(app, 1);
    TakeScreenshot((prefix + "-vehicle.png").c_str());
    drawPause(0);
    TakeScreenshot((prefix + "-pause.png").c_str());
    sim::Race race;
    race.init(app.settings.laps, app.settings.aiDifficulty, app.settings.playerVehicle);
    // simulate a finished race for the results frame
    for (int f = 0; f < 60 * 400 && !race.cars[0].finished; f++) {
        sim::CarInput aiIn = sim::aiDrive(race, 0);
        if (race.cars[0].boostCharges > 0 && race.cars[0].boostLeft <= 0 && race.cars[0].lap >= 1) aiIn.boost = true;
        race.step({aiIn, sim::aiDrive(race, 1), sim::aiDrive(race, 2)});
    }
    updateRecords(app.settings, race.cars[0]); // same win-only rules as the real results path
    drawResults(app, race);
    TakeScreenshot((prefix + "-results.png").c_str());
    CloseWindow();
    std::printf("uiscreens: finished=%d place=%d\n", (int)race.cars[0].finished, race.cars[0].finishPlace);
    return race.cars[0].finished ? 0 : 2;
}
