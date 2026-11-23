#include "sim.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace sim {

float len(Vec2 a) { return std::sqrt(dot(a, a)); }
static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float wrapAngle(float a) {
    while (a > 3.14159265f) a -= 6.2831853f;
    while (a < -3.14159265f) a += 6.2831853f;
    return a;
}

// Catmull-Rom interpolation between p1 and p2.
static Vec2 catmull(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, float t) {
    float t2 = t * t, t3 = t2 * t;
    auto f = [&](float a, float b, float c, float d) {
        return 0.5f * ((2.f * b) + (-a + c) * t + (2.f * a - 5.f * b + 4.f * c - d) * t2 + (-a + 3.f * b - 3.f * c + d) * t3);
    };
    return {f(p0.x, p1.x, p2.x, p3.x), f(p0.y, p1.y, p2.y, p3.y)};
}

void Track::build(const std::vector<Vec2>& ctrl, float hw, int samplesPerSegment) {
    halfWidth = hw;
    center.clear();
    const int n = (int)ctrl.size();
    for (int i = 0; i < n; i++) {
        Vec2 p0 = ctrl[(i + n - 1) % n], p1 = ctrl[i], p2 = ctrl[(i + 1) % n], p3 = ctrl[(i + 2) % n];
        for (int j = 0; j < samplesPerSegment; j++)
            center.push_back(catmull(p0, p1, p2, p3, (float)j / samplesPerSegment));
    }
    cum.assign(center.size(), 0);
    total = 0;
    for (size_t i = 1; i <= center.size(); i++) {
        total += len(center[i % center.size()] - center[i - 1]);
        if (i < center.size()) cum[i] = total;
    }
    // Checkpoints: start/finish plus 7 evenly spaced gates.
    checkpoints.clear();
    for (int k = 0; k < 8; k++) checkpoints.push_back((int)(k * center.size() / 8));
    startIndex = 0;
    startHeading = std::atan2(dirAt(0).y, dirAt(0).x);
}

Vec2 Track::pointAt(float d) const {
    d = std::fmod(d, total);
    if (d < 0) d += total;
    // binary search on cum
    size_t lo = 0, hi = center.size() - 1;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (cum[mid] < d) lo = mid + 1; else hi = mid;
    }
    size_t i1 = lo % center.size(), i0 = (i1 + center.size() - 1) % center.size();
    float segLen = len(center[i1] - center[i0]);
    if (segLen < 1e-6f) return center[i0];
    float t = (d - cum[i0]) / segLen;
    return center[i0] + (center[i1] - center[i0]) * clampf(t, 0.f, 1.f);
}

Vec2 Track::dirAt(float d) const {
    Vec2 a = pointAt(d), b = pointAt(d + 1.f);
    Vec2 v = b - a;
    float l = len(v);
    return l > 1e-6f ? v * (1.f / l) : Vec2{1, 0};
}

float Track::nearest(Vec2 p, float hint) const {
    // Local search around hint (in distance units), convert to index window.
    float avg = total / (float)center.size();
    int win = 40;
    int center0 = (int)std::fmod(hint / avg, (float)center.size());
    float best = 1e30f;
    size_t besti = 0;
    for (int k = -win; k <= win; k++) {
        size_t i = (size_t)((center0 + k + (int)center.size() * 4) % center.size());
        float d = len(p - center[i]);
        if (d < best) { best = d; besti = i; }
    }
    return cum[besti];
}

std::vector<Vec2> neonLoopControlPoints() {
    // Original closed circuit: long straight, hairpin, S-curves, sweeper.
    return {
        {0, 0}, {120, 0}, {190, 10}, {230, 45}, {235, 100},
        {200, 140}, {140, 150}, {110, 185}, {120, 240}, {90, 285},
        {30, 290}, {-25, 260}, {-40, 200}, {-20, 150}, {-60, 110}, {-70, 50}, {-40, 15}
    };
}

