/* 
Base code for joint class with Applied Parallel computing
Using a framebuffer as a basis to implement Gaussian blur in GLSL
Currently will make 2 FBOs and textures (only uses one in base code)
and writes out frame as a .png (Texture_output.png)
upper right quad on screen is blue-er to show fragment shader effect on texture

Winter 2017 - ZJW (Piddington texture write)
Look for "TODO" in this file and write new shaders
*/

#include <iostream>
#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "GLSL.h"
#include "Program.h"
#include "MatrixStack.h"
#include "Shape.h"
//#include "GLTextureWriter.h"
#include "Texture.h"

// value_ptr for glm
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

const float PI = 3.14159;
const float DEG_85 = 1.48353;
const int NUM_COMPONENTS = 6;

using namespace std;
using namespace glm;

GLFWwindow *window; // Main application window
string RESOURCE_DIR = ""; // Where the resources are loaded from
shared_ptr<Program> prog, prog1, prog2;
shared_ptr<Shape> cylinder, sphere;

Texture texture2;

int g_width = 512;
int g_height = 512;
int g_GiboLen = 6;

double yorig;
double phi = 0;
bool firsttime = true;

glm::vec3 target(0, 2, 0), eye(0, 3, 5);
float theta = 0, radius = 5;

typedef struct Component {
	float angle;
	bool selected;
} Component;

vec3 goal = vec3(0, 0, 0);

Component components[NUM_COMPONENTS];
int selectedComponent = 2;

//global reference to texture FBO
GLuint depthBuf;

//geometry for texture render
GLuint quad_VertexArrayID;
GLuint quad_vertexbuffer;
GLuint GrndBuffObj, GrndNorBuffObj, GrndTexBuffObj, GIndxBuffObj;

//forward declaring a useful function listed later
void SetMaterial(shared_ptr<Program> *p, int i);

void initComponents() {
	int i;
	for (i = 0; i < NUM_COMPONENTS; i++) {
		components[i].angle = 0;
		components[i].selected = false;
	}
	components[0].selected = true;
	components[1].angle = PI / 4;
}

/**** geometry set up for a quad *****/
void initQuad() {

 	//now set up a simple quad for rendering FBO
  	glGenVertexArrays(1, &quad_VertexArrayID);
  	glBindVertexArray(quad_VertexArrayID);

  	static const GLfloat g_quad_vertex_buffer_data[] = {
    		-1.0f, -1.0f, 0.0f,
    		1.0f, -1.0f, 0.0f,
    		-1.0f,  1.0f, 0.0f,
    		-1.0f,  1.0f, 0.0f,
    		1.0f, -1.0f, 0.0f,
    		1.0f,  1.0f, 0.0f,
  	};

  	glGenBuffers(1, &quad_vertexbuffer);
  	glBindBuffer(GL_ARRAY_BUFFER, quad_vertexbuffer);
  	glBufferData(GL_ARRAY_BUFFER, sizeof(g_quad_vertex_buffer_data), g_quad_vertex_buffer_data, GL_STATIC_DRAW);

}

static void initProg(shared_ptr<Program> *p, string vertShader, string fragShader) {
	(*p) = make_shared<Program>();
	(*p)->setVerbose(true);
	(*p)->setShaderNames(RESOURCE_DIR + vertShader, RESOURCE_DIR + fragShader);
	(*p)->init();
	(*p)->addUniform("P");
	(*p)->addUniform("M");
	(*p)->addUniform("V");
	(*p)->addAttribute("vertPos");
	(*p)->addAttribute("vertNor");
}

/* lots of initialization to set up the opengl state and data */
static void initGL()
{
	GLSL::checkVersion();
  	int width, height;
  	glfwGetFramebufferSize(window, &width, &height);

	// Set background color.
	glClearColor(.12f, .34f, .56f, 1.0f);
	// Enable z-buffer test.
	glEnable(GL_DEPTH_TEST);

	cylinder = make_shared<Shape>();
	cylinder->loadMesh(RESOURCE_DIR + "cylinder.obj");
	cylinder->resize();
	cylinder->init();

	sphere = make_shared<Shape>();
	sphere->loadMesh(RESOURCE_DIR + "sphere.obj");
	sphere->resize();
	sphere->init();

	//Initialize the geometry to render a quad to the screen
	initQuad();

	texture2.setFilename(RESOURCE_DIR + "grass.bmp");
	texture2.setUnit(2);
	texture2.setName("Texture2");
	texture2.init();

	GLSL::printError();

	initProg(&prog, "simple_vert.glsl", "simple_frag.glsl");
	prog->addUniform("MatAmb");
	prog->addUniform("MatDif");
	prog->addUniform("MatSpec");
	prog->addUniform("shine");
	prog->addUniform("center");
	prog->addUniform("viewVector");
	
	// Initialize the GLSL program to render the obj
	initProg(&prog1, "simple_vert.glsl", "simple_frag.glsl");
	prog1->addUniform("MatAmb");
	prog1->addUniform("MatDif");
	prog1->addUniform("MatSpec");
	prog1->addUniform("shine");
	prog1->addAttribute("vertTex");
	glGenRenderbuffers(1, &depthBuf);

	// Initialize the ground program
	initProg(&prog2, "ground_vert.glsl", "ground_frag.glsl");
	prog2->addUniform("Texture2");
	prog2->addTexture(&texture2);

  	//set up depth necessary since we are rendering a mesh that needs depth test
  	glBindRenderbuffer(GL_RENDERBUFFER, depthBuf);
  	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
  	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuf);
  	
	//more FBO set up
	GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
  	glDrawBuffers(1, DrawBuffers);
}

