#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include "json.hpp"

// ---------- DEFINES Y TIPOS -------------
#define NUM_COLUMNAS 25
#define NUM_FILAS 15
#define MAX_PUNTOS 10000
#define NUM_AVIONES 6
using json = nlohmann::json;


float colToX(int col);
float filaToY(int fila);
struct Punto {
    float x, y;
};
struct FocoPais {
    std::vector<Punto> focosPorPeriodo[20]; // Hasta 20 periodos
};

struct Celda {
    int col, fila;
};

struct Pais {
    std::string nombre;
    Celda celda;
};

struct Region {
    std::string nombre;
    std::vector<Pais> paises;
};

typedef struct {
    GLubyte *dibujo;
    GLuint bpp, largo, ancho, ID;
} textura;

typedef struct {
	int origen[2];       // Coordenadas de celda de origen.
	int destino[2];      // Coordenadas de celda destino.
	float x, y;          // Posici n actual en pantalla.
	float t;             // Interpolaci n de movimiento [0..1].
	int estado;          // 0 = va, 1 = girando, 2 = regresa.
	float anguloGiro;    //  ngulo actual durante el giro.
	int direccion;       // 0 = ida, 1 = regreso.
	float anguloDireccion; // Direcci n de vuelo en grados.
	float velocidad; // Velocidad de interpolaci n por frame (inversamente proporcional a la distancia)
} Avion;


Avion aviones[NUM_AVIONES];// Instancia de avi n.



// Crea un nuevo avi n
Avion crearAvion(int origen[2], int destino[2]) {
	Avion avion;
	
	avion.origen[0] = origen[0];
	avion.origen[1] = origen[1];
	avion.destino[0] = destino[0];
	avion.destino[1] = destino[1];
	
	avion.t = 0.0f;
	avion.estado = 0;
	avion.anguloGiro = 0.0f;
	avion.direccion = 0;
	
	float dx = colToX(destino[0]) - colToX(origen[0]);
	float dy = filaToY(destino[1]) - filaToY(origen[1]);
	avion.anguloDireccion = atan2(dy, dx) * 180.0f / M_PI;
	
	float distancia = sqrt(dx*dx + dy*dy);    // Distancia en pantalla
	//float velocidadDeseada = 0.01f;  // Puedes subirlo más si quieres: 0.015f, 0.02f
    float duracion = 6.0f;                    // Tiempo deseado en segundos para cruzar toda la pantalla
	avion.velocidad = (1.0f / (distancia * 60.0f / duracion)) * 0.01f;// Frames para cubrir distancia
    //avion.velocidad = velocidadDeseada;
	
	avion.x = colToX(origen[0]);
	avion.y = filaToY(origen[1]);
	
	return avion;
}

