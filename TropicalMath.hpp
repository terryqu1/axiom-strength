#pragma once
#include "node.hpp"
#include "AutogradEngine.hpp"
#include <algorithm>

inline Node* tropical_max(Node* a, Node* b) {
    float max_val = std::max(a->val, b->val);

    Node* out = global_engine.create_node(max_val, {a, b});

    out->_backward = [out, a, b]() {
        if (a->val > b->val) {
            a->grad += out->grad;
        } 
        else if (b->val > a->val) {
            b->grad += out->grad;
        }
        else {
            a->grad += out->grad * 0.5f;
            b->grad += out->grad * 0.5f;
        }
    };

    return out;
}