static void initGeom() {
   	float g_groundSize = 20;
   	float g_groundY = 0;

  	/* A x-z plane at y = g_groundY of dimension [-g_groundSize, g_groundSize]^2*/
    	float GrndPos[] = {
    		-g_groundSize, g_groundY, -g_groundSize,
    		-g_groundSize, g_groundY,  g_groundSize,
     		g_groundSize, g_groundY,  g_groundSize,
     		g_groundSize, g_groundY, -g_groundSize
    	};

    	float GrndNorm[] = {
     		0, 1, 0,
     		0, 1, 0,
     		0, 1, 0,
     		0, 1, 0,
     		0, 1, 0,
     		0, 1, 0
    	};


  	static GLfloat GrndTex[] = {
      		0, 0, /* back*/
      		0, 20,
      		20, 20,
      		20, 0 
	};

    	unsigned short idx[] = {0, 1, 2, 0, 2, 3};


   	GLuint VertexArrayID;
        /*generate the VAO*/
        glGenVertexArrays(1, &VertexArrayID);
        glBindVertexArray(VertexArrayID);

    	g_GiboLen = 6;
    	glGenBuffers(1, &GrndBuffObj);
    	glBindBuffer(GL_ARRAY_BUFFER, GrndBuffObj);
    	glBufferData(GL_ARRAY_BUFFER, sizeof(GrndPos), GrndPos, GL_STATIC_DRAW);

    	glGenBuffers(1, &GrndNorBuffObj);
    	glBindBuffer(GL_ARRAY_BUFFER, GrndNorBuffObj);
    	glBufferData(GL_ARRAY_BUFFER, sizeof(GrndNorm), GrndNorm, GL_STATIC_DRAW);
    
    	glGenBuffers(1, &GrndTexBuffObj);
    	glBindBuffer(GL_ARRAY_BUFFER, GrndTexBuffObj);
    	glBufferData(GL_ARRAY_BUFFER, sizeof(GrndTex), GrndTex, GL_STATIC_DRAW);

    	glGenBuffers(1, &GIndxBuffObj);
    	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GIndxBuffObj);
    	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
}

