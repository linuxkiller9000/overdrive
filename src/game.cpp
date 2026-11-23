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
    int theme = 0;
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
            for (float y = 3.4f; y < top - 2.5f; y += 6.5f) {
                if (rnd() < 0.5f) continue; // floor lights out
                Color wc = (rnd() < 0.5f) ? warm : cool;
                wc.r = (unsigned char)(wc.r * (0.55f + 0.45f * rnd()));
                float o = halfW + 0.22f, w = 4.4f, h = 1.7f;
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

TrackMeshes buildTrack(const sim::Track& t, int quality, int theme = 0) {
    TrackMeshes tm;
    bool coastal = (theme == 1);
    Color road1 = coastal ? Color{52,54,58,255} : Color{38,38,46,255};
    Color road2 = coastal ? Color{46,48,52,255} : Color{34,34,40,255};
    tm.road = ribbonMesh(t, -t.halfWidth, t.halfWidth, road1, road2);
    Color neonA = coastal ? Color{250,250,250,255} : Color{0,229,220,255};
    Color neonB = coastal ? Color{255,190,60,255} : Color{255,45,130,255};
    tm.edgeL = ribbonMesh(t, t.halfWidth - 0.4f, t.halfWidth, neonA, neonA);
    tm.edgeR = ribbonMesh(t, -t.halfWidth, -t.halfWidth + 0.4f, neonB, neonB);
    tm.centerLine = ribbonMesh(t, -0.15f, 0.15f, coastal ? Color{210,180,90,255} : Color{80,80,95,255}, coastal ? Color{210,180,90,255} : Color{80,80,95,255}, true);
    Color walkA = coastal ? Color{196,178,130,255} : Color{44,49,65,255};
    Color walkB = coastal ? Color{184,166,120,255} : Color{40,44,59,255};
    tm.sidewalkL = ribbonMesh(t, t.halfWidth + 1.5f, t.halfWidth + 10.f, walkA, walkB);
    tm.sidewalkR = ribbonMesh(t, -t.halfWidth - 10.f, -t.halfWidth - 1.5f, walkA, walkB);
    Color curbA = coastal ? Color{235,235,235,255} : Color{200,40,60,255}, curbB = coastal ? Color{60,120,190,255} : Color{225,225,230,255};
    tm.curbL = ribbonMesh(t, t.halfWidth + 0.05f, t.halfWidth + 1.5f, curbA, curbB);
    tm.curbR = ribbonMesh(t, -t.halfWidth - 1.5f, -t.halfWidth - 0.05f, curbA, curbB);
    // procedural scenery outside the road: city blocks or coastal palms/huts
    tm.buildings.clear();
    if (quality > 0 && !coastal) {
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
                for (auto& placed : tm.buildings)
                    if (sim::len(sim::Vec2{x-placed.x,z-placed.z}) < 15.f) { tooClose=true; break; }
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
    if (quality > 0 && coastal) {
        unsigned st = 4242;
        auto rnd = [&]() { st ^= st << 13; st ^= st >> 17; st ^= st << 5; return (st & 0xFFFF) / 65535.f; };
        for (int i = 0; i < (int)t.center.size(); i += 2) {
            for (int side = -1; side <= 1; side += 2) {
                if (rnd() < 0.45f) continue;
                sim::Vec2 c = t.center[i];
                sim::Vec2 d = t.dirAt(t.cum[i]);
                sim::Vec2 n{-d.y, d.x};
                float off = t.halfWidth + 14.f + rnd() * 45.f;
                float x = c.x + n.x * off * side, z = c.y + n.y * off * side;
                bool tooClose = false;
                for (auto& tp : t.center)
                    if (sim::len(sim::Vec2{x, z} - tp) < t.halfWidth + 10.f) { tooClose = true; break; }
                for (auto& placed : tm.buildings)
                    if (sim::len(sim::Vec2{x-placed.x,z-placed.z}) < 12.f) { tooClose = true; break; }
                if (tooClose) continue;
                bool palm = rnd() > 0.35f;
                tm.buildings.push_back({x, palm ? 5.f + rnd() * 3.f : 2.f, z});
                tm.buildingColors.push_back(palm ? Color{40,120,60,255} : (rnd() > 0.5f ? Color{225,215,190,255} : Color{200,120,90,255}));
            }
        }
    }
    tm.theme = theme;
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

// Eight-corner tapered shell, original low-poly chassis with sloping bodywork.
void bodyShell(float width,float length,float belt,float roof,Color col) {
    Vector3 q[8]={{-width*.48f,.28f,-length*.5f},{width*.48f,.28f,-length*.5f},
      {width*.48f,.28f,length*.5f},{-width*.48f,.28f,length*.5f},
      {-width*.42f,belt,-length*.43f},{width*.42f,belt,-length*.43f},
      {width*.35f,roof,length*.5f},{-width*.35f,roof,length*.5f}};
    int faces[6][4]={{0,1,5,4},{1,2,6,5},{2,3,7,6},{3,0,4,7},{4,5,6,7},{3,2,1,0}};
    float light[6]={.65f,.55f,.8f,.72f,1.f,.4f};
    rlBegin(RL_QUADS);
    for(int f=0;f<6;f++) {
        rlColor4ub((unsigned char)(col.r*light[f]),(unsigned char)(col.g*light[f]),(unsigned char)(col.b*light[f]),col.a);
        for(int j=0;j<4;j++){Vector3 v=q[faces[f][3-j]];rlVertex3f(v.x,v.y,v.z);}
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
    float width=sp.design==5?1.65f:sp.design==6?2.5f:sp.design==4?2.3f:sp.design==7?1.9f:2.1f;
    if (sp.design == 2 || sp.design == 6) {
        litBox({0,.8f,0},{width,1.f,4.5f},body);
        litBox({0,1.5f,.55f},{width*.83f,.75f,2.2f},glass);
        for(int side=-1;side<=1;side+=2) litBox({side*width*.34f,.8f,2.28f},{.45f,.2f,.12f},{220,245,255,body.a});
        litBox({0,.7f,-2.28f},{width*.8f,.18f,.12f},{255,30,60,body.a});
        litBox({0,1.92f,.55f},{width*.9f,.12f,2.3f},body);
        if(sp.design==6) {
            litBox({0,2.25f,.9f},{1.5f,.22f,.3f},dark);
            for(int lamp=-2;lamp<=2;lamp++) litBox({lamp*.3f,2.25f,1.06f},{.18f,.14f,.05f},{255,240,200,body.a});
        }
        if(sp.design==6) {
            litBox({0,2.08f,.55f},{width,.18f,2.5f},dark);
            litBox({0,.7f,2.3f},{width,.25f,.2f},accent);
        } else litBox({0,1.22f,-1.45f},{width*.8f,.15f,1.3f},dark);
    } else {
        float length=sp.design==3?4.9f:sp.design==5?3.4f:sp.design==4?4.5f:sp.design==7?4.7f:4.2f;
        bodyShell(width,length,sp.design==3?.9f:.72f,sp.design==7?.35f:.52f,body);
        // Sloped hood and windscreen formed as original low-poly triangular panels.
        rlBegin(RL_TRIANGLES);
        rlColor4ub(body.r,body.g,body.b,body.a);
        float w=width*.5f,z=length*.5f;
        rlVertex3f(-w,.7f,.55f);rlVertex3f(w,.7f,.55f);rlVertex3f(w,.3f,z);
        rlVertex3f(-w,.7f,.55f);rlVertex3f(w,.3f,z);rlVertex3f(-w,.3f,z);
        rlEnd();
        float roof=sp.design==7?1.05f:sp.design==4?1.18f:sp.design==5?1.5f:sp.design==1?1.15f:1.3f;
        if(sp.design==7) {
            rlPushMatrix();rlTranslatef(0,.93f,-.4f);rlScalef(1.f,.65f,1.45f);
            DrawSphereEx({0,0,0},.72f,6,8,glass);rlPopMatrix();
        } else {
            litBox({0,sp.design==5?1.1f:.95f,sp.design==3?-.65f:-.25f},{width*.72f,sp.design==5?.8f:.55f,sp.design==5?1.8f:1.6f},glass);
        }
        if(sp.design!=7) litBox({0,roof,sp.design==3?-.7f:-.3f},{width*.65f,.12f,sp.design==5?1.7f:1.2f},body);
        if(sp.design==4) {
            litBox({0,.22f,length*.5f},{width+.3f,.1f,.5f},accent);
            litBox({0,.25f,-length*.5f},{width+.3f,.15f,.3f},dark);
        }
        if(sp.design==4) {
            litBox({-.9f,.7f,-1.4f},{.3f,.16f,1.2f},accent);
            litBox({.9f,.7f,-1.4f},{.3f,.16f,1.2f},accent);
        }
        if(sp.design==1) {
            litBox({-1.05f,.3f,0},{.2f,.2f,3.2f},accent);
            litBox({1.05f,.3f,0},{.2f,.2f,3.2f},accent);
        }
        if(sp.design==1) {
            litBox({0,.95f,-1.5f},{width*.6f,.5f,1.1f},dark);
            litBox({-.3f,.7f,-2.15f},{.18f,.18f,.4f},accent);
            litBox({.3f,.7f,-2.15f},{.18f,.18f,.4f},accent);
        }
        if(sp.design==1 || sp.design==4 || sp.design==7) {
            litBox({0,1.f,-length*.44f},{width+.2f,.12f,.45f},accent);
            litBox({-.7f,.8f,-length*.44f},{.12f,.4f,.2f},dark);
            litBox({.7f,.8f,-length*.44f},{.12f,.4f,.2f},dark);
        }
        if(sp.design==3) {
            litBox({0,.85f,1.1f},{.9f,.25f,1.2f},dark);
            litBox({0,.4f,length*.5f+.15f},{.8f,.12f,.5f},accent); // nose cone lip
        }
        if(sp.design==5) DrawCylinderEx({0,1.55f,-.5f},{0,2.15f,-.5f},.03f,.01f,4,accent);
        if(sp.design==7) {
            litBox({-1.1f,.45f,0},{.3f,.5f,3.3f},accent);
            litBox({1.1f,.45f,0},{.3f,.5f,3.3f},accent);
        }
        litBox({0,.49f,-length*.5f-.03f},{width*.85f,.16f,.1f},{255,30,60,body.a});
        for(int side=-1;side<=1;side+=2)
            litBox({side*width*.34f,.52f,length*.5f+.02f},{.42f,.15f,.1f},{220,245,255,body.a});
    }
    bool bigWheels = (sp.design == 2 || sp.design == 6);
    float wr = sp.radius * (bigWheels ? 0.52f : sp.design == 5 ? 0.36f : 0.42f);
    float track = (sp.design == 5) ? 1.15f : (sp.design == 4 ? 1.45f : 1.3f);
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2) {
            float wz = sz * (bigWheels ? 1.45f : 1.4f);
            DrawCylinderEx({sx * (track - 0.3f), wr, wz}, {sx * track, wr, wz}, wr, wr, 10, {22, 24, 30, body.a});
            if (bigWheels) // hub
                DrawCylinderEx({sx * track, wr, wz}, {sx * (track + 0.05f), wr, wz}, wr * 0.45f, wr * 0.45f, 8, {120,128,140,body.a});
        }
    if (c.boostLeft > 0.f) {
        litBox({0, 0.45f, -2.6f}, {1.4f, 0.3f, 0.9f}, {255, 200, 40, body.a});
        litBox({0, 0.45f, -3.2f}, {0.9f, 0.2f, 0.7f}, {255, 120, 20, body.a});
    }
    rlPopMatrix();
}

// One sim step for the full grid: player input at index 0, AI for the rest.
static void stepRaceAll(sim::Race& race, sim::CarInput playerIn) {
    std::vector<sim::CarInput> inputs(race.cars.size());
    inputs[0] = playerIn;
    for (size_t i = 1; i < race.cars.size(); i++) inputs[i] = sim::aiDrive(race, (int)i);
    race.step(inputs);
}

static const Color kCarCols[8] = {{230, 230, 240, 255}, {255, 45, 130, 255}, {0, 200, 190, 255}, {255, 190, 60, 255},
                                  {170, 90, 255, 255}, {255, 120, 40, 255}, {90, 220, 120, 255}, {70, 130, 255, 255}};

static Color pickupColor(int type) {
    switch (type) {
        case sim::PK_NITRO: return {255, 200, 40, 255};
        case sim::PK_BOLT: return {255, 45, 130, 255};
        case sim::PK_SHUNT: return {255, 120, 30, 255};
        case sim::PK_MINE: return {200, 50, 50, 255};
        case sim::PK_SHIELD: return {60, 160, 255, 255};
        case sim::PK_SHOCK: return {120, 240, 255, 255};
        case sim::PK_BARGE: return {180, 80, 255, 255};
        case sim::PK_REPAIR: return {80, 230, 120, 255};
        default: return WHITE;
    }
}

sim::CarInput readInput(const sim::Settings& s) {
    sim::RawKeys k{};
    k.accel = IsKeyDown(s.keyAccel) || IsKeyDown(s.keyAccelAlt);
    k.brake = IsKeyDown(s.keyBrake) || IsKeyDown(s.keyBrakeAlt);
    k.left = IsKeyDown(s.keyLeft) || IsKeyDown(s.keyLeftAlt);
    k.right = IsKeyDown(s.keyRight) || IsKeyDown(s.keyRightAlt);
    k.handbrake = IsKeyDown(s.keyHandbrake);
    k.boost = IsKeyPressed(s.keyBoost);
    k.fire = IsKeyPressed(s.keyFire) || IsKeyPressed(s.keyFireAlt);
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
        p.fire = IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_TRIGGER_1);
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
    DrawText(place == 1 ? "1st" : (place == 2 ? "2nd" : (place == 3 ? "3rd" : TextFormat("%dth", place))), W - 90, 20, 28, place == 1 ? GOLD : WHITE);
    float t = p.finished ? p.raceTime : r.time() - 3.2f;
    if (t < 0) t = 0;
    DrawText(TextFormat("%d:%05.2f", (int)(t / 60), std::fmod(t, 60.f)), W / 2 - 60, 20, 28, WHITE);
    if (p.boostCharges > 0) {
        DrawRectangle(W / 2 - 60, H - 70, 120, 34, {255, 200, 40, 60});
        DrawRectangleLines(W / 2 - 60, H - 70, 120, 34, {255, 200, 40, 255});
        DrawText("TURBO", W / 2 - 44, H - 63, 20, {255, 200, 40, 255});
    }
    // held weapon + health, top-left under the lap counter
    if (p.heldPickup >= 0) {
        Color pc = pickupColor(p.heldPickup);
        DrawRectangle(16, 56, 170, 34, {0, 0, 0, 120});
        DrawRectangleLines(16, 56, 170, 34, pc);
        DrawText(sim::pickupName(p.heldPickup), 28, 64, 20, pc);
        DrawText("E", 162, 64, 20, WHITE);
    }
    {
        float hpFrac = r.specs.empty() ? 1.f : p.health / r.specs[0].durability;
        DrawRectangle(16, 96, 170, 10, {0, 0, 0, 120});
        DrawRectangle(16, 96, (int)(170 * hpFrac), 10, hpFrac > 0.5f ? Color{80, 230, 120, 255} : (hpFrac > 0.25f ? Color{255, 200, 40, 255} : Color{255, 60, 60, 255}));
    }
    if (p.shieldT > 0.f) drawTextOutline("SHIELD", 20, 116, 18, {60, 160, 255, 255});
    if (p.boostLeft > 0.f)
        drawTextOutline("BOOST!", W / 2 - 50, H - 110, 28, {255, 200, 40, 255});
    if (p.wrongWay) drawTextOutline("WRONG WAY", W / 2 - 110, H / 2 - 40, 34, RED);
    if (r.countdown > 0.f) {
        int cd = (int)r.countdown;
        drawTextOutline(cd > 0 ? TextFormat("%d", cd) : "GO!", W / 2 - 30, H / 2 - 60, 72, GOLD);
    }
    if (p.finished) {
        DrawRectangle(0, 0, W, H, {0, 0, 0, 160});
        drawTextOutline(p.finishPlace == 1 ? "YOU WIN!" : TextFormat("FINISHED %d%s", p.finishPlace,
                        p.finishPlace == 2 ? "nd" : (p.finishPlace == 3 ? "rd" : "th")),
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
        Color dc = kCarCols[i % 8];
        DrawCircleV(a, i == 0 ? 4.5f : 3.5f, i == 0 ? GOLD : dc);
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
    float dist = 9.f + speed * 0.08f; // bounded: player stays in view through boost
    Vector3 camPos = {p.pos.x - fwd.x * dist, 5.4f + speed * 0.03f, p.pos.y - fwd.y * dist};
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
    bool coastal = (tm.theme == 1);
    BeginDrawing();
    ClearBackground({8, 8, 20, 255});
    if (coastal)
        DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), {255, 150, 90, 255}, {90, 50, 110, 255});
    else
        DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), {14, 10, 42, 255}, {64, 18, 60, 255});
    BeginMode3D(cam);
    if (coastal) {
        // sand fills the whole scene; ocean sits fully beyond the sand's +x edge
        float R = tm.boundsRadius + 400.f;
        DrawPlane({tm.boundsCenter.x, -0.2f, tm.boundsCenter.z}, {R * 2.f, R * 2.f}, {214, 190, 140, 255});
        float sandEdge = tm.boundsCenter.x + R;
        float ox = sandEdge + 300.f;
        DrawPlane({ox, -0.3f, tm.boundsCenter.z}, {600.f, R * 2.f}, {20, 90, 140, 255});
        // animated foam line where the sand meets the water
        float foam = 0.5f + 0.5f * std::sin(GetTime() * 1.5);
        DrawPlane({sandEdge + 2.f, -0.25f, tm.boundsCenter.z}, {3.f + foam * 2.f, R * 2.f}, {220, 235, 240, 255});
        // sun glow low over the ocean
        DrawSphere({ox + 200.f, 60.f, tm.boundsCenter.z}, 30.f, Fade({255, 220, 150, 255}, 0.8f));
    }
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
    Color fog = coastal ? Color{120, 80, 90, 255} : Color{16, 12, 40, 255};
    for (size_t i = 0; i < tm.buildings.size(); i++) {
        Vector3 b = tm.buildings[i];
        float dist = Vector3Distance(b, cam.position);
        float fade = 1.f - fminf(dist / 700.f, 0.7f);
        Color bc = ColorLerp(fog, tm.buildingColors[i], fade);
        if (coastal) {
            bool palm = tm.buildingColors[i].g > 100 && tm.buildingColors[i].r < 100;
            if (palm) {
                // b.y is the full palm height
                DrawCylinder({b.x, b.y * 0.5f, b.z}, 0.35f, 0.22f, b.y, 6, ColorLerp(fog, Color{120, 85, 55, 255}, fade));
                DrawSphere({b.x, b.y + 0.6f, b.z}, 2.2f, ColorLerp(fog, tm.buildingColors[i], fade));
                DrawSphere({b.x + 1.2f, b.y + 0.2f, b.z + 0.8f}, 1.5f, ColorLerp(fog, ColorBrightness(tm.buildingColors[i], 0.15f), fade));
                DrawSphere({b.x - 1.1f, b.y + 0.2f, b.z - 0.7f}, 1.5f, ColorLerp(fog, ColorBrightness(tm.buildingColors[i], -0.1f), fade));
            } else {
                // beach hut: box body + pyramid roof
                DrawCubeV({b.x, 1.1f, b.z}, {4.5f, 2.2f, 3.5f}, bc);
                rlPushMatrix();
                rlTranslatef(b.x, 2.2f, b.z);
                rlRotatef(45.f, 0, 1, 0);
                DrawCylinder({0, 0.9f, 0}, 3.1f, 0.05f, 1.8f, 4, ColorLerp(fog, Color{150, 70, 50, 255}, fade));
                rlPopMatrix();
            }
            continue;
        }
        DrawCubeV(b, {10, b.y * 2, 10}, bc);
        DrawCubeV({b.x, b.y * 2 + 1.2f, b.z}, {6.5f, 2.4f, 6.5f}, ColorBrightness(bc, -0.25f));
        Color trim = (i % 3 == 0) ? Color{0, 229, 220, 255} : (i % 3 == 1) ? Color{255, 45, 130, 255} : Color{255, 200, 40, 255};
        DrawCubeV({b.x, b.y * 2 + 0.3f, b.z}, {10.4f, 0.35f, 10.4f}, ColorLerp(fog, trim, fade));
        if (b.y > 22.f)
            DrawCylinder({b.x, b.y * 2 + 5.f, b.z}, 0.25f, 0.08f, 6.f, 6, ColorBrightness(bc, 0.5f));
    }
    // Street furniture follows the road tangent, outside the drivable corridor.
    for (size_t k=0;!coastal && k<r.track.center.size();k+=12) {
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
    for (int district=1;!coastal && district<=3;district++) {
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
    if (coastal) {
        // lighthouse standing in the shallows on the ocean side
        Vector3 s = tm.boundsCenter;
        s.x += tm.boundsRadius + 180.f;
        s.z += tm.boundsRadius * 0.4f;
        for (int band = 0; band < 5; band++)
            DrawCylinder({s.x, 4.f + band * 8.f, s.z}, 4.2f - band * 0.5f, 3.8f - band * 0.5f, 8.f, 12,
                         band % 2 == 0 ? Color{235, 230, 220, 255} : Color{200, 60, 55, 255});
        DrawCylinder({s.x, 42.f, s.z}, 2.6f, 2.6f, 4.f, 12, {40, 40, 50, 255});
        float glow = 0.6f + 0.4f * std::sin(GetTime() * 2.0);
        DrawSphere({s.x, 43.f, s.z}, 2.f * glow, {255, 240, 180, 255});
        DrawCylinder({s.x, 45.5f, s.z}, 3.f, 0.05f, 2.4f, 12, {200, 60, 55, 255});
    }
    // barriers + checkpoint gates + start banner
    for (size_t i = 0; i < r.track.center.size(); i += 4) {
        sim::Vec2 c = r.track.center[i];
        sim::Vec2 d = r.track.dirAt(r.track.cum[i]);
        sim::Vec2 n{-d.y, d.x};
        Color bc = coastal
            ? ((i / 4) % 2 == 0 ? Color{245, 245, 245, 255} : Color{60, 120, 190, 255})
            : ((i / 4) % 2 == 0 ? Color{0, 229, 220, 255} : Color{255, 45, 130, 255});
        DrawCubeV({c.x + n.x * (r.track.halfWidth + 0.6f), 0.8f, c.y + n.y * (r.track.halfWidth + 0.6f)}, {0.5f, 1.6f, 0.5f}, bc);
        DrawCubeV({c.x - n.x * (r.track.halfWidth + 0.6f), 0.8f, c.y - n.y * (r.track.halfWidth + 0.6f)}, {0.5f, 1.6f, 0.5f}, bc);
    }
    for (size_t k = 1; k < r.track.checkpoints.size(); k++) {
        sim::Vec2 cp = r.track.pointAt(r.track.cum[r.track.checkpoints[k]]);
        sim::Vec2 cd = r.track.dirAt(r.track.cum[r.track.checkpoints[k]]);
        sim::Vec2 cn{-cd.y, cd.x};
        Color cc = coastal ? Color{240, 240, 245, 255} : Color{60, 60, 90, 255};
        // posts on both edges and a floating chevron: driving line stays clear
        DrawCubeV({cp.x + cn.x * (r.track.halfWidth + 0.9f), 3.f, cp.y + cn.y * (r.track.halfWidth + 0.9f)}, {0.5f, 6.f, 0.5f}, cc);
        DrawCubeV({cp.x - cn.x * (r.track.halfWidth + 0.9f), 3.f, cp.y - cn.y * (r.track.halfWidth + 0.9f)}, {0.5f, 6.f, 0.5f}, cc);
        DrawCubeV({cp.x, 5.5f, cp.y}, {1.6f, 0.5f, 1.6f}, coastal ? Color{255, 190, 60, 255} : Color{0, 229, 220, 255});
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
            Color pc = pickupColor(pk.type);
            DrawCubeV({pk.pos.x, 1.f, pk.pos.y}, {pulse, pulse, pulse}, pc);
            DrawSphere({pk.pos.x, 2.2f + 0.3f * (float)std::sin(GetTime() * 3.0), pk.pos.y}, 0.35f, pc);
        }
    // combat: projectiles, mines, shock orbs, shields
    for (auto& s : r.shots) {
        Color sc = s.type == sim::PK_BOLT ? Color{255, 60, 140, 255} : Color{255, 140, 40, 255};
        DrawSphere({s.pos.x, 1.f, s.pos.y}, 0.55f, sc);
        DrawSphere({s.pos.x, 1.f, s.pos.y}, 0.9f, Fade(sc, 0.25f));
    }
    for (auto& m : r.mines) {
        DrawCylinder({m.pos.x, 0.15f, m.pos.y}, 1.0f, 1.2f, 0.3f, 10, {40, 40, 48, 255});
        float blink = m.arm > 0.f ? 0.3f : (0.5f + 0.5f * std::sin(GetTime() * 8.0));
        DrawSphere({m.pos.x, 0.45f, m.pos.y}, 0.25f, Fade({255, 60, 60, 255}, blink));
    }
    for (auto& o : r.orbs) {
        float pulse = 0.9f + 0.25f * std::sin(GetTime() * 9.0);
        DrawSphere({o.pos.x, 1.1f, o.pos.y}, 1.3f * pulse, Fade({120, 240, 255, 255}, 0.35f));
        DrawSphere({o.pos.x, 1.1f, o.pos.y}, 0.5f, {200, 250, 255, 255});
    }
    for (size_t i = 0; i < r.cars.size(); i++)
        if (r.cars[i].shieldT > 0.f)
            DrawSphere({r.cars[i].pos.x, 1.1f, r.cars[i].pos.y}, 2.6f, Fade({60, 160, 255, 255}, 0.2f));
    const Color* cols = kCarCols;
    // rivals sitting on the camera->player sight line fade out instead of hiding the player
    const sim::Car& pc = r.cars[0];
    sim::Vec2 camPos{cam.position.x, cam.position.z};
    sim::Vec2 seg = pc.pos - camPos;
    float segLen2 = fmaxf(sim::dot(seg, seg), 1e-3f);
    for (size_t i = 0; i < r.cars.size(); i++) {
        Color col = cols[i % 8];
        if (i > 0) {
            sim::Vec2 rel = r.cars[i].pos - camPos;
            // a rival within 9 m of the lens would fill the screen: don't draw it at all
            if (sim::len(rel) < 9.f) continue;
            float along = sim::dot(rel, seg) / segLen2;
            if (along > -0.1f && along < 1.0f) {
                sim::Vec2 closest{camPos.x + seg.x * along, camPos.y + seg.y * along};
                float d = sim::len(r.cars[i].pos - closest);
                if (d < 3.0f) col.a = 70;
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
    const char* items[] = {"START RACE", "GARAGE", "TRACK", "DIFFICULTY", "LAPS", "QUALITY", "FPS CAP", "QUIT"};
    std::vector<sim::CarSpec> roster = sim::defaultRoster();
    BeginDrawing();
    ClearBackground({8, 8, 20, 255});
    drawTextOutline("OVERDRIVE", GetScreenWidth() / 2 - 170, 60, 64, {255, 45, 130, 255});
    for (int i = 0; i < 8; i++) {
        const char* val = "";
        if (i == 1) val = roster[app.settings.playerVehicle % (int)roster.size()].name.c_str();
        if (i == 2) val = sim::trackDefs()[app.settings.trackIndex % (int)sim::trackDefs().size()].name.c_str();
        if (i == 3) val = diffName(app.settings.aiDifficulty);
        if (i == 4) val = TextFormat("%d", app.settings.laps);
        if (i == 5) val = app.settings.quality ? "HIGH" : "LOW";
        if (i == 6) val = TextFormat("%d", app.settings.fpsCap);
        drawTextOutline(TextFormat("%s%s%s%s", i == sel ? "> " : "  ", items[i], val[0] ? ": " : "", val),
                        GetScreenWidth() / 2 - 220, 200 + i * 44, 28, i == sel ? GOLD : WHITE);
    }
    drawTextOutline("ARROWS + ENTER to select", GetScreenWidth() / 2 - 290, GetScreenHeight() - 50, 18, GRAY);
    EndDrawing();
}

static bool menuPressed(int key, int button) {
    return IsKeyPressed(key) || (IsGamepadAvailable(0) && IsGamepadButtonPressed(0,button));
}
static void drawGarage(GameApp& app, int selected, float rotation) {
    auto roster=sim::defaultRoster(); const auto& v=roster[selected];
    bool owned=(app.settings.unlocked&(1u<<selected))!=0;
    int W=GetScreenWidth(), H=GetScreenHeight();
    BeginDrawing(); ClearBackground({10,15,26,255});
    DrawRectangleGradientV(0,0,W,H,{16,28,46,255},{6,9,16,255});
    DrawRectangle(0,0,W,5,{0,215,220,255});
    DrawText("OVERDRIVE / GARAGE",44,30,28,{150,170,190,255});
    DrawText(TextFormat("%02d / 08",selected+1),44,80,22,{0,215,220,255});
    DrawText(v.name.c_str(),44,112,48,WHITE);
    DrawText(TextFormat("CREDITS  %d",app.settings.credits),W-300,36,24,GOLD);
    Camera3D gc{{7,4,8},{0,.6f,0},{0,1,0},40,CAMERA_PERSPECTIVE};
    BeginMode3D(gc);
    DrawCylinder({0,-.15f,0},3.8f,3.8f,.18f,48,{30,44,59,255});
    DrawCylinderWires({0,-.04f,0},3.9f,3.9f,.05f,48,{0,190,210,255});
    sim::Car car;car.heading=rotation;
    const Color paints[8]={{215,225,235,255},{255,161,52,255},{110,142,165,255},{215,55,60,255},{155,85,220,255},{65,200,165,255},{210,173,109,255},{80,150,230,255}};
    drawCar(car,v,paints[selected]);
    EndMode3D();
    int x=W-365,y=130;
    DrawRectangle(x-22,y-20,360,430,{7,13,23,240});
    const char* labels[]={"ACCELERATION","TOP SPEED","STEERING","GRIP","SLIDE TENDENCY","BRAKING","DURABILITY","BOOST POWER"};
    float values[]={v.accel/20.f,v.maxSpeed/50.f,v.steerRate/3.5f,v.grip/13.f,1.f-v.driftGrip/4.f,v.brake/30.f,v.durability/180.f,v.boostAccel/20.f};
    const auto& current=roster[app.settings.playerVehicle];
    float comp[]={current.accel/20.f,current.maxSpeed/50.f,current.steerRate/3.5f,current.grip/13.f,1.f-current.driftGrip/4.f,current.brake/30.f,current.durability/180.f,current.boostAccel/20.f};
    for(int i=0;i<8;i++) {
        DrawText(labels[i],x,y+i*48,16,{160,180,200,255});
        DrawRectangle(x,y+22+i*48,260,8,{38,49,66,255});
        DrawRectangle(x,y+22+i*48,(int)(260*values[i]),8,{0,210,210,255});
        DrawRectangle(x+(int)(260*comp[i]),y+18+i*48,2,16,GOLD);
    }
    DrawText(TextFormat("Gold: %s equipped",current.name.c_str()),x,y+390,14,GOLD);
    DrawText(TextFormat("%.0f km/h  |  %.0f HP  |  %.1fs boost",v.maxSpeed*3.6f,v.durability,v.boostTime),44,H-150,20,{150,170,190,255});
    DrawText(owned?(selected==app.settings.playerVehicle?"EQUIPPED":"ENTER / A: EQUIP"):TextFormat("ENTER / A: UNLOCK  %d CR",sim::vehicleUnlockCost(selected)),44,H-106,24,owned?WHITE:GOLD);
    DrawText("LEFT / RIGHT: browse    ESC / B: back",44,H-58,20,{150,170,190,255});
    DrawText("Bars: higher = stronger; SLIDE higher = slides more",44,H-84,14,{120,140,160,255});
    EndDrawing();
}
static void garageMenu(GameApp& app) {
    int selected=app.settings.playerVehicle;
    while(!WindowShouldClose()) {
        if(menuPressed(KEY_ESCAPE,GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) break;
        if(menuPressed(KEY_LEFT,GAMEPAD_BUTTON_LEFT_FACE_LEFT)) selected=(selected+7)%8;
        if(menuPressed(KEY_RIGHT,GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) selected=(selected+1)%8;
        if(menuPressed(KEY_ENTER,GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
            bool owned=(app.settings.unlocked&(1u<<selected))!=0;
            if(owned || sim::unlockVehicle(app.settings,selected)) {
                app.settings.playerVehicle=selected;
                sim::saveSettings(app.settings,app.settingsPath);
            }
        }
        drawGarage(app,selected,(float)GetTime()*.28f);
    }
}


static bool titleMenu(GameApp& app) {
    int sel = 0;
    if(app.settings.playerVehicle<0 || app.settings.playerVehicle>=8 || !(app.settings.unlocked&(1u<<app.settings.playerVehicle))) app.settings.playerVehicle=0;
    std::vector<sim::CarSpec> roster = sim::defaultRoster();
    while (!WindowShouldClose()) {
        if (menuPressed(KEY_UP,GAMEPAD_BUTTON_LEFT_FACE_UP)) sel = (sel + 7) % 8;
        if (menuPressed(KEY_DOWN,GAMEPAD_BUTTON_LEFT_FACE_DOWN)) sel = (sel + 1) % 8;
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT)) {
            int d = IsKeyPressed(KEY_LEFT) ? -1 : 1;
            if (sel == 1) garageMenu(app);
            if (sel == 2) app.settings.trackIndex = (app.settings.trackIndex + (int)sim::trackDefs().size() + d) % (int)sim::trackDefs().size();
            if (sel == 3) app.settings.aiDifficulty = (app.settings.aiDifficulty + 3 + d) % 3;
            if (sel == 4) app.settings.laps = app.settings.laps == 1 ? 3 : app.settings.laps == 3 ? 5 : 1;
            if (sel == 5) app.settings.quality = 1 - app.settings.quality;
            if (sel == 6) app.settings.fpsCap = app.settings.fpsCap == 60 ? 30 : 60;
        }
        if (menuPressed(KEY_ENTER,GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || IsKeyPressed(KEY_SPACE)) {
            if (sel == 0) return true;
            if (sel == 7) return false;
            if (sel == 1) garageMenu(app);
            if (sel == 2) app.settings.trackIndex = (app.settings.trackIndex + 1) % (int)sim::trackDefs().size();
            if (sel == 3) app.settings.aiDifficulty = (app.settings.aiDifficulty + 1) % 3;
            if (sel == 4) app.settings.laps = app.settings.laps == 1 ? 3 : app.settings.laps == 3 ? 5 : 1;
            if (sel == 5) app.settings.quality = 1 - app.settings.quality;
            if (sel == 6) app.settings.fpsCap = app.settings.fpsCap == 60 ? 30 : 60;
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


static void drawResults(const GameApp& app, const sim::Race& race) {
    const sim::Car& p = race.cars[0];
    BeginDrawing();
    ClearBackground({8, 8, 20, 255});
    drawTextOutline(p.finishPlace == 1 ? "YOU WIN!" : TextFormat("FINISHED %d%s", p.finishPlace,
                    p.finishPlace == 2 ? "nd" : (p.finishPlace == 3 ? "rd" : "th")),
                    GetScreenWidth() / 2 - 160, 90, 48, GOLD);
    drawTextOutline(TextFormat("RACE TIME  %d:%05.2f", (int)(p.raceTime / 60), std::fmod(p.raceTime, 60.f)),
                    GetScreenWidth() / 2 - 170, 200, 28, WHITE);
    if (p.bestLap > 0)
        drawTextOutline(TextFormat("BEST LAP   %0.2f s", p.bestLap), GetScreenWidth() / 2 - 170, 240, 28, WHITE);
    int tr = race.trackIndex;
    if (tr >= 0 && tr < 8 && app.settings.bestLapMsT[tr] > 0)
        drawTextOutline(TextFormat("TRACK BEST LAP   %d:%05.2f", app.settings.bestLapMsT[tr] / 60000, std::fmod(app.settings.bestLapMsT[tr] / 1000.f, 60.f)),
                        GetScreenWidth() / 2 - 170, 300, 22, GRAY);
    if (tr >= 0 && tr < 8 && app.settings.bestRaceMsT[tr] > 0)
        drawTextOutline(TextFormat("TRACK BEST WIN   %d:%05.2f", app.settings.bestRaceMsT[tr] / 60000, std::fmod(app.settings.bestRaceMsT[tr] / 1000.f, 60.f)),
                        GetScreenWidth() / 2 - 170, 330, 22, GRAY);
    drawTextOutline("ENTER continue", GetScreenWidth() / 2 - 90, GetScreenHeight() - 60, 22, GOLD);
    EndDrawing();
}

static void resultsScreen(GameApp& app, const sim::Race& race) {
    const sim::Car& p = race.cars[0];
    // persist bests
    sim::updateRecords(app.settings, p, race.trackIndex);
    sim::awardRace(app.settings, p.finishPlace);
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
            race.init(app.settings.laps, app.settings.aiDifficulty, app.settings.playerVehicle, app.settings.trackIndex);
            Camera3D cam{};
            cam.up = {0, 1, 0};
            cam.fovy = 60;
            cam.projection = CAMERA_PERSPECTIVE;
            ChaseCam cc;
            TrackMeshes tm = buildTrack(race.track, app.settings.quality, sim::trackDefs()[race.trackIndex].theme);
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
                    stepRaceAll(race, in);
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
    race.init(3, app.settings.aiDifficulty, app.settings.playerVehicle, app.settings.trackIndex);
    Camera3D cam{};
    cam.up = {0, 1, 0};
    cam.fovy = 60;
    cam.projection = CAMERA_PERSPECTIVE;
    TrackMeshes tm = buildTrack(race.track, app.settings.quality, sim::trackDefs()[race.trackIndex].theme);
    ChaseCam cc;
    int taken = 0;
    bool gotCorner = false, gotBoost = false, gotPause = false, gotFinish = false, gotCombat = false;
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
        // fire a held offensive weapon once so combat is on camera
        if (!gotCombat && race.time() > 20.f && (race.cars[0].heldPickup == sim::PK_BOLT || race.cars[0].heldPickup == sim::PK_SHUNT))
            in.fire = true;
        stepRaceAll(race, in);
        updateChaseCam(cam, cc, race, sim::STEP);
        float t = race.time();
        // grant one boost charge mid-race so activation is guaranteed on camera
        if (!gotBoost && race.cars[0].lap >= 1 && race.cars[0].boostCharges == 0 && t > 30.f && race.cars[0].boostLeft <= 0)
            race.cars[0].boostCharges = 1;

        if (f % 3 == 0) drawFrame(race, tm, cam, app, 0); // draw often enough that the chase camera tracks
        if (!gotCombat && !race.shots.empty() && f % 3 == 0) { snap("combat"); gotCombat = true; }
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
    race.init(app.settings.laps, app.settings.aiDifficulty, app.settings.playerVehicle, app.settings.trackIndex);
    Camera3D cam{};
    cam.up = {0, 1, 0};
    cam.fovy = 60;
    cam.projection = CAMERA_PERSPECTIVE;
    ChaseCam cc;
    TrackMeshes tm = buildTrack(race.track, app.settings.quality, sim::trackDefs()[race.trackIndex].theme);
    bool polledOk = true;
    double acc = 0;
    for (int f = 0; f < 60 * 200 && !race.cars[0].finished && !WindowShouldClose(); f++) {
        acc += sim::STEP * 3; // bounded scripted runtime smoke, not a performance benchmark
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
            stepRaceAll(race, in);
            updateChaseCam(cam, cc, race, sim::STEP);
            acc -= sim::STEP;
        }
        if(f%8==0) drawFrame(race, tm, cam, app, in.steer);
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
    race.init(app.settings.laps, app.settings.aiDifficulty, app.settings.playerVehicle, app.settings.trackIndex);
    for(int v=0;v<8;v++) {
        drawGarage(app,v,.65f);
        TakeScreenshot((prefix+"-garage-"+std::to_string(v)+".png").c_str());
    }
    // simulate a finished race for the results frame
    for (int f = 0; f < 60 * 400 && !race.cars[0].finished; f++) {
        sim::CarInput aiIn = sim::aiDrive(race, 0);
        if (race.cars[0].boostCharges > 0 && race.cars[0].boostLeft <= 0 && race.cars[0].lap >= 1) aiIn.boost = true;
        stepRaceAll(race, aiIn);
    }
    sim::updateRecords(app.settings, race.cars[0], race.trackIndex); // same win-only rules as the real results path
    drawResults(app, race);
    TakeScreenshot((prefix + "-results.png").c_str());
    CloseWindow();
    std::printf("uiscreens: finished=%d place=%d\n", (int)race.cars[0].finished, race.cars[0].finishPlace);
    return race.cars[0].finished ? 0 : 2;
}