void moverAvion(Avion* a) {
	if (a->estado == 0) { // Estado 0: avi n va hacia el destino
		if (a->t < 1.0f) {
			a->t += a->velocidad;	// Avanza lentamente interpolando su posici n
			if (a->t >= 1.0f) {
				a->t = 1.0f;        // Asegura que no pase de 1
				a->estado = 1;      // Cambia a estado de giro
				a->anguloGiro = 0.0f; // Reinicia el  ngulo de giro
			}
		}
	}
	else if (a->estado == 1) { // Estado 1: el avi n est  girando
		a->anguloGiro += 2.0f; // Aumenta gradualmente el  ngulo de rotaci n
		if (a->anguloGiro >= 180.0f) {
			a->anguloGiro = 0.0f;  // Reinicia el giro
			a->estado = 2;         // Cambia al estado de regreso
			a->direccion = 1 - a->direccion; // Invierte direcci n: ida/regreso
			
			// Intercambia origen y destino
			int temp[2] = { a->origen[0], a->origen[1] };
			a->origen[0] = a->destino[0];
			a->origen[1] = a->destino[1];
			a->destino[0] = temp[0];
			a->destino[1] = temp[1];
			a->t = 0.0f; // Reinicia interpolaci n
			
			// Recalcula la direcci n de vuelo en grados
			float dx = colToX(a->destino[0]) - colToX(a->origen[0]);
			float dy = filaToY(a->destino[1]) - filaToY(a->origen[1]);
			a->anguloDireccion = atan2(dy, dx) * 180.0 / M_PI;
		}
	}
	else if (a->estado == 2) { // Estado 2: avi n regresa al punto de origen
		if (a->t < 1.0f) {
			a->t += a->velocidad; // ? ahora la velocidad depende de la distancia
			if (a->t >= 1.0f) {
				a->t = 1.0f;
				a->estado = 1;        // Al llegar, entra nuevamente en estado de giro
				a->anguloGiro = 0.0f; // Reinicia  ngulo de giro
			}
		}
	}
	
	
	// Calcula posici n interpolada entre origen y destino seg n t
	float x0 = colToX(a->origen[0]);
	float y0 = filaToY(a->origen[1]);
	float x1 = colToX(a->destino[0]);
	float y1 = filaToY(a->destino[1]);
	
	a->x = x0 + a->t * (x1 - x0); // Interpolaci n lineal en X
	a->y = y0 + a->t * (y1 - y0); // Interpolaci n lineal en Y
}
void dibujarAvion(Avion* a) {
	glDisable(GL_TEXTURE_2D); // Desactiva texturas para dibujar en color s lido
	glPushMatrix();           // Guarda la matriz actual
	
	// Posiciona el avi n en su coordenada (x, y)
	glTranslatef(a->x, a->y, 0.05f);
	
	// Escala el modelo a tama o peque o
	glScalef(0.03f, 0.03f, 0.03f);
	
	// Si est  girando, interpolar entre direcci n de ida y regreso
	if (a->estado == 1) {
		float anguloInicio = a->direccion ? a->anguloDireccion + 180.0f : a->anguloDireccion;
		float anguloFin    = a->direccion ? a->anguloDireccion : a->anguloDireccion + 180.0f;
		
		// Calcula rotaci n interpolada seg n cu nto ha girado
		float anguloInterpolado = anguloInicio + (anguloFin - anguloInicio) * (a->anguloGiro / 180.0f);
		glRotatef(anguloInterpolado, 0.0f, 0.0f, 1.0f);
	} else {
		glRotatef(a->anguloDireccion, 0.0f, 0.0f, 1.0f); // Rotaci n fija
	}
	
	glRotatef(-90.0f, 0.0f, 0.0f, 1.0f); // Ajusta orientaci n del avi n
	
	glColor3f(1.0f, 1.0f, 1.0f); // Color blanco del avi n
	
	// Dibuja partes del avi n usando cubos escalados y posicionados
	
	// Cuerpo principal
	glPushMatrix();
	glScalef(0.5f, 2.0f, 0.5f);
	glutSolidCube(1.0f);
	glPopMatrix();
	
	// Cabina superior
	glPushMatrix();
	glTranslatef(0.0f, 1.25f, 0.0f);
	glScalef(0.4f, 0.5f, 0.4f);
	glutSolidCube(1.0f);
	glPopMatrix();
	
	// Ala izquierda
	glPushMatrix();
	glTranslatef(-0.6f, 0.0f, 0.0f);
	glScalef(1.5f, 0.4f, 0.5f);
	glutSolidCube(1.0f);
	glPopMatrix();
	
	// Ala derecha
	glPushMatrix();
	glTranslatef(0.6f, 0.0f, 0.0f);
	glScalef(1.5f, 0.4f, 0.5f);
	glutSolidCube(1.0f);
	glPopMatrix();
	
	// Estabilizador vertical (cola)
	glPushMatrix();
	glTranslatef(0.0f, -1.0f, 0.25f);
	glScalef(0.1f, 0.5f, 0.4f);
	glutSolidCube(1.0f);
	glPopMatrix();
	
	// Estabilizador horizontal (cola)
	glPushMatrix();
	glTranslatef(0.0f, -1.0f, 0.0f);
	glScalef(1.0f, 0.1f, 0.4f);
	glutSolidCube(1.0f);
	glPopMatrix();
	
	glPopMatrix();           // Restaura la matriz de transformaciones
	glEnable(GL_TEXTURE_2D); // Reactiva texturas para otros objetos
}