/* The render loop - this function is called repeatedly during the OGL program run */
static void render()
{
	// Get current frame buffer size.
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	glViewport(0, 0, width, height);

	//set up to render to buffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	// Clear framebuffer.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	/* Leave this code to just draw the meshes alone */
	//Use the matrix stack for Lab 6
   	float aspect = width/(float)height;

   	// Create the matrix stacks - please leave these alone for now
   	auto P = make_shared<MatrixStack>();
   	auto M = make_shared<MatrixStack>();
        glm::vec3 up(0, 1, 0);

	// Recalculate eye position
	eye.x = radius * sin(theta);
	eye.z = radius * cos(theta);

	glm::mat4 V = glm::lookAt(eye, target, up);
   	// Apply perspective projection.
   	P->pushMatrix();
   	P->perspective(45.0f, aspect, 0.01f, 100.0f);

	prog->bind();
	glUniformMatrix4fv(prog->getUniform("P"), 1, GL_FALSE, value_ptr(P->topMatrix()));
	glUniformMatrix4fv(prog->getUniform("V"), 1, GL_FALSE, value_ptr(V));

	// Create the base
	SetMaterial(&prog, 5);
	M->pushMatrix();
		M->loadIdentity();
		M->translate(vec3(0, 0.5, 0));
		M->pushMatrix();
			M->scale(vec3(1, 0.5, 1));
			glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, 
				value_ptr(M->topMatrix()));
		M->popMatrix();
		cylinder->draw(prog);
		
		// Draw a lazy susan
		SetMaterial(&prog, selectedComponent == 0 ? 6 : 2);
		M->translate(vec3(0, 0.625, 0));
		M->rotate(components[0].angle, vec3(0, 1, 0));
		M->pushMatrix();
			M->scale(vec3(1, 0.125, 1));
			glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, 
				value_ptr(M->topMatrix()));
			cylinder->draw(prog);
		M->popMatrix();

		// Draw elbows 
		int i;
		for (i = 1; i < NUM_COMPONENTS; i++) {
		   SetMaterial(&prog, 3);
		   M->translate(vec3(0, 0.25, 0));
		   M->pushMatrix();
			M->scale(vec3(0.5, 0.5, 0.5));
			glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, 
				value_ptr(M->topMatrix()));
			sphere->draw(prog);
		   M->popMatrix();
		   SetMaterial(&prog, selectedComponent == i ? 6 : 2);
		   M->translate(vec3(0, 0.25, 0));
		   M->rotate(components[i].angle, vec3(0, 0, 1));
		   M->translate(vec3(0, 0.75, 0));
		   M->pushMatrix();
			M->scale(vec3(0.25, 1, 0.25));
			glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, 
				value_ptr(M->topMatrix()));
			cylinder->draw(prog);
		   M->popMatrix();
		   M->translate(vec3(0, 1, 0));
		}
	M->popMatrix();

	prog->unbind();

        /*draw the ground plane */
        prog2->bind();
	M->pushMatrix();
		M->loadIdentity();
        glUniformMatrix4fv(prog2->getUniform("P"), 1, GL_FALSE, value_ptr(P->topMatrix()));
        glUniformMatrix4fv(prog2->getUniform("M"), 1, GL_FALSE, value_ptr(M->topMatrix()));
	glUniformMatrix4fv(prog2->getUniform("V"), 1, GL_FALSE, value_ptr(V));

        glEnableVertexAttribArray(0);
   	glBindBuffer(GL_ARRAY_BUFFER, GrndBuffObj);
   	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

        glEnableVertexAttribArray(1);
   	glBindBuffer(GL_ARRAY_BUFFER, GrndNorBuffObj);
   	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
         
        glEnableVertexAttribArray(2);
   	glBindBuffer(GL_ARRAY_BUFFER, GrndTexBuffObj);
   	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);

   	/* draw!*/
   	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GIndxBuffObj);
   	glDrawElements(GL_TRIANGLES, g_GiboLen, GL_UNSIGNED_SHORT, 0);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
        
        prog2->unbind();
	M->popMatrix();
   	P->popMatrix();
}

/* helper function */
static void error_callback(int error, const char *description) {
	cerr << description << endl;
}

static void computeAngles(vec3 goal) {

	

}

/* key callback */
static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, GL_TRUE);
	}
	if (key == GLFW_KEY_A && (action == GLFW_PRESS
		|| action == GLFW_REPEAT)) {
		theta -= 0.1;
	} else if (key == GLFW_KEY_D && (action == GLFW_PRESS
		|| action == GLFW_REPEAT)) {
		theta += 0.1;
	} else if (key == GLFW_KEY_S && (action == GLFW_PRESS
		|| action == GLFW_REPEAT)) {
		radius += 0.2;
	} else if (key == GLFW_KEY_W && (action == GLFW_PRESS
		|| action == GLFW_REPEAT)) {
		radius -= 0.2;
	} else if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
		selectedComponent = ++selectedComponent % NUM_COMPONENTS;
	} else if (key == GLFW_KEY_RIGHT && (action == GLFW_PRESS ||
		action == GLFW_REPEAT)) {
		components[selectedComponent].angle -= 0.1;
	} else if (key == GLFW_KEY_LEFT && (action == GLFW_PRESS ||
		action == GLFW_REPEAT)) {
		components[selectedComponent].angle += 0.1;
	} else if (key == GLFW_KEY_ENTER && action == GLFW_PRESS) {
		scanf("%f%f%f", &goal.x, &goal.y, &goal.z);
		computeAngles(goal);
	}
}

static void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
        if (firsttime) {
                yorig = ypos;
                firsttime = 0;
        } else {
		double dy = yorig - ypos;
		double scale = 2* PI / (float) g_width;
                phi -= dy * scale;
                phi = phi > DEG_85 ? DEG_85 : phi;
                phi = phi < -DEG_85 ? -DEG_85 : phi;
                yorig = ypos;
		eye = glm::vec3(
			eye.x,
			sin(phi) + target.y,
			eye.z);
        }
}

/* resize window call back */
static void resize_callback(GLFWwindow *window, int width, int height) {
   	g_width = width;
   	g_height = height;
   	glViewport(0, 0, width, height);
}

