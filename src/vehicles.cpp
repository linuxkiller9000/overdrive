#include "sim.h"
#include <algorithm>

namespace sim {
std::vector<CarSpec> defaultRoster() {
    std::vector<CarSpec> cars;
    auto add=[&](const char* name,int design,float accel,float speed,float steer,float grip,float drift,float brake,float hp,float boost,float duration,float mass) {
        CarSpec c; c.name=name; c.design=design; c.accel=accel; c.maxSpeed=speed;
        c.steerRate=steer; c.grip=grip; c.driftGrip=drift; c.brake=brake;
        c.durability=hp; c.boostAccel=boost; c.boostTime=duration; c.mass=mass;
        c.radius=mass>1.5f?1.3f:1.1f; cars.push_back(c);
    };
    // Every row is a physical tradeoff, shared unchanged by AI and player.
    add("STANDARD",0,14,42,2.60f,9.0f,2.2f,22,110,12,2.50f,1.00f);
    add("SPRINTER",1,18,39,2.40f,8.4f,2.0f,24,85,14,2.20f,.85f);
    add("HAULER",2,11,40,2.05f,11.0f,3.0f,20,180,10,3.20f,1.60f);
    add("AFTERBURN",3,13,48,2.20f,8.0f,2.5f,21,75,16,2.00f,.90f);
    add("SIDEWINDER",4,15,41,3.00f,8.8f,1.2f,25,95,11,2.70f,.95f);
    add("VOLT",5,17,38,3.20f,10.5f,1.8f,27,80,13,2.30f,.75f);
    add("RIDGEBACK",6,12,43,2.35f,12.0f,3.5f,23,150,9,3.50f,1.40f);
    add("PHANTOM",7,16,45,2.75f,7.5f,1.6f,26,90,18,1.80f,.80f);
    return cars;
}
int vehicleUnlockCost(int index) {
    static const int costs[8]={0,0,0,200,240,280,320,400};
    return index>=0 && index<8?costs[index]:-1;
}
bool unlockVehicle(Settings& s,int index) {
    int cost=vehicleUnlockCost(index);
    if(cost<0 || (s.unlocked&(1u<<index)) || s.credits<cost) return false;
    s.credits-=cost; s.unlocked|=(1u<<index); return true;
}
void awardRace(Settings& s,int place) {
    if(place<1) return;
    s.racesCompleted++; if(place==1) s.wins++;
    s.credits+=place==1?120:place==2?80:50;
}
}