std::vector<CarSpec> defaultRoster() {
    CarSpec balanced;  balanced.name = "STANDARD";
    CarSpec sprinter;  sprinter.name = "SPRINTER";
    sprinter.accel = 18.f; sprinter.maxSpeed = 39.f; sprinter.steerRate = 2.4f; sprinter.skill = 0.97f;
    CarSpec heavy;     heavy.name = "HAULER";
    heavy.accel = 11.f; heavy.maxSpeed = 45.f; heavy.grip = 11.f; heavy.driftGrip = 3.f;
    heavy.radius = 1.3f; heavy.skill = 0.95f;
    return {balanced, sprinter, heavy};
}

void Race::init(int laps, float aiSkill, int playerVehicle) {
    lapsToWin = laps;
    tick = 0; countdown = 3.2f; finishedCount = 0;
    specs = defaultRoster();
    // player drives the chosen vehicle; AI take the other specs with the difficulty multiplier
    std::vector<CarSpec> roster = defaultRoster();
    CarSpec player = roster[playerVehicle % (int)roster.size()];
    specs[0] = player;
    for (size_t i = 1; i < specs.size(); i++) {
        specs[i] = roster[(playerVehicle + i) % roster.size()];
        specs[i].skill *= aiSkill;
    }
    track.build(neonLoopControlPoints(), 9.f, 24);
    cars.clear();
    int nCars = (int)specs.size();
    Vec2 dir = track.dirAt(0);
    Vec2 right = {-dir.y, dir.x};
    for (int i = 0; i < nCars; i++) {
        Car c;
        // grid: two columns behind the start line
        float back = -8.f - 7.f * (i / 2);
        float side = (i % 2 == 0 ? 3.f : -3.f);
        c.pos = track.pointAt(0) + dir * back + right * side;
        c.heading = track.startHeading;
        c.nextCp = 1;
        c.sHint = 0;
        cars.push_back(c);
    }
    // Boost pickups: three spots, alternating track sides.
    pickups.clear();
    for (int k = 0; k < 3; k++) {
        float d = track.total * (0.2f + 0.3f * k);
        Vec2 dd = track.dirAt(d);
        Vec2 rr = {-dd.y, dd.x};
        Pickup p;
        p.pos = track.pointAt(d) + rr * (k % 2 == 0 ? 4.f : -4.f);
        pickups.push_back(p);
    }
}

uint32_t Race::rand() const {
    rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;
    return rngState;
}

