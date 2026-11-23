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
    Model road, edgeL, edgeR;
    std::vector<Vector3> buildings;
    std::vector<Color> buildingColors;
    Vector3 boundsCenter{0, 0, 0};
    float boundsRadius = 400.f;
};

Model ribbonMesh(const sim::Track& t, float inner, float outer, Color c1, Color c2) {
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
    for (int i = 0; i < n; i++) {
        int a = 2 * i, b = 2 * i + 1, c = 2 * i + 2, d = 2 * i + 3;
        idx[6 * i + 0] = a; idx[6 * i + 1] = b; idx[6 * i + 2] = c;
        idx[6 * i + 3] = c; idx[6 * i + 4] = b; idx[6 * i + 5] = d;
    }
    Mesh m{};
    m.vertexCount = (int)verts.size();
    m.triangleCount = 2 * n;
    m.vertices = (float*)MemAlloc(verts.size() * sizeof(Vector3));
    m.colors = (unsigned char*)MemAlloc(colors.size() * sizeof(Color));
    m.indices = (unsigned short*)MemAlloc(idx.size() * sizeof(unsigned short));
    memcpy(m.vertices, verts.data(), verts.size() * sizeof(Vector3));
    memcpy(m.colors, colors.data(), colors.size() * sizeof(Color));
    memcpy(m.indices, idx.data(), idx.size() * sizeof(unsigned short));
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
                float off = t.halfWidth + 62.f + rnd() * 40.f;
                float x = c.x + n.x * off * side, z = c.y + n.y * off * side;
                // keep the camera corridor clear: reject if near ANY track point
                bool tooClose = false;
                for (auto& tp : t.center)
                    if (sim::len(sim::Vec2{x, z} - tp) < t.halfWidth + 55.f) { tooClose = true; break; }
                if (tooClose) continue;
                float h = 12.f + rnd() * 42.f;
                tm.buildings.push_back({x, h * 0.5f, z});
                Color bc{(unsigned char)(20 + rnd() * 30), (unsigned char)(25 + rnd() * 35), (unsigned char)(60 + rnd() * 60), 255};
                if (rnd() > 0.6f) bc = (rnd() > 0.5f) ? Color{10, 90, 90, 255} : Color{90, 15, 45, 255};
                tm.buildingColors.push_back(bc);
            }
        }
    }
    return tm;
}

void drawCar(const sim::Car& c, const sim::CarSpec& sp, Color body) {
    Matrix m = MatrixMultiply(MatrixRotateY(-c.heading), MatrixTranslate(c.pos.x, 0, c.pos.y));
    rlPushMatrix();
    rlMultMatrixf(MatrixToFloat(m));
    DrawCubeV({0, 0.45f, 0}, {2.2f, 0.55f, 4.2f}, body);
    DrawCubeV({0, 0.95f, 0.2f}, {1.8f, 0.5f, 2.0f}, ColorBrightness(body, -0.35f));
    float wr = sp.radius * 0.42f;
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2)
            DrawCylinder({sx * 1.05f, wr, sz * 1.4f}, wr, wr, 0.35f, 8, {20, 20, 24, 255});
    if (c.boostLeft > 0.f)
        DrawCubeV({0, 0.45f, -2.4f}, {1.4f, 0.3f, 0.8f}, {255, 200, 40, 255});
    rlPopMatrix();
}

sim::CarInput readInput(const sim::Settings& s) {
    sim::CarInput in{};
    if (IsKeyDown(s.keyAccel) || IsKeyDown(s.keyAccelAlt)) in.throttle = 1;
    if (IsKeyDown(s.keyBrake) || IsKeyDown(s.keyBrakeAlt)) in.brake = 1;
    if (IsKeyDown(s.keyLeft) || IsKeyDown(s.keyLeftAlt)) in.steer -= 1;
    if (IsKeyDown(s.keyRight) || IsKeyDown(s.keyRightAlt)) in.steer += 1;
    if (IsKeyDown(s.keyHandbrake)) in.handbrake = true;
    if (IsKeyPressed(s.keyBoost)) in.boost = true;
    if (IsGamepadAvailable(0)) {
        float ax = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        float dz = s.padDeadzone;
        if (std::fabs(ax) > dz) in.steer = ax * s.padSteerSens;
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) in.steer = -1; // button steering works too
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) in.steer = 1;
        float trig = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_TRIGGER);
        if (trig > 0.1f) in.throttle = trig;
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) in.throttle = 1; // button accel works too
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT)) in.brake = 1;
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_UP)) in.handbrake = true;
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) in.boost = true;
    }
    in.steer = fminf(fmaxf(in.steer, -1.f), 1.f);
    return in;
}

