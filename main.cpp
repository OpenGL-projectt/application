#include <GL/freeglut.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>
#include <vector>
#include <cfloat>
#include <set> // Pour gérer la sélection multiple
#include <unordered_map> // Pour gérer la visibilité des meshes

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
std::set<int> selectedMeshes; // Liste des meshes sélectionnés
bool selectionMode = false; // Mode de sélection activé/désactivé

// Visibilité des meshes
std::unordered_map<int, bool> meshVisibility; // Carte pour stocker la visibilité des meshes
std::unordered_map<int, GLfloat[4]> meshColors; // Carte pour stocker les couleurs des meshes
std::unordered_map<int, aiVector3D> meshPositions; // Carte pour stocker les positions des meshes

// Déplacement des meshes sélectionnés
float selectedMeshTranslateX = 0.0f; // Déplacement en X des meshes sélectionnés
float selectedMeshTranslateY = 0.0f; // Déplacement en Y des meshes sélectionnés
float selectedMeshTranslateZ = 0.0f; // Déplacement en Z des meshes sélectionnés

// Sources de lumière
const int NUM_LIGHTS = 4; // Nombre de sources de lumière (sud, nord, ouest, est)
bool lightEnabled[NUM_LIGHTS] = {true, true, true, true}; // État des sources de lumière
GLfloat lightColors[NUM_LIGHTS][4] = {
    {1.0f, 1.0f, 1.0f, 1.0f}, // Lumière sud (blanche par défaut)
    {1.0f, 1.0f, 1.0f, 1.0f}, // Lumière nord (blanche par défaut)
    {1.0f, 1.0f, 1.0f, 1.0f}, // Lumière ouest (blanche par défaut)
    {1.0f, 1.0f, 1.0f, 1.0f}  // Lumière est (blanche par défaut)
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
    return std::max({size.x, size.y, size.z}) * 2.0f; // Distance en fonction de la taille du modèle
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

        // Initialiser la visibilité du mesh à true (visible par défaut)
        meshVisibility[i] = true;

        // Initialiser la couleur du mesh à blanc par défaut
        GLfloat defaultColor[4] = {1.0f, 1.0f, 1.0f, 1.0f}; // Blanc
        std::copy(defaultColor, defaultColor + 4, meshColors[i]);

        // Initialiser la position du mesh à (0, 0, 0)
        meshPositions[i] = aiVector3D(0.0f, 0.0f, 0.0f);
    }

    // Ajuster la distance de la caméra
    cameraDistance = calculateInitialDistance(scene);
}

// Fonction récursive pour dessiner le modèle
void renderNode(const aiNode* node, const aiScene* scene, bool selectionMode = false, bool renderSelectedOnly = false) {
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        int meshIndex = node->mMeshes[i]; // Correct index of the mesh

        // Vérifier si le mesh est visible
        if (!meshVisibility[meshIndex]) {
            continue; // Passer au mesh suivant si celui-ci est masqué
        }

        // Si renderSelectedOnly est activé, ignorer les meshes non sélectionnés
        if (renderSelectedOnly && selectedMeshes.find(meshIndex) == selectedMeshes.end()) {
            continue;
        }

        if (selectionMode) {
            // En mode sélection, on utilise l'indice du mesh comme identifiant
            glPushName(meshIndex); // Utiliser l'indice du mesh comme identifiant
        }

        // Appliquer la couleur du mesh (sauf en mode contour)
        if (!renderSelectedOnly) {
            glColor4fv(meshColors[meshIndex]); // Toujours appliquer la couleur personnalisée
        }

        aiMesh* mesh = scene->mMeshes[meshIndex]; // Correctly getting mesh from scene using index

        // Appliquer la position du mesh
        glPushMatrix();
        aiVector3D position = meshPositions[meshIndex];
        glTranslatef(position.x, position.y, position.z);

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
            glPopName(); // Retirer l'identifiant du mesh de la pile
        }
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        renderNode(node->mChildren[i], scene, selectionMode, renderSelectedOnly);
    }
}

