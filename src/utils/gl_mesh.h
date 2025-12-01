#pragma once
#include <GL/glew.h>
#include <vector>
#include <cstddef>

// Interleaved vertex: position(3) + normal(3)
// fitting our lab8 tessellation design
struct GLVertexPN {
    GLfloat x, y, z;      // position
    GLfloat nx, ny, nz;   // normal
};

struct GLMesh{
    GLuint vao = 0, vbo = 0;
    GLsizei vertexCount =0;

    //upload interleaved float array [px, py, pz, nx, ny, ...]
    void uploadinterleavedPN(const std::vector<float> & interlPN){
        if (vao || vbo) destroy();
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     interlPN.size()*sizeof(GLfloat),
                     interlPN.data(), GL_STATIC_DRAW);

        const GLsizei stride = sizeof(GLVertexPN); // 6 floats (24B)

        glEnableVertexAttribArray(0); // a_pos
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<void*>(offsetof(GLVertexPN, x)));

        glEnableVertexAttribArray(1); // a_nor
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<void*>(offsetof(GLVertexPN, nx)));

        glBindVertexArray(0);
        vertexCount = static_cast<GLsizei>(interlPN.size() / 6);
    }

    //upload interleaved float array [px, py, pz, nx, ny, cr, cg, cb]  for voxel terrian generation
    void uploadinterleavedPNC(const std::vector<float> & interlPNC){
        if (vao || vbo) destroy();
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     interlPNC.size()*sizeof(GLfloat),
                     interlPNC.data(), GL_STATIC_DRAW);

        const GLsizei stride = 9 * sizeof(GLfloat); // 9 floats (36B)

        glEnableVertexAttribArray(0); // a_pos
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

        glEnableVertexAttribArray(1); // a_nor
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3*sizeof(GLfloat)));

        glEnableVertexAttribArray(2); // a_col
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(6*sizeof(GLfloat)));

        glBindVertexArray(0);
        vertexCount = static_cast<GLsizei>(interlPNC.size() / 9);
    }

    void draw() const {
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
        glBindVertexArray(0);
    }

    void drawInstanced(GLsizei instanceCount) const {
        if (instanceCount <= 0) return;
        glBindVertexArray(vao);
        glDrawArraysInstanced(GL_TRIANGLES, 0, vertexCount, instanceCount);
        glBindVertexArray(0);
    }

    void destroy() {
        if(vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        vao = vbo = 0;
        vertexCount = 0;
    }
};