void drawHud(const sim::Race& r, const GameApp& app, int W, int H) {
    const sim::Car& p = r.cars[0];
    float spd = std::fabs(sim::dot(p.vel, {std::cos(p.heading), std::sin(p.heading)})) * 3.6f;
    DrawText(TextFormat("%3.0f", spd), W - 150, H - 110, 64, WHITE);
    DrawText("km/h", W - 145, H - 40, 20, GRAY);
    DrawText(TextFormat("LAP %d/%d", p.lap + 1 > r.lapsToWin ? r.lapsToWin : p.lap + 1, r.lapsToWin), 20, 20, 28, WHITE);
    int place = 1;
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
        DrawText("BOOST!", W / 2 - 50, H - 110, 28, {255, 200, 40, 255});
    if (p.wrongWay) DrawText("WRONG WAY", W / 2 - 110, H / 2 - 40, 34, RED);
    if (r.countdown > 0.f) {
        int cd = (int)r.countdown;
        DrawText(cd > 0 ? TextFormat("%d", cd) : "GO!", W / 2 - 30, H / 2 - 60, 72, GOLD);
    }
    if (p.finished) {
        DrawRectangle(0, 0, W, H, {0, 0, 0, 120});
        DrawText(p.finishPlace == 1 ? "YOU WIN!" : TextFormat("FINISHED %d%s", p.finishPlace, p.finishPlace == 2 ? "nd" : "rd"),
                 W / 2 - 140, H / 2 - 40, 40, GOLD);
        DrawText("Press ESC", W / 2 - 60, H / 2 + 20, 20, GRAY);
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

static void drawFrame(const sim::Race& r, const TrackMeshes& tm, Camera3D& cam, const GameApp& app, float steerVis) {
    const sim::Car& p = r.cars[0];
    sim::Vec2 fwd{std::cos(p.heading), std::sin(p.heading)};
    float speed = sim::len(p.vel);
    float dist = 9.f + speed * 0.12f;
    Vector3 camPos = {p.pos.x - fwd.x * dist, 5.f + speed * 0.04f, p.pos.y - fwd.y * dist};
    static Vector3 smoothed = camPos;
    smoothed = Vector3Lerp(smoothed, camPos, 0.18f);
    cam.position = smoothed;
    cam.target = {p.pos.x + fwd.x * 3.f, 0.8f, p.pos.y + fwd.y * 3.f};
    cam.fovy = 60.f + speed * 0.35f + (p.boostLeft > 0 ? 8.f : 0.f);
    BeginDrawing();
    ClearBackground({8, 8, 20, 255});
    BeginMode3D(cam);
    DrawPlane({0, -0.05f, 0}, {1600, 1600}, {16, 16, 28, 255});
    DrawModel(tm.road, {0, 0, 0}, 1.f, WHITE);
    DrawModel(tm.edgeL, {0, 0.02f, 0}, 1.f, WHITE);
    DrawModel(tm.edgeR, {0, 0.02f, 0}, 1.f, WHITE);
    for (size_t i = 0; i < tm.buildings.size(); i++)
        DrawCubeV(tm.buildings[i], {10, tm.buildings[i].y * 2, 10}, tm.buildingColors[i]);
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
    for (size_t i = 0; i < r.cars.size(); i++) drawCar(r.cars[i], r.specs[i], cols[i % 3]);
    EndMode3D();
    drawHud(r, app, GetScreenWidth(), GetScreenHeight());
    EndDrawing();
}

int runGame(GameApp& app) {
    configureWindow(app.settings);
    SetExitKey(0); // ESC handled manually for pause
    sim::Race race;
    race.init(3);
    Camera3D cam{};
    cam.up = {0, 1, 0};
    cam.fovy = 60;
    cam.projection = CAMERA_PERSPECTIVE;
    TrackMeshes tm = buildTrack(race.track, app.settings.quality);
    bool paused = false;
    double acc = 0;
    while (!WindowShouldClose()) {
        if (IsKeyPressed(app.settings.keyPause)) paused = !paused;
        if (IsKeyPressed(KEY_F3)) app.showFps = !app.showFps;
        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();
        acc += GetFrameTime();
        sim::CarInput in = paused ? sim::CarInput{} : readInput(app.settings);
        while (acc >= sim::STEP) {
            sim::CarInput ai = sim::aiDrive(race, 1);
            sim::CarInput ai2 = sim::aiDrive(race, 2);
            race.step({in, ai, ai2});
            acc -= sim::STEP;
        }
        drawFrame(race, tm, cam, app, in.steer);
        if (paused) {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 140});
            DrawText("PAUSED", GetScreenWidth() / 2 - 80, GetScreenHeight() / 2 - 30, 36, WHITE);
            DrawText("ESC resume | F3 fps | F11 fullscreen", GetScreenWidth() / 2 - 150, GetScreenHeight() / 2 + 20, 18, GRAY);
            EndDrawing();
        }
    }
    sim::saveSettings(app.settings, app.settingsPath);
    CloseWindow();
    return 0;
}

int runScreenshots(GameApp& app, const std::string& prefix, int warmFrames, int shotEvery, int shotCount) {
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "overdrive-demo");
    SetTargetFPS(600);
    sim::Race race;
    race.init(3);
    Camera3D cam{};
    cam.up = {0, 1, 0};
    cam.fovy = 60;
    cam.projection = CAMERA_PERSPECTIVE;
    TrackMeshes tm = buildTrack(race.track, app.settings.quality);
    int taken = 0;
    bool gotCorner = false, gotBoost = false, gotPause = false, gotFinish = false;
    auto snap = [&](const char* tag) {
        char name[256];
        std::snprintf(name, sizeof(name), "%s-%02d-%s.png", prefix.c_str(), taken, tag);
        TakeScreenshot(name);
        taken++;
    };
    for (int f = 0; f < 60 * 300; f++) {
        sim::CarInput in = sim::aiDrive(race, 0); // player drives the racing line
        if (race.cars[0].boostCharges > 0 && race.cars[0].boostLeft <= 0 && race.cars[0].lap >= 1) in.boost = true;
        race.step({in, sim::aiDrive(race, 1), sim::aiDrive(race, 2)});
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
