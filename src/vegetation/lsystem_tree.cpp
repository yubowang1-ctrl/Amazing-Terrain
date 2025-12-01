#include "lsystem_tree.h"
#include <stack>
#include <random>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace {
std::mt19937 s_rng(1337);
std::uniform_real_distribution<float> s_jitter01(-1.f, 1.f);
}

LSystemTree::LSystemTree(const LSystemParams& p)
    : m_params(p)
{}

void LSystemTree::rewrite(const std::unordered_map<char, std::string>& rules) {
    std::string s = m_string;
    for (int it = 0; it < m_params.iterations; ++it) {
        std::string next;
        next.reserve(s.size() * 3);
        for (char c : s) {
            auto itRule = rules.find(c);
            if (itRule != rules.end()) next += itRule->second;
            else                       next.push_back(c);
        }
        s.swap(next);
    }
    m_string = std::move(s);
}

static glm::mat4 segmentMatrix(const glm::vec3& p0,
                               const glm::vec3& p1,
                               float radius)
{
    glm::vec3 dir = p1 - p0;
    float len = glm::length(dir);
    if (len < 1e-4f) return glm::mat4(1.f);

    glm::vec3 w = glm::normalize(dir);
    glm::vec3 up(0.f, 1.f, 0.f);

    // calculate rotation from up to w
    glm::vec3 axis = glm::cross(up, w);
    float axisLen = glm::length(axis);
    glm::mat4 R(1.f);
    if (axisLen < 1e-4f) {
        if (glm::dot(up, w) < 0.f) {
            // reverse: rotate 180° around the X-axis
            R = glm::rotate(glm::mat4(1.f), glm::pi<float>(), glm::vec3(1.f, 0.f, 0.f));
        }
    } else {
        axis = axis / axisLen;
        float cosAng = glm::clamp(glm::dot(up, w), -1.f, 1.f);
        float angle  = std::acos(cosAng);
        R = glm::rotate(glm::mat4(1.f), angle, axis);
    }

    // original Cylinder height is 1 (y in [-0.5, 0.5]).
    // therefore, the Y scaling factor is the desired segment length len.
    const float overlapK = 1.05f; // slightly > 1, slight overlap between segments to conceal gaps.
    float scaleY = len * overlapK;

    glm::mat4 S = glm::scale(glm::mat4(1.f),
                             glm::vec3(radius, scaleY, radius));

    glm::vec3 mid = 0.5f * (p0 + p1);
    glm::mat4 T = glm::translate(glm::mat4(1.f), mid);

    return T * R * S;
}