static void stepCar(Race& r, Car& c, const CarSpec& sp, const CarInput& in) {
    if (c.finished) return;
    bool frozen = r.countdown > 0.f;
    float throttle = frozen ? 0.f : clampf(in.throttle, 0.f, 1.f);
    float brake = frozen ? 0.f : clampf(in.brake, 0.f, 1.f);
    float steer = frozen ? 0.f : clampf(in.steer, -1.f, 1.f);

    if (!frozen && in.boost && c.boostCharges > 0 && c.boostLeft <= 0.f) {
        c.boostCharges--;
        c.boostLeft = sp.boostTime;
    }
    bool boosting = c.boostLeft > 0.f;
    if (boosting) c.boostLeft -= STEP;

    Vec2 fwd = {std::cos(c.heading), std::sin(c.heading)};
    float speed = dot(c.vel, fwd);

    float maxSp = sp.maxSpeed + (boosting ? sp.boostAccel * 1.2f : 0.f);
    float acc = throttle * (sp.accel + (boosting ? sp.boostAccel : 0.f));
    if (speed < maxSp) c.vel = c.vel + fwd * (acc * STEP);
    // brake / reverse
    if (brake > 0.f) {
        if (speed > 0.5f) c.vel = c.vel - fwd * (sp.brake * brake * STEP);
        else if (speed > -sp.reverseSpeed) c.vel = c.vel - fwd * (sp.accel * 0.6f * brake * STEP);
    }
    // drag + rolling resistance
    float spd = len(c.vel);
    if (spd > 1e-5f) {
        float drag = 0.35f * spd * spd * 0.01f + 0.6f;
        float ns = spd - drag * STEP;
        c.vel = c.vel * (ns > 0 ? ns / spd : 0.f);
    }
    // steering, speed-sensitive
    float steerAuthority = clampf(1.f - std::fabs(speed) / (sp.maxSpeed * 1.6f), 0.25f, 1.f);
    float dirSign = speed >= 0 ? 1.f : -1.f;
    c.heading = wrapAngle(c.heading + steer * sp.steerRate * steerAuthority * dirSign * STEP);
    // lateral grip: remove part of sideways velocity (less while drifting)
    Vec2 fwd2 = {std::cos(c.heading), std::sin(c.heading)};
    Vec2 right = {-fwd2.y, fwd2.x};
    float lat = dot(c.vel, right);
    float grip = in.handbrake && !frozen ? sp.driftGrip : sp.grip;
    c.vel = c.vel - right * (lat * clampf(grip * STEP, 0.f, 1.f));

    c.pos = c.pos + c.vel * STEP;

    // Track walls: clamp to halfWidth from centerline.
    float s = r.track.nearest(c.pos, c.sHint);
    c.sHint = s;
    Vec2 onTrack = r.track.pointAt(s);
    Vec2 off = c.pos - onTrack;
    float dist = len(off);
    float maxD = r.track.halfWidth - sp.radius;
    if (dist > maxD && dist > 1e-5f) {
        Vec2 nrm = off * (1.f / dist);
        c.pos = onTrack + nrm * maxD;
        float vn = dot(c.vel, nrm);
        if (vn > 0) c.vel = c.vel - nrm * (vn * 1.4f); // slight bounce
    }

    // Checkpoints and laps.
    float cpS = r.track.cum[r.track.checkpoints[c.nextCp]];
    Vec2 cpPos = r.track.pointAt(cpS);
    if (len(c.pos - cpPos) < r.track.halfWidth) {
        c.nextCp++;
        if (c.nextCp >= (int)r.track.checkpoints.size()) {
            c.nextCp = 0;
        }
        if (c.nextCp == 1) { // crossed start/finish
            c.lap++;
            float t = r.time();
            c.lastLap = t - c.lapStart;
            if (c.bestLap < 0 || c.lastLap < c.bestLap) c.bestLap = c.lastLap;
            c.lapStart = t;
            if (c.lap >= r.lapsToWin) {
                c.finished = true;
                c.finishPlace = ++r.finishedCount;
            }
        }
    }
    // progress for placing
    c.progress = (float)c.lap * r.track.total + s;
    // wrong way: moving against track direction
    Vec2 tdir = r.track.dirAt(s);
    c.wrongWay = spd > 3.f && dot(c.vel, tdir) < -1.f;
    if (!c.finished) c.raceTime = r.time();
}

void Race::step(const std::vector<CarInput>& inputs) {
    tick++;
    if (countdown > 0.f) countdown -= STEP;
    for (size_t i = 0; i < cars.size(); i++) {
        CarInput in = i < inputs.size() ? inputs[i] : CarInput{};
        stepCar(*this, cars[i], specs[i], in);
    }
    // car-car collision: simple circle push-out
    for (size_t i = 0; i < cars.size(); i++)
        for (size_t j = i + 1; j < cars.size(); j++) {
            Vec2 d = cars[j].pos - cars[i].pos;
            float dist = len(d);
            float minD = specs[i].radius + specs[j].radius;
            if (dist < minD && dist > 1e-5f) {
                Vec2 n = d * (1.f / dist);
                Vec2 push = n * ((minD - dist) * 0.5f);
                cars[i].pos = cars[i].pos - push;
                cars[j].pos = cars[j].pos + push;
                float rel = dot(cars[j].vel - cars[i].vel, n);
                if (rel < 0) {
                    Vec2 imp = n * (rel * 0.7f);
                    cars[i].vel = cars[i].vel + imp;
                    cars[j].vel = cars[j].vel - imp;
                }
            }
        }
    // pickups
    for (auto& p : pickups) {
        if (!p.active) {
            p.respawn -= STEP;
            if (p.respawn <= 0) p.active = true;
            continue;
        }
        for (auto& c : cars) {
            if (len(c.pos - p.pos) < 2.2f) {
                c.boostCharges = 1;
                p.active = false;
                p.respawn = 10.f;
                break;
            }
        }
    }
}

