/* Objeto 3D - Atividade Acadêmica Computação Gráfica - Módulo 5
 * Júlia Oliveira
 * Iluminação Phong com técnica de iluminação de 3 pontos:
 *   - Luz principal (key), preenchimento (fill) e fundo (back)
 *   - Posicionadas automaticamente a partir do objeto principal (objetos[0])
 *   - Atenuação na parcela difusa
 *
 * Controles:
 *   TAB       - alterna objeto selecionado: 0 -> 1 -> 2 -> 0
 *   R         - modo Girar    -> setas giram, X/Y/Z = eixo
 *   T         - modo Transladar -> setas transladam
 *   S         - modo Escalar  -> setas/+/- escalam
 *   1/2/3     - liga/desliga luz principal / preenchimento / fundo
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

struct Material {
    string arquivoTextura;
    glm::vec3 ka = {0.2f, 0.2f, 0.2f};
    glm::vec3 kd = {0.8f, 0.8f, 0.8f};
    glm::vec3 ks = {0.5f, 0.5f, 0.5f};
    float ns = 32.0f;
};

struct Luz {
    glm::vec3 posicao;
    glm::vec3 cor;
    float intensidade;
    bool ativa = true;
};

void key_callback(GLFWwindow* janela, int tecla, int scancode, int acao, int modo);
GLuint carregarOBJ(const string& caminho, int& nVertices);
Material lerMTL(const string& caminhomtl);
GLuint carregarTextura(const string& caminho);
int inicializarShader();

const GLuint LARGURA = 1000, ALTURA = 1000;

// Vertex shader: posição (0), UV (1), normal (2)
const GLchar* fonteVertice =
    "#version 450\n"
    "layout (location = 0) in vec3 posicao;\n"
    "layout (location = 1) in vec2 coordUV;\n"
    "layout (location = 2) in vec3 normal;\n"
    "uniform mat4 model;\n"
    "out vec2 uvFragmento;\n"
    "out vec3 posFragmento;\n"
    "out vec3 normalFragmento;\n"
    "void main()\n"
    "{\n"
    "    vec4 posMundo = model * vec4(posicao, 1.0);\n"
    "    gl_Position = posMundo;\n"
    "    posFragmento = vec3(posMundo);\n"
    "    normalFragmento = mat3(transpose(inverse(model))) * normal;\n"
    "    uvFragmento = coordUV;\n"
    "}\0";

// Fragment shader: Phong com 3 luzes pontuais e atenuação na difusa
const GLchar* fonteFragmento =
    "#version 450\n"
    "in vec2 uvFragmento;\n"
    "in vec3 posFragmento;\n"
    "in vec3 normalFragmento;\n"
    "out vec4 corSaida;\n"
    "uniform sampler2D amostradorTextura;\n"
    "uniform vec3 posCamera;\n"
    "uniform vec3 ka;\n"
    "uniform vec3 kd;\n"
    "uniform vec3 ks;\n"
    "uniform float ns;\n"
    "struct LuzPontual {\n"
    "    vec3 posicao;\n"
    "    vec3 cor;\n"
    "    float intensidade;\n"
    "    bool ativa;\n"
    "};\n"
    "uniform LuzPontual luzes[3];\n"
    "void main()\n"
    "{\n"
    "    vec3 corTex = vec3(texture(amostradorTextura, uvFragmento));\n"
    "    vec3 norm = normalize(normalFragmento);\n"
    "    vec3 dirCamera = normalize(posCamera - posFragmento);\n"
    "    vec3 resultado = ka * 0.15 * corTex;\n"
    "    for (int i = 0; i < 3; i++) {\n"
    "        if (!luzes[i].ativa) continue;\n"
    "        vec3 vetorLuz = luzes[i].posicao - posFragmento;\n"
    "        float dist = length(vetorLuz);\n"
    "        vec3 dirLuz = vetorLuz / dist;\n"
    "        float atenuacao = 1.0 / (1.0 + 1.0 * dist + 0.5 * dist * dist);\n"
    "        float difusa = max(dot(norm, dirLuz), 0.0);\n"
    "        resultado += kd * corTex * luzes[i].cor * luzes[i].intensidade * difusa * atenuacao;\n"
    "        vec3 dirReflexao = reflect(-dirLuz, norm);\n"
    "        float especular = pow(max(dot(dirCamera, dirReflexao), 0.0), ns);\n"
    "        resultado += ks * luzes[i].cor * luzes[i].intensidade * especular;\n"
    "    }\n"
    "    corSaida = vec4(resultado, 1.0);\n"
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
Luz luzes[3];

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

// Recalcula posição das 3 luzes a partir do objeto principal (objetos[0])
void atualizarPosicaoLuzes() {
    float e = objetos[0].escala;
    glm::vec3 p = objetos[0].posicao;
    luzes[0].posicao = p + glm::vec3( e * 2.0f,  e * 2.0f,  e * 3.0f); // principal: frente-direita, acima
    luzes[1].posicao = p + glm::vec3(-e * 2.0f,  e * 0.5f,  e * 2.0f); // preenchimento: esquerda, suave
    luzes[2].posicao = p + glm::vec3( e * 0.0f,  e * 1.5f, -e * 3.0f); // fundo: atrás, acima
}

int main()
{
    glfwInit();

    GLFWwindow* janela = glfwCreateWindow(LARGURA, ALTURA, "Objeto 3D - Phong 3 Luzes", nullptr, nullptr);
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

    GLint locModelo    = glGetUniformLocation(programaShader, "model");
    GLint locCamera    = glGetUniformLocation(programaShader, "posCamera");
    GLint locKa        = glGetUniformLocation(programaShader, "ka");
    GLint locKd        = glGetUniformLocation(programaShader, "kd");
    GLint locKs        = glGetUniformLocation(programaShader, "ks");
    GLint locNs        = glGetUniformLocation(programaShader, "ns");
    glUniform1i(glGetUniformLocation(programaShader, "amostradorTextura"), 0);

    // Câmera implícita em z=2 olhando para a origem
    glUniform3f(locCamera, 0.0f, 0.0f, 2.0f);

    // Cacheia locations dos uniforms das luzes
    GLint locLuzPos[3], locLuzCor[3], locLuzInt[3], locLuzAtiva[3];
    for (int i = 0; i < 3; i++) {
        string b = "luzes[" + to_string(i) + "].";
        locLuzPos[i]   = glGetUniformLocation(programaShader, (b + "posicao").c_str());
        locLuzCor[i]   = glGetUniformLocation(programaShader, (b + "cor").c_str());
        locLuzInt[i]   = glGetUniformLocation(programaShader, (b + "intensidade").c_str());
        locLuzAtiva[i] = glGetUniformLocation(programaShader, (b + "ativa").c_str());
    }

    glEnable(GL_DEPTH_TEST);

    // Lê MTL e carrega textura — compartilhados pelos 3 objetos
    Material mat = lerMTL("assets/Suzanne.mtl");
    GLuint idTextura = carregarTextura(mat.arquivoTextura.empty() ? "" : "assets/" + mat.arquivoTextura);

    // Coeficientes Phong do material (setar uma vez)
    glUniform3fv(locKa, 1, glm::value_ptr(mat.ka));
    glUniform3fv(locKd, 1, glm::value_ptr(mat.kd));
    glUniform3fv(locKs, 1, glm::value_ptr(mat.ks));
    glUniform1f(locNs, mat.ns);

    // Carrega geometria — VAO compartilhado entre as 3 instâncias
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

    // Cores das luzes: principal=branco quente, preenchimento=azul frio, fundo=branco quente
    luzes[0].cor = glm::vec3(1.0f, 1.0f, 0.95f);
    luzes[0].intensidade = 1.0f;
    luzes[1].cor = glm::vec3(0.8f, 0.9f, 1.0f);
    luzes[1].intensidade = 0.5f;
    luzes[2].cor = glm::vec3(1.0f, 0.9f, 0.8f);
    luzes[2].intensidade = 0.7f;

    while (!glfwWindowShouldClose(janela))
    {
        glfwPollEvents();

        string nomeObjeto = "Objeto " + to_string(modoAtivo);
        string nomeModo = (modoTransformacao == GIRAR)      ? "GIRAR"
                        : (modoTransformacao == TRANSLADAR) ? "TRANSLADAR"
                        :                                     "ESCALAR";
        string estadoLuzes = string(luzes[0].ativa ? "1" : "_")
                           + (luzes[1].ativa ? "2" : "_")
                           + (luzes[2].ativa ? "3" : "_");
        glfwSetWindowTitle(janela,
            ("Objeto 3D | Julia Oliveira | Phong [luzes:" + estadoLuzes + "]  |  ["
             + nomeObjeto + "]  [" + nomeModo + "]  |  R/T/S  TAB  1/2/3=luzes").c_str());

        glClearColor(0.08f, 0.12f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Atualiza posições das luzes a partir do objeto principal
        atualizarPosicaoLuzes();
        for (int i = 0; i < 3; i++) {
            glUniform3fv(locLuzPos[i],   1, glm::value_ptr(luzes[i].posicao));
            glUniform3fv(locLuzCor[i],   1, glm::value_ptr(luzes[i].cor));
            glUniform1f (locLuzInt[i],      luzes[i].intensidade);
            glUniform1i (locLuzAtiva[i],    luzes[i].ativa ? 1 : 0);
        }

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
        if (tecla == GLFW_KEY_1) luzes[0].ativa = !luzes[0].ativa;
        if (tecla == GLFW_KEY_2) luzes[1].ativa = !luzes[1].ativa;
        if (tecla == GLFW_KEY_3) luzes[2].ativa = !luzes[2].ativa;
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

// Lê MTL e retorna Material com Ka/Kd/Ks/Ns/textura (defaults se ausentes no arquivo)
Material lerMTL(const string& caminhomtl)
{
    Material mat;
    ifstream arq(caminhomtl);
    if (!arq.is_open()) {
        cerr << "Arquivo MTL nao encontrado: " << caminhomtl << endl;
        return mat;
    }
    string linha;
    while (getline(arq, linha)) {
        istringstream ss(linha);
        string palavra;
        ss >> palavra;
        if      (palavra == "map_Kd") ss >> mat.arquivoTextura;
        else if (palavra == "Ka")     ss >> mat.ka.r >> mat.ka.g >> mat.ka.b;
        else if (palavra == "Kd")     ss >> mat.kd.r >> mat.kd.g >> mat.kd.b;
        else if (palavra == "Ks")     ss >> mat.ks.r >> mat.ks.g >> mat.ks.b;
        else if (palavra == "Ns")     ss >> mat.ns;
    }
    return mat;
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

// Carrega geometria do OBJ: buffer (x,y,z, s,t, nx,ny,nz) por vértice
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

                if (!coordUV.empty()) {
                    buffer.push_back(coordUV[ti].s);
                    buffer.push_back(coordUV[ti].t);
                } else {
                    buffer.push_back(0.0f);
                    buffer.push_back(0.0f);
                }

                if (!normais.empty()) {
                    buffer.push_back(normais[ni].x);
                    buffer.push_back(normais[ni].y);
                    buffer.push_back(normais[ni].z);
                } else {
                    buffer.push_back(0.0f);
                    buffer.push_back(0.0f);
                    buffer.push_back(1.0f);
                }
            }
        }
    }
    arq.close();

    // Stride: 8 floats (x,y,z, s,t, nx,ny,nz)
    GLuint vbo, vao;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, buffer.size() * sizeof(GLfloat), buffer.data(), GL_STATIC_DRAW);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Atributo 0: posição (x, y, z)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    // Atributo 1: coordenada UV (s, t)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    // Atributo 2: normal (nx, ny, nz)
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(5 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    nVertices = (int)(buffer.size() / 8);
    return vao;
}