// ------------ MAPA PAÍSES Y REGIONES ----------------

// Mapeo simplificado (puedes expandir/agregar países según tu JSON)
Region regiones[] = {
    {"America Norte/Central", { {"USA", {4,9}},{"USA", {4,8}},{"USA", {3,11}},{"USA", {5,8}},{"USA", {3,9}},{"USA", {3,8}},{"USA", {7,3}},{"USA", {1,11}},{"USA", {8,13}},{"USA", {8,11}},{"USA", {3,11}},{"USA", {9,13}},{"USA", {6,11}},
	{"USA", {5,9}},{"USA", {5,13}},{"USA", {3,10}},{"USA", {6,9}},{"USA", {8,12}},{"USA", {4,10}},{"USA", {4,11}}, {"Canada",{2,11}},{"Canada",{6,10}},{"Guatemala",{5,6}},
	{"Cuba",{4,9}}, {"Cuba",{4,9}},{"Cuba",{4,9}}, {"Mexico",{6,7}},{"Mexico",{4,7}} }},
    {"America Sur", { {"Brasil",{7,5}},{"Argentina",{19,11}},{"Argentina",{13,4}} ,{"Argentina",{7,2}},{"Peru",{5,11}}, {"Colombia",{8,4}},{"Chile",{8,3}},{"Chile",{15,9}}}},
    {"Europa Occidental", { {"Alemania",{12,9}}, {"Francia",{13,9}}, {"UK",{11,9}}, {"Espania",{13,10}}, {"Espania",{14,9}}, {"Italia",{13,8}} }},
    {"Europa Este/Rusia", { {"Polonia",{13,11}}, {"Ucrania",{14,8}},{"Ucrania",{11,8}}, {"Rusia",{15,8}},{"Rusia",{15,10}},{"Rusia",{16,10}},{"Rusia",{17,11}},{"Rusia",{14,10}},{"Rusia",{15,11}},{"Rusia",{15,12}},
	{"Rusia",{21,11}},{"Rusia",{18,10}},{"Rusia",{17,9}},{"Rusia",{18,9}},{"Rusia",{19,9}}, {"Rusia",{20,9}},{"Rusia",{19,9}}, {"Rusia",{18,11}},{"Rusia",{17,8}},
	{"Rusia",{20,11}},{"Rusia",{22,11}},{"Rusia",{16,11}},{"Rusia",{18,10}},{"Rusia",{18,11}},{"Rusia",{17,11}},{"Rusia",{18,12}},{"Rusia",{16,9}},{"Rusia",{19,10}},{"Rusia",
	{20,11}},{"Rusia",{23,11}},{"Rusia",{23,10}},{"Rusia",{18,13}},{"Rusia",{12,6}},{"Rusia",{13,6}},{"Rusia",{7,4}},
	{"Rusia",{20,12}},{"Rusia",{12,10}},{"Rusia",{19,12}}, {"Rumanía",{15,10}} }},
    {"Africa", { {"Sudafrica",{11,7}},{"Sudafrica",{11,6}},{"Sudafrica",{12,7}},{"Egipto",{6,4}},{"Egipto",{6,5}},{"Egipto",{6,1}},{"Sudafrica",{13,7}},  {"Marruecos",{17,10}}, 
	 {"Libia",{13,5}},{"Sudafrica",{14,6}},{"Tunez",{12,8}},{"Sudafrica",{14,5}},{"Sudafrica",{14,4}},{"Etiopia",{10,11}},{"Sudafrica",{13,3}}, 
	{"Sudafrica",{14,7}},{"Sudafrica",{19,5}},{"Sudafrica",{12,5}} }},
    {"Oriente Medio", { {"Irán",{16,8}}, {"Arabia Saudi",{15,7}}, {"Irak",{16,7}},{"Israel",{17,7}},{"Israel",{18,7}},{"Israel",{18,8}},{"Israel",{19,8}}, {"Turquia",{15,8}} }},
    {"Asia ", { {"India",{20,9}}, {"Bangladesh",{19,7}}, {"China",{13,5}}, {"Japon",{21,10}}, {"Corea Sur",{21,8}}, {"Mongolia",{21,5}} }},
    {"Sudeste Asiatico y Oceania", { {"Indonesia",{21,4}}, {"Filipinas",{20,10}}, {"Filipinas",{19,6}}, {"Filipinas",{20,8}}, {"Filipinas",{20,6}} ,{"Tailandia",{21,3}}, {"Malasia",{18,6}}, 
	{"Australia",{20,3}},{"Nueva Zelanda",{22,3}}, {"Nueva Zelanda",{23,2}} }}
};
const int numRegiones = sizeof(regiones) / sizeof(regiones[0]);

