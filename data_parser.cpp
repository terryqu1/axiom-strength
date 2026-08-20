#include <string>
#include <vector>
#include "session.hpp"
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <sstream>

Action buildSquat(double in_leverage_bias, double in_tempo_stress, double in_stance_width, double in_ROM) {
    squat squat_instance;
    squat_instance.leverage_bias = in_leverage_bias;
    squat_instance.leverage_bias = in_leverage_bias;
    squat_instance.tempo_stress = in_tempo_stress;
    squat_instance.stance_width = in_stance_width;
    squat_instance.range_of_motion = in_ROM;

    Action action;
    action.lift_state = squat_instance;

    return action;
}

Action buildDeadlift(double leverage_bias, double stance_width, double tempo_stress) {
    deadlift deadlift_instance;

    deadlift_instance.leverage_bias = leverage_bias;
    deadlift_instance.stance_width = stance_width;
    deadlift_instance.tempo_stress = tempo_stress;
    
    Action action;
    action.lift_state = deadlift_instance;

    return action;
}

Action buildBench(double incline_bias, double leverage_bias, double tempo_stress, double range_of_motion) {
    bench bench_instance;

    bench_instance.incline_bias = incline_bias;
    bench_instance.leverage_bias = leverage_bias;
    bench_instance.tempo_stress = tempo_stress;
    bench_instance.range_of_motion = range_of_motion;
    
    Action action;
    action.lift_state = bench_instance;

    return action;
}

std::unordered_map<string, Action> lift_variations = {
    {"High Bar Pause Squat", buildSquat(0.3, 1, 0.3, 0)}
};

