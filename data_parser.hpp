#pragma once
#include <string>
#include <vector>
#include "session.hpp"
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <sstream>


vector<Session> initializeLifts(string training_log_file);