// --------------- GLOBALES ---------------
std::map<std::string, FocoPais> focosPorPais;
json covidData;
const std::vector<std::string> periodos = {
    "2020-01_to_2020-04", "2020-05_to_2020-08", "2020-09_to_2020-12",
    "2021-01_to_2021-04", "2021-05_to_2021-08", "2021-09_to_2021-12",
    "2022-01_to_2022-04", "2022-05_to_2022-08", "2022-09_to_2022-12",
    "2023-01_to_2023-04", "2023-05_to_2023-08"
};
int periodoActual = 0;
float pulso = 0.0f;
int windowWidth = 1000, windowHeight = 850;
textura fondo;

// ------------ FUNCIONES DE MAPA --------------------
float colToX(int col) {
    return -1.01f + (col + 0.5f) * (2.0f / NUM_COLUMNAS);
}
float filaToY(int fila) {
    return -1.0f + (fila + 0.5f) * (2.0f / NUM_FILAS);
}
void cargarDatosCovidJson() {
    std::ifstream in("casos_covid.json");
    if (!in) { std::cerr << "No se pudo abrir el archivo JSON\n"; exit(1);}
    in >> covidData;
}
int cargaTGA(const char *nombre, textura *imagen) {
    GLubyte cabeceraTGA[12] = {0,0,2,0,0,0,0,0,0,0,0,0};
    GLubyte compararTGA[12]; GLubyte cabecera[6];
    GLuint bytesPorPixel, tamanoImagen, temp, tipo = GL_RGBA;
    FILE *archivo = fopen(nombre, "rb");
    if (!archivo || fread(compararTGA,1,12,archivo)!=12 ||
        memcmp(cabeceraTGA,compararTGA,12)!=0 ||
        fread(cabecera,1,6,archivo)!=6) {
        printf("No se pudo cargar la imagen %s\n", nombre);
        if (archivo) fclose(archivo); return 0;
    }
    imagen->ancho = cabecera[1]*256 + cabecera[0];
    imagen->largo = cabecera[3]*256 + cabecera[2];
    imagen->bpp   = cabecera[4];
    if (imagen->ancho<=0||imagen->largo<=0||(imagen->bpp!=24&&imagen->bpp!=32)) {
        printf("Datos inválidos en %s\n", nombre); fclose(archivo); return 0;
    }
    bytesPorPixel = imagen->bpp / 8;
    tamanoImagen = imagen->ancho * imagen->largo * bytesPorPixel;
    imagen->dibujo = (GLubyte*)malloc(tamanoImagen);
    if (!imagen->dibujo||fread(imagen->dibujo,1,tamanoImagen,archivo)!=tamanoImagen){
        printf("Error leyendo datos de %s\n", nombre); if (imagen->dibujo) free(imagen->dibujo); fclose(archivo); return 0;}
    for (unsigned int i=0;i<tamanoImagen;i+=bytesPorPixel){
        temp = imagen->dibujo[i]; imagen->dibujo[i] = imagen->dibujo[i+2]; imagen->dibujo[i+2]=temp;}
    fclose(archivo);
    glGenTextures(1,&imagen->ID); glBindTexture(GL_TEXTURE_2D, imagen->ID);
    tipo = (imagen->bpp == 24) ? GL_RGB : GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D,0,tipo,imagen->ancho,imagen->largo,0,tipo,GL_UNSIGNED_BYTE,imagen->dibujo);
    glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    return 1;
}
void focosDePersonas(float x, float y) {
    float radioBase = 0.015f, radioPulso = radioBase + 0.015f * sin(pulso);
    int segmentos = 40;
    float aspectRatio = (float)windowWidth / windowHeight;
    // Círculo interno
    glDisable(GL_TEXTURE_2D);
    glColor4f(1, 0, 0, 1.0f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y);
    for (int i=0;i<=segmentos;i++) {
        float theta = 2.0f * M_PI * i / segmentos;
        float dx = cos(theta)*radioBase/aspectRatio;
        float dy = sin(theta)*radioBase;
        glVertex2f(x+dx, y+dy);
    } glEnd();
    // Anillo pulsante
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1,0,0,0.3f);
    glBegin(GL_TRIANGLE_FAN); glVertex2f(x,y);
    for (int i=0;i<=segmentos;i++) {
        float theta = 2.0f * M_PI * i / segmentos;
        float dx = cos(theta)*radioPulso/aspectRatio;
        float dy = sin(theta)*radioPulso;
        glVertex2f(x+dx,y+dy);
    }
    glEnd();
    glDisable(GL_BLEND); glEnable(GL_TEXTURE_2D);
}