vector<Session> initializeLifts(string training_log_file) {

    ifstream training_log_ifstream(training_log_file);

    if (!training_log_ifstream.is_open()) {
        throw invalid_argument("Couldn't open " + training_log_file);
    }

    vector<Session> Sessions;
    string line;

    // 1. Skip the CSV Header Row
    getline(training_log_ifstream, line); 

    // 2. Track the date outside the loop to group actions
    string current_date = "";
    Session current_session;

    while (getline(training_log_ifstream, line)) {
        stringstream ss(line);
        string parsed_string;
        
        Action new_action;
        Observation new_obs;
        string row_date;
        int counter = 1;

        // Parse a single Action (one row)
        while (getline(ss, parsed_string, ',')) {
            
            // Skip empty columns if they happen
            if (parsed_string.empty()) {
                counter++;
                continue;
            }

            switch (counter) {
                case 1: // Date
                    row_date = parsed_string;
                    break;
                case 3: { // Exercise Type
                    auto iter = lift_variations.find(parsed_string);
                    
                    if (iter != lift_variations.end()) {
                        new_action = iter->second; 
                    } else {
                        if (parsed_string.find("Bench") != string::npos || parsed_string.find("Spoto") != string::npos) {
                            // Defaults for Competition Bench
                            double incline = 0.0, leverage = 0.3, tempo = 0.0, rom = 0.0; 

                            vector<pair<string, double>> inclines = {{"Incline", 0.3}, {"Shoulder Press", 1.0}};
                            vector<pair<string, double>> grips = {{"Close", 0.1}, {"Wide", 0.7}};
                            vector<pair<string, double>> tempos = {{"Pause", 1.0}, {"3 Count", 3.0}, {"Long Pause", 5.0}};
                            vector<pair<string, double>> roms = {{"Spoto", 0.3}, {"Board", 0.3}};

                            for (const auto& mod : inclines) if (parsed_string.find(mod.first) != string::npos) incline = mod.second;
                            for (const auto& mod : grips) if (parsed_string.find(mod.first) != string::npos) leverage = mod.second;
                            for (const auto& mod : tempos) if (parsed_string.find(mod.first) != string::npos) tempo = mod.second;
                            for (const auto& mod : roms) if (parsed_string.find(mod.first) != string::npos) rom = mod.second;
                            
                            new_action = buildBench(incline, leverage, tempo, rom);

                        } else if (parsed_string.find("Squat") != string::npos || parsed_string.find("Good Morning") != string::npos) {
                            // Defaults for Normal Comp Squat
                            double leverage = 0.7, tempo = 0.0, stance = 0.3, rom = 0.0; 

                            vector<pair<string, double>> leverages = {{"Front", 0.0}, {"High Bar", 0.3},{"Low Bar", 0.7}, {"Good Morning", 1.0}};
                            vector<pair<string, double>> tempos = {{"Pause", 1.0}, {"Tempo", 3.0}};
                            vector<pair<string, double>> stances = {{"Close", 0.0}, {"Wide", 0.7}};
                            vector<pair<string, double>> roms = {{"Deep", 1.0}, {"Deficit", 1.0}};

                            for (const auto& mod : leverages) if (parsed_string.find(mod.first) != string::npos) leverage = mod.second;
                            for (const auto& mod : tempos) if (parsed_string.find(mod.first) != string::npos) tempo = mod.second;
                            for (const auto& mod : stances) if (parsed_string.find(mod.first) != string::npos) stance = mod.second;
                            for (const auto& mod : roms) if (parsed_string.find(mod.first) != string::npos) rom = mod.second;

                            new_action = buildSquat(leverage, tempo, stance, rom);

                        } else if (parsed_string.find("Deadlift") != string::npos || parsed_string.find("DL") != string::npos) {
                            // Defaults for Conventional Deadlift
                            double leverage = 1.0, stance = 0.4, tempo = 0.0; 

                            vector<pair<string, double>> stances = {{"Narrow", 0.0}, {"Hybrid", 0.4}, {"Sumo", 0.7}, {"Ultra Sumo", 1.0}};
                            vector<pair<string, double>> tempos = {{"Pause", 2.0}, {"Long Pause", 5.0}};

                            for (const auto& mod : stances) {
                                if (parsed_string.find(mod.first) != string::npos) {
                                    stance = mod.second;
                                    if (mod.first == "Sumo" || mod.first == "Ultra Sumo") leverage = 0.0;
                                    if (mod.first == "Hybrid") leverage = 0.5;
                                }
                            }
                            for (const auto& mod : tempos) if (parsed_string.find(mod.first) != string::npos) tempo = mod.second;

                            new_action = buildDeadlift(leverage, stance, tempo);
                        }

                    }
                    break;
                }
                case 4: { // Weight
                    if (!parsed_string.empty()) {
                        try {
                            double weight = stod(parsed_string);

                            std::visit([weight](auto&& alt) {
                                using T = std::decay_t<decltype(alt)>;
                                if constexpr (std::is_same_v<T, std::monostate>) {
                                    ;
                                } else {
                                    alt.load = weight;
                                }
                            }, new_action.lift_state);
                        } catch (const invalid_argument& e) {
                            cout << "Invalid input: string isn't a number\n";
                        }
                    }
                    break;
                }
                case 5: { // Reps
                    if (!parsed_string.empty()) {
                        try {
                            double reps = stod(parsed_string);
                            std::visit([reps](auto&& alt) {
                                using T = std::decay_t<decltype(alt)>;
                                if constexpr (std::is_same_v<T, std::monostate>) {
                                    ;
                                } else {
                                    alt.reps = reps;
                                }
                            }, new_action.lift_state);
                            
                        } catch (const invalid_argument& e) {
                            cout << "Invalid input: string isn't a number\n";
                        }
                    }
                    break;
                }
                case 6: { // RPE
                    if (!parsed_string.empty()) {
                        try {
                            double rpe = stod(parsed_string);

                            std::visit([rpe](auto&& alt){
                                using T = std::decay_t<decltype(alt)>;
                                if constexpr (std::is_same_v<T, std::monostate>) {
                                    ;
                                } else {
                                    alt.rpe = rpe;
                                }
                            }, new_action.lift_state);
                            
                        } catch (const invalid_argument& e) {}
                    }
                    break;
                }
                case 8:
                    new_obs.bodyweight = stod(parsed_string);
                    break;
                case 9:
                    new_obs.active_energy_burned = stod(parsed_string);
                    break;
                case 10:
                    new_obs.basal_energy_burned = stod(parsed_string);
                    break;
                case 11: // total sleep minutes
                    new_obs.total_sleep_minutes = stod(parsed_string);
                    break;
                case 15:
                    new_obs.hrv = stod(parsed_string);
                    break;
                case 18: // set mean velocity m/s
                    new_obs.set_mean_velocity = stod(parsed_string);
                    break;
                case 24:
                    new_obs.velocity_loss = stod(parsed_string);
                    break;
                case 25: // set mean force N
                    new_obs.set_mean_force = stod(parsed_string);
                    break;
                case 27:
                    new_obs.peak_force = stod(parsed_string);
            }
            counter++;
        }

        // 3. Session Grouping Logic
        // If the date changed, we've moved to a new workout day.
        if (row_date != current_date) {
            // As long as it's not the very first line, save the completed session
            if (!current_date.empty()) {
                Sessions.push_back(current_session);
            }
            // Reset for the new session
            current_session = Session(); 
            current_session.setDate(row_date);
            current_date = row_date;
        }

        // Add the parsed Action to the currently active Session
        // (Assuming your Session class has a vector called `actions` or similar)
        current_session.addAction(new_action, new_obs); 
    }

    // 4. Final Push
    // The loop ends before the very last session gets pushed, so we do it here.
    if (!current_date.empty()) {
        Sessions.push_back(current_session);
    }

    return Sessions;
}