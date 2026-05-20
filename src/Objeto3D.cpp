/* Objeto 3D - Atividade Acadêmica Computação Gráfica - Módulo 4
 * Júlia Oliveira
 * Texturas: leitura de coord UV do OBJ, arquivo MTL e carregamento de textura.
 *
 * Diferença de design em relação ao Cubo.cpp:
 *   - MTL e textura são carregados UMA VEZ em main() e compartilhados pelos 3 objetos.
 *   - carregarOBJ() cuida apenas da geometria (sem acoplamento à textura).
 *   - lerMTL() é uma função independente que extrai o nome do arquivo de textura.
 *
 * Controles:
 *   TAB       - alterna objeto selecionado: 0 -> 1 -> 2 -> 0
 *   R         - modo Girar    -> setas giram, X/Y/Z = eixo
 *   T         - modo Transladar -> setas transladam
 *   S         - modo Escalar  -> setas/+/- escalam
 *   ESC       - fecha a janela
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

void key_callback(GLFWwindow* janela, int tecla, int scancode, int acao, int modo);
GLuint carregarOBJ(const string& caminho, int& nVertices);
string lerMTL(const string& caminhomtl);
GLuint carregarTextura(const string& caminho);
int inicializarShader();

const GLuint LARGURA = 1000, ALTURA = 1000;

// Vertex shader: posicao (location 0) e coordenada UV (location 1)
const GLchar* fonteVertice =
    "#version 450\n"
    "layout (location = 0) in vec3 posicao;\n"
    "layout (location = 1) in vec2 coordUV;\n"
    "uniform mat4 model;\n"
    "out vec2 uvFragmento;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = model * vec4(posicao, 1.0);\n"
    "    uvFragmento = coordUV;\n"
    "}\0";

// Fragment shader: amostra a textura com as coordenadas UV interpoladas
const GLchar* fonteFragmento =
    "#version 450\n"
    "in vec2 uvFragmento;\n"
    "out vec4 corSaida;\n"
    "uniform sampler2D amostradorTextura;\n"
    "void main()\n"
    "{\n"
    "    corSaida = texture(amostradorTextura, uvFragmento);\n"
    "}\n\0";

struct Objeto {
    GLuint vao;
    GLuint textura;
    int nVertices;
    glm::vec3 posicao;
    float escala;
    float anguloX, anguloY, anguloZ;

    Objeto(GLuint v, GLuint tex, int nv, glm::vec3 pos, float s = 0.25f)
        : vao(v), textura(tex), nVertices(nv), posicao(pos), escala(s),
          anguloX(0.0f), anguloY(0.0f), anguloZ(0.0f) {}
};

vector<Objeto> objetos;

int modoAtivo = 0;  // índice do objeto selecionado

enum Modo { GIRAR, TRANSLADAR, ESCALAR };
Modo modoTransformacao = TRANSLADAR;

const float PASSO_TRANSLACAO = 0.08f;
const float PASSO_ESCALA     = 0.04f;
const float ESCALA_MIN       = 0.04f;
const float PASSO_ROTACAO    = glm::radians(7.0f);

template<typename Fn>
void aplicarAoSelecionado(Fn fn) {
    fn(objetos[modoAtivo]);
}

int main()
{
    glfwInit();

    GLFWwindow* janela = glfwCreateWindow(LARGURA, ALTURA, "Objeto 3D - Texturas", nullptr, nullptr);
    glfwMakeContextCurrent(janela);
    glfwSetKeyCallback(janela, key_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cout << "Falha ao inicializar GLAD" << endl;
        return -1;
    }

    cout << "Renderer: " << glGetString(GL_RENDERER) << endl;
    cout << "OpenGL version: " << glGetString(GL_VERSION) << endl;

    int largura, altura;
    glfwGetFramebufferSize(janela, &largura, &altura);
    glViewport(0, 0, largura, altura);

    GLuint programaShader = inicializarShader();
    glUseProgram(programaShader);
    GLint locModelo = glGetUniformLocation(programaShader, "model");
    glUniform1i(glGetUniformLocation(programaShader, "amostradorTextura"), 0);

    glEnable(GL_DEPTH_TEST);

    // Lê o MTL e carrega a textura uma única vez — todos os objetos compartilham
    string nomeTextura = lerMTL("assets/Suzanne.mtl");
    GLuint idTextura = carregarTextura(nomeTextura.empty() ? "" : "assets/" + nomeTextura);

    // Carrega a geometria do OBJ e cria 3 instâncias em arranjo triangular
    int nv;
    GLuint vaoCompartilhado = carregarOBJ("assets/modelo.obj", nv);

    if (vaoCompartilhado == (GLuint)-1) {
        cerr << "Falha ao carregar modelo OBJ." << endl;
        glfwTerminate();
        return -1;
    }

    objetos.push_back(Objeto(vaoCompartilhado, idTextura, nv, glm::vec3(-0.45f, -0.3f, 0.0f), 0.2f));
    objetos.push_back(Objeto(vaoCompartilhado, idTextura, nv, glm::vec3( 0.45f, -0.3f, 0.0f), 0.2f));
    objetos.push_back(Objeto(vaoCompartilhado, idTextura, nv, glm::vec3( 0.00f,  0.5f, 0.0f), 0.2f));

    while (!glfwWindowShouldClose(janela))
    {
        glfwPollEvents();

        string nomeObjeto = "Objeto " + to_string(modoAtivo);
        string nomeModo = (modoTransformacao == GIRAR)      ? "GIRAR"
                        : (modoTransformacao == TRANSLADAR) ? "TRANSLADAR"
                        :                                     "ESCALAR";
        glfwSetWindowTitle(janela,
            ("Objeto 3D | Julia Oliveira  |  [" + nomeObjeto + "]  [" + nomeModo + "]"
             + "  |  R/T/S=modo  TAB=selecionar").c_str());

        glClearColor(0.08f, 0.12f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, idTextura);

        for (int i = 0; i < (int)objetos.size(); i++)
        {
            glm::mat4 matriz = glm::mat4(1.0f);
            matriz = glm::translate(matriz, objetos[i].posicao);
            matriz = glm::rotate(matriz, objetos[i].anguloX, glm::vec3(1.0f, 0.0f, 0.0f));
            matriz = glm::rotate(matriz, objetos[i].anguloY, glm::vec3(0.0f, 1.0f, 0.0f));
            matriz = glm::rotate(matriz, objetos[i].anguloZ, glm::vec3(0.0f, 0.0f, 1.0f));
            matriz = glm::scale(matriz, glm::vec3(objetos[i].escala));

            glUniformMatrix4fv(locModelo, 1, GL_FALSE, glm::value_ptr(matriz));
            glBindVertexArray(objetos[i].vao);
            glDrawArrays(GL_TRIANGLES, 0, objetos[i].nVertices);
        }
        glBindVertexArray(0);

        glfwSwapBuffers(janela);
    }

    glfwTerminate();
    return 0;
}

void key_callback(GLFWwindow* janela, int tecla, int scancode, int acao, int modo)
{
    if (tecla == GLFW_KEY_ESCAPE && acao == GLFW_PRESS)
        glfwSetWindowShouldClose(janela, GL_TRUE);

    if (tecla == GLFW_KEY_TAB && acao == GLFW_PRESS)
        modoAtivo = (modoAtivo + 1) % (int)objetos.size();

    if (acao == GLFW_PRESS) {
        if (tecla == GLFW_KEY_R) modoTransformacao = GIRAR;
        if (tecla == GLFW_KEY_T) modoTransformacao = TRANSLADAR;
        if (tecla == GLFW_KEY_S) modoTransformacao = ESCALAR;
    }

    if (acao == GLFW_PRESS || acao == GLFW_REPEAT)
    {
        if (modoTransformacao == TRANSLADAR) {
            if (tecla == GLFW_KEY_LEFT)  aplicarAoSelecionado([](Objeto& o){ o.posicao.x -= PASSO_TRANSLACAO; });
            if (tecla == GLFW_KEY_RIGHT) aplicarAoSelecionado([](Objeto& o){ o.posicao.x += PASSO_TRANSLACAO; });
            if (tecla == GLFW_KEY_UP)    aplicarAoSelecionado([](Objeto& o){ o.posicao.y += PASSO_TRANSLACAO; });
            if (tecla == GLFW_KEY_DOWN)  aplicarAoSelecionado([](Objeto& o){ o.posicao.y -= PASSO_TRANSLACAO; });
        }

        if (modoTransformacao == ESCALAR) {
            if (tecla == GLFW_KEY_EQUAL || tecla == GLFW_KEY_UP)
                aplicarAoSelecionado([](Objeto& o){ o.escala += PASSO_ESCALA; });
            if (tecla == GLFW_KEY_MINUS || tecla == GLFW_KEY_DOWN)
                aplicarAoSelecionado([](Objeto& o){
                    o.escala -= PASSO_ESCALA;
                    if (o.escala < ESCALA_MIN) o.escala = ESCALA_MIN;
                });
        }

        if (modoTransformacao == GIRAR) {
            if (tecla == GLFW_KEY_LEFT)  aplicarAoSelecionado([](Objeto& o){ o.anguloY -= PASSO_ROTACAO; });
            if (tecla == GLFW_KEY_RIGHT) aplicarAoSelecionado([](Objeto& o){ o.anguloY += PASSO_ROTACAO; });
            if (tecla == GLFW_KEY_UP)    aplicarAoSelecionado([](Objeto& o){ o.anguloX -= PASSO_ROTACAO; });
            if (tecla == GLFW_KEY_DOWN)  aplicarAoSelecionado([](Objeto& o){ o.anguloX += PASSO_ROTACAO; });
            if (tecla == GLFW_KEY_X)     aplicarAoSelecionado([](Objeto& o){ o.anguloX += PASSO_ROTACAO; });
            if (tecla == GLFW_KEY_Y)     aplicarAoSelecionado([](Objeto& o){ o.anguloY += PASSO_ROTACAO; });
            if (tecla == GLFW_KEY_Z)     aplicarAoSelecionado([](Objeto& o){ o.anguloZ += PASSO_ROTACAO; });
        }
    }
}

int inicializarShader()
{
    GLuint shaderVertice = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(shaderVertice, 1, &fonteVertice, NULL);
    glCompileShader(shaderVertice);
    GLint compilado;
    GLchar mensagemErro[512];
    glGetShaderiv(shaderVertice, GL_COMPILE_STATUS, &compilado);
    if (!compilado) {
        glGetShaderInfoLog(shaderVertice, 512, NULL, mensagemErro);
        cout << "ERRO::VERTEX_SHADER: " << mensagemErro << endl;
    }

    GLuint shaderFragmento = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(shaderFragmento, 1, &fonteFragmento, NULL);
    glCompileShader(shaderFragmento);
    glGetShaderiv(shaderFragmento, GL_COMPILE_STATUS, &compilado);
    if (!compilado) {
        glGetShaderInfoLog(shaderFragmento, 512, NULL, mensagemErro);
        cout << "ERRO::FRAGMENT_SHADER: " << mensagemErro << endl;
    }

    GLuint programa = glCreateProgram();
    glAttachShader(programa, shaderVertice);
    glAttachShader(programa, shaderFragmento);
    glLinkProgram(programa);
    glGetProgramiv(programa, GL_LINK_STATUS, &compilado);
    if (!compilado) {
        glGetProgramInfoLog(programa, 512, NULL, mensagemErro);
        cout << "ERRO::SHADER_PROGRAM: " << mensagemErro << endl;
    }

    glDeleteShader(shaderVertice);
    glDeleteShader(shaderFragmento);
    return programa;
}

// Lê arquivo MTL e retorna o nome do arquivo de textura difusa (linha map_Kd)
string lerMTL(const string& caminhomtl)
{
    ifstream arq(caminhomtl);
    if (!arq.is_open()) {
        cerr << "Arquivo MTL nao encontrado: " << caminhomtl << endl;
        return "";
    }
    string linha, nomeArquivo;
    while (getline(arq, linha)) {
        istringstream ss(linha);
        string palavra;
        ss >> palavra;
        if (palavra == "map_Kd") {
            ss >> nomeArquivo;
            break;
        }
    }
    return nomeArquivo;
}

// Carrega textura via stb_image; fallback: xadrez ciano/escuro se arquivo ausente
GLuint carregarTextura(const string& caminho)
{
    GLuint idTex;
    glGenTextures(1, &idTex);
    glBindTexture(GL_TEXTURE_2D, idTex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_set_flip_vertically_on_load(true);
    int larg, alt, canais;
    unsigned char* dados = stbi_load(caminho.c_str(), &larg, &alt, &canais, 0);

    if (dados) {
        GLenum formato = (canais == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, formato, larg, alt, 0, formato, GL_UNSIGNED_BYTE, dados);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(dados);
        cout << "Textura carregada: " << caminho << " (" << larg << "x" << alt << ")" << endl;
    } else {
        // Xadrez ciano/escuro 8x8 como indicador visual de textura ausente
        unsigned char pixels[8 * 8 * 3];
        for (int i = 0; i < 8; i++)
            for (int j = 0; j < 8; j++) {
                int c = ((i / 4) + (j / 4)) % 2;
                pixels[(i * 8 + j) * 3 + 0] = c ? 0   : 40;
                pixels[(i * 8 + j) * 3 + 1] = c ? 210 : 40;
                pixels[(i * 8 + j) * 3 + 2] = c ? 210 : 40;
            }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 8, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
        glGenerateMipmap(GL_TEXTURE_2D);
        cerr << "Textura nao encontrada (" << caminho << "), usando xadrez." << endl;
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    return idTex;
}

// Carrega geometria do OBJ: buffer (x,y,z,s,t) por vértice — sem acoplamento à textura
GLuint carregarOBJ(const string& caminho, int& nVertices)
{
    vector<glm::vec3> vertices;
    vector<glm::vec2> coordUV;
    vector<glm::vec3> normais;
    vector<GLfloat>   buffer;

    ifstream arq(caminho);
    if (!arq.is_open()) {
        cerr << "Erro ao abrir o arquivo " << caminho << endl;
        return (GLuint)-1;
    }

    string linha;
    while (getline(arq, linha)) {
        istringstream ss(linha);
        string palavra;
        ss >> palavra;

        if (palavra == "v") {
            glm::vec3 v;
            ss >> v.x >> v.y >> v.z;
            vertices.push_back(v);
        } else if (palavra == "vt") {
            glm::vec2 vt;
            ss >> vt.s >> vt.t;
            coordUV.push_back(vt);
        } else if (palavra == "vn") {
            glm::vec3 vn;
            ss >> vn.x >> vn.y >> vn.z;
            normais.push_back(vn);
        } else if (palavra == "f") {
            string token;
            while (ss >> token) {
                int vi = 0, ti = 0, ni = 0;
                istringstream ssf(token);
                string idx;
                if (getline(ssf, idx, '/')) vi = !idx.empty() ? stoi(idx) - 1 : 0;
                if (getline(ssf, idx, '/')) ti = !idx.empty() ? stoi(idx) - 1 : 0;
                if (getline(ssf, idx))      ni = !idx.empty() ? stoi(idx) - 1 : 0;

                buffer.push_back(vertices[vi].x);
                buffer.push_back(vertices[vi].y);
                buffer.push_back(vertices[vi].z);

                // Coordenadas UV substituem a cor por vértice do módulo anterior
                if (!coordUV.empty()) {
                    buffer.push_back(coordUV[ti].s);
                    buffer.push_back(coordUV[ti].t);
                } else {
                    buffer.push_back(0.0f);
                    buffer.push_back(0.0f);
                }
            }
        }
    }
    arq.close();

    // Monta VBO e VAO — stride: 5 floats (x, y, z, s, t)
    GLuint vbo, vao;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, buffer.size() * sizeof(GLfloat), buffer.data(), GL_STATIC_DRAW);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Atributo 0: posição (x, y, z)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    // Atributo 1: coordenada UV (s, t)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    nVertices = (int)(buffer.size() / 5);
    return vao;
}
