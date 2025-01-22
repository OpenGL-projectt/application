#include <GL/freeglut.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>
#include <vector>
#include <cfloat>
#include <set>
#include <unordered_map>
#include <chrono> // For time keeping

// Paramètres de la caméra
float cameraAngleX = 0.0f;
float cameraAngleY = 0.0f;
float cameraDistance = 5.0f;
float cameraPosX = 0.0f;
float cameraPosY = 0.0f;

// Contrôle de la souris
bool isDragging = false;
int lastMouseX, lastMouseY;

// Données du modèle
const aiScene* scene = nullptr;
Assimp::Importer importer;
std::string modelPath = "drone.obj";

// Sélection
std::set<int> selectedMeshes;
bool selectionMode = false;

// Visibilité des meshes
std::unordered_map<int, bool> meshVisibility;
std::unordered_map<int, GLfloat[4]> meshColors;
std::unordered_map<int, aiVector3D> meshPositions;
std::unordered_map<int, float> meshRotations; // Rotation around the Y-axis

// Animation
bool isAnimating = false;
auto animationStartTime = std::chrono::steady_clock::now(); // Record the animation start time

// Déplacement des meshes sélectionnés
float selectedMeshTranslateX = 0.0f;
float selectedMeshTranslateY = 0.0f;
float selectedMeshTranslateZ = 0.0f;

// Sources de lumière
const int NUM_LIGHTS = 4;
bool lightEnabled[NUM_LIGHTS] = {true, true, true, true};
GLfloat lightColors[NUM_LIGHTS][4] = {
    {1.0f, 1.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.0f}
};

// Fonction pour charger le modèle et calculer la distance initiale
float calculateInitialDistance(const aiScene* scene) {
    aiVector3D min(FLT_MAX, FLT_MAX, FLT_MAX);
    aiVector3D max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[i];
        for (unsigned int j = 0; j < mesh->mNumVertices; ++j) {
            aiVector3D vertex = mesh->mVertices[j];
            min.x = std::min(min.x, vertex.x);
            min.y = std::min(min.y, vertex.y);
            min.z = std::min(min.z, vertex.z);
            max.x = std::max(max.x, vertex.x);
            max.y = std::max(max.y, vertex.y);
            max.z = std::max(max.z, vertex.z);
        }
    }

    aiVector3D size = max - min;
    return std::max({size.x, size.y, size.z}) * 2.0f;
}

void loadModel(const std::string& path) {
    scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "Erreur de chargement du modèle : " << importer.GetErrorString() << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "Modèle chargé avec succès : " << path << std::endl;

    std::cout << "Nombre de meshes dans le modèle : " << scene->mNumMeshes << std::endl;

    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[i];
        std::string meshName = mesh->mName.C_Str();
        std::cout << "Mesh " << i << " : " << meshName << std::endl;

        meshVisibility[i] = true;

        GLfloat defaultColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        std::copy(defaultColor, defaultColor + 4, meshColors[i]);

        meshPositions[i] = aiVector3D(0.0f, 0.0f, 0.0f);
        meshRotations[i] = 0.0f; // Initialize rotation
    }

    cameraDistance = calculateInitialDistance(scene);
}

// Fonction récursive pour dessiner le modèle
void renderNode(const aiNode* node, const aiScene* scene, bool selectionMode = false, bool renderSelectedOnly = false) {
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        int meshIndex = node->mMeshes[i];

        if (!meshVisibility[meshIndex]) {
            continue;
        }

        if (renderSelectedOnly && selectedMeshes.find(meshIndex) == selectedMeshes.end()) {
            continue;
        }

        if (selectionMode) {
            glPushName(meshIndex);
        }

        if (!renderSelectedOnly) {
            glColor4fv(meshColors[meshIndex]);
        }

        aiMesh* mesh = scene->mMeshes[meshIndex];

        glPushMatrix();
        aiVector3D position = meshPositions[meshIndex];
        glTranslatef(position.x, position.y, position.z);
        glRotatef(meshRotations[meshIndex], 0.0f, 1.0f, 0.0f); // Apply Rotation

        glBegin(GL_TRIANGLES);
        for (unsigned int j = 0; j < mesh->mNumFaces; j++) {
            aiFace face = mesh->mFaces[j];
            for (unsigned int k = 0; k < face.mNumIndices; k++) {
                unsigned int index = face.mIndices[k];
                if (mesh->HasNormals()) {
                    aiVector3D normal = mesh->mNormals[index];
                    glNormal3f(normal.x, normal.y, normal.z);
                }
                if (mesh->HasTextureCoords(0)) {
                    aiVector3D texCoord = mesh->mTextureCoords[0][index];
                    glTexCoord2f(texCoord.x, texCoord.y);
                }
                aiVector3D vertex = mesh->mVertices[index];
                glVertex3f(vertex.x, vertex.y, vertex.z);
            }
        }
        glEnd();

        glPopMatrix();

        if (selectionMode) {
            glPopName();
        }
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        renderNode(node->mChildren[i], scene, selectionMode, renderSelectedOnly);
    }
}

