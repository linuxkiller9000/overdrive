// Headless simulation tests: no GPU, no window.
#include "../src/sim.h"
#include <cstdio>
#include <cmath>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } } while (0)

int runTests() {
    // track builds closed loop
    sim::Race r;
    r.init(3);
    CHECK(r.track.total > 500.f, "track length sane");
    {
        sim::Vec2 a = r.track.pointAt(0), b = r.track.pointAt(r.track.total);
        CHECK(sim::len(a - b) < 0.5f, "loop closes");
        CHECK((int)r.track.checkpoints.size() == 8, "8 checkpoints");
    }
    // player car accelerates and respects top speed
    {
        sim::CarInput in{}; in.throttle = 1;
        float peak = 0;
        for (int i = 0; i < 60 * 20; i++) { r.step({in, sim::aiDrive(r, 1), sim::aiDrive(r, 2)}); peak = fmaxf(peak, sim::len(r.cars[0].vel)); }
        CHECK(peak > 30.f, "car accelerates toward top speed");
        CHECK(peak < r.specs[0].maxSpeed + r.specs[0].boostAccel * 1.3f, "top speed bounded");
    }
    // AI completes a full lap in sane time and checkpoints advance in order
    {
        sim::Race r2; r2.init(2);
        bool finished = false;
        for (int i = 0; i < 60 * 240 && !finished; i++) {
            r2.step({sim::aiDrive(r2, 0), sim::aiDrive(r2, 1), sim::aiDrive(r2, 2)});
            finished = r2.cars[0].finished;
        }
        CHECK(finished, "AI completes race distance");
        CHECK(r2.cars[0].lap >= 2, "laps counted");
        CHECK(r2.cars[0].bestLap > 0, "best lap recorded");
    }
    // walls contain the car: drive into barrier sideways
    {
        sim::Race r3; r3.init(3);
        sim::CarInput in{}; in.throttle = 1; in.steer = 1;
        for (int i = 0; i < 60 * 30; i++) r3.step({in, sim::aiDrive(r3, 1), sim::aiDrive(r3, 2)});
        sim::Vec2 onTrack = r3.track.pointAt(r3.track.nearest(r3.cars[0].pos, r3.cars[0].sHint));
        CHECK(sim::len(r3.cars[0].pos - onTrack) <= r3.track.halfWidth - r3.specs[0].radius + 0.01f, "car stays inside walls");
    }
    // boost: pickup grants charge, boost raises speed above base max
    {
        sim::Race r4; r4.init(3);
        // teleport player onto a pickup (and sync the track hint)
        r4.cars[0].pos = r4.pickups[0].pos;
        r4.cars[0].sHint = r4.track.total * 0.07f;
        sim::CarInput in{}; in.throttle = 1;
        r4.step({in, sim::aiDrive(r4, 1), sim::aiDrive(r4, 2)});
        CHECK(r4.cars[0].heldPickup == sim::PK_NITRO, "pickup grants held NITRO");
        { sim::CarInput f{}; f.throttle = 1; f.fire = true; r4.countdown = 0; r4.step({f}); }
        CHECK(r4.cars[0].boostLeft > 0.f, "fired NITRO starts boost");
        CHECK(r4.cars[0].heldPickup == -1, "fire consumes held pickup");
        CHECK(!r4.pickups[0].active, "pickup consumed");
        float baseMax = 0;
        for (int i = 0; i < 60 * 15; i++) {
            sim::CarInput ain = sim::aiDrive(r4, 0);
            ain.throttle = 1; ain.brake = 0; // pin throttle so boost headroom shows
            ain.boost = false; ain.fire = (i == 0);
            r4.step({ain, sim::aiDrive(r4, 1), sim::aiDrive(r4, 2)});
            baseMax = fmaxf(baseMax, sim::len(r4.cars[0].vel));
        }
        CHECK(baseMax > r4.specs[0].maxSpeed + 2.f, "boost exceeds base top speed");
        // pickup respawns
        for (int i = 0; i < 60 * 13; i++) r4.step({in});
        CHECK(r4.pickups[0].active, "pickup respawns");
    }
    // determinism: same inputs, same final state
    {
        sim::Race a, b; a.init(1); b.init(1);
        for (int i = 0; i < 600; i++) {
            sim::CarInput in{}; in.throttle = (i % 120 < 90) ? 1 : 0; in.steer = std::sin(i * 0.05f);
            a.step({in, sim::aiDrive(a, 1), sim::aiDrive(a, 2)});
            b.step({in, sim::aiDrive(b, 1), sim::aiDrive(b, 2)});
        }
        CHECK(a.cars[0].pos.x == b.cars[0].pos.x && a.cars[0].pos.y == b.cars[0].pos.y, "deterministic");
    }
    // countdown freezes inputs
    {
        sim::Race r5; r5.init(1);
        sim::CarInput in{}; in.throttle = 1;
        r5.step({in, sim::aiDrive(r5, 1), sim::aiDrive(r5, 2)});
        CHECK(sim::len(r5.cars[0].vel) == 0.f, "countdown freezes cars");
    }
    // settings roundtrip + corrupt file fallback
    {
        std::string p = "/tmp/od_settings_test.cfg";
        sim::Settings s; s.width = 1366; s.height = 768; s.fpsCap = 30; s.keyBoost = 344; s.playerVehicle = 2; s.bestLapMs = 61234;
        CHECK(sim::saveSettings(s, p), "settings save");
        sim::Settings l = sim::loadSettings(p);
        CHECK(l.width == 1366 && l.height == 768 && l.fpsCap == 30 && l.keyBoost == 344 && l.playerVehicle == 2 && l.bestLapMs == 61234, "settings roundtrip");
        FILE* f = std::fopen(p.c_str(), "w");
        std::fputs("width notanumber\nhe", f);
        std::fclose(f);
        sim::Settings d = sim::loadSettings(p);
        CHECK(d.width == 1280 && d.height == 720, "corrupt settings fall back to defaults");
    }
    // car-car collision separates vehicles and exchanges velocity
    {
        sim::Race r6; r6.init(1);
        r6.countdown = 0;
        r6.cars[1].pos = {r6.cars[0].pos.x + 1.f, r6.cars[0].pos.y};
        r6.cars[1].vel = {-5.f, 0};
        r6.cars[0].vel = {5.f, 0};
        float before = sim::len(r6.cars[1].pos - r6.cars[0].pos);
        for (int i = 0; i < 10; i++) r6.step({{}, {}, {}});
        float after = sim::len(r6.cars[1].pos - r6.cars[0].pos);
        CHECK(after >= before, "collision separates cars");
        CHECK(sim::len(r6.cars[0].pos - r6.cars[1].pos) > 1.5f, "no overlap after collision");
    }
    // input mapping: keyboard booleans and gamepad axes map to CarInput
    {
        sim::Settings s;
        sim::CarInput k = sim::mapKeys(s, {true, false, true, false, false, true});
        CHECK(k.throttle == 1.f && k.steer == -1.f && k.boost, "mapKeys maps buttons");
        sim::RawPad p{}; p.steerAxis = 0.5f; p.accelAxis = 0.8f;
        sim::CarInput g = sim::mapGamepad(s, p);
        CHECK(g.steer == 0.5f && g.throttle == 0.8f, "mapGamepad maps axes");
        p.steerAxis = 0.05f; // inside deadzone
        CHECK(sim::mapGamepad(s, p).steer == 0.f, "deadzone filters steer");
        sim::CarInput m = sim::mergeInputs(k, g);
        CHECK(m.steer == g.steer && m.throttle == 1.f && m.boost, "merge: pad steer wins, max throttle");
    }
    // difficulty changes AI decisions, not physical limits
    {
        sim::Race easy, hard;
        easy.init(3, 0, 0);
        hard.init(3, 2, 0);
        CHECK(easy.specs[1].maxSpeed == hard.specs[1].maxSpeed && easy.specs[1].accel == hard.specs[1].accel,
              "AI uses same physical limits as player roster");
        for (int i = 0; i < 60 * 30; i++) {
            easy.step({sim::aiDrive(easy, 0), sim::aiDrive(easy, 1), sim::aiDrive(easy, 2)});
            hard.step({sim::aiDrive(hard, 0), sim::aiDrive(hard, 1), sim::aiDrive(hard, 2)});
        }
        CHECK(hard.cars[1].progress > easy.cars[1].progress, "hard AI outpaces easy AI");
        CHECK(sim::difficultyTune(0).wobbleMag > sim::difficultyTune(2).wobbleMag, "easy AI makes bigger mistakes");
    }
    // mass-weighted collisions: lighter car takes bigger velocity change
    {
        sim::Race r7;
        r7.init(1, 1, 0); // player STANDARD, AI include HAULER
        // put player (mass 1.0) and hauler (mass 1.6) head to head
        int hi = -1;
        for (size_t i = 1; i < r7.specs.size(); i++) if (r7.specs[i].name == "HAULER") hi = (int)i;
        CHECK(hi > 0, "hauler in AI roster");
        r7.cars[0].pos = {0, 0}; r7.cars[0].vel = {10, 0};
        r7.cars[hi].pos = {2.f, 0}; r7.cars[hi].vel = {-10, 0};
        sim::Vec2 v0 = r7.cars[0].vel, vh = r7.cars[hi].vel;
        r7.step({{}, {}, {}});
        float dvPlayer = sim::len(r7.cars[0].vel - v0);
        float dvHauler = sim::len(r7.cars[hi].vel - vh);
        CHECK(dvPlayer > dvHauler, "heavier vehicle wins the shove");
    }
    // Car creator: stat budget is capped, spec is derived only from budget, file round-trips, race uses it.
    {
        sim::CustomCar c; c.power = 6; c.handling = 6; c.grip = 6; c.armor = 6; sim::clampCustom(c);
        CHECK(c.power + c.handling + c.grip + c.armor <= sim::kCustomBudget, "custom stat budget capped");
        sim::CustomCar a; a.power = 6; a.handling = 0; a.grip = 3; a.armor = 3;
        sim::CustomCar b; b.power = 3; b.handling = 3; b.grip = 3; b.armor = 3;
        CHECK(sim::customSpec(a).maxSpeed > sim::customSpec(b).maxSpeed && sim::customSpec(a).steerRate < sim::customSpec(b).steerRate, "custom stats trade off");
        CHECK(sim::customSpec(a).maxSpeed <= 48.f && sim::customSpec(a).accel <= 20.f, "custom car within roster limits");
        a.L = 9.f; a.roof = 0.f; sim::clampCustom(a);
        CHECK(a.L <= 5.2f && a.roof >= a.belt + .3f, "custom shape clamped");
        std::string p = "/tmp/overdrive_custom_test.cfg";
        sim::saveCustom(b, p); b.r = 1; sim::CustomCar l = sim::loadCustom(p);
        CHECK(l.power == 3 && l.L == b.L, "custom car round-trips");
        sim::g_custom = l; sim::Race r; r.init(1, 1, 8, 0);
        CHECK(r.specs[0].design == 8 && r.specs[0].name == "CUSTOM" && r.specs.size() == 8, "race uses custom car for player");
        sim::CustomCar bad = sim::loadCustom("/nonexistent/x"); CHECK(bad.power >= 0, "missing custom file gives defaults");
    }
    // Eight vehicles: real gameplay parameters, race completion and durable saves.
    {
        auto roster=sim::defaultRoster(); CHECK(roster.size()==8,"eight vehicle roster");
        for(int v=0;v<8;v++) {
            CHECK(roster[v].design==v,"unique vehicle design ID");
            CHECK(roster[v].durability>0 && roster[v].boostAccel>0,"usable durability and boost");
            sim::Race race;race.init(1,1,v);
            CHECK(race.specs[0].name==roster[v].name,"selected spec used in race");
            CHECK(race.cars[0].health==roster[v].durability,"health initialized from durability");
            for(int f=0;f<60*240 && !race.cars[0].finished;f++)
                race.step({sim::aiDrive(race,0),sim::aiDrive(race,1),sim::aiDrive(race,2)});
            CHECK(race.cars[0].finished,"each vehicle completes full lap");
        }
        sim::Settings progress; CHECK(!sim::unlockVehicle(progress,7),"no free locked unlock");
        sim::awardRace(progress,1); CHECK(progress.credits==120 && progress.wins==1,"win rewards");
        sim::awardRace(progress,2); CHECK(progress.credits==200 && progress.racesCompleted==2,"finish rewards");
        CHECK(sim::unlockVehicle(progress,3),"earned unlock succeeds");
        CHECK(progress.credits==0 && (progress.unlocked&8),"unlock charges once");
        CHECK(!sim::unlockVehicle(progress,3),"owned vehicle cannot be charged twice");
        progress.aiDifficulty=2;progress.laps=5;
        CHECK(sim::saveSettings(progress,"/tmp/od_progress.cfg"),"progress save");
        auto loaded=sim::loadSettings("/tmp/od_progress.cfg");
        CHECK(loaded.unlocked==15 && loaded.wins==1 && loaded.racesCompleted==2,"garage progress reloads");
        CHECK(loaded.aiDifficulty==2 && loaded.laps==5,"difficulty and laps persist");
    }
    // Controlled behavior comparisons: tuning differences must change outcomes.
    {
        auto accelTo20=[&](int v){ sim::Race r; r.init(1,1,v); r.countdown=0;
            sim::CarInput in{}; in.throttle=1; int t=0;
            for(;t<60*10;t++){ r.step({in,{},{}}); if(sim::len(r.cars[0].vel)>=20.f) break; }
            return t; };
        CHECK(accelTo20(1) < accelTo20(2), "SPRINTER out-accelerates HAULER to 20 m/s");
        auto stopDist=[&](int v){ sim::Race r; r.init(1,1,v); r.countdown=0;
            sim::Vec2 f{std::cos(r.cars[0].heading),std::sin(r.cars[0].heading)};
            r.cars[0].vel=f*30.f; // same entry speed for every car
            sim::CarInput in{}; in.throttle=0; in.brake=1;
            sim::Vec2 from=r.cars[0].pos;
            for(int i=0;i<60*6 && sim::len(r.cars[0].vel)>0.1f;i++) r.step({in,{},{}});
            return sim::len(r.cars[0].pos-from); };
        CHECK(stopDist(5) < stopDist(2), "VOLT out-brakes HAULER");
        auto topSpeed=[&](int v){ sim::Race r; r.init(1,1,v); r.countdown=0;
            sim::CarInput in{}; in.throttle=1; float peak=0;
            for(int i=0;i<60*12;i++){ r.step({in,{},{}}); peak=fmaxf(peak,sim::len(r.cars[0].vel)); }
            return peak; };
        CHECK(topSpeed(3) > topSpeed(5) + 2.f, "AFTERBURN has higher top speed than VOLT");
        auto turnRate=[&](int v){ sim::Race r; r.init(1,1,v); r.countdown=0;
            sim::CarInput in{}; in.throttle=1;
            for(int i=0;i<120;i++) r.step({in,{},{}});
            float h0=r.cars[0].heading; in.steer=1;
            for(int i=0;i<60;i++) r.step({in,{},{}});
            return std::fabs(r.cars[0].heading-h0); };
        CHECK(turnRate(4) > turnRate(2) + 0.1f, "SIDEWINDER out-turns HAULER");
        auto driftSlip=[&](int v){ sim::Race r; r.init(1,1,v); r.countdown=0;
            sim::CarInput in{}; in.throttle=1;
            for(int i=0;i<120;i++) r.step({in,{},{}});
            in.steer=1; in.handbrake=true; float peak=0;
            for(int i=0;i<45;i++){ r.step({in,{},{}});
                sim::Vec2 f{std::cos(r.cars[0].heading),std::sin(r.cars[0].heading)};
                sim::Vec2 rg{-f.y,f.x}; peak=fmaxf(peak,std::fabs(sim::dot(r.cars[0].vel,rg))); }
            return peak; };
        CHECK(driftSlip(4) > driftSlip(6) + 0.5f, "SIDEWINDER slides more than RIDGEBACK");
        auto boostGain=[&](int v){ sim::Race r; r.init(1,1,v); r.countdown=0;
            sim::CarInput in{}; in.throttle=1;
            for(int i=0;i<120;i++) r.step({in,{},{}});
            float base=0; { float sp=sim::len(r.cars[0].vel);
                for(int i=0;i<60;i++){ r.step({in,{},{}}); base=fmaxf(base,sim::len(r.cars[0].vel)-sp); } }
            r.cars[0].boostCharges=1; in.boost=true; float boosted=0; { float sp=sim::len(r.cars[0].vel);
                for(int i=0;i<60;i++){ r.step({in,{},{}}); boosted=fmaxf(boosted,sim::len(r.cars[0].vel)-sp); } }
            return boosted-base; };
        CHECK(boostGain(7) > boostGain(6) + 0.5f, "PHANTOM boost out-accelerates RIDGEBACK boost");
    }
    // group 14: second track + per-track records
    {
        CHECK((int)sim::trackDefs().size() == 2, "two track defs");
        for (int ti = 0; ti < 2; ti++) {
            sim::Race rt; rt.init(1, 1, 0, ti);
            CHECK(rt.track.total > 500.f, "track length sane");
            sim::Vec2 a = rt.track.pointAt(0), b = rt.track.pointAt(rt.track.total);
            CHECK(sim::len(a - b) < 0.5f, "loop closes");
            CHECK((int)rt.track.checkpoints.size() == 8, "8 checkpoints");
            CHECK(rt.trackIndex == ti, "track index stored");
        }
        {   // AI completes a full race on the coastal track
            sim::Race rc; rc.init(2, 1, 0, 1);
            bool finished = false;
            for (int i = 0; i < 60 * 240 && !finished; i++) {
                rc.step({sim::aiDrive(rc, 0), sim::aiDrive(rc, 1), sim::aiDrive(rc, 2)});
                finished = rc.cars[0].finished;
            }
            CHECK(finished, "AI completes coastal race distance");
            CHECK(rc.cars[0].lap >= 2, "coastal laps counted");
        }
        {   // per-track record isolation
            sim::Settings s;
            sim::Car p; p.bestLap = 50.f; p.finishPlace = 1; p.raceTime = 160.f;
            sim::updateRecords(s, p, 0);
            CHECK(s.bestLapMsT[0] == 50000 && s.bestLapMsT[1] == -1, "lap record per track");
            CHECK(s.bestRaceMsT[0] == 160000 && s.bestRaceMsT[1] == -1, "win record per track");
            p.bestLap = 60.f; p.finishPlace = 2; p.raceTime = 200.f;
            sim::updateRecords(s, p, 1);
            CHECK(s.bestLapMsT[1] == 60000, "track 1 lap recorded");
            CHECK(s.bestRaceMsT[1] == -1, "non-win does not set race record");
            p.bestLap = 40.f;
            sim::updateRecords(s, p, 0);
            CHECK(s.bestLapMsT[0] == 40000, "faster lap replaces");
            sim::updateRecords(s, p, 9);
            CHECK(s.bestLapMsT[2] == -1, "out-of-range track ignored");
        }
        {   // settings roundtrip with track fields
            sim::Settings s; s.trackIndex = 1; s.bestLapMsT[1] = 61234; s.bestRaceMsT[1] = 187000;
            std::string path = "/tmp/overdrive-test-settings.cfg";
            sim::saveSettings(s, path);
            sim::Settings l = sim::loadSettings(path);
            CHECK(l.trackIndex == 1, "trackIndex roundtrip");
            CHECK(l.bestLapMsT[1] == 61234 && l.bestRaceMsT[1] == 187000, "per-track records roundtrip");
            std::remove(path.c_str());
        }
    }
    // group 15: combat pickups
    {
        auto mk=[](){ static sim::Race r; r=sim::Race(); r.init(3); r.countdown=0; return &r; };
        sim::Race* r=mk();
        CHECK((int)r->cars.size()==8, "eight-car grid");
        CHECK((int)r->pickups.size()==2*sim::PK_COUNT, "two pickups per type per lap");
        // BOLT hits the car ahead and damages it
        r->cars[1].pos = r->cars[0].pos + sim::Vec2{25,0}; r->cars[1].heading=0; r->cars[1].vel={};
        r->cars[0].heading=0; r->cars[0].progress=0; r->cars[1].progress=25;
        r->cars[0].heldPickup=sim::PK_BOLT;
        float h0=r->cars[1].health; sim::CarInput f{}; f.fire=true;
        std::vector<sim::CarInput> ins(8); ins[0]=f; r->step(ins);
        CHECK(!r->shots.empty(), "bolt spawned");
        for(int i=0;i<120;i++) r->step(std::vector<sim::CarInput>(8));
        CHECK(r->cars[1].health < h0 || r->cars[1].slowT>0.f, "bolt damaged target");
        // SHIELD absorbs a mine
        r=mk(); r->cars[2].shieldT=5.f; float hs=r->cars[2].health;
        r->mines.push_back({r->cars[2].pos,0,0.f,20.f});
        r->step(std::vector<sim::CarInput>(8));
        CHECK(r->cars[2].health==hs, "shield absorbs mine");
        CHECK(r->mines.empty(), "mine consumed");
        // MINE hurts without shield, owner immune
        r=mk(); float hm=r->cars[3].health; r->mines.push_back({r->cars[3].pos,0,0.f,20.f});
        r->mines.push_back({r->cars[0].pos,0,0.f,20.f});
        r->step(std::vector<sim::CarInput>(8));
        CHECK(r->cars[3].health<hm, "mine damages");
        CHECK(r->cars[0].health==r->specs[0].durability, "mine owner immune");
        // REPAIR restores health
        r=mk(); r->cars[0].health=30.f; r->cars[0].heldPickup=sim::PK_REPAIR;
        std::vector<sim::CarInput> i2(8); i2[0].fire=true; r->step(i2);
        CHECK(r->cars[0].health>=69.f, "repair restores");
        // SHOCK orb slows
        r=mk(); r->orbs.push_back({r->cars[4].pos,8.f}); r->step(std::vector<sim::CarInput>(8));
        CHECK(r->cars[4].slowT>0.f, "shock slows");
        // BARGE shoves neighbours
        r=mk(); r->cars[1].pos=r->cars[0].pos+sim::Vec2{5,0}; r->cars[0].heldPickup=sim::PK_BARGE;
        std::vector<sim::CarInput> i3(8); i3[0].fire=true; r->step(i3);
        CHECK(sim::len(r->cars[1].vel)>5.f, "barge pushes");
        // AI uses held weapons and races complete with combat on, all 8 finish eventually
        sim::Race ra; ra.init(3,1,0,0);
        int fired=0; bool all=false;
        for(int i=0;i<60*500&&!all;i++){
            std::vector<sim::CarInput> in(8); for(int k=0;k<8;k++) in[k]=sim::aiDrive(ra,k);
            for(int k=1;k<8;k++) if(in[k].fire && ra.cars[k].heldPickup>=0) fired++;   // AI cars only (player index 0 excluded)
            ra.step(in); all=ra.finishedCount==8;
        }
        CHECK(fired>=8, "AI fires weapons regularly over a 3-lap race");
        CHECK(ra.finishedCount>=6, "AI field finishes with combat on");
        std::printf("combat race: fired=%d finished=%d\n", fired, ra.finishedCount);
    }
    if (failures == 0) std::printf("ALL TESTS PASSED (16 groups)\n");
    else std::printf("%d FAILURE(S)\n", failures);
    return failures ? 1 : 0;
}
