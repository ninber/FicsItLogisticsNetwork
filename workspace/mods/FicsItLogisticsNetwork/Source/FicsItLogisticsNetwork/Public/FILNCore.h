#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <array>
namespace filn {
enum class State : std::uint8_t { Idle, Reserved, ToProvider, Loading, ToRequester, Unloading, Returning, Completed, Recovery };
enum class Event { ProviderDock, ProviderLeave, RequesterDock, RequesterLeave, HomeStopped };
inline State Advance(State s, Event e) {
 if(s==State::ToProvider && e==Event::ProviderDock) return State::Loading;
 if(s==State::Loading && e==Event::ProviderLeave) return State::ToRequester;
 if(s==State::ToRequester && e==Event::RequesterDock) return State::Unloading;
 if(s==State::Unloading && e==Event::RequesterLeave) return State::Returning;
 if(s==State::Returning && e==Event::HomeStopped) return State::Completed;
 return State::Recovery;
}
inline State AfterReload(State s) { return s==State::Idle ? s : State::Recovery; }
class Ownership {
 std::unordered_map<std::string,std::string> Trucks, Bays;
public:
 bool Pair(const std::string& truck,const std::string& bay) {
  if(truck.empty() || bay.empty()) return false;
  auto t=Trucks.find(truck); if(t!=Trucks.end()) return t->second==bay;
  if(Bays.contains(bay)) return false;
  Trucks.emplace(truck,bay); Bays.emplace(bay,truck); return true;
 }
 bool CanMove(const std::string& truck,const std::string& oldBay,const std::string& newBay) const {
  const auto t=Trucks.find(truck), b=Bays.find(oldBay);
  if(newBay.empty() || t==Trucks.end() || b==Bays.end() || t->second!=oldBay || b->second!=truck) return false;
  return newBay==oldBay || !Bays.contains(newBay);
 }
 bool Move(const std::string& truck,const std::string& oldBay,const std::string& newBay) {
  if(!CanMove(truck,oldBay,newBay)) return false;
  if(oldBay==newBay) return true;
  Bays.emplace(newBay,truck); Trucks.at(truck)=newBay; Bays.erase(oldBay); return true;
 }
 bool Unpair(const std::string& truck,const std::string& bay) {
  auto t=Trucks.find(truck), b=Bays.find(bay);
  if(t==Trucks.end()||b==Bays.end()||t->second!=bay||b->second!=truck) return false;
  Trucks.erase(t); Bays.erase(b); return true;
 }
};
// Only called on the authoritative game thread. No partial mutation on ordinary rejection.
// M1 capacity is one; M2 can replace slot keys while preserving this atomic contract.
class Reservations {
 using Keys=std::array<std::string,3>;
 std::unordered_map<std::string,Keys> Jobs;
 std::unordered_map<std::string,std::string> Occupied;
public:
 bool Acquire(const std::string& job,const std::string& truck,const std::string& provider,const std::string& requester) {
  if(job.empty()||truck.empty()||provider.empty()||requester.empty()||provider==requester||Jobs.contains(job)) return false;
  Keys keys={"truck:"+truck,"station:"+provider,"station:"+requester};
  for(const auto& key:keys) if(Occupied.contains(key)) return false;
  Jobs.emplace(job,keys); for(const auto& key:keys) Occupied.emplace(key,job); return true;
 }
 bool AcquireDelivery(const std::string& job,const std::string& truck,const std::string& requester) {
  if(job.empty() || truck.empty() || requester.empty() || Jobs.contains(job)) return false;
  Keys keys={"truck:"+truck,"station:"+requester,""};
  for(const auto& key:keys) if(!key.empty() && Occupied.contains(key)) return false;
  Jobs.emplace(job,keys);
  for(const auto& key:keys) if(!key.empty()) Occupied.emplace(key,job);
  return true;
 }
 void Release(const std::string& job) {
  auto it=Jobs.find(job); if(it==Jobs.end()) return;
  for(const auto& key:it->second) Occupied.erase(key);
  Jobs.erase(it);
 }
 bool OwnsJob(const std::string& job,const std::string& truck) const {
  const auto i=Jobs.find(job);
  return i!=Jobs.end()&&i->second[0]=="truck:"+truck;
 }
 bool StationBusy(const std::string& station) const { return Occupied.contains("station:"+station); }
 std::size_t Size() const { return Jobs.size(); }
};
}
