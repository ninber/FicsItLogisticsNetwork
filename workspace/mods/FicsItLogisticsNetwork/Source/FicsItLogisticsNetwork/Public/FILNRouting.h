#pragma once
#include <cmath>
#include <cstddef>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

namespace filn::roads {
// A copied directed, vehicle-compatible graph. Never reads an actor off-thread.
class ReverseSearch {
    struct Edge { std::size_t from; double length; };
    std::vector<std::vector<Edge>> incoming;
    std::vector<double> distances;
    using Candidate = std::pair<double,std::size_t>;
    std::priority_queue<Candidate,std::vector<Candidate>,std::greater<Candidate>> queue;
    std::size_t active = 0, edge = 0;
    double activeDistance = 0;
    bool expanding = false, started = false, complete = false;
public:
    explicit ReverseSearch(std::size_t count) : incoming(count), distances(count,std::numeric_limits<double>::infinity()) {}
    bool AddEdge(std::size_t from,std::size_t to,double length) {
        if(started || from>=incoming.size() || to>=incoming.size() || !std::isfinite(length) || length<0) return false;
        incoming[to].push_back({from,length}); return true;
    }
    bool Start(std::size_t target) {
        if(started || target>=incoming.size()) return false;
        started=true; distances[target]=0; queue.emplace(0,target); return true;
    }
    // Each heap pop and each examined edge costs one unit. A high-degree vertex
    // cannot evade the work budget. Incomplete results must never be dispatched.
    std::size_t Step(std::size_t budget) {
        std::size_t work=0;
        while(started && !complete && work<budget) {
            if(expanding) {
                if(edge==incoming[active].size()) { expanding=false; continue; }
                const auto e=incoming[active][edge++]; ++work;
                const double next=activeDistance+e.length;
                if(std::isfinite(next) && next<distances[e.from]) {
                    distances[e.from]=next; queue.emplace(next,e.from);
                }
            } else {
                if(queue.empty()) { complete=true; break; }
                const auto [cost,node]=queue.top(); queue.pop(); ++work;
                if(cost!=distances[node]) continue;
                active=node; activeDistance=cost; edge=0; expanding=true;
            }
        }
        return work;
    }
    bool Complete() const { return complete; }
    double Distance(std::size_t from) const {
        if(!complete || from>=distances.size() || !std::isfinite(distances[from])) return -1;
        return distances[from];
    }
};
}
