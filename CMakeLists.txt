cmake_minimum_required(VERSION 3.10)
project(PowerliftingAI)

# 1. Enforce C++20 standard for std::span and std::variant
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 2. Enable aggressive compiler optimization (CRITICAL for CEM-MPC speed)
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif()

# (Optional) Group headers so they show up neatly in IDEs like VSCode or CLion
set(HEADERS
    session.hpp
    particle_filter.hpp
    cem_mpc.hpp
    box_muller.hpp
)

# 3. Create the executable and link all implementation files
# Note: Replace dataparser.cpp with the actual filename of your parser if different
add_executable(Proj 
    main.cpp 
    particle_filter.cpp 
    data_parser.cpp 
    box_muller.cpp
    gaussianProcess.cpp
    ${HEADERS}
)