#pragma once
#include "CoreMinimal.h"
#include "WheeledVehicles/FGVehicleSubsystem.h"
#include "FILNRouting.h"

/** Game-thread only, bounded lazy reverse-distance cache. Stock/job changes are not keys. */
class FFILNRoadCache {
 struct FEntry {
  TWeakObjectPtr<UFGVehiclePathNetwork> Network;
  const UFGVehiclePathPreset* Preset=nullptr;
  FGuid Target;
  int32 Revision=0, NetworkId=0, Nodes=0, Segments=0, Cursor=0;
  uint64 Used=0;
  bool Failed=false, Started=false;
  filn::roads::ReverseSearch Search;
  FEntry(UFGVehiclePathNetwork* N,const UFGVehiclePathPreset* P,const FGuid& T)
   :Network(N),Preset(P),Target(T),Revision(N->GetNetworkPathTraversabilityChangelist()),NetworkId(N->GetNetworkID()),
    Nodes(N->GetPathNodes().Num()),Segments(N->GetPathSegments().Num()),Search(Nodes) {}
  bool Current(UFGVehiclePathNetwork* N) const {
   return Network.Get()==N && Revision==N->GetNetworkPathTraversabilityChangelist() && NetworkId==N->GetNetworkID() &&
    Nodes==N->GetPathNodes().Num() && Segments==N->GetPathSegments().Num();
  }
 };
 TArray<TSharedPtr<FEntry>> Entries;
 uint64 Clock=0;
public:
 uint64 Hits=0, Misses=0, Invalidations=0, Work=0;
 void Clear() { Entries.Empty(); }
 // false = computation pending; true with Cost<0 = conclusively unavailable.
 bool Distance(AFGVehicleSubsystem* Vehicles,const FGuid& From,const FGuid& To,const UFGVehiclePathPreset* Preset,double& Cost) {
  check(IsInGameThread()); Cost=-1;
  if(!Vehicles || !Preset || !From.IsValid() || !To.IsValid()) return true;
  auto* N=Vehicles->FindNetworkByPathNodeGuid(To);
  if(!N || N!=Vehicles->FindNetworkByPathNodeGuid(From)) return true;
  if(N->GetPathNodes().Num()>8192 || N->GetPathSegments().Num()>32768) return true;
  for(int32 I=Entries.Num()-1;I>=0;--I) {
   auto& E=Entries[I];
   if(!E->Network.IsValid() || (E->Network.Get()==N && !E->Current(N))) { Entries.RemoveAtSwap(I); ++Invalidations; }
  }
  TSharedPtr<FEntry> Entry;
  for(auto& E:Entries) if(E->Network.Get()==N && E->Preset==Preset && E->Target==To) { Entry=E; break; }
  if(!Entry) {
   ++Misses;
   if(Entries.Num()>=32) {
    int32 Oldest=0; for(int32 I=1;I<Entries.Num();++I) if(Entries[I]->Used<Entries[Oldest]->Used) Oldest=I;
    Entries.RemoveAtSwap(Oldest);
   }
   Entry=MakeShared<FEntry>(N,Preset,To); Entries.Add(Entry);
  } else ++Hits;
  auto& E=*Entry; E.Used=++Clock;
  int32 Budget=128;
  while(E.Cursor<E.Segments && Budget>0) {
   const auto& S=N->GetPathSegments()[E.Cursor++]; --Budget; ++Work;
   if(N->CanVehicleTraverseSegment(S,Preset) && !E.Search.AddEdge(S.FromNodeIndex,S.ToNodeIndex,S.SegmentLength)) E.Failed=true;
  }
  if(E.Cursor<E.Segments) return false;
  if(E.Failed) return true;
  if(!E.Started) {
   E.Started=true;
   const int32 Target=N->FindPathNodeIndexByGuid(To);
   if(Target==INDEX_NONE || !E.Search.Start(Target)) { E.Failed=true; return true; }
  }
  Work+=E.Search.Step(Budget);
  if(!E.Search.Complete()) return false;
  const int32 Source=N->FindPathNodeIndexByGuid(From);
  if(Source!=INDEX_NONE) Cost=E.Search.Distance(Source);
  return true;
 }
};