// Initialiser OpenGL
void initOpenGL() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);

    GLfloat lightPosition[NUM_LIGHTS][4] = {
        {0.0f, -1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {-1.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 0.0f}
    };

    for (int i = 0; i < NUM_LIGHTS; ++i) {
        glLightfv(GL_LIGHT0 + i, GL_POSITION, lightPosition[i]);
        glLightfv(GL_LIGHT0 + i, GL_DIFFUSE, lightColors[i]);
        glLightfv(GL_LIGHT0 + i, GL_SPECULAR, lightColors[i]);
        glEnable(GL_LIGHT0 + i);
    }

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
}

// Fonction pour gérer la molette de la souris
void mouseWheel(int wheel, int direction, int x, int y) {
    if (direction > 0) {
        cameraDistance -= 0.5f;
    } else {
        cameraDistance += 0.5f;
    }

    if (cameraDistance < 1.0f) cameraDistance = 1.0f;
    if (cameraDistance > 50.0f) cameraDistance = 50.0f;

    glutPostRedisplay();
}

// Fonction d'affichage
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    glTranslatef(0.0f, 0.0f, -cameraDistance);
    glRotatef(cameraAngleX, 1.0f, 0.0f, 0.0f);
    glRotatef(cameraAngleY, 0.0f, 1.0f, 0.0f);
    glTranslatef(-cameraPosX, -cameraPosY, 0.0f);

    if (scene && scene->mRootNode) {
        renderNode(scene->mRootNode, scene);
    }

    if (selectionMode && !selectedMeshes.empty()) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(2.0f);
        glColor3f(0.0f, 0.0f, 0.0f);

        renderNode(scene->mRootNode, scene, false, true);

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glutSwapBuffers();
}

void updateAnimation() {
     if (isAnimating && !selectedMeshes.empty()) {
         auto currentTime = std::chrono::steady_clock::now();
         auto elapsedSeconds = std::chrono::duration<float>(currentTime - animationStartTime).count();
         
        float rotationSpeed = 100.0f; // Rotation speed in degrees per second
         
         for(int meshIndex : selectedMeshes) {
            meshRotations[meshIndex] = elapsedSeconds * rotationSpeed;
         }

         glutPostRedisplay();
    }
}


// Fonction de sélection d'un objet
void selectObject(int x, int y, bool addToSelection) {
    GLuint buffer[512];
    GLint viewport[4];

    glGetIntegerv(GL_VIEWPORT, viewport);
    glSelectBuffer(512, buffer);
    glRenderMode(GL_SELECT);

    glInitNames();
    glPushName(0);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluPickMatrix(x, viewport[3] - y, 5.0, 5.0, viewport);
    gluPerspective(45.0, (double)viewport[2] / (double)viewport[3], 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);

    glPushMatrix();
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -cameraDistance);
    glRotatef(cameraAngleX, 1.0f, 0.0f, 0.0f);
    glRotatef(cameraAngleY, 0.0f, 1.0f, 0.0f);
    glTranslatef(-cameraPosX, -cameraPosY, 0.0f);

    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        glLoadName(i);

        aiMesh* mesh = scene->mMeshes[i];
        glBegin(GL_TRIANGLES);
        for (unsigned int j = 0; j < mesh->mNumFaces; ++j) {
            aiFace face = mesh->mFaces[j];
            for (unsigned int k = 0; k < face.mNumIndices; ++k) {
                unsigned int index = face.mIndices[k];
                aiVector3D vertex = mesh->mVertices[index];
                glVertex3f(vertex.x, vertex.y, vertex.z);
            }
        }
        glEnd();
    }

    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    GLint hits = glRenderMode(GL_RENDER);
    if (hits > 0) {
        int selectedIdx = buffer[3];

        if (addToSelection) {
            if (selectedMeshes.find(selectedIdx) != selectedMeshes.end()) {
                selectedMeshes.erase(selectedIdx);
                std::cout << "Mesh désélectionnée : " << scene->mMeshes[selectedIdx]->mName.C_Str() << "\n";
            } else {
                selectedMeshes.insert(selectedIdx);
                std::cout << "Mesh sélectionnée : " << scene->mMeshes[selectedIdx]->mName.C_Str() << "\n";
            }
        } else {
            selectedMeshes.clear();
            selectedMeshes.insert(selectedIdx);
            std::cout << "Mesh sélectionnée : " << scene->mMeshes[selectedIdx]->mName.C_Str() << "\n";
        }
    } else {
        if (!addToSelection) {
            selectedMeshes.clear();
        }
    }

    glutPostRedisplay();
}

