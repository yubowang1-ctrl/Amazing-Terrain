#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>

struct LSystemParams {
    // Adjustable: params for tree generation
    int   iterations     = 3;      // bush: 2,3 is enough
    float stepLength     = 0.06f;  // Shorter segments → Lower overall height
    float baseAngleDeg   = 35.0f;  // Branching angle -> angle of branches spreading outwards
    float angleJitterDeg = 8.0f;   // random jitter with each rotation (for aesthetic purposes)
    float baseRadius     = 0.03f;  // slightly thinner main trunk
    float radiusDecay    = 0.8f;   // Slow decay prevents branches from becoming too thin.
    float leafDensity = 1.0f;
};

// each branch segment corresponds to a Cylinder model matrix + radius
// reusing proj. 5 setups
struct BranchInstance {
    glm::mat4 model;
    float     radius;
};

struct LeafInstance {
    glm::mat4 model;
};

class LSystemTree {
public:
    explicit LSystemTree(const LSystemParams& p);

    // generate L-system string and interpret it as BranchInstance
    void generate(const std::string& axiom,
                  const std::unordered_map<char, std::string>& rules);

    const std::vector<BranchInstance>& branches() const { return m_branches; }
    const std::vector<LeafInstance>&   leaves()   const { return m_leaves; }

private:
    LSystemParams m_params;
    std::string   m_string;
    std::vector<BranchInstance> m_branches;
    std::vector<LeafInstance> m_leaves;

    void rewrite(const std::unordered_map<char, std::string>& rules);
    void interpret(); // turtle graphics

    struct Turtle {
        glm::vec3 pos;
        glm::vec3 forward;
        glm::vec3 up;
        glm::vec3 right;
        float     radius;
    };
};
