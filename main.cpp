#include <GL/freeglut.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>
#include <vector>
#include <cfloat>

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
std::string modelPath = "drone.obj"; // Chemin pour macOS

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
    return std::max({size.x, size.y, size.z}) * 2.0f; // Distance en fonction de la taille du modèle
}

void loadModel(const std::string& path) {
    scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "Erreur de chargement du modèle : " << importer.GetErrorString() << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "Modèle chargé avec succès : " << path << std::endl;

    // Ajuster la distance de la caméra
    cameraDistance = calculateInitialDistance(scene);
}

// Fonction récursive pour dessiner le modèle
void renderNode(const aiNode* node, const aiScene* scene) {
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

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
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        renderNode(node->mChildren[i], scene);
    }
}

// Initialiser OpenGL
void initOpenGL() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    GLfloat lightPosition[] = {1.0f, 1.0f, 1.0f, 0.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
}

// Fonction pour gérer la molette de la souris
void mouseWheel(int wheel, int direction, int x, int y) {
    if (direction > 0) {
        // Zoom in
        cameraDistance -= 0.5f;
    } else {
        // Zoom out
        cameraDistance += 0.5f;
    }

    // Limites de la distance de la caméra
    if (cameraDistance < 1.0f) cameraDistance = 1.0f;
    if (cameraDistance > 50.0f) cameraDistance = 50.0f;

    glutPostRedisplay();
}

// Fonction d'affichage
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    // Position de la caméra
    glTranslatef(0.0f, 0.0f, -cameraDistance);
    glRotatef(cameraAngleX, 1.0f, 0.0f, 0.0f);
    glRotatef(cameraAngleY, 0.0f, 1.0f, 0.0f);
    glTranslatef(-cameraPosX, -cameraPosY, 0.0f);

    // Dessiner le modèle
    if (scene && scene->mRootNode) {
        renderNode(scene->mRootNode, scene);
    }

    glutSwapBuffers();
}

// Gestion des événements souris
void mouseMotion(int x, int y) {
    if (isDragging) {
        cameraAngleY += (x - lastMouseX) * 0.2f;
        cameraAngleX += (y - lastMouseY) * 0.2f;
        lastMouseX = x;
        lastMouseY = y;
        glutPostRedisplay();
    }
}

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        isDragging = true;
        lastMouseX = x;
        lastMouseY = y;
    } else if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
        isDragging = false;
    } else if (button == GLUT_RIGHT_BUTTON) {
        cameraDistance += (state == GLUT_DOWN) ? -0.5f : 0.5f;
        if (cameraDistance < 1.0f) cameraDistance = 1.0f;
        glutPostRedisplay();
    }
}

// Gestion des événements clavier
void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 's':
            cameraPosY += 1.0f;
            break;
        case 'w':
            cameraPosY -= 1.0f;
            break;
        case 'd':
            cameraPosX -= 1.0f;
            break;
        case 'a':
            cameraPosX += 1.0f;
            break;
        case 'r':
            cameraAngleX = 0.0f;
            cameraAngleY = 0.0f;
            cameraDistance = 5.0f;
            cameraPosX = 0.0f;
            cameraPosY = 0.0f;
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
    // Initialiser GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("3D Drone Viewer");

    // Initialiser OpenGL
    initOpenGL();

    // Charger le modèle
    loadModel(modelPath);

    // Enregistrer les callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouse);
    glutMotionFunc(mouseMotion);
    glutMouseWheelFunc(mouseWheel); // Activer la gestion de la molette de la souris

    // Boucle principale
    glutMainLoop();

    return 0;
}