// Initialiser OpenGL
void initOpenGL() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING); // Activer l'éclairage
    glEnable(GL_COLOR_MATERIAL);

    // Configurer les propriétés des sources de lumière
    GLfloat lightPosition[NUM_LIGHTS][4] = {
        {0.0f, -1.0f, 0.0f, 0.0f}, // Lumière sud (directionnelle)
        {0.0f, 1.0f, 0.0f, 0.0f},  // Lumière nord (directionnelle)
        {-1.0f, 0.0f, 0.0f, 0.0f}, // Lumière ouest (directionnelle)
        {1.0f, 0.0f, 0.0f, 0.0f}   // Lumière est (directionnelle)
    };

    for (int i = 0; i < NUM_LIGHTS; ++i) {
        glLightfv(GL_LIGHT0 + i, GL_POSITION, lightPosition[i]);
        glLightfv(GL_LIGHT0 + i, GL_DIFFUSE, lightColors[i]);
        glLightfv(GL_LIGHT0 + i, GL_SPECULAR, lightColors[i]);
        glEnable(GL_LIGHT0 + i); // Activer la lumière par défaut
    }

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

    // Première passe : Rendre les meshes normalement
    if (scene && scene->mRootNode) {
        renderNode(scene->mRootNode, scene);
    }

    // Deuxième passe : Rendre les meshes sélectionnés en mode fil de fer avec un contour noir
    if (selectionMode && !selectedMeshes.empty()) { // Vérifier si le mode sélection est activé
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // Passer en mode fil de fer
        glLineWidth(2.0f); // Définir l'épaisseur du contour
        glColor3f(0.0f, 0.0f, 0.0f); // Couleur noire pour le contour

        // Rendre uniquement les meshes sélectionnés
        renderNode(scene->mRootNode, scene, false, true);

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // Revenir au mode de remplissage
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
            // Si la touche Shift est enfoncée, ajouter ou retirer le mesh de la sélection
            if (selectedMeshes.find(selectedIdx) != selectedMeshes.end()) {
                selectedMeshes.erase(selectedIdx); // Désélectionner
                std::cout << "Mesh désélectionnée : " << scene->mMeshes[selectedIdx]->mName.C_Str() << "\n";
            } else {
                selectedMeshes.insert(selectedIdx); // Sélectionner
                std::cout << "Mesh sélectionnée : " << scene->mMeshes[selectedIdx]->mName.C_Str() << "\n";
            }
        } else {
            // Si la touche Shift n'est pas enfoncée, vider la sélection et sélectionner le nouveau mesh
            selectedMeshes.clear();
            selectedMeshes.insert(selectedIdx);
            std::cout << "Mesh sélectionnée : " << scene->mMeshes[selectedIdx]->mName.C_Str() << "\n";
        }
    } else {
        if (!addToSelection) {
            // Si aucun mesh n'est sélectionné et que la touche Shift n'est pas enfoncée, vider la sélection
            selectedMeshes.clear();
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

// Gestion du déplacement de la souris
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

// Gestion des événements clavier
void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 's':
            // Activer/désactiver le mode de sélection
            selectionMode = !selectionMode;
            std::cout << "Mode sélection : " << (selectionMode ? "activé" : "désactivé") << std::endl;
            break;
        case 'w': // Déplacer les meshes sélectionnés vers le haut
            if (!selectionMode && !selectedMeshes.empty()) {
                for (int meshIndex : selectedMeshes) {
                    meshPositions[meshIndex].y += 1.0f; // Déplacer en Y
                }
            } else {
                cameraPosY -= 1.0f; // Déplacer la caméra vers le bas
            }
            break;
        case 'a': // Déplacer les meshes sélectionnés vers la gauche
            if (!selectionMode && !selectedMeshes.empty()) {
                for (int meshIndex : selectedMeshes) {
                    meshPositions[meshIndex].x -= 1.0f; // Déplacer en X
                }
            } else {
                cameraPosX += 1.0f; // Déplacer la caméra vers la gauche
            }
            break;
        case 'd': // Déplacer les meshes sélectionnés vers la droite
            if (!selectionMode && !selectedMeshes.empty()) {
                for (int meshIndex : selectedMeshes) {
                    meshPositions[meshIndex].x += 1.0f; // Déplacer en X
                }
            } else {
                cameraPosX -= 1.0f; // Déplacer la caméra vers la droite
            }
            break;
        case 'x': // Déplacer les meshes sélectionnés vers le bas
            if (!selectionMode && !selectedMeshes.empty()) {
                for (int meshIndex : selectedMeshes) {
                    meshPositions[meshIndex].y -= 1.0f; // Déplacer en Y
                }
            } else {
                cameraPosY += 1.0f; // Déplacer la caméra vers le haut
            }
            break;
        case 'h': // Touche H pour masquer/afficher les éléments sélectionnés
            if (!selectedMeshes.empty()) {
                for (int meshIndex : selectedMeshes) {
                    meshVisibility[meshIndex] = !meshVisibility[meshIndex]; // Basculer la visibilité
                    std::cout << "Mesh " << meshIndex << " (" << scene->mMeshes[meshIndex]->mName.C_Str() << ") : "
                              << (meshVisibility[meshIndex] ? "visible" : "masqué") << "\n";
                }
                glutPostRedisplay(); // Redessiner la scène
            }
            break;
        case 'r': // Touche R pour changer la couleur des meshes sélectionnés en rouge
        case 'g': // Touche G pour changer la couleur des meshes sélectionnés en vert
        case 'b': // Touche B pour changer la couleur des meshes sélectionnés en bleu
            if (!selectedMeshes.empty()) {
                GLfloat newColor[4] = {0.0f, 0.0f, 0.0f, 1.0f}; // Nouvelle couleur

                if (key == 'r') {
                    newColor[0] = 1.0f; // Rouge
                } else if (key == 'g') {
                    newColor[1] = 1.0f; // Vert
                } else if (key == 'b') {
                    newColor[2] = 1.0f; // Bleu
                }

                // Appliquer la nouvelle couleur à tous les meshes sélectionnés
                for (int meshIndex : selectedMeshes) {
                    std::copy(newColor, newColor + 4, meshColors[meshIndex]);
                }

                std::cout << "Couleur des meshes sélectionnés changée en ";
                if (key == 'r') std::cout << "rouge\n";
                else if (key == 'g') std::cout << "vert\n";
                else if (key == 'b') std::cout << "bleu\n";
                glutPostRedisplay(); // Redessiner la scène
            }
            break;
        case '1': // Touche 1 pour activer/désactiver la lumière sud
        case '2': // Touche 2 pour activer/désactiver la lumière nord
        case '3': // Touche 3 pour activer/désactiver la lumière ouest
        case '4': // Touche 4 pour activer/désactiver la lumière est
            {
                int lightIndex = key - '1'; // Convertir la touche en indice de lumière (0 à 3)
                lightEnabled[lightIndex] = !lightEnabled[lightIndex]; // Basculer l'état de la lumière

                if (lightEnabled[lightIndex]) {
                    glEnable(GL_LIGHT0 + lightIndex); // Activer la lumière
                    std::cout << "Lumière " << lightIndex + 1 << " activée\n";
                } else {
                    glDisable(GL_LIGHT0 + lightIndex); // Désactiver la lumière
                    std::cout << "Lumière " << lightIndex + 1 << " désactivée\n";
                }
                glutPostRedisplay(); // Redessiner la scène
            }
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