CarInput aiDrive(const Race& race, int i) {
    const Car& c = race.cars[i];
    const CarSpec& sp = race.specs[i];
    CarInput in{};
    if (c.finished) return in;
    Vec2 fwd = {std::cos(c.heading), std::sin(c.heading)};
    float speed = dot(c.vel, fwd);
    // pure pursuit: aim at a point ahead on the centerline
    float look = 6.f + speed * 0.55f;
    Vec2 target = race.track.pointAt(c.sHint + look);
    Vec2 to = target - c.pos;
    float ang = wrapAngle(std::atan2(to.y, to.x) - c.heading);
    in.steer = clampf(ang * 2.2f, -1.f, 1.f);
    // target speed from curvature ahead
    Vec2 d1 = race.track.dirAt(c.sHint + look * 0.5f);
    Vec2 d2 = race.track.dirAt(c.sHint + look);
    float curve = std::fabs(wrapAngle(std::atan2(d2.y, d2.x) - std::atan2(d1.y, d1.x)));
    float targetSpeed = sp.maxSpeed * sp.skill / (1.f + curve * 2.4f);
    if (speed < targetSpeed) in.throttle = 1.f;
    else { in.brake = clampf((speed - targetSpeed) * 0.3f, 0.f, 1.f); }
    // occasional human-like lift-off wobble from rng
    if ((race.rand() & 0x3FF) == 0) in.steer = clampf(in.steer + ((int)(race.rand() % 21) - 10) * 0.02f, -1.f, 1.f);
    // use boost on straights
    if (c.boostCharges > 0 && curve < 0.05f && speed > sp.maxSpeed * 0.7f) in.boost = true;
    return in;
}

// ---- settings ----
bool saveSettings(const Settings& s, const std::string& path) {
    std::ofstream f(path, std::ios::trunc);
    if (!f) return false;
    f << "width " << s.width << "\nheight " << s.height << "\nfullscreen " << s.fullscreen
      << "\nfpsCap " << s.fpsCap << "\nquality " << s.quality
      << "\nmasterVol " << s.masterVol << "\nmusicVol " << s.musicVol << "\nsfxVol " << s.sfxVol
      << "\nmuted " << s.muted << "\nplayerVehicle " << s.playerVehicle
      << "\nkeyAccel " << s.keyAccel << "\nkeyBrake " << s.keyBrake << "\nkeyLeft " << s.keyLeft
      << "\nkeyRight " << s.keyRight << "\nkeyAccelAlt " << s.keyAccelAlt << "\nkeyBrakeAlt " << s.keyBrakeAlt
      << "\nkeyLeftAlt " << s.keyLeftAlt << "\nkeyRightAlt " << s.keyRightAlt
      << "\nkeyHandbrake " << s.keyHandbrake << "\nkeyBoost " << s.keyBoost << "\nkeyPause " << s.keyPause
      << "\npadDeadzone " << s.padDeadzone << "\npadSteerSens " << s.padSteerSens
      << "\nbestLapMs " << s.bestLapMs << "\nbestRaceMs " << s.bestRaceMs << "\n";
    return f.good();
}

