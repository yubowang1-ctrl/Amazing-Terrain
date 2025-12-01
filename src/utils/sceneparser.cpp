#include "sceneparser.h"
#include "scenefilereader.h"
#include <glm/gtx/transform.hpp>

#include <chrono>
#include <iostream>

// Helper function: Tree Traverse
static void travNode(SceneNode* node, const glm::mat4& pCTM, RenderData& out){
    if (!node) return;

    // construct current CTM (follow the node's internal sequence)
    glm::mat4 local(1.0f);
    for (SceneTransformation* t : node->transformations){
        switch (t -> type){
        case TransformationType::TRANSFORMATION_TRANSLATE:
            local *= glm::translate(t->translate);
            break;
        case TransformationType::TRANSFORMATION_SCALE:
            local *= glm::scale(t->scale);
            break;
        case TransformationType::TRANSFORMATION_ROTATE:
            local *= glm::rotate(t->angle, t->rotate);
            break;
        case TransformationType::TRANSFORMATION_MATRIX:
            local *= t->matrix;
            break;
        }
    }

    // current CTM
    glm::mat4 ctm = pCTM * local;

    // primitives for current node, write in RenderDat.shapes container
    for (ScenePrimitive* p : node->primitives) {
        RenderShapeData s;
        s.primitive = *p;
        s.ctm = ctm;
        out.shapes.push_back(std::move(s));
    }

    // lights for current node, write in RenderData.lights (converted into world space)
    for (const SceneLight* l : node->lights) {
        SceneLightData L{}; // we have to put SceneLightData info into RenderData.lights container
        L.id = l->id;
        L.type = l->type;
        L.color = l->color;
        L.function = l->function;
        L.penumbra = l->penumbra;
        L.angle = l->angle;

        // location: calculated except for directional light by def
        if (L.type != LightType::LIGHT_DIRECTIONAL){
            L.pos = ctm * glm::vec4(0,0,0,1);
        }
        // location: calculated except for point light by def
        if (L.type != LightType::LIGHT_POINT){
            glm::vec3 dirW = glm::vec3(ctm * glm::vec4(glm::vec3(l->dir), 0.0f));
            // Normalize since the dir of light only need unit vec. (avoid ''unwanted'' transformation by ctm)
            if (glm::length(dirW) > 0.0f) dirW = glm::normalize(dirW);
            L.dir = glm::vec4(dirW, 0.0f);
        }
        out.lights.push_back(std::move(L));
    }

    // traverse children node
    for (SceneNode* child : node->children) {
        travNode(child, ctm, out);
    }
}

bool SceneParser::parse(std::string filepath, RenderData &renderData) {
    ScenefileReader fileReader = ScenefileReader(filepath);
    bool success = fileReader.readJSON();
    if (!success) {
        return false;
    }

    // Task 5: populate renderData with global data, and camera data;
    renderData.cameraData = fileReader.getCameraData();
    renderData.globalData = fileReader.getGlobalData();

    // empty the container first before fill in shapes/lights
    renderData.shapes.clear();
    renderData.lights.clear();

    // Task 6: populate renderData's list of primitives and their transforms.
    //         This will involve traversing the scene graph, and we recommend you
    //         create a helper function to do so!
    SceneNode* root = fileReader.getRootNode();
    travNode(root, glm::mat4(1.0f), renderData);

    return true;
}
