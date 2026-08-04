#pragma once
#include <stdexcept>
#include <vector>
#include "particle_filter.hpp"

struct Lift {
    Action action;
    Observation obs;
    Lift(Action action, Observation obs) : action(action), obs(obs) {
    }
};

class Session {
    private:
    vector<Lift> lifts;
    string date;

    public:

    void addAction(const Action& action, const Observation& obs) {
        lifts.emplace_back(action, obs);
    }

    void setDate(const string& in_date) {
        date = in_date;
    }
    
    // Optional: Getter if you need to read it later
    string getDate() const { return date; }

    const Lift& viewLift() const {
        if (lifts.empty()) {
            throw std::out_of_range("No lifts are present in current session.");
        }
        return lifts.back();
    }

    const vector<Lift>& getLifts() const { 
        return lifts; 
    }


    // initializes a squat variation
    Action buildSquat(double in_leverage_bias, double in_tempo_stress, double in_stance_width, double in_ROM);
    Action buildDeadlift(double leverage_bias, double stance_width, double tempo_stress);
    Action buildBench(double incline_bias, double leverage_bias, double tempo_stress, double range_of_motion);
    friend std::ostream& operator<<(std::ostream& os, const Session& session);
    friend std::ostream& operator<<(std::ostream& os, const Lift& lift);
};