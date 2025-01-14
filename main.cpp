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

// Sélection
int selectedMesh = -1; // Indice du mesh sélectionné
bool selectionMode = false; // Mode de sélection activé/désactivé

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
void mouseMotion(int x, int y) {
    if (isDragging && !selectionMode) { // Rotation uniquement si le mode de sélection est désactivé
        cameraAngleY += (x - lastMouseX) * 0.2f; // Mettre à jour l'angle horizontal
        cameraAngleX += (y - lastMouseY) * 0.2f; // Mettre à jour l'angle vertical

        // Limiter l'angle vertical pour éviter des rotations excessives
        if (cameraAngleX > 89.0f) cameraAngleX = 89.0f;
        if (cameraAngleX < -89.0f) cameraAngleX = -89.0f;

        lastMouseX = x; // Mettre à jour la dernière position X de la souris
        lastMouseY = y; // Mettre à jour la dernière position Y de la souris

        glutPostRedisplay(); // Redessiner la scène
    }
}
void loadModel(const std::string& path) {
    scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "Erreur de chargement du modèle : " << importer.GetErrorString() << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "Modèle chargé avec succès : " << path << std::endl;

    // Afficher le nombre de meshes
    std::cout << "Nombre de meshes dans le modèle : " << scene->mNumMeshes << std::endl;

    // Afficher les noms des meshes
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[i];
        std::string meshName = mesh->mName.C_Str(); // Récupérer le nom du mesh
        std::cout << "Mesh " << i << " : " << meshName << std::endl;
    }

    // Ajuster la distance de la caméra
    cameraDistance = calculateInitialDistance(scene);
}

// Fonction récursive pour dessiner le modèle
void renderNode(const aiNode* node, const aiScene* scene, bool selectionMode = false) {
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        int meshIndex = node->mMeshes[i]; // Correct index of the mesh

        if (selectionMode) {
            // En mode sélection, on utilise l'indice du mesh comme identifiant
            glPushName(meshIndex); // Utiliser l'indice du mesh comme identifiant
        }

        if (!selectionMode && selectedMesh == meshIndex) {
            // Si le mesh est sélectionné, on le colore en bleu
            glColor3f(0.0f, 0.0f, 1.0f);
        } else {
            // Sinon, on utilise la couleur par défaut
            glColor3f(1.0f, 1.0f, 1.0f);
        }

        aiMesh* mesh = scene->mMeshes[meshIndex]; // Correctly getting mesh from scene using index
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

        if (selectionMode) {
            glPopName(); // Retirer l'identifiant du mesh de la pile
        }
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        renderNode(node->mChildren[i], scene, selectionMode);
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

    // Parcourir tous les meshes du modèle
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        glLoadName(i); // Utiliser l'indice du mesh comme identifiant

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
        int selectedIdx = buffer[3]; // Récupérer l'indice du mesh sélectionné
        if (addToSelection) {
            if (selectedMesh == selectedIdx) {
                selectedMesh = -1; // Désélectionner si déjà sélectionné
                std::cout << "Mesh désélectionnée : " << scene->mMeshes[selectedIdx]->mName.C_Str() << "\n";
            } else {
                selectedMesh = selectedIdx; // Sélectionner le mesh
                aiMesh* mesh = scene->mMeshes[selectedMesh];
                std::cout << "Mesh sélectionnée : " << mesh->mName.C_Str() << "\n";
            }
        } else {
            selectedMesh = selectedIdx; // Sélectionner le mesh
            aiMesh* mesh = scene->mMeshes[selectedMesh];
            std::cout << "Mesh sélectionnée : " << mesh->mName.C_Str() << "\n";
        }
    } else {
        if (!addToSelection) {
            selectedMesh = -1; // Aucun mesh sélectionné
        }
    }

    glutPostRedisplay();
}

// Gestion des événements souris
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        if (selectionMode) {
            bool addToSelection = (glutGetModifiers() & GLUT_ACTIVE_SHIFT); // Vérifier si Shift est enfoncé
            selectObject(x, y, addToSelection);
        } else {
            // Rotation de la caméra
            isDragging = true;
            lastMouseX = x;
            lastMouseY = y;
        }
    } else if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
        isDragging = false;
    }
}

// Gestion des événements clavier
void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 's':
            // Activer/désactiver le mode de sélection
            selectionMode = !selectionMode;
            std::cout << "Mode sélection : " << (selectionMode ? "activé" : "désactivé") << std::endl;
            break;
        case 'w':
            cameraPosY -= 1.0f;
            break;
        case 'a':
            cameraPosX += 1.0f;
            break;
        case 'd':
            cameraPosX -= 1.0f;
            break;
        case 27: // Touche Échap
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