// ----------- SIMULACIÓN DE EXPANSIÓN --------------

void expandirFocos(FocoPais &fp, Celda celda, int periodo, int nuevosFocos) {
    if (periodo == 0) {
        float x = colToX(celda.col), y = filaToY(celda.fila);
        fp.focosPorPeriodo[0] = { {x, y} };
        return;
    }

    if (!fp.focosPorPeriodo[periodo].empty()) return;

    fp.focosPorPeriodo[periodo] = fp.focosPorPeriodo[periodo - 1];
    int prevCount = fp.focosPorPeriodo[periodo].size();

    for (int i = 0; i < nuevosFocos; ++i) {
        int base = rand() % prevCount;
        float baseX = fp.focosPorPeriodo[periodo][base].x;
        float baseY = fp.focosPorPeriodo[periodo][base].y;

        float angle = ((float)rand() / RAND_MAX) * 2.0f * M_PI;

        // Aumenta el radio según el periodo (crecimiento más libre al final)
        float baseRadius = 0.01f;
        float maxRadius = 0.02f + 0.03f * (periodo / 10.0f);  // se hace más grande con el tiempo
        float radius = baseRadius + ((float)rand() / RAND_MAX) * maxRadius;

        float nx = baseX + cos(angle) * radius;
        float ny = baseY + sin(angle) * radius;

        if (nx < -1.0f || nx > 1.0f || ny < -1.0f || ny > 1.0f)
            continue;

        // Calcula columna/fila nueva
        int nuevaCol = (int)((nx + 1.0f) * (NUM_COLUMNAS / 2.0f));
        int nuevaFila = (int)((ny + 1.0f) * (NUM_FILAS / 2.0f));

        // Calcula la tolerancia de expansión según el periodo
        int tolerancia = (periodo < 4) ? 1 : (periodo < 8 ? 2 : 3);  // más permisivo en periodos altos

        if (abs(nuevaCol - celda.col) > tolerancia || abs(nuevaFila - celda.fila) > tolerancia)
            continue;  // punto demasiado lejos de la base

        fp.focosPorPeriodo[periodo].push_back({ nx, ny });
    }
}



