#pragma once
#include <vector>
#include <functional>

using namespace std;

class AutogradEngine;

extern AutogradEngine global_engine;

struct Node {
    float val;
    float grad = 0;
    vector<Node*> dependencies;
    function<void()> _backward = [](){};
};

Node* operator*(Node& a, Node& b);