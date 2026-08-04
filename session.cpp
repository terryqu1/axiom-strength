// class for session
#include "session.hpp"
#include "particle_filter.hpp"
#include <iostream>

using namespace std;

std::ostream& operator<<(std::ostream& os, const Lift& lift) {
    os << "Action: " << lift.action << " | Obs: " << lift.obs;
    return os;
}

std::ostream& operator<<(std::ostream& os, const Session& session) {
    os << "Date: " << session.getDate() << ":\n";
    for (auto const& lift : session.lifts) {
        os << lift << "\n";
    }
    return os;
}