// ---------- CÁLCULO DE NÚMERO DE FOCOS POR PERIODO/PÁIS ----------
int calcularNuevosFocos(const std::string &region, const std::string &pais, int periodo) {
    // Relación de cantidad de casos vs cantidad de focos (ajusta el divisor para menos/más densidad)
    int casosActuales = 0, casosPrevios = 0;
    try {
        casosActuales = covidData[region][pais][periodos[periodo]];
        if (periodo>0) casosPrevios = covidData[region][pais][periodos[periodo-1]];
    } catch (...) {}
    int nuevos = (casosActuales-casosPrevios)/300000; // cada 100,000 casos -> 1 nuevo foco (ajusta aquí)
    if (periodo == 0 && casosActuales > 0) return 1; // al menos 1 foco inicial
    if (nuevos < 0) nuevos = 0;
    return nuevos;
}


// ----------- DIBUJAR PANEL DE DATOS (OMITIDO AQUÍ POR ESPACIO, IGUAL AL TUYO) -------------
// ... puedes agregar aquí tu función para el panel de datos del JSON ....

//----------------Panel de datos................//

void renderTexto(float x, float y, const char* texto, float size) {
    glColor4f(1, 1, 1, 0.1f); // Blanco puro para el texto
    glRasterPos2f(x, y);
    for (const char* c = texto; *c; ++c)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *c);
}



std::vector<std::string> ordenColumnas = {
    "America Norte/Central",
    "America Sur",
    "Europa Occidental",
    "Europa Este/Rusia",
    "Africa",
    "Oriente Medio",
    "Asia ",  
    "Sudeste Asiatico y Oceania"
    
};

void dibujarPanelDatosCovidJson() {
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f); // Panel blanco

    // Panel inferior ajustado para 850 px de alto
    float y_panel0 = -1.33f, y_panel1 = -1.0f;
    float x_panel0 = -1.0f,  x_panel1 =  1.0f;
    glBegin(GL_QUADS);
    glVertex2f(x_panel0, y_panel0);
    glVertex2f(x_panel1, y_panel0);
    glVertex2f(x_panel1, y_panel1);
    glVertex2f(x_panel0, y_panel1);
    glEnd();

    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);

    // Texto negro
    glColor3f(0.0f, 0.0f, 0.0f);

    // Configuración de columnas
    float x_inicial = -0.97f;   // Primer columna a la izquierda
    float espacio_col = 0.23f;  // Espacio horizontal entre columnas
    float y_titulo = -1.09f;    // Y inicial para el título
    float espacio_fila = 0.03f; // Espacio entre filas de texto

    // Título del periodo (se pone arriba a la izquierda)
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "Periodo: %s", periodos[periodoActual].c_str());
    renderTexto(-0.2f, -1.04f, buffer, 0.022f); 

    // Recorre las columnas ordenadas
    for (size_t col = 0; col < ordenColumnas.size(); ++col) {
        float x = x_inicial + col * espacio_col;
        float y = y_titulo;

        // Título de la división
        renderTexto(x, y, ordenColumnas[col].c_str(),0.018f);
        y -= espacio_fila;

        // Busca la división en tu JSON y escribe los países debajo
        auto itDiv = covidData.find(ordenColumnas[col]);
        if (itDiv != covidData.end()) {
            for (auto itPais = itDiv.value().begin(); itPais != itDiv.value().end(); ++itPais) {
                std::string pais = itPais.key();
                int victimas = 0;
                try {
                    victimas = itPais.value().at(periodos[periodoActual]);
                } catch (...) {}
                snprintf(buffer, sizeof(buffer), "%s: %d", pais.c_str(), victimas);
                renderTexto(x + 0.01f, y, buffer ,0.013f);
                y -= espacio_fila;
            }
        }
    }
}

