#include "GpuTimer.hpp"

#include <GL/glew.h>
#include <GL/glut.h>

#include <iostream>
#include <string>
#include <vector>
#include <cassert>

#include <assimp/Importer.hpp> // C++ interface.
#include <assimp/DefaultLogger.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#define ASSERT(expr) assert(expr)
#define LOG(expr) std::cout << #expr << " = " << (expr) << std::endl

const std::string kMeshFilename = "armadillo.obj";
int gWinId = -1;
int gNumIndices = -1;
GpuTimer gGpuTimer;
SampleStats<float, 1000> gSS;
int gFrameCounter = 0;

struct Vertex
{
    aiVector3D position;
    aiVector3D normal;
};
typedef unsigned int Index;

void keyboard(unsigned char key, int x, int y);
void reshape(int, int);
void display();
void setupShaders();
void setupMesh();

int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitWindowSize(1280, 720);
    glutInitWindowPosition(100, 100);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
    gWinId = glutCreateWindow("Assimp");
    glutKeyboardFunc(keyboard);
    glutDisplayFunc(display);
    glutIdleFunc(display);
    glutReshapeFunc(reshape);

    glewInit();

    setupShaders();
    setupMesh();
    glutMainLoop();

    return 0;
}

void keyboard(unsigned char key, int x, int y)
{
    if (key == 27) { // Escape.
        glutDestroyWindow(gWinId);
        return;
    }
    glutPostRedisplay();
}

void reshape(int width, int height)
{
    glViewport(0, 0, width, height);
}

void display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE); // More geometry needs to be processed.

    gGpuTimer.start();
    glDrawElements(GL_TRIANGLES, gNumIndices, GL_UNSIGNED_INT, 0);
    gSS.add(gGpuTimer.elapsed());

    gFrameCounter++;
    if (gFrameCounter == 1000) { // Log every 1000 frames (about 5 seconds on my ancient laptop).
        gFrameCounter = 0;
        LOG(gSS.average());
        LOG(gSS.min());
        LOG(gSS.max());
    }

    glutSwapBuffers();
}

void setupMesh()
{
    // Log everything.
    Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE, aiDefaultLogStream_STDOUT);

    Assimp::Importer assImport;
    const aiScene* assScene = assImport.ReadFile(kMeshFilename.c_str(),
                                                 aiProcess_GenSmoothNormals      |
                                                 aiProcess_Triangulate           |
                                                 aiProcess_JoinIdenticalVertices |
                                                 aiProcess_ImproveCacheLocality); // Tipsify optimization.
    ASSERT(assScene);
    ASSERT(assScene->mNumMeshes == 1);
    ASSERT(assScene->mMeshes[0]->HasNormals());

    const aiMesh* mesh = assScene->mMeshes[0];
    LOG(assScene->mMeshes[0]->mNumVertices);
    LOG(assScene->mMeshes[0]->mNumFaces);

    ASSERT(sizeof(aiVector3D) == 3*4);
    ASSERT(sizeof(Vertex) == 6*4);

    // Interleave positions and normals.
    std::vector<Vertex> vertices;
    vertices.reserve(mesh->mNumVertices);
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        Vertex v;
        v.position = mesh->mVertices[i];
        v.normal = mesh->mNormals[i];
        vertices.push_back(v);
    }

    GLuint vb;
    glGenBuffers(1, &vb);
    glBindBuffer(GL_ARRAY_BUFFER, vb);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<GLvoid*>(3*4));

    std::vector<Index> indices;
    indices.reserve(mesh->mNumFaces * 3);
    for (unsigned int face = 0; face < mesh->mNumFaces; face++) {
        indices.push_back(mesh->mFaces[face].mIndices[0]);
        indices.push_back(mesh->mFaces[face].mIndices[1]);
        indices.push_back(mesh->mFaces[face].mIndices[2]);
    }
    gNumIndices = indices.size();

    GLuint ib;
    glGenBuffers(1, &ib);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(Index), &indices[0], GL_STATIC_DRAW);

    ASSERT(glGetError() == GL_NO_ERROR);
}

void setupShaders()
{
    const std::string vsSource = "attribute vec3 position;\n"
                                 "attribute vec3 normal;\n"
                                 "varying vec3 vNormal;\n"
                                 "uniform mat4 mvp;\n"
                                 "void main() { vNormal = normal; gl_Position = mvp * vec4(position, 1.0); }\n";
    const std::string fsSource = "varying vec3 vNormal;\n"
                                 "vec3 normal = normalize(vNormal);\n"
                                 "vec3 diffuse = max(0.0, dot(normal, vec3(0.0, -1.0, 0.0))) * vec3(0.7, 0.7, 0.0);\n"
                                 "void main() { gl_FragColor.rgb = diffuse; }\n";
    const GLenum types[] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER};
    GLuint ids[2];
    for (int i = 0; i < 2; i++) {
        ids[i] = glCreateShader(types[i]);
        const char* ptr = (i == 0) ? &vsSource[0] : &fsSource[0];
        glShaderSource(ids[i], 1, &ptr, 0);
        glCompileShader(ids[i]);
        GLint status = 0;
        glGetShaderiv(ids[i], GL_COMPILE_STATUS, &status);
        ASSERT(status);
    }

    GLuint shaderId = glCreateProgram();
    glAttachShader(shaderId, ids[0]);
    glAttachShader(shaderId, ids[1]);
    glBindAttribLocation(shaderId, 0, "position");
    glBindAttribLocation(shaderId, 1, "normal");
    glLinkProgram(shaderId);
    GLint linked = 0;
    glGetProgramiv(shaderId, GL_LINK_STATUS, &linked);
    ASSERT(linked);

    GLint mvpLoc = glGetUniformLocation(shaderId, "mvp");
    ASSERT(mvpLoc != -1);
    glUseProgram(shaderId);
    const float mvp[16] = {
       -0.67087, -0.273615,    0.235224, 0.229416,
       0,         0.912049,    0.705671, 0.688247,
       0.223623, -0.820844,    0.705671, 0.688247,
       0,        -7.49333e-08, 2.03209,  2.17945};
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp);

    ASSERT(glGetError() == GL_NO_ERROR);
}
