#pragma once
#include "node.hpp"

class AutogradEngine {
private:
    std::vector<Node*> arena;

public: 
    Node* create_node(float val, std::vector<Node*> deps) {
        Node* out = new Node;
        out->val = val;
        out->dependencies = deps;
        arena.push_back(out);
        return out;
    }

};