// ------------- DISPLAY -----------------
void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    // Fondo
    glBindTexture(GL_TEXTURE_2D, fondo.ID);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex2f(-1, -1);
    glTexCoord2f(1, 1); glVertex2f(1, -1);
    glTexCoord2f(1, 0); glVertex2f(1, 1);
    glTexCoord2f(0, 0); glVertex2f(-1, 1);
    glEnd();

    // Dibuja los focos por cada país
    for (const auto& par : focosPorPais) {
    const FocoPais& fp = par.second;
    for (const auto& foco : fp.focosPorPeriodo[periodoActual]) {
        focosDePersonas(foco.x, foco.y);
    }
}
	

    // Puedes poner aquí tu panel de datos...
    dibujarPanelDatosCovidJson();

for (int i = 0; i < NUM_AVIONES; i++) {
    moverAvion(&aviones[i]);
    dibujarAvion(&aviones[i]);
}


    glutSwapBuffers();
}

void reshape(int w, int h) {
    windowWidth = w; windowHeight = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluOrtho2D(-1, 1, -1.33, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
}
void idle() {
    pulso += 0.1f; if (pulso > 2*M_PI) pulso = 0;
    glutPostRedisplay();
}

// ------- GENERA LOS FOCOS DE CADA PAÍS POR PERIODO, SOLO UNA VEZ ---------
void actualizarFocosHastaPeriodo(int periodo) {
    int contadorGlobal = 0; // contador único para cada entrada
    for (int r = 0; r < numRegiones; r++) {
        Region& reg = regiones[r];
        for (auto& pais : reg.paises) {
            // Generar clave única combinando nombre + índice
            std::string claveUnica = pais.nombre + "_" + std::to_string(contadorGlobal++);

            // Crear o acceder al FocoPais con la clave única
            FocoPais& fp = focosPorPais[claveUnica];

            // Generar los focos para todos los periodos hasta el actual
            for (int p = 0; p <= periodo; ++p) {
                int nuevos = calcularNuevosFocos(reg.nombre, pais.nombre, p);
                expandirFocos(fp, pais.celda, p, nuevos);
            }
        }
    }
}

// ---------- ENTRADA DE TECLADO --------------
void teclasEspeciales(int key, int x, int y) {
    if (key == GLUT_KEY_RIGHT) {
        if (periodoActual+1 < (int)periodos.size()) {
            periodoActual++;
            actualizarFocosHastaPeriodo(periodoActual);
        }
        glutPostRedisplay();
    }
    if (key == GLUT_KEY_LEFT) {
        if (periodoActual>0) {
            periodoActual--;
        }
        glutPostRedisplay();
    }
}

// ----------- INICIALIZACIÓN -------------------
void init() {
    glEnable(GL_TEXTURE_2D);
    srand(time(NULL));
    if (!cargaTGA("fondo.tga", &fondo)) {
        printf("No se pudo cargar la textura de fondo.\n");
        exit(1);
    }
    // Calcula todos los focos hasta el primer periodo
    actualizarFocosHastaPeriodo(periodoActual);

    //----------------------------Dibujar aviones----------------------------//
	
	// Coordenadas origen/destino para 6 aviones
	int origenes[NUM_AVIONES][2] = {
		{7, 2}, {6, 1}, {8, 4}, {13, 5}, {17, 9}, {21, 3}
	};
	int destinos[NUM_AVIONES][2] = {
		{14, 9}, {12, 10}, {6, 10}, {18, 7}, {12, 5}, {3, 9}
	};
	
	// Crear cada avi n con sus coordenadas
	for (int i = 0; i < NUM_AVIONES; i++) {
		aviones[i] = crearAvion(origenes[i], destinos[i]);
	}
}

// ---------------- MAIN -------------------
int main(int argc, char **argv) {
    cargarDatosCovidJson();
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(windowWidth, windowHeight);
    glutCreateWindow("Simulador de Expansión de Focos COVID");
    glutSpecialFunc(teclasEspeciales);
    init();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutIdleFunc(idle);
    glutMainLoop();
    return 0;
}