//helper function to set materials for shadin
void SetMaterial(shared_ptr<Program> *p, int i) {
  switch (i) {
    	case 0: //shiny blue plastic
 		glUniform3f((*p)->getUniform("MatAmb"), 0.02, 0.04, 0.2);
 		glUniform3f((*p)->getUniform("MatDif"), 0.0, 0.16, 0.9);
		glUniform3f((*p)->getUniform("MatSpec"), 0.14, 0.2, 0.8);
		glUniform1f((*p)->getUniform("shine"), 120.0);
      		break;
    	case 1: // flat grey
 		glUniform3f((*p)->getUniform("MatAmb"), 0.13, 0.13, 0.14);
 		glUniform3f((*p)->getUniform("MatDif"), 0.3, 0.3, 0.4);
		glUniform3f((*p)->getUniform("MatSpec"), 0.3, 0.3, 0.4);
		glUniform1f((*p)->getUniform("shine"), 4.0);
      		break;
    	case 2: //brass
 		glUniform3f((*p)->getUniform("MatAmb"), 0.3294, 0.2235, 0.02745);
 		glUniform3f((*p)->getUniform("MatDif"), 0.7804, 0.5686, 0.11373);
		glUniform3f((*p)->getUniform("MatSpec"), 0.9922, 0.941176, 0.80784);
		glUniform1f((*p)->getUniform("shine"), 27.9);
      		break;
	case 3: //copper
 		glUniform3f((*p)->getUniform("MatAmb"), 0.1913, 0.0735, 0.0225);
 		glUniform3f((*p)->getUniform("MatDif"), 0.7038, 0.27048, 0.0828);
		glUniform3f((*p)->getUniform("MatSpec"), 0.257, 0.1376, 0.08601);
		glUniform1f((*p)->getUniform("shine"), 12.8);
      		break;
	case 4: //turqouise
 		glUniform3f((*p)->getUniform("MatAmb"), 0.1, 0.18725, 0.1745);
 		glUniform3f((*p)->getUniform("MatDif"), 0.396, 0.74151, 0.69102);
		glUniform3f((*p)->getUniform("MatSpec"), 0.297254, 0.30829, 0.306678);
		glUniform1f((*p)->getUniform("shine"), 0.1 * 128);
      		break;
	case 5: //black rubber
 		glUniform3f((*p)->getUniform("MatAmb"), 0.02, 0.02, 0.02);
 		glUniform3f((*p)->getUniform("MatDif"), 0.01, 0.01, 0.01);
		glUniform3f((*p)->getUniform("MatSpec"), 0.4, 0.4, 0.4);
		glUniform1f((*p)->getUniform("shine"), 0.078125 * 128);
      		break;
	case 6: // emerald
 		glUniform3f((*p)->getUniform("MatAmb"), 0.0215, 0.1745, 0.0215);
 		glUniform3f((*p)->getUniform("MatDif"), 0.07568, 0.61424, 0.07568);
		glUniform3f((*p)->getUniform("MatSpec"), 0.633, 0.727811, 0.633);
		glUniform1f((*p)->getUniform("shine"), 0.6 * 128);
      		break;
	case 7: //pearl
 		glUniform3f((*p)->getUniform("MatAmb"), 0.25, 0.20725, 0.20725);
 		glUniform3f((*p)->getUniform("MatDif"), 1, 0.829, 0.829);
		glUniform3f((*p)->getUniform("MatSpec"), 0.296648, 0.296648, 0.296648);
		glUniform1f((*p)->getUniform("shine"), 0.088 * 128);
		break;
  }
}

int main(int argc, char **argv) {
	if(argc < 2) {
		cout << "Please specify the resource directory." << endl;
		return 0;
	}
	RESOURCE_DIR = argv[1] + string("/");

	// Set error callback.
	glfwSetErrorCallback(error_callback);
	// Initialize the library.
	if(!glfwInit()) {
		return -1;
	}
   	//request the highest possible version of OGL - important for mac
   	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
   	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
   	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
   	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);

	// Create a windowed mode window and its OpenGL context.
	window = glfwCreateWindow(g_width, g_height, "FBO test", NULL, NULL);
	if(!window) {
		glfwTerminate();
		return -1;
	}
	// Make the window's context current.
	glfwMakeContextCurrent(window);
	// Initialize GLEW.
	glewExperimental = true;
	if(glewInit() != GLEW_OK) {
		cerr << "Failed to initialize GLEW" << endl;
		return -1;
	}
	//weird bootstrap of glGetError
   	glGetError();
	cout << "OpenGL version: " << glGetString(GL_VERSION) << endl;
   	cout << "GLSL version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;

	// Set vsync.
	glfwSwapInterval(1);
	// Set keyboard callback.
	glfwSetKeyCallback(window, key_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
   	//set the window resize call back
   	glfwSetFramebufferSizeCallback(window, resize_callback);

	// Initialize scene. Note geometry initialized in init now
	initComponents();
	initGL();
	initGeom();

	// Loop until the user closes the window.
	while(!glfwWindowShouldClose(window)) {
		// Render scene.
		render();
		// Swap front and back buffers.
		glfwSwapBuffers(window);
		// Poll for and process events.
		glfwPollEvents();
	}
	// Quit program.
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