// Gestion des événements souris
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        if (selectionMode) {
            bool addToSelection = (glutGetModifiers() & GLUT_ACTIVE_SHIFT);
            selectObject(x, y, addToSelection);
        } else {
            isDragging = true;
            lastMouseX = x;
            lastMouseY = y;
        }
    } else if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
        isDragging = false;
    }
}

// Gestion du déplacement de la souris
void mouseMotion(int x, int y) {
    if (isDragging && !selectionMode) {
        cameraAngleY += (x - lastMouseX) * 0.2f;
        cameraAngleX += (y - lastMouseY) * 0.2f;

        if (cameraAngleX > 89.0f) cameraAngleX = 89.0f;
        if (cameraAngleX < -89.0f) cameraAngleX = -89.0f;

        lastMouseX = x;
        lastMouseY = y;

        glutPostRedisplay();
    }
}

// Gestion des événements clavier
void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 's':
            selectionMode = !selectionMode;
            std::cout << "Mode sélection : " << (selectionMode ? "activé" : "désactivé") << std::endl;
            break;
        case 'w':
            if (!selectionMode && !selectedMeshes.empty()) {
                for (int meshIndex : selectedMeshes) {
                    meshPositions[meshIndex].y += 1.0f;
                }
            } else {
                cameraPosY -= 1.0f;
            }
            break;
        case 'a':
            if (!selectionMode && !selectedMeshes.empty()) {
                for (int meshIndex : selectedMeshes) {
                    meshPositions[meshIndex].x -= 1.0f;
                }
            } else {
                cameraPosX += 1.0f;
            }
            break;
        case 'd':
            if (!selectionMode && !selectedMeshes.empty()) {
                for (int meshIndex : selectedMeshes) {
                    meshPositions[meshIndex].x += 1.0f;
                }
            } else {
                cameraPosX -= 1.0f;
            }
            break;
        case 'x':
             if (!selectionMode && !selectedMeshes.empty()) {
                for (int meshIndex : selectedMeshes) {
                    meshPositions[meshIndex].y -= 1.0f;
                }
            } else {
                cameraPosY += 1.0f;
            }
            break;
        case 'h':
            if (!selectedMeshes.empty()) {
                for (int meshIndex : selectedMeshes) {
                    meshVisibility[meshIndex] = !meshVisibility[meshIndex];
                    std::cout << "Mesh " << meshIndex << " (" << scene->mMeshes[meshIndex]->mName.C_Str() << ") : "
                              << (meshVisibility[meshIndex] ? "visible" : "masqué") << "\n";
                }
                glutPostRedisplay();
            }
            break;
       case 'r':
            if (!selectedMeshes.empty()) {
                 GLfloat newColor[4] = {1.0f, 0.0f, 0.0f, 1.0f}; // Red

                for (int meshIndex : selectedMeshes) {
                    std::copy(newColor, newColor + 4, meshColors[meshIndex]);
                }
                std::cout << "Couleur des meshes sélectionnés changée en rouge\n";
                glutPostRedisplay();
            }
            break;
        case 't':
             isAnimating = !isAnimating;
            if (isAnimating) {
                animationStartTime = std::chrono::steady_clock::now();
                 std::cout << "Animation activée\n";
            } else {
                std::cout << "Animation désactivée\n";
            }
           
            break;
        case 'g':
        case 'b':
            if (!selectedMeshes.empty()) {
                GLfloat newColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};

                 if (key == 'g') {
                    newColor[1] = 1.0f;
                } else if (key == 'b') {
                    newColor[2] = 1.0f;
                }

                for (int meshIndex : selectedMeshes) {
                    std::copy(newColor, newColor + 4, meshColors[meshIndex]);
                }

                std::cout << "Couleur des meshes sélectionnés changée en ";
                 if (key == 'g') std::cout << "vert\n";
                else if (key == 'b') std::cout << "bleu\n";
                glutPostRedisplay();
            }
            break;
        case '1':
        case '2':
        case '3':
        case '4':
            {
                int lightIndex = key - '1';
                lightEnabled[lightIndex] = !lightEnabled[lightIndex];

                if (lightEnabled[lightIndex]) {
                    glEnable(GL_LIGHT0 + lightIndex);
                    std::cout << "Lumière " << lightIndex + 1 << " activée\n";
                } else {
                    glDisable(GL_LIGHT0 + lightIndex);
                    std::cout << "Lumière " << lightIndex + 1 << " désactivée\n";
                }
                glutPostRedisplay();
            }
            break;
        case 27:
            exit(0);
    }
    glutPostRedisplay();
}

// Redimensionner la fenêtre
void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)w / (double)h, 1.0, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("3D Drone Viewer");

    initOpenGL();
    loadModel(modelPath);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouse);
    glutMotionFunc(mouseMotion);
    glutMouseWheelFunc(mouseWheel);
    
    // Timer Function to drive the animation
     glutIdleFunc(updateAnimation);

    glutMainLoop();

    return 0;
}