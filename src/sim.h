// OVERDRIVE simulation core: pure C++17, no raylib, headless-testable.
// Fixed 60 Hz timestep. Deterministic for a given input sequence and seed.
#pragma once
#include <vector>
#include <cstdint>
#include <string>

namespace sim {

constexpr float STEP = 1.0f / 60.0f;

struct Vec2 {
    float x = 0, y = 0;
};
inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vec2 operator*(Vec2 a, float s) { return {a.x * s, a.y * s}; }
inline float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
float len(Vec2 a);

struct CarInput {
    float throttle = 0;   // 0..1
    float brake = 0;      // 0..1
    float steer = 0;      // -1..1 (left negative)
    bool handbrake = false;
    bool boost = false;
};

struct CarSpec {
    std::string name = "STANDARD";
    float accel = 14.f;        // m/s^2 at full throttle
    float brake = 22.f;        // m/s^2
    float maxSpeed = 42.f;     // m/s
    float reverseSpeed = 8.f;  // m/s
    float steerRate = 2.6f;    // rad/s at low speed
    float grip = 9.f;          // lateral correction rate
    float driftGrip = 2.2f;    // lateral grip while handbraking
    float boostAccel = 12.f;   // extra accel while boosting
    float boostTime = 2.5f;    // seconds per charge
    float radius = 1.1f;       // collision radius
    int design = 0;            // original body design ID
    float durability = 100.f;   // maximum health, used by impact damage/combat
    float mass = 1.f;          // collision inertia (shared with player vehicles)
};

// AI driving quality: decision pace/mistakes only, never physical advantages.
struct AITune {
    float pace = 0.95f;        // fraction of spec limits the AI asks for
    float steerGain = 2.2f;    // reaction sharpness
    float wobbleMag = 0.2f;    // mistake magnitude (rad applied to steer)
    int wobbleMask = 0x3FF;    // mistake frequency: rand()&mask == 0
    bool usesBoost = true;
};
AITune difficultyTune(int level); // 0 easy, 1 normal, 2 hard

struct Car {
    Vec2 pos, vel;
    float health = 100.f;
    float heading = 0;
    int nextCp = 1;
    int lap = 0;
    float progress = 0;      // monotonic distance for placing
    float sHint = 0;         // nearest-track-distance hint
    float lapStart = 0, lastLap = -1, bestLap = -1;
    float raceTime = 0;
    bool finished = false;
    int finishPlace = 0;
    int boostCharges = 0;
    float boostLeft = 0;
    bool wrongWay = false;
};

struct Pickup {
    Vec2 pos;
    bool active = true;
    float respawn = 0;
};

struct Track {
    std::vector<Vec2> center;     // sampled closed centerline
    std::vector<float> cum;       // cumulative distance per sample
    float total = 0;
    float halfWidth = 9.f;
    std::vector<int> checkpoints; // sample indices, in order; index 0 = start/finish
    int startIndex = 0;
    float startHeading = 0;

    void build(const std::vector<Vec2>& ctrl, float halfWidth, int samplesPerSegment);
    Vec2 pointAt(float d) const;
    Vec2 dirAt(float d) const;
    float nearest(Vec2 p, float hint) const; // distance along track of closest sample
};

struct TrackDef {
    std::string name;
    std::vector<Vec2> ctrl;
    float halfWidth = 9.f;
    int theme = 0;              // 0 neon city, 1 coastal
};
const std::vector<TrackDef>& trackDefs();
std::vector<CarSpec> defaultRoster();

struct Race {
    Track track;
    std::vector<Car> cars;
    std::vector<CarSpec> specs;
    std::vector<Pickup> pickups;
    int lapsToWin = 3;
    int finishedCount = 0;
    uint64_t tick = 0;
    float countdown = 3.2f;   // inputs ignored while > 0
    mutable uint32_t rngState = 0x9E3779B9u;

    AITune aiTune[8];
    int trackIndex = 0;
    void init(int laps, int aiDifficulty = 1, int playerVehicle = 0, int track = 0);
    void step(const std::vector<CarInput>& inputs);
    uint32_t rand() const;
    float time() const { return tick * STEP; }
};

// Pure-pursuit AI controller for one car.
CarInput aiDrive(const Race& race, int carIndex);

// Settings (pure, testable). Simple key=value file.
struct Settings {
    int width = 1280, height = 720;
    bool fullscreen = false;
    int fpsCap = 60;              // 30 or 60
    int quality = 1;              // 0 low, 1 high (buildings, particles)
    float masterVol = 1.f, musicVol = 1.f, sfxVol = 1.f;
    bool muted = false;
    int playerVehicle = 0;
    int aiDifficulty = 1;       // 0 rookie, 1 pro, 2 ace
    int laps = 3;               // 1, 3 or 5
    int keyAccel = 265, keyBrake = 264, keyLeft = 263, keyRight = 262, // arrows
        keyAccelAlt = 87, keyBrakeAlt = 83, keyLeftAlt = 65, keyRightAlt = 68, // WASD
        keyHandbrake = 32, keyBoost = 340 /*left shift*/, keyPause = 256 /*esc*/;
    float padDeadzone = 0.18f, padSteerSens = 1.0f;
    int trackIndex = 0;
    int bestLapMs = -1, bestRaceMs = -1;      // legacy, migrated into per-track arrays
    int bestLapMsT[8] = {-1,-1,-1,-1,-1,-1,-1,-1};
    int bestRaceMsT[8] = {-1,-1,-1,-1,-1,-1,-1,-1};
    int credits = 0, racesCompleted = 0, wins = 0;
    unsigned unlocked = 7; // first three vehicles owned, the rest earned offline
};

// Records update: best lap any finish; best RACE time only counts on a win.
void updateRecords(Settings& s, const Car& p, int track);

// Pure input mapping shared by the interactive path (polled hardware) and the
// demo path (scripted axes), so both exercise the same translation to CarInput.
struct RawKeys { bool accel = false, brake = false, left = false, right = false, handbrake = false, boost = false; };
struct RawPad { float steerAxis = 0, accelAxis = 0; bool left = false, right = false, accel = false, brake = false, handbrake = false, boost = false; };
CarInput mapKeys(const Settings& s, const RawKeys& k);
CarInput mapGamepad(const Settings& s, const RawPad& p);
CarInput mergeInputs(CarInput a, CarInput b); // analog steer: nonzero pad axis wins

int vehicleUnlockCost(int index);
bool unlockVehicle(Settings& s, int index);
void awardRace(Settings& s, int place); // once per completed player race
bool saveSettings(const Settings& s, const std::string& path);
Settings loadSettings(const std::string& path); // corrupt/missing -> defaults

} // namespace sim
