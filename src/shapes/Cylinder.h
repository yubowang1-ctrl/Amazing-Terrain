#pragma once

#include <vector>
#include <glm/glm.hpp>

class Cylinder
{
public:
    void updateParams(int param1, int param2);
    std::vector<float> generateShape() { return m_vertexData; }

private:
    void insertVec3(std::vector<float> &data, glm::vec3 v);
    void setVertexData();

    void makeWedge(float th0, float th1);               // one radial wedge: side + top/bottom caps
    void makeSideStrip(float th0, float th1);           // vertical strip on the curved side
    void makeCapRing(bool isTop, float th0, float th1); // ring sector on top/bottom cap

    // Members
    std::vector<float> m_vertexData;
    int   m_param1 = 1;   // vertical (height) tessellation
    int   m_param2 = 3;   // radial tessellation
    float m_radius = 0.5f;
    float m_yTop   = 0.5f;
    float m_yBot   = -0.5f;
};