void LSystemTree::interpret()
{
    m_branches.clear();
    m_leaves.clear();

    std::stack<Turtle> stack;
    Turtle t;

    // used as a trunk taper / branch tapering
    int  branchDepth = 0;  // at which level is the branch currently occurring (0 indicates the main branch)

    // initial turtle: root at the origin, extending +Y
    t.pos     = glm::vec3(0.f);
    t.forward = glm::vec3(0.f, 1.f, 0.f);
    // t.forward = glm::normalize(glm::vec3((s_jitter01(s_rng) * 0.15f),
    //     1.f, (s_jitter01(s_rng) * 0.15f)));
    t.up      = glm::vec3(0.f, 0.f, 1.f);
    t.right   = glm::cross(t.forward, t.up);
    t.radius  = m_params.baseRadius;

    float baseAngleRad = glm::radians(m_params.baseAngleDeg);
    float jitterMaxRad = glm::radians(m_params.angleJitterDeg);

    auto rotateAround = [&](float sign, const glm::vec3 &axis) {
        float jitter = jitterMaxRad * s_jitter01(s_rng);
        float a      = sign * (baseAngleRad + jitter);
        glm::mat4 R  = glm::rotate(glm::mat4(1.f), a, axis);
        t.forward    = glm::normalize(glm::vec3(R * glm::vec4(t.forward, 0.f)));
        t.right      = glm::normalize(glm::cross(t.forward, t.up));
        t.up         = glm::normalize(glm::cross(t.right, t.forward));
    };

    // small clump of leaves
    auto emitLeafCluster = [&](const glm::vec3 &center, float branchRadius)
    {
        // 1) First, grow a very short branch that extends diagonally from the center.
        // Use forward + up directions, plus a little randomness.
        glm::vec3 twigBaseDir = glm::normalize(0.4f * t.forward + 0.8f * t.up);
        glm::vec3 jitterDir   = glm::normalize(
            glm::vec3(s_jitter01(s_rng), s_jitter01(s_rng), s_jitter01(s_rng)));
        glm::vec3 twigDir     = glm::normalize(twigBaseDir + 0.4f * jitterDir);

        // The length of the twig is somewhat random relative to the stepLength.
        float twigLen = 0.25f * m_params.stepLength *
                        (0.7f + 0.6f * (0.5f + 0.5f * s_jitter01(s_rng))); // ≈ 0.175~0.325
        glm::vec3 twigEnd = center + twigDir * twigLen;

        BranchInstance twig;
        twig.radius = branchRadius * 0.5f; // half the thickness of the current branch
        twig.model  = segmentMatrix(center, twigEnd, twig.radius);
        m_branches.push_back(twig);

        // 2) Grow the leaves near the end of the twig
        glm::vec3 leafCenter = twigEnd;

        float rNorm = glm::clamp(branchRadius / m_params.baseRadius, 0.2f, 1.0f);

        // The thinner the branch, the more leaves there are
        int baseLeafCount  = 26 + int((1.0f - rNorm) * 32.0f);
        int leafCount = int(baseLeafCount * m_params.leafDensity);

        glm::vec3 up    = t.up;
        glm::vec3 right = t.right;
        glm::vec3 fwd   = t.forward;

        // Adjust the distribution radius according to the thickness of the branches;
        float radiusScale = glm::mix(0.6f, 1.1f, 1.0f - rNorm);

        for (int i = 0; i < leafCount; ++i) {
            float u = 0.5f * (s_jitter01(s_rng) + 1.0f); // [0,1]
            float v = 0.5f * (s_jitter01(s_rng) + 1.0f);

            float ang = glm::two_pi<float>() * u;

            // Adjustable: Lateral dispersion radius + offset along the branch direction
            float rr      = (0.01f + 0.02f * v) * radiusScale;   // 0.01~0.03, scaling with thickness
            float along   = 0.01f + 0.03f * v;                  // distance along forward direction
            float upBias  = 0.2f  + 0.8f  * v;                   // further -> higher

            glm::vec3 offset =
                fwd * along +
                std::cos(ang) * right * rr * 1.1f +
                std::sin(ang) * up    * rr * upBias;

            glm::vec3 p = leafCenter + offset;

            // ellipsoidal leaflets, the size of which is also somewhat random.
            float baseScale = 0.010f;
            float s = baseScale * (0.7f + 0.8f * v); // 0.007~0.018
            s *= (0.85f + 0.3f * s_jitter01(s_rng));
            glm::vec3 leafScale = glm::vec3(s, s * 0.55f, s);

            glm::mat4 M = glm::translate(glm::mat4(1.f), p);
            float yaw   = glm::two_pi<float>() * 0.5f * (s_jitter01(s_rng) + 1.f);
            M = glm::rotate(M, yaw, t.up);
            M = glm::scale(M, leafScale);

            LeafInstance leaf;
            leaf.model = M;
            m_leaves.push_back(leaf);
        }
    };


    for (char c : m_string) {
        switch (c) {
        case 'F': {
            glm::vec3 p0 = t.pos;
            float step = m_params.stepLength;
            glm::vec3 p1 = p0 + t.forward * step;
            t.pos = p1;

            // weaker form of tropism, only effective on minor branches ---
            const glm::vec3 tropismDir(0.f, 1.f, 0.f); // world +Y 是“向上”

            if (t.radius < m_params.baseRadius * 0.7f) { // main trunk straight.
                float rNorm = glm::clamp(t.radius / m_params.baseRadius, 0.2f, 1.0f);
                float bendStrength = 0.05f;
                float k = bendStrength * (1.0f - rNorm);

                glm::vec3 newF = glm::normalize(t.forward + tropismDir * k);
                t.forward = newF;
                t.right   = glm::normalize(glm::cross(t.forward, t.up));
                t.up      = glm::normalize(glm::cross(t.right, t.forward));
            }
            // --- End of tropism ---

            BranchInstance seg;
            seg.radius = t.radius;
            seg.model  = segmentMatrix(p0, p1, seg.radius);
            m_branches.push_back(seg);

            // a cluster of small leaves may occasionally hang on slender branch,
            if (t.radius < m_params.baseRadius * 0.8f) {
                float r = 0.5f * (s_jitter01(s_rng) + 1.0f);
                if (r < 0.9f) {
                    emitLeafCluster(t.pos, t.radius);
                }
            }
            break;
        }
        case 'X':
            // a "node": a cluster of leaves grows here.
            emitLeafCluster(t.pos, t.radius);
            break;
        case '+': // yaw (left / right)
            rotateAround(+1.f, t.up);
            break;
        case '-':
            rotateAround(-1.f, t.up);
            break;
        case '&': // pitch
            rotateAround(+1.f, t.right);
            break;
        case '^':
            rotateAround(-1.f, t.right);
            break;
        case '[':
            // push the current turtle state onto the stack.
            stack.push(t);
            // radius of the branch narrows
            t.radius *= m_params.radiusDecay;

            // add a short random roll here to break the plane.
            {
                float roll = jitterMaxRad * 0.7f * s_jitter01(s_rng); // +-(angleJitter*0.7)

                // rotate around the current forward position, changing the up/right position.
                glm::mat4 R = glm::rotate(glm::mat4(1.f), roll, t.forward);
                t.up    = glm::normalize(glm::vec3(R * glm::vec4(t.up,    0.f)));
                t.right = glm::normalize(glm::cross(t.forward, t.up));
            }
            break;

        case ']':
            if (!stack.empty()) {
                t = stack.top();
                stack.pop();
                branchDepth = std::max(0, branchDepth - 1);
            }
            break;

        default:
            break;
        }
    }
}

void LSystemTree::generate(const std::string& axiom,
                           const std::unordered_map<char, std::string>& rules)
{
    m_string = axiom;
    rewrite(rules);
    interpret();
}
