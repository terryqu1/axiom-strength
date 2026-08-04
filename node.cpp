#include "node.hpp"
#include "AutogradEngine.hpp"

Node* operator*(Node& a, Node& b) {
    float out_val = a.val * b.val;

    Node* out = global_engine.create_node(out_val, {&a,&b});

    out->_backward = [out, &a, &b]() {
        a.grad += out->grad * b.val;
        b.grad += out->grad * a.val;
    };

    return out;
}