Settings loadSettings(const std::string& path) {
    Settings s;
    std::ifstream f(path);
    if (!f) return s;
    std::string key;
    Settings tmp = s;
    while (f >> key) {
        std::string val;
        if (!(f >> val)) { return s; } // corrupt -> defaults
        try {
            if (key == "width") tmp.width = std::stoi(val);
            else if (key == "height") tmp.height = std::stoi(val);
            else if (key == "fullscreen") tmp.fullscreen = std::stoi(val) != 0;
            else if (key == "fpsCap") tmp.fpsCap = std::stoi(val);
            else if (key == "quality") tmp.quality = std::stoi(val);
            else if (key == "masterVol") tmp.masterVol = std::stof(val);
            else if (key == "musicVol") tmp.musicVol = std::stof(val);
            else if (key == "sfxVol") tmp.sfxVol = std::stof(val);
            else if (key == "muted") tmp.muted = std::stoi(val) != 0;
            else if (key == "playerVehicle") tmp.playerVehicle = std::stoi(val);
            else if (key == "aiDifficulty") tmp.aiDifficulty = std::stoi(val);
            else if (key == "laps") tmp.laps = std::stoi(val);
            else if (key == "keyAccel") tmp.keyAccel = std::stoi(val);
            else if (key == "keyBrake") tmp.keyBrake = std::stoi(val);
            else if (key == "keyLeft") tmp.keyLeft = std::stoi(val);
            else if (key == "keyRight") tmp.keyRight = std::stoi(val);
            else if (key == "keyAccelAlt") tmp.keyAccelAlt = std::stoi(val);
            else if (key == "keyBrakeAlt") tmp.keyBrakeAlt = std::stoi(val);
            else if (key == "keyLeftAlt") tmp.keyLeftAlt = std::stoi(val);
            else if (key == "keyRightAlt") tmp.keyRightAlt = std::stoi(val);
            else if (key == "keyHandbrake") tmp.keyHandbrake = std::stoi(val);
            else if (key == "keyBoost") tmp.keyBoost = std::stoi(val);
            else if (key == "keyPause") tmp.keyPause = std::stoi(val);
            else if (key == "padDeadzone") tmp.padDeadzone = std::stof(val);
            else if (key == "padSteerSens") tmp.padSteerSens = std::stof(val);
            else if (key == "bestLapMs") tmp.bestLapMs = std::stoi(val);
            else if (key == "bestRaceMs") tmp.bestRaceMs = std::stoi(val);
        } catch (...) { return s; }
    }
    return tmp;
}

} // namespace sim

namespace sim {

CarInput mapKeys(const Settings& s, const RawKeys& k) {
    (void)s; // key codes already resolved by the caller via IsKeyDown
    CarInput in{};
    if (k.accel) in.throttle = 1;
    if (k.brake) in.brake = 1;
    if (k.left) in.steer -= 1;
    if (k.right) in.steer += 1;
    in.handbrake = k.handbrake;
    in.boost = k.boost;
    return in;
}

CarInput mapGamepad(const Settings& s, const RawPad& p) {
    CarInput in{};
    float dz = s.padDeadzone;
    if (std::fabs(p.steerAxis) > dz) in.steer = p.steerAxis * s.padSteerSens;
    if (p.left) in.steer = -1;
    if (p.right) in.steer = 1;
    if (p.accelAxis > 0.1f) in.throttle = p.accelAxis;
    if (p.accel) in.throttle = 1;
    if (p.brake) in.brake = 1;
    in.handbrake = p.handbrake;
    in.boost = p.boost;
    if (in.steer > 1.f) in.steer = 1.f;
    if (in.steer < -1.f) in.steer = -1.f;
    return in;
}

CarInput mergeInputs(CarInput a, CarInput b) {
    CarInput in = a;
    if (b.throttle > in.throttle) in.throttle = b.throttle;
    if (b.brake > in.brake) in.brake = b.brake;
    if (b.steer != 0.f) in.steer = b.steer;
    in.handbrake = a.handbrake || b.handbrake;
    in.boost = a.boost || b.boost;
    return in;
}

} // namespace sim
