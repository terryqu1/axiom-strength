#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <iostream>
#include "session.hpp"

using namespace std;

int main() {

    ifstream file("training_log2.csv");

    if (!file.is_open()) {
        cout << "Could not open file";
        return -1;
    }

    vector<Session> sessions;

    string line;
    vector<vector<double>> data;

    while (getline(file, line)) {
        stringstream ss(line);
        string value;
        vector<double> row;
        while (getline(ss, value, ',')){
            try {
                if (value == "Bench") {

                }
                double d = stod(value);
                row.push_back(d);
            } catch (invalid_argument& e) {
                cerr << "Input error\n";
            }
            if (!row.empty()) {
                data.push_back(row);
            }
        }
    }


    return 0;
}