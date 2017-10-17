#include <GL/glew.h> // include GLEW and new version of GL on Windows
#include <GLFW/glfw3.h> // GLFW helper library
#include <GLM/glm.hpp>
#include <GLM/gtc/matrix_transform.hpp> // glm::translate, glm::rotate, glm::scale, glm::perspective
#include <GLM/gtc/type_ptr.hpp> // glm::value_ptr
#include <stdio.h>
#include <string>
#include <iostream>
#define reportError(s) _ReportError(__LINE__, (s))

using namespace std;
using namespace glm;

void _ReportError(int ln, const string str)
{
	GLuint err = glGetError();
	if (!err) return;
	const GLubyte* glerr = gluErrorString(err);
	printf("******************************************\n%d: %s: GLError %d: %s\n", ln, str.c_str(), err, glerr);
}

void glfwErrorCB(int error, const char* description)
{
	fputs(description, stderr);
}

GLuint initShader(const char* source, GLenum type)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, (const GLchar**)&source, NULL);
	glCompileShader(shader);

	GLint  compiled;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled)
	{
		fprintf(stderr, "Failed to compile shader!\n");
		fprintf(stderr, "%s\n", source);

		GLint maxLength = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

		char *infoLog = new char[maxLength];
		glGetShaderInfoLog(shader, maxLength, NULL, infoLog);

		std::cout << infoLog << std::endl;

		free(infoLog);

		exit(EXIT_FAILURE);
	}
	return shader;

}

GLuint initShaders(const char* vertShaderSrc, const char *fragShaderSrc)
{
	GLuint program = glCreateProgram();
	glAttachShader(program, initShader(vertShaderSrc, GL_VERTEX_SHADER));
	glAttachShader(program, initShader(fragShaderSrc, GL_FRAGMENT_SHADER));
	glLinkProgram(program);

	/* link and error check */
	GLint  linked;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (!linked)
	{
		cerr << "Shader failed to link!\n";
		GLint  logSize = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logSize);

		char *infoLog = new char[logSize];
		glGetProgramInfoLog(program, logSize, NULL, infoLog);
		cout << infoLog << endl;
		free(infoLog);
		exit(EXIT_FAILURE);
	}
	else
	{
		cout << "Shaders link successful!\n";
	}

	return program;
}

struct structLight { vec4 pos; vec3 color; GLfloat intensity; };
struct structMaterial { vec3 ambient, diffuse, specular; GLfloat shininess; };

const float speed = 0.0002;
float Cx = 0, Cy = 0, Cz = 0, ang = 0;
int dir[] = { 0, 0,  0, 0,  0, 0,      0, 0,  0, 0,  0, 0 };
bool phong = true;
int camPresetMode = 0, changeCamPos = 0, numLights;
const int maxVertices = 400, maxIndices = maxVertices*3;
vec3 pointOfInterest, cameraLocation, cameraUp, camPresetPos[4], POIPresetPos[4];
mat4 Projection, View, PV;
structLight light[2];
structMaterial copper, silver, gold;

class prop
{
	public:
		int numIndices, index[maxIndices], outline;
		vec3 vertex[maxVertices], normal[maxVertices], propColor, center;
		GLuint vertexPos[2], normalPos[2], VAO, VBO, IBO, shaderProg[2], MUniformLoc[2], PVUniformLoc[2];
		GLuint propColorLoc[2], ConstsLoc[2], L0PosLoc[2], L0ColorLoc[2], L1PosLoc[2], L1ColorLoc[2], ambiCompLoc[2], diffCompLoc[2], specCompLoc[2];
		mat4 Model;
		structMaterial material;
		void init(int, int, vec3, vec3, mat4, structMaterial, bool);
		void render();
};

void prop::init(int l_numVertices, int l_numIndices, vec3 l_color, vec3 l_center, mat4 l_Model, structMaterial l_material, bool l_outline)
{
	int l_triIndex, l_triVertexIndex, l_Bindex, l_Cindex, j;
	int l_sizeOfVertices = sizeof(vec3)*l_numVertices;
	int l_sizeOfM_Shin = sizeof(GLfloat)*l_numVertices;
	vec3 l_AB, l_AC;
	numIndices = l_numIndices;
	propColor = l_color;
	center = l_center;
	outline = l_outline;
	Model = l_Model;
	material = l_material;

	// Calculating NORMALS
	if (l_outline == false)
	{
		//cout << "===========================================================================" << endl;
		for (int i = 0; i < l_numVertices; i++)
		{
			normal[i] = vec3(0.0f);
			//cout << "------------------------------------\nVertex = " << i << endl;
			for (int j = 0; j < l_numIndices; j++)
			{
				if (index[j] == i)
				{
					l_triIndex = j / 3;
					l_triVertexIndex = j % 3;
					//cout << "Found Vertex in triangle = " << l_triIndex << endl;
					//cout << "Place = " << l_triVertexIndex << endl;
					if (l_triVertexIndex == 0) { l_Bindex = index[j + 1]; l_Cindex = index[j + 2]; j = j + 2; }
					else if (l_triVertexIndex == 1) { l_Bindex = index[j + 1]; l_Cindex = index[j - 1]; j++; }
					else                            { l_Bindex = index[j - 2]; l_Cindex = index[j - 1]; }
					//cout << "The other two vertices in the triangle are = " << l_Bindex << ", " << l_Cindex << endl;
					l_AB = vertex[l_Bindex] - vertex[i];
					l_AC = vertex[l_Cindex] - vertex[i];
					//cout << "Normal before calculation = " << normal[i].x << ", " << normal[i].y << ", " << normal[i].z << endl;
					normal[i] = normalize(normal[i] + normalize(cross(l_AB, l_AC)));
					//cout << "Normal after calculation = " << normal[i].x << ", " << normal[i].y << ", " << normal[i].z << "\n- - - - - - - - - - - - - - - - - - " << endl;
				}
			}
		}
	}

	// VAO VBO IBO
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, l_sizeOfVertices * 2, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, l_sizeOfVertices, vertex);
	if (l_outline == false)  { glBufferSubData(GL_ARRAY_BUFFER, l_sizeOfVertices, l_sizeOfVertices, normal); }

	glGenBuffers(1, &IBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(int)*l_numIndices, index, GL_STATIC_DRAW);

	const char* vertexShader =
		"#version 400\n"

		"in vec3 vertexPos;"
		"in vec3 normalPos;"

		"uniform vec3 ambiComp;"
		"uniform vec3 diffComp;"
		"uniform vec3 specComp;"
		"uniform vec3 constants;"

		"uniform vec4 light0pos;"
		"uniform vec3 light0color;"
		"uniform vec4 light1pos;"
		"uniform vec3 light1color;"

		"uniform mat4 Model;"
		"uniform mat4 PV;"

		"uniform vec3 propColor;"
		"out vec3 color;"

		"void main ()"
		"{"
		"    float shinComp = constants.z;"
		"    vec4 vertex    = Model * vec4(vertexPos, 1.0f);"
		"    gl_Position    = PV * vertex;"
		"    vec3 L0        = normalize( vec3(light0pos - vertex) );"
		"    vec3 L1        = normalize( vec3(light1pos) );"
		"    vec3 N         = normalize( vec3( Model * vec4(normalPos, 0.0f) ) );"

		"    vec3 diffProd0 = vec3(0.0f);"
		"    vec3 specProd0 = vec3(0.0f);"
		"    if(dot(N,L0) > 0)"
		"    {"
		"        diffProd0 = diffComp * max( dot(N, L0), 0.0f );"
		"        vec3 R0   = normalize( reflect(-L0, N) );"
		"        vec3 V    = normalize( vec3(-vertex) );"
		"        specProd0 = specComp * pow( max( dot(R0, V), 0.0f ), shinComp );"
		"    }"

		"    vec3 diffProd1 = vec3(0.0f);"
		"    vec3 specProd1 = vec3(0.0f);"
		"    if(dot(N,L1) > 0)"
		"    {"
		"        diffProd1 = diffComp * max( dot(N, L1), 0.0f );"
		"        vec3 R1   = normalize( reflect(-L1, N) );"
		"        vec3 V    = normalize( vec3(-vertex) );"
		"        specProd1 = specComp * pow( max( dot(R1, V), 0.0f ), shinComp );"
		"    }"

		"    color = clamp( (   propColor * ( ((light0color + light1color) * ambiComp) + (light0color * (diffProd0 + specProd0)) + (light1color * (diffProd1 + specProd1)) )   ), 0.0f, 1.0f );"
		"}";

	const char* vertexShader0 =
		"#version 400\n"
		"in vec3 vertexPos;"
		"uniform mat4 Model;"
		"uniform mat4 PV;"
		"uniform vec3 propColor;"
		"out vec3 color;"
		"void main ()"
		"{"
		"    gl_Position = PV * Model * vec4(vertexPos, 1.0f);"
		"	 color = propColor; "
		"}";

	const char* vertexShader1 =
		"#version 400\n"

		"in vec3 vertexPos;"
		"in vec3 normalPos;"

		"uniform vec4 light0pos;"
		"uniform vec4 light1pos;"
		"uniform mat4 Model;"
		"uniform mat4 PV;"

		"out vec3 fN;"
		"out vec3 fL0;"
		"out vec3 fL1;"
		"out vec3 fV;"

		"void main ()"
		"{"
		"    vec4 vertex = Model * vec4(vertexPos, 1.0f);"
		"    gl_Position = PV * vertex;"
		"    fL0         = vec3(light0pos - vertex);"
		"    fL1         = vec3(light1pos);"
		"    fN          = vec3( Model * vec4(normalPos, 0.0f) );"
		"    fV          = vec3(-vertex);"
		"}";

	/*mat4 l_View = lookAt(vec3(0.0f, 0.0f, 1.0f), vec3(0.0f), vec3(1.0f, 0.0f, 0.0f));
	cout << "vertex[16] = " << vertex[16].x << ", " << vertex[16].y << ", " << vertex[16].z << endl;
	vec3 ver = vec3(l_View * l_Model * vec4(vertex[16], 1.0f));
	cout << "ver        = " << ver.x << ", " << ver.y << ", " << ver.z << endl;
	cout << "normal[16] = " << normal[16].x << ", " << normal[16].y << ", " << normal[16].z << endl;
	vec3 N = normalize(vec3(l_View * l_Model * vec4(normal[16], 0.0f)));
	cout << "N          = " << N.x << ", " << N.y << ", " << N.z << endl;
	vec3 lightPos = vec3(1.0f, 1.0f, 1.0f);
	cout << "lightPos   = " << lightPos.x << ", " << lightPos.y << ", " << lightPos.z << endl;
	vec3 L = vec3(l_View * vec4(lightPos, 1.0f));
	cout << "L          = " << L.x << ", " << L.y << ", " << L.z << endl;
	L = normalize(L - ver);
	cout << "L-ver      = " << L.x << ", " << L.y << ", " << L.z << endl;
	cout << "ambiComp   = " << material.ambient.x << ", " << material.ambient.y << ", " << material.ambient.z << endl;
	vec3 ambiProd       = l_Ka * material.ambient;
	cout << "ambiProd   = " << ambiProd.x << ", " << ambiProd.y << ", " << ambiProd.z << endl;
	cout << "diffComp   = " << material.diffuse.x << ", " << material.diffuse.y << ", " << material.diffuse.z << endl;
	vec3 lightColor     = vec3(1.0f);
	vec3 diffProd       = l_Ka * material.diffuse * lightColor;
	cout << "diffProd   = " << diffProd.x << ", " << diffProd.y << ", " << diffProd.z << endl;
	float dotProd       = max(dot(N, L), 0.0f);
	cout << "dot(N, L)  = " << dotProd << endl;
	diffProd            = diffProd * dotProd;
	cout << "diffProd   = " << diffProd.x << ", " << diffProd.y << ", " << diffProd.z << endl;
	/*cout << "specComp   = " << material.specular.x << ", " << material.specular.y << ", " << material.specular.z << endl;
	vec3 specProd       = l_Ks * material.specular * lightColor;
	cout << "specProd   = " << specProd.x << ", " << specProd.y << ", " << specProd.z << endl;
	cout << "shininess  = " << material.shininess << endl;
	dotProd             = dot(-L, N);
	cout << "dot(-L, N) = " << dotProd << endl;*/
	/*vec3 finalColor = clamp(propColor * (ambiProd + diffProd), 0.0f, 1.0f);
	cout << "color      = " << finalColor.x << ", " << finalColor.y << ", " << finalColor.z << endl;*/

	const char* fragmentShader =
		"#version 400\n"
		"in vec3 color;"
		"out vec4 frag_color;"
		"void main ()"
		"{"
		"    frag_color = vec4(color, 1.0);"
		"}";

	const char* fragmentShader1 =
		"#version 400\n"

		"in vec3 fN;"
		"in vec3 fL0;"
		"in vec3 fL1;"
		"in vec3 fV;"

		"uniform vec3 ambiComp;"
		"uniform vec3 diffComp;"
		"uniform vec3 specComp;"
		"uniform vec3 constants;"
		"uniform vec3 light0color;"
		"uniform vec3 light1color;"
		"uniform vec3 propColor;"

		"out vec4 frag_color;"

		"void main ()"
		"{"
		"    float shinComp = constants.z;"
		"    vec3 N         = normalize(fN);"
		"    vec3 L0        = normalize(fL0);"
		"    vec3 L1        = normalize(fL1);"

		"    vec3 diffProd0 = vec3(0.0f);"
		"    vec3 specProd0 = vec3(0.0f);"
		"    if(dot(N,L0) > 0)"
		"    {"
		"        vec3 V    = normalize(fV);"
		"        vec3 R0   = normalize( reflect(-L0, N) );"
		"        diffProd0 = diffComp * max( dot(N, L0), 0.0f );"
		"        specProd0 = specComp * pow( max( dot(R0, V), 0.0f ), shinComp );"
		"    }"

		"    vec3 diffProd1 = vec3(0.0f);"
		"    vec3 specProd1 = vec3(0.0f);"
		"    if(dot(N,L1) > 0)"
		"    {"
		"        vec3 V    = normalize(fV);"
		"        vec3 R1   = normalize( reflect(-L1, N) );"
		"        diffProd1 = diffComp * max( dot(N, L1), 0.0f );"
		"        specProd1 = specComp * pow( max( dot(R1, V), 0.0f ), shinComp );"
		"    }"

		"    frag_color = vec4(clamp( (   propColor * ( ((light0color + light1color) * ambiComp) + (light0color * (diffProd0 + specProd0)) + (light1color * (diffProd1 + specProd1)) )   ), 0.0f, 1.0f ), 1.0f);"
		"}";

	if (l_outline == true)
	{
		shaderProg[0] = initShaders(vertexShader0, fragmentShader);
		j = 1;
	}
	else
	{
		shaderProg[0] = initShaders(vertexShader,  fragmentShader);
		shaderProg[1] = initShaders(vertexShader1, fragmentShader1);
		j = 2;
	}

	for (int i = 0; i < j; i++)
	{
		glUseProgram(shaderProg[i]);

		vertexPos[i] = glGetAttribLocation(shaderProg[i], "vertexPos");
		if (vertexPos[i] < 0) cerr << "couldn't find vertexPos in shader\n";
		glEnableVertexAttribArray(vertexPos[i]);
		glVertexAttribPointer(vertexPos[i], 3, GL_FLOAT, GL_FALSE, 0, (void *)0);

		MUniformLoc[i] = glGetUniformLocation(shaderProg[i], "Model");
		if (MUniformLoc[i] < 0) cerr << "couldn't find Model in shader\n";

		PVUniformLoc[i] = glGetUniformLocation(shaderProg[i], "PV");
		if (PVUniformLoc[i] < 0) cerr << "couldn't find PV in shader\n";

		propColorLoc[i] = glGetUniformLocation(shaderProg[i], "propColor");
		if (propColorLoc[i] < 0) cerr << "couldn't find propColor in shader\n";

		if (l_outline == false)
		{
			normalPos[i] = glGetAttribLocation(shaderProg[i], "normalPos");
			if (normalPos[i] < 0) cerr << "couldn't find normalPos in shader\n";
			glEnableVertexAttribArray(normalPos[i]);
			glVertexAttribPointer(normalPos[i], 3, GL_FLOAT, GL_FALSE, 0, (void *)l_sizeOfVertices);

			ambiCompLoc[i] = glGetUniformLocation(shaderProg[i], "ambiComp");
			if (ambiCompLoc[i] < 0) cerr << "couldn't find ambiComp in shader\n";

			diffCompLoc[i] = glGetUniformLocation(shaderProg[i], "diffComp");
			if (diffCompLoc[i] < 0) cerr << "couldn't find diffComp in shader\n";

			specCompLoc[i] = glGetUniformLocation(shaderProg[i], "specComp");
			if (specCompLoc[i] < 0) cerr << "couldn't find specComp in shader\n";

			ConstsLoc[i] = glGetUniformLocation(shaderProg[i], "constants");
			if (ConstsLoc[i] < 0) cerr << "couldn't find constants in shader\n";

			L0PosLoc[i] = glGetUniformLocation(shaderProg[i], "light0pos");
			if (L0PosLoc[i] < 0) cerr << "couldn't find light0pos in shader\n";

			L0ColorLoc[i] = glGetUniformLocation(shaderProg[i], "light0color");
			if (L0ColorLoc[i] < 0) cerr << "couldn't find light0color in shader\n";

			L1PosLoc[i] = glGetUniformLocation(shaderProg[i], "light1pos");
			if (L1PosLoc[i] < 0) cerr << "couldn't find light1pos in shader\n";

			L1ColorLoc[i] = glGetUniformLocation(shaderProg[i], "light1color");
			if (L1ColorLoc[i] < 0) cerr << "couldn't find light1color in shader\n";
		}
	}
}

void prop::render()
{
	int i;
	glBindVertexArray(VAO);
	if (outline == true || phong == false)
		i = 0;
	else
		i = 1;
	glUseProgram(shaderProg[i]);

	glUniformMatrix4fv(MUniformLoc[i], 1, GL_FALSE, value_ptr(Model));
	glUniformMatrix4fv(PVUniformLoc[i], 1, GL_FALSE, value_ptr(PV));
	glUniform3fv(propColorLoc[i], 1, value_ptr(propColor));

	if (outline == true)
		glDrawElements(GL_LINE_LOOP, numIndices, GL_UNSIGNED_INT, 0);
	else
	{
		glUniform3fv(ambiCompLoc[i], 1, value_ptr(material.ambient));
		glUniform3fv(diffCompLoc[i], 1, value_ptr(material.diffuse));
		glUniform3fv(specCompLoc[i], 1, value_ptr(material.specular));
		glUniform4fv(L0PosLoc[i], 1, value_ptr(light[0].pos));
		glUniform3fv(L0ColorLoc[i], 1, value_ptr(light[0].intensity * light[0].color));
		glUniform4fv(L1PosLoc[i], 1, value_ptr(light[1].pos));
		glUniform3fv(L1ColorLoc[i], 1, value_ptr(light[1].intensity * light[1].color));
		glUniform3fv(ConstsLoc[i], 1, value_ptr(vec3(0.0f, 0.0f, material.shininess)));

		glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, 0);
	}
}

prop island, ground, cube, ecdcA, ecdcB, bayhall;

void initialize()
{
	//-----------------------------MATERIALS-------------------------------

	// COPPER
	copper.ambient   = vec3(0.191250f, 0.073500f, 0.022500f);
	copper.diffuse   = vec3(0.703800f, 0.270480f, 0.082800f);
	copper.specular  = vec3(0.256777f, 0.137622f, 0.086014f);
	copper.shininess = 12.8;

	// SILVER
	silver.ambient   = vec3(0.192250f, 0.192250f, 0.192250f);
	silver.diffuse   = vec3(0.507540f, 0.507540f, 0.507540f);
	silver.specular  = vec3(0.508273f, 0.508273f, 0.508273f);
	silver.shininess = 51.2;

	// GOLD
	gold.ambient   = vec3(0.247250f, 0.199500f, 0.074500f);
	gold.diffuse   = vec3(0.751640f, 0.606480f, 0.226480f);
	gold.specular  = vec3(0.628281f, 0.555802f, 0.366065f);
	gold.shininess = 51.2;
	
	//------------------------------ISLAND---------------------------------
	vec3 islandVertex[] =
	{
		vec3(1.03208f, 0.86614f, 0.0f), /*1811376580*/		vec3(1.39635f, 1.87327f, 0.0f), /*242358198	*/
		vec3(1.40699f, 1.89816f, 0.0f), /*242358207 */	    vec3(1.42827f, 1.94451f, 0.0f), /*242358204	*/
		vec3(1.50805f, 1.93163f, 0.0f), /*242358189 */	    vec3(1.50945f, 1.94805f, 0.0f), /*1837961149*/
		vec3(1.47155f, 1.95470f, 0.0f), /*948505916 */	    vec3(1.47585f, 1.97181f, 0.0f), /*948505923	*/
		vec3(1.48250f, 1.99112f, 0.0f), /*948505927 */	    vec3(1.49959f, 2.01151f, 0.0f), /*948505931	*/
		vec3(1.49200f, 2.04477f, 0.0f), /*948505934 */	    vec3(1.49959f, 2.07159f, 0.0f), /*948505937	*/
		vec3(1.52429f, 2.11879f, 0.0f), /*948505941 */	    vec3(1.53474f, 2.15098f, 0.0f), /*948505946	*/
		vec3(1.54993f, 2.19282f, 0.0f), /*948505952 */	    vec3(1.57463f, 2.21857f, 0.0f), /*948505960	*/
		vec3(1.60787f, 2.22286f, 0.0f), /*948505968 */	    vec3(1.60312f, 2.24969f, 0.0f), /*948505977	*/
		vec3(1.61737f, 2.28080f, 0.0f), /*948505998 */	    vec3(1.62912f, 2.25918f, 0.0f), /*1837961148*/
		vec3(1.63570f, 2.27410f, 0.0f), /*242358176 */	    vec3(1.58099f, 2.32817f, 0.0f), /*242358173	*/
		vec3(1.73904f, 2.76162f, 0.0f), /*493789690 */	    vec3(1.85439f, 3.12160f, 0.0f), /*242358182	*/
		vec3(1.82015f, 3.12898f, 0.0f), /*1837826363*/		vec3(1.81125f, 3.11369f, 0.0f), /*1837912651*/
		vec3(1.74289f, 3.06602f, 0.0f), /*1837826372*/		vec3(1.58731f, 3.07455f, 0.0f), /*1837826366*/
		vec3(1.52083f, 3.05846f, 0.0f), /*1837826371*/		vec3(1.38387f, 3.05851f, 0.0f), /*1837826367*/
		vec3(1.31264f, 3.08636f, 0.0f), /*1837826365*/		vec3(1.23457f, 3.06234f, 0.0f), /*1811376935*/
		vec3(1.17475f, 2.99287f, 0.0f), /*1811376889*/		vec3(1.13590f, 2.81956f, 0.0f), /*1811376874*/
		vec3(1.13203f, 2.81353f, 0.0f), /*1811376870*/		vec3(1.12600f, 2.80412f, 0.0f), /*1811376867*/
		vec3(1.07980f, 2.77630f, 0.0f), /*1811376694*/		vec3(1.01499f, 2.76485f, 0.0f), /*1811376552*/
		vec3(0.93654f, 2.71044f, 0.0f), /*1811376414*/		vec3(0.85766f, 2.59042f, 0.0f), /*1811376403*/
		vec3(0.85769f, 2.55099f, 0.0f), /*1837912634*/		vec3(0.83339f, 2.50518f, 0.0f), /*1837912676*/
		vec3(0.79312f, 2.47736f, 0.0f), /*1837912660*/		vec3(0.82043f, 2.56613f, 0.0f), /*1837912678*/
		vec3(0.79637f, 2.57966f, 0.0f), /*1837912641*/		vec3(0.84638f, 2.65047f, 0.0f), /*1837912646*/
		vec3(0.80675f, 2.65496f, 0.0f), /*1811376326*/		vec3(0.75282f, 2.61910f, 0.0f), /*1811376286*/
		vec3(0.74458f, 2.60303f, 0.0f), /*1811376280*/		vec3(0.72917f, 2.57274f, 0.0f), /*1811376268*/
		vec3(0.72973f, 2.56101f, 0.0f), /*1811376270*/		vec3(0.73193f, 2.54802f, 0.0f), /*1811376274*/
		vec3(0.73248f, 2.52267f, 0.0f), /*1811376276*/		vec3(0.72807f, 2.49980f, 0.0f), /*1811376266*/
		vec3(0.72258f, 2.47879f, 0.0f), /*1811376264*/		vec3(0.72203f, 2.46024f, 0.0f), /*1811376261*/
		vec3(0.72092f, 2.43922f, 0.0f), /*1811376256*/		vec3(0.71817f, 2.43427f, 0.0f), /*1811376219*/
		vec3(0.71818f, 2.43181f, 0.0f), /*1811376221*/		vec3(0.72038f, 2.42624f, 0.0f), /*1811376254*/
		vec3(0.71928f, 2.41882f, 0.0f), /*1811376229*/		vec3(0.72094f, 2.40275f, 0.0f), /*1811376259*/
		vec3(0.72038f, 2.39657f, 0.0f), /*1811376252*/		vec3(0.71872f, 2.39100f, 0.0f), /*1811376226*/
		vec3(0.71323f, 2.38112f, 0.0f), /*1811376215*/		vec3(0.71212f, 2.37802f, 0.0f), /*1811376207*/
		vec3(0.71213f, 2.37370f, 0.0f), /*1811376211*/		vec3(0.71983f, 2.36567f, 0.0f), /*1811376250*/
		vec3(0.73084f, 2.34155f, 0.0f), /*1811376272*/		vec3(0.74569f, 2.31622f, 0.0f), /*1811376283*/
		vec3(0.75944f, 2.28098f, 0.0f), /*1811376292*/		vec3(0.78310f, 2.23895f, 0.0f), /*1811376300*/
		vec3(0.83702f, 2.16292f, 0.0f), /*1811376332*/		vec3(0.85847f, 2.13016f, 0.0f), /*1811376343*/
		vec3(0.86014f, 2.12645f, 0.0f), /*1811376347*/		vec3(0.86014f, 2.11841f, 0.0f), /*1811376345*/
		vec3(0.85737f, 2.11224f, 0.0f), /*1811376339*/		vec3(0.85682f, 2.10421f, 0.0f), /*1811376337*/
		vec3(0.85847f, 2.09741f, 0.0f), /*1811376341*/		vec3(0.86179f, 2.08874f, 0.0f), /*1811376349*/
		vec3(0.87608f, 2.06217f, 0.0f), /*1811376354*/		vec3(0.88323f, 2.04177f, 0.0f), /*1811376356*/
		vec3(0.88929f, 2.03002f, 0.0f), /*1811376362*/		vec3(0.91404f, 1.99974f, 0.0f), /*1811376406*/
		vec3(0.92009f, 1.99479f, 0.0f), /*1811376411*/		vec3(0.93825f, 1.98923f, 0.0f), /*1811376417*/
		vec3(0.95420f, 1.98861f, 0.0f), /*1811376423*/		vec3(0.96686f, 1.99170f, 0.0f), /*1811376429*/
		vec3(0.97016f, 1.99046f, 0.0f), /*1811376431*/		vec3(0.98061f, 1.97934f, 0.0f), /*1811376453*/
		vec3(0.99436f, 1.96760f, 0.0f), /*1811376527*/		vec3(1.00041f, 1.96017f, 0.0f), /*1811376530*/
		vec3(1.01472f, 1.93916f, 0.0f), /*1811376550*/		vec3(1.02847f, 1.91505f, 0.0f), /*1811376572*/
		vec3(1.04498f, 1.90269f, 0.0f), /*1811376615*/		vec3(1.06808f, 1.87240f, 0.0f), /*1811376653*/
		vec3(1.08899f, 1.83964f, 0.0f), /*1811376735*/		vec3(1.11760f, 1.78092f, 0.0f), /*1811376846*/
		vec3(1.14291f, 1.71787f, 0.0f), /*1811376878*/		vec3(1.16161f, 1.67768f, 0.0f), /*1811376886*/
		vec3(1.17756f, 1.64678f, 0.0f), /*1811376893*/		vec3(1.19131f, 1.61587f, 0.0f), /*1811376906*/
		vec3(1.19077f, 1.61030f, 0.0f), /*1811376903*/		vec3(1.18802f, 1.60907f, 0.0f), /*1811376899*/
		vec3(1.17590f, 1.59547f, 0.0f), /*1811376891*/		vec3(1.14071f, 1.57940f, 0.0f), /*1811376877*/
		vec3(1.13520f, 1.57198f, 0.0f), /*1811376873*/		vec3(1.14070f, 1.56147f, 0.0f), /*1811376875*/
		vec3(1.14841f, 1.55529f, 0.0f), /*1811376881*/		vec3(1.16161f, 1.54787f, 0.0f), /*1811376885*/
		vec3(1.17371f, 1.54170f, 0.0f), /*1811376888*/		vec3(1.19076f, 1.53365f, 0.0f), /*1811376902*/
		vec3(1.20012f, 1.53180f, 0.0f), /*1811376911*/		vec3(1.20177f, 1.52438f, 0.0f), /*1811376916*/
		vec3(1.19627f, 1.52315f, 0.0f), /*1811376909*/		vec3(1.17866f, 1.52500f, 0.0f), /*1811376894*/
		vec3(1.15776f, 1.53242f, 0.0f), /*1811376883*/		vec3(1.13355f, 1.54231f, 0.0f), /*1811376871*/
		vec3(1.12035f, 1.55282f, 0.0f), /*1811376856*/		vec3(1.10824f, 1.55282f, 0.0f), /*1811376816*/
		vec3(1.08404f, 1.55962f, 0.0f), /*1811376708*/		vec3(1.07469f, 1.57137f, 0.0f), /*1811376681*/
		vec3(1.07359f, 1.58806f, 0.0f), /*1811376678*/		vec3(1.08349f, 1.61340f, 0.0f), /*1811376701*/
		vec3(1.07964f, 1.61650f, 0.0f), /*1811376691*/		vec3(1.05543f, 1.61092f, 0.0f), /*1811376627*/
		vec3(1.04333f, 1.59609f, 0.0f), /*1811376611*/		vec3(1.03728f, 1.56765f, 0.0f), /*1811376594*/
		vec3(1.03727f, 1.53922f, 0.0f), /*1811376591*/		vec3(1.04223f, 1.50274f, 0.0f), /*1811376598*/
		vec3(1.04278f, 1.47988f, 0.0f), /*1811376601*/		vec3(1.05213f, 1.44217f, 0.0f), /*1811376621*/
		vec3(1.05708f, 1.42734f, 0.0f), /*1811376634*/		vec3(1.05818f, 1.39890f, 0.0f), /*1811376638*/
		vec3(1.06478f, 1.37727f, 0.0f), /*1811376645*/		vec3(1.06588f, 1.35810f, 0.0f), /*1811376648*/
		vec3(1.06918f, 1.33400f, 0.0f), /*1811376666*/		vec3(1.07028f, 1.30803f, 0.0f), /*1811376673*/
		vec3(1.07578f, 1.28825f, 0.0f), /*1811376686*/		vec3(1.08459f, 1.28084f, 0.0f), /*1811376714*/
		vec3(1.08515f, 1.29258f, 0.0f), /*1811376728*/		vec3(1.08018f, 1.32163f, 0.0f), /*1811376695*/
		vec3(1.07523f, 1.35625f, 0.0f), /*1811376684*/		vec3(1.07248f, 1.38220f, 0.0f), /*1811376676*/
		vec3(1.07154f, 1.38510f, 0.0f), /*1811376674*/		vec3(1.06865f, 1.39395f, 0.0f), /*1811376659*/
		vec3(1.06836f, 1.40368f, 0.0f), /*1811376656*/		vec3(1.06808f, 1.41313f, 0.0f), /*1811376651*/
		vec3(1.08074f, 1.42116f, 0.0f), /*1811376698*/		vec3(1.08404f, 1.42116f, 0.0f), /*1811376705*/
		vec3(1.08679f, 1.41930f, 0.0f), /*1811376730*/		vec3(1.11595f, 1.37541f, 0.0f), /*1811376842*/
		vec3(1.12034f, 1.36738f, 0.0f), /*1811376855*/		vec3(1.12199f, 1.36058f, 0.0f), /*1811376862*/
		vec3(1.12199f, 1.35254f, 0.0f), /*1811376861*/		vec3(1.12034f, 1.34450f, 0.0f), /*1811376854*/
		vec3(1.11814f, 1.33709f, 0.0f), /*1811376848*/		vec3(1.12364f, 1.31422f, 0.0f), /*1811376864*/
		vec3(1.12310f, 1.30000f, 0.0f), /*1811376863*/		vec3(1.12089f, 1.29381f, 0.0f), /*1811376860*/
		vec3(1.11429f, 1.28701f, 0.0f), /*1811376838*/		vec3(1.10714f, 1.28145f, 0.0f), /*1811376795*/
		vec3(1.10329f, 1.28145f, 0.0f), /*1811376792*/		vec3(1.10109f, 1.28393f, 0.0f), /*1811376787*/
		vec3(1.09724f, 1.29753f, 0.0f), /*1811376765*/		vec3(1.09613f, 1.29814f, 0.0f), /*1811376762*/
		vec3(1.09338f, 1.29691f, 0.0f), /*1811376749*/		vec3(1.09173f, 1.27712f, 0.0f), /*1811376744*/
		vec3(1.09119f, 1.22706f, 0.0f), /*1811376740*/		vec3(1.08458f, 1.19305f, 0.0f), /*1811376711*/
		vec3(1.07742f, 1.17823f, 0.0f), /*1811376688*/		vec3(1.06918f, 1.16586f, 0.0f), /*1811376663*/
		vec3(1.05707f, 1.13929f, 0.0f), /*1811376630*/		vec3(1.05102f, 1.11888f, 0.0f), /*1811376619*/
		vec3(1.04497f, 1.10219f, 0.0f), /*1811376613*/		vec3(1.03616f, 1.06757f, 0.0f), /*1811376584*/
		vec3(1.01691f, 1.00700f, 0.0f), /*1811376556*/		vec3(1.00865f, 0.99216f, 0.0f), /*1811376544*/
		vec3(0.98444f, 0.95323f, 0.0f), /*1811376503*/		vec3(0.98334f, 0.94767f, 0.0f), /*1811376499*/
		vec3(0.98554f, 0.92355f, 0.0f), /*1811376511*/		vec3(0.98499f, 0.91737f, 0.0f), /*1811376508*/
		vec3(0.98169f, 0.90996f, 0.0f), /*1811376495*/		vec3(0.97619f, 0.90439f, 0.0f), /*1811376435*/
		vec3(0.97398f, 0.89512f, 0.0f), /*1811376433*/		vec3(0.97784f, 0.88338f, 0.0f), /*1811376437*/
		vec3(0.98169f, 0.87781f, 0.0f), /*1811376491*/		vec3(0.98884f, 0.86977f, 0.0f), /*1811376517*/
		vec3(1.00094f, 0.86297f, 0.0f), /*1811376538*/		vec3(1.02021f, 0.85864f, 0.0f), /*1811376560*/
		vec3(1.02459f, 0.85926f, 0.0f), /*1811376568*/		vec3(1.03208f, 0.86614f, 0.0f)  /*181137658	*/
	};

	const int islandNumVertices = sizeof(islandVertex) / sizeof(vec3);
	for (int i = 0; i < islandNumVertices; i++) { island.vertex[i] = islandVertex[i] + vec3(-1.25f, -2.4f, 0.0f); }
	for (int i = 0; i < islandNumVertices; i++) { island.index[i]  = i; }

	island.init(islandNumVertices, islandNumVertices, vec3(1.0, 1.0, 1.0), vec3(0.0f), mat4(1.0f), copper, true);

	//------------------------------GROUND---------------------------------
	ground.vertex[0] = vec3(0.5f,  0.5f, 0.0f);      ground.vertex[2] = vec3(-0.5f, -0.5f, 0.0f);
	ground.vertex[1] = vec3(-0.5f, 0.5f, 0.0f);		 ground.vertex[3] = vec3(0.5f, -0.5f, 0.0f);

	int groundIndex[] = { 0, 1, 2,      0, 2, 3 };

	const int groundNumIndices = sizeof(groundIndex) / sizeof(int);
	for (int i = 0; i < groundNumIndices; i++) { ground.index[i] = groundIndex[i]; }

	ground.init(4, groundNumIndices, vec3(0.1f, 0.1f, 0.1f), vec3(0.0f), mat4(1.0f), silver, false);

	//-------------------------------CUBE----------------------------------
	vec3 cubeVertex[] = 
	{
		vec3( 0.025f,  0.025f, 0.0f),		vec3(-0.025f,  0.025f, 0.0f),
		vec3(-0.025f, -0.025f, 0.0f),		vec3( 0.025f, -0.025f, 0.0f),
		vec3( 0.025f,  0.025f, 0.05f),		vec3(-0.025f,  0.025f, 0.05f),
		vec3(-0.025f, -0.025f, 0.05f),		vec3( 0.025f, -0.025f, 0.05f)
	};

	int cubeVIndex[] =
	{
		0, 1, 4, 5,      1, 2, 5, 6,
		2, 3, 6, 7,      3, 0, 7, 4,
		4, 5, 6, 7
	};

	int cubeIndex[] =
	{
		 0,  1,  2,       3,  2,  1,       4,  5,  6,       7,  6,  5,
		 8,  9, 10,      11, 10,  9,      12, 13, 14,      15, 14, 13,
		16, 17, 18,      16, 18, 19
	};

	const int cubeNumVertices = sizeof(cubeVIndex) / sizeof(int);
	for (int i = 0; i < cubeNumVertices; i++) { cube.vertex[i] = cubeVertex[cubeVIndex[i]]; }

	const int cubeNumIndices  = sizeof(cubeIndex)  / sizeof(int);
	for (int i = 0; i < cubeNumIndices;  i++) { cube.index[i]  = cubeIndex[i]; }
	
	cube.init(cubeNumVertices, cubeNumIndices, vec3(1.0f, 0.2f, 0.2f), vec3(0.0f), mat4(1.0f), copper, false);

	//------------------------------ECDC-A---------------------------------
	vec3 ecdcAVertex[] =
	{
		vec3(0.941f, 0.233f, 0.0f),		vec3(0.922f, 0.337f, 0.0f),
		vec3(0.902f, 0.462f, 0.0f),		vec3(1.003f, 0.468f, 0.0f),
		vec3(1.003f, 0.562f, 0.0f),		vec3(1.019f, 0.661f, 0.0f),
		vec3(1.052f, 0.748f, 0.0f),		vec3(1.096f, 0.828f, 0.0f),
		vec3(1.147f, 0.898f, 0.0f),		vec3(1.182f, 0.933f, 0.0f),
		vec3(1.159f, 0.964f, 0.0f),		vec3(1.179f, 0.981f, 0.0f),
		vec3(1.208f, 0.962f, 0.0f),		vec3(1.208f, 0.946f, 0.0f),
		vec3(1.218f, 0.932f, 0.0f),		vec3(1.231f, 0.931f, 0.0f),
		vec3(1.244f, 0.940f, 0.0f),		vec3(1.244f, 0.954f, 0.0f),
		vec3(1.237f, 0.965f, 0.0f),		vec3(1.222f, 0.970f, 0.0f),
		vec3(1.212f, 1.003f, 0.0f),		vec3(1.381f, 1.112f, 0.0f),
		vec3(1.320f, 1.208f, 0.0f),		vec3(1.407f, 1.262f, 0.0f),
		vec3(1.387f, 1.292f, 0.0f),		vec3(1.301f, 1.238f, 0.0f),
		vec3(1.240f, 1.338f, 0.0f),		vec3(1.083f, 1.240f, 0.0f),
		vec3(1.146f, 1.143f, 0.0f),		vec3(1.062f, 1.081f, 0.0f),
		vec3(1.018f, 1.128f, 0.0f),		vec3(0.979f, 1.087f, 0.0f),
		vec3(0.901f, 1.004f, 0.0f),		vec3(0.822f, 0.886f, 0.0f),
		vec3(0.766f, 0.753f, 0.0f),		vec3(0.732f, 0.595f, 0.0f),
		vec3(0.728f, 0.448f, 0.0f),		vec3(0.751f, 0.289f, 0.0f),
		vec3(0.882f, 0.326f, 0.0f),		vec3(0.909f, 0.226f, 0.0f),
		vec3(0.941f, 0.233f, 0.216f),	vec3(0.922f, 0.337f, 0.216f),
		vec3(0.902f, 0.462f, 0.216f),	vec3(1.003f, 0.468f, 0.216f),
		vec3(1.003f, 0.562f, 0.216f),	vec3(1.019f, 0.661f, 0.216f),
		vec3(1.052f, 0.748f, 0.216f),	vec3(1.096f, 0.828f, 0.216f),
		vec3(1.147f, 0.898f, 0.216f),	vec3(1.182f, 0.933f, 0.216f),
		vec3(1.159f, 0.964f, 0.216f),	vec3(1.179f, 0.981f, 0.216f),
		vec3(1.208f, 0.962f, 0.216f),	vec3(1.208f, 0.946f, 0.216f),
		vec3(1.218f, 0.932f, 0.216f),	vec3(1.231f, 0.931f, 0.216f),
		vec3(1.244f, 0.940f, 0.216f),	vec3(1.244f, 0.954f, 0.216f),
		vec3(1.237f, 0.965f, 0.216f),	vec3(1.222f, 0.970f, 0.216f),
		vec3(1.212f, 1.003f, 0.216f),	vec3(1.381f, 1.112f, 0.216f),
		vec3(1.320f, 1.208f, 0.216f),	vec3(1.407f, 1.262f, 0.216f),
		vec3(1.387f, 1.292f, 0.216f),	vec3(1.301f, 1.238f, 0.216f),
		vec3(1.240f, 1.338f, 0.216f),	vec3(1.083f, 1.240f, 0.216f),
		vec3(1.146f, 1.143f, 0.216f),	vec3(1.062f, 1.081f, 0.216f),
		vec3(1.018f, 1.128f, 0.216f),	vec3(0.979f, 1.087f, 0.216f),
		vec3(0.901f, 1.004f, 0.216f),	vec3(0.822f, 0.886f, 0.216f),
		vec3(0.766f, 0.753f, 0.216f),	vec3(0.732f, 0.595f, 0.216f),
		vec3(0.728f, 0.448f, 0.216f),	vec3(0.751f, 0.289f, 0.216f),
		vec3(0.882f, 0.326f, 0.216f),	vec3(0.909f, 0.226f, 0.216f),
	};

	int ecdcAVIndex[] =
	{
		 0,  1,  2, 40, 41, 42, // 0 - 2,3 - 5
		 2,  3, 42, 43, // 6 - 9
		 3,  4,  5,  6,  7,  8,  9, 43, 44, 45, 46, 47, 48, 49, // 10 - 16,17 - 23
		 9, 10, 49, 50, // 24 - 27
		10, 11, 50, 51, // 28 - 31
		11, 12, 51, 52, // 32 - 35
		12, 13, 14, 15, 16, 17, 18, 19, 52, 53, 54, 55, 56, 57, 58, 59, // 36 - 43,44 - 51
		19, 20, 59, 60, // 52 - 55
		20, 21, 60, 61, // 56 - 59
		21, 22, 61, 62, // 60 - 63
		22, 23, 62, 63, // 64 - 67
		23, 24, 63, 64, // 68 - 71
		24, 25, 64, 65, // 72 - 75
		25, 26, 65, 66, // 76 - 79
		26, 27, 66, 67, // 80 - 81
		27, 28, 67, 68, // 84 - 87
		28, 29, 68, 69, // 88 - 91
		29, 30, 69, 70, // 92 - 95
		30, 31, 32, 33, 34, 35, 36, 37, 70, 71, 72, 73, 74, 75, 76, 77, // 96 - 103,104 - 111
		37, 38, 77, 78, // 112 - 115
		38, 39, 78, 79, // 116 - 119
		39,  0, 79, 40, // 120 - 123

		40, 41, 42, 43, 44, 45, 46, 47, 48, 49, // 124 - 133
		50, 51, 52, 53, 54, 55, 56, 57, 58, 59, // 134 - 143
		60, 61, 62, 63, 64, 65, 66, 67, 68, 69, // 144 - 153
		70, 71, 72, 73, 74, 75, 76, 77, 78, 79  // 154 - 163
	};

	int ecdcAIndex[] =
	{
		  0,   1,   3,        1,   2,   4,
		  4,   3,   1,        5,   4,   2, // 0 - 2,3 - 5
		  6,   7,   8,        9,   8,   7, // 6 - 9
		 10,  11,  17,       11,  12,  18,
		 12,  13,  19,       13,  14,  20,
		 14,  15,  21,       15,  16,  22,
		 18,  17,  11,       19,  18,  12,
		 20,  19,  13,       21,  20,  14,
		 22,  21,  15,       23,  22,  16, // 10 - 16,17 - 23
		 24,  25,  26,       27,  26,  25, // 24 - 27
		 28,  29,  30,       31,  30,  29, // 28 - 31
		 32,  33,  34,       35,  34,  33, // 32 - 35
		 36,  37,  44,       37,  38,  45,
		 38,  39,  46,       39,  40,  47,
		 40,  41,  48,       41,  42,  49,
		 42,  43,  50,       45,  44,  37,
		 46,  45,  38,       47,  46,  39,
		 48,  47,  40,       49,  48,  41,
		 50,  49,  42,       51,  50,  43, // 36 - 43,44 - 51
		 52,  53,  54,       55,  54,  53, // 52 - 55
		 56,  57,  58,       59,  58,  57, // 56 - 59
		 60,  61,  62,       63,  62,  61, // 60 - 63
		 64,  65,  66,       67,  66,  65, // 64 - 67
		 68,  69,  70,       71,  70,  69, // 68 - 71
		 72,  73,  74,       75,  74,  73, // 72 - 75
		 76,  77,  78,       79,  78,  77, // 76 - 79
		 80,  81,  82,       83,  82,  81, // 80 - 83
		 84,  85,  86,       87,  86,  85, // 84 - 87
		 88,  89,  90,       91,  90,  89, // 88 - 91
		 92,  93,  94,       95,  94,  93, // 92 - 95
		 96,  97, 104,       97,  98, 105,
		 98,  99, 106,       99, 100, 107,
		100, 101, 108,      101, 102, 109,
		102, 103, 110,      105, 104,  97,
		106, 105,  98,      107, 106,  99,
		108, 107, 100,      109, 108, 101,
		110, 109, 102,      111, 110, 103, //  96 - 103,104 - 111
		112, 113, 114,      115, 114, 113, // 112 - 115
		116, 117, 118,      119, 118, 117, // 116 - 119
		120, 121, 122,      123, 122, 121, // 120 - 123

		124, 125, 162,      162, 163, 124,
		125, 126, 162,      126, 160, 162,      160, 161, 162,
		126, 127, 128,      126, 128, 159,      159, 160, 126,
		128, 129, 158,      158, 159, 128,
		129, 130, 157,      157, 158, 129,
		130, 131, 156,      156, 157, 130,
		131, 132, 155,      155, 156, 131,
		132, 133, 134,      132, 134, 153,
		132, 153, 155,      153, 154, 155,
		134, 135, 153,      135, 152, 153,      135, 144, 152,
		135, 136, 144,      136, 143, 144,
		136, 137, 143,      142, 143, 137,
		137, 138, 142,      141, 142, 138,
		138, 139, 141,      140, 141, 139,
		144, 145, 152,      145, 146, 152,      146, 149, 152,
		146, 147, 148,      148, 149, 146,
		149, 150, 151,      149, 151, 152
	};

	const int ecdcANumVertices = sizeof(ecdcAVIndex) / sizeof(int);
	for (int i = 0; i < ecdcANumVertices; i++) { ecdcA.vertex[i] = (ecdcAVertex[ecdcAVIndex[i]]/9.25f) + vec3(0.175f, 0.06f, 0.0f); }

	const int ecdcANumIndices = sizeof(ecdcAIndex) / sizeof(int);
	for (int i = 0; i < ecdcANumIndices; i++) { ecdcA.index[i] = ecdcAIndex[i]; }

	ecdcA.init(ecdcANumVertices, ecdcANumIndices, vec3(0.1, 0.1, 0.5), ((vec3(1.083f, 0.862f, 0.0f)/9.25f)+ vec3(0.175f, 0.06f, 0.0f)), mat4(1.0f), copper, false);

	//------------------------------ECDC-B---------------------------------
	vec3 ecdcBVertex[] =
	{
		vec3(0.585f, 0.551f, 0.0f),			vec3(0.624f, 0.561f, 0.0f),
		vec3(0.660f, 0.593f, 0.0f),			vec3(0.685f, 0.639f, 0.0f),
		vec3(0.698f, 0.638f, 0.0f),			vec3(0.714f, 0.697f, 0.0f),
		vec3(0.693f, 0.701f, 0.0f),			vec3(0.698f, 0.726f, 0.0f),
		vec3(0.677f, 0.730f, 0.0f),			vec3(0.647f, 0.778f, 0.0f),
		vec3(0.596f, 0.807f, 0.0f),			vec3(0.590f, 0.794f, 0.0f),
		vec3(0.569f, 0.807f, 0.0f),			vec3(0.507f, 0.786f, 0.0f),
		vec3(0.462f, 0.751f, 0.0f),			vec3(0.440f, 0.697f, 0.0f),
		vec3(0.446f, 0.638f, 0.0f),			vec3(0.478f, 0.594f, 0.0f),
		vec3(0.528f, 0.568f, 0.0f),			vec3(0.585f, 0.570f, 0.0f),
		vec3(0.585f, 0.551f, 0.216f),		vec3(0.624f, 0.561f, 0.216f),
		vec3(0.660f, 0.593f, 0.216f),		vec3(0.685f, 0.639f, 0.216f),
		vec3(0.698f, 0.638f, 0.216f),		vec3(0.714f, 0.697f, 0.216f),
		vec3(0.693f, 0.701f, 0.216f),		vec3(0.698f, 0.726f, 0.216f),
		vec3(0.677f, 0.730f, 0.216f),		vec3(0.647f, 0.778f, 0.216f),
		vec3(0.596f, 0.807f, 0.216f),		vec3(0.590f, 0.794f, 0.216f),
		vec3(0.569f, 0.807f, 0.216f),		vec3(0.507f, 0.786f, 0.216f),
		vec3(0.462f, 0.751f, 0.216f),		vec3(0.440f, 0.697f, 0.216f),
		vec3(0.446f, 0.638f, 0.216f),		vec3(0.478f, 0.594f, 0.216f),
		vec3(0.528f, 0.568f, 0.216f),		vec3(0.585f, 0.570f, 0.216f)
	};

	int ecdcBVIndex[] = 
	{
		 0,  1,  2,  3, 20, 21, 22, 23, // 0 - 3,4 - 7
		 3,  4, 23, 24, //  8 - 11
		 4,  5, 24, 25, // 12 - 15
		 5,  6, 25, 26, // 16 - 19
		 6,  7, 26, 27, // 20 - 23
		 7,  8, 27, 28, // 24 - 27
		 8,  9, 10, 28, 29, 30, // 28 - 30,31 - 33
		10, 11, 30, 31, // 34 - 37
		11, 12, 31, 32, // 38 - 41
		12, 13, 14, 15, 16, 17, 18, 19, 32, 33, 34, 35, 36, 37, 38, 39, // 42 - 49,50 - 57
		19,  0, 39, 20, // 58 - 61

		20, 21, 22, 23, 24, 25, 26, 27, 28, 29, // 62 - 71
		30, 31, 32, 33, 34, 35, 36, 37, 38, 39  // 72 - 81
	};

	int ecdcBIndex[] = 
	{
		 0,  1,  5,       5,  4,  0,
		 1,  2,  6,       6,  5,  1,
		 2,  3,  7,       7,  6,  2, //  0 -  3,4 - 7
		 8,  9, 10,      11, 10,  9, //  8 - 11
		12, 13, 14,      15, 14, 13, // 12 - 15
		16, 17, 18,      19, 18, 17, // 16 - 19
		20, 21, 22,      23, 22, 21, // 20 - 23
		24, 25, 26,      27, 26, 25, // 24 - 27
		28, 29, 32,      32, 31, 28,
		29, 30, 33,      33, 32, 29, // 28 - 30,31 - 33
		34, 35, 36,      37, 36, 35, // 34 - 37
		38, 39, 40,      41, 40, 39, // 38 - 41
		42, 43, 51,      51, 50, 42,
		43, 44, 52,      52, 51, 43,
		44, 45, 53,      53, 52, 44,
		45, 46, 54,      54, 53, 45,
		46, 47, 55,      55, 54, 46,
		47, 48, 56,      56, 55, 47,
		48, 49, 57,      57, 56, 48, // 42 - 49,50 - 57
		58, 59, 60,      61, 60, 59, // 58 - 61

		62, 63, 81,      63, 64, 81,      64, 65, 81,
		65, 79, 81,      79, 80, 81,
		65, 77, 79,      77, 78, 79,
		65, 66, 68,      66, 67, 68,
		65, 70, 77,      65, 68, 70,      68, 69, 70,
		70, 71, 73,      71, 72, 73,
		70, 73, 75,      73, 74, 75,
		70, 75, 77,      75, 76, 77
	};

	const int ecdcBNumVertices = sizeof(ecdcBVIndex) / sizeof(int);
	for (int i = 0; i < ecdcBNumVertices; i++) { ecdcB.vertex[i] = (ecdcBVertex[ecdcBVIndex[i]] / 9.25f) + vec3(0.175f, 0.06f, 0.0f); }

	const int ecdcBNumIndices = sizeof(ecdcBIndex) / sizeof(int);
	for (int i = 0; i < ecdcBNumIndices; i++) { ecdcB.index[i] = ecdcBIndex[i]; }

	ecdcB.init(ecdcBNumVertices, ecdcBNumIndices, vec3(0.1, 0.5, 0.1), ((vec3(0.595f, 0.681f, 0.0f) / 9.25f) + vec3(0.175f, 0.06f, 0.0f)), mat4(1.0f), silver, false);

	//-----------------------------BAY-HALL--------------------------------
	vec3 bayhallVertex[] =
	{
		vec3(-1.499f, -1.166f, 0.0f),		vec3(-0.888f, -1.429f, 0.0f),
		vec3(-0.755f, -1.121f, 0.0f),		vec3(-1.356f, -0.855f, 0.0f),
		vec3(-1.499f, -1.166f, 0.153f),		vec3(-0.888f, -1.429f, 0.153f),
		vec3(-0.876f, -1.402f, 0.153f),		vec3(-1.485f, -1.137f, 0.153f),
		vec3(-1.485f, -1.137f, 0.291f),		vec3(-0.876f, -1.402f, 0.291f),
		vec3(-0.755f, -1.121f, 0.291f),		vec3(-1.356f, -0.855f, 0.291f),
		vec3(-1.391f, -0.974f, 0.291f),		vec3(-1.350f, -0.992f, 0.291f),
		vec3(-1.381f, -1.059f, 0.291f),		vec3(-1.216f, -1.132f, 0.291f),
		vec3(-1.170f, -1.031f, 0.291f),		vec3(-1.286f, -0.975f, 0.291f),
		vec3(-1.272f, -0.944f, 0.291f),		vec3(-1.360f, -0.906f, 0.291f),
		vec3(-1.391f, -0.974f, 0.347f),		vec3(-1.350f, -0.992f, 0.347f),
		vec3(-1.381f, -1.059f, 0.347f),		vec3(-1.216f, -1.132f, 0.347f),
		vec3(-1.170f, -1.031f, 0.347f),		vec3(-1.286f, -0.975f, 0.347f),
		vec3(-1.272f, -0.944f, 0.347f),		vec3(-1.360f, -0.906f, 0.347f),
		vec3(-1.103f, -1.157f, 0.291f),		vec3(-0.905f, -1.244f, 0.291f),
		vec3(-0.860f, -1.142f, 0.291f),		vec3(-1.058f, -1.055f, 0.291f),
		vec3(-1.103f, -1.157f, 0.347f),		vec3(-0.905f, -1.244f, 0.347f),
		vec3(-0.860f, -1.142f, 0.347f),		vec3(-1.058f, -1.055f, 0.347f),
		vec3(-0.985f, -1.169f, 0.347f),		vec3(-0.919f, -1.198f, 0.347f),
		vec3(-0.901f, -1.157f, 0.347f),		vec3(-0.967f, -1.128f, 0.347f),
		vec3(-0.985f, -1.169f, 0.291f),		vec3(-0.919f, -1.198f, 0.291f),
		vec3(-0.901f, -1.157f, 0.291f),		vec3(-0.967f, -1.128f, 0.291f)
	};

	int bayhallVIndex[] =
	{
		 0,  1,  4,  5, //  0 -  3
		 4,  5,  7,  6, //  4 -  7
		 7,  6,  8,  9, //  8 - 11
		 2,  3, 10, 11, // 12 - 15
		 1,  2, 10,  9, 6,  5, // 16 - 21
		 3,  0,  4,  7, 8, 11, // 22 - 27
		 8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 28, 29, 30, 31, // 28 - 43
		12, 13, 20, 21, // 44 - 47
		13, 14, 21, 22, // 48 - 51
		14, 15, 22, 23, // 52 - 55
		15, 16, 23, 24, // 56 - 59
		16, 17, 24, 25, // 60 - 63
		17, 18, 25, 26, // 64 - 67
		18, 19, 26, 27, // 68 - 71
		19, 12, 27, 20, // 72 - 75
		28, 29, 32, 33, // 76 - 79
		29, 30, 33, 34, // 80 - 83
		30, 31, 34, 35, // 84 - 87
		31, 28, 35, 32, // 88 - 91
		20, 21, 22, 23, 24, 25, 26, 27, //  92 -  99
		32, 33, 34, 35, 36, 37, 38, 39, // 100 - 107
		41, 40, 37, 36, // 108 - 111
		42, 41, 38, 37, // 112 - 115
		43, 42, 39, 38, // 116 - 119
		40, 43, 36, 39, // 120 - 123
		40, 41, 43, 42  // 124 - 127
	};

	int bayhallIndex[] =
	{
		  0,   1,   2,     3,   2,   1, //   0 -   3
		  4,   5,   6,     7,   6,   5, //   4 -   7
		  8,   9,  10,    11,  10,   9, //   8 -  11
		 12,  13,  14,    15,  14,  13, //  12 -  15
		 16,  17,  20,    16,  20,  21,
		 17,  18,  20,    18,  19,  20, //  16 -  21
		 22,  23,  25,    23,  24,  25,
		 22,  25,  27,    25,  26,  27, //  22 -  27
		 28,  29,  35,    29,  40,  35,
		 28,  35,  34,    29,  41,  40,
		 28,  34,  32,    29,  30,  41,
		 28,  32,  31,    30,  42,  41,
		 31,  32,  39,    30,  43,  42,
		 31,  39,  38,    30,  31,  43,
		 31,  38,  43,    32,  34,  33,
		 35,  40,  36,    40,  43,  36,
		 36,  43,  38,    36,  38,  37, //  28 -  43
		 44,  45,  46,    47,  46,  45, //  44 -  47
		 48,  49,  50,    51,  50,  49, //  48 -  51
		 52,  53,  54,    55,  54,  53, //  52 -  55
		 56,  57,  58,    59,  58,  57, //  56 -  59
		 60,  61,  62,    63,  62,  61, //  60 -  63
		 64,  65,  66,    67,  66,  65, //  64 -  67
		 68,  69,  70,    71,  70,  69, //  68 -  71
		 72,  73,  74,    75,  74,  73, //  72 -  75
		 76,  77,  78,    79,  78,  77, //  76 -  79
		 80,  81,  82,    83,  82,  81, //  80 -  83
		 84,  85,  86,    87,  86,  85, //  84 -  87
		 88,  89,  90,    91,  90,  89, //  88 -  91
		 92,  93,  99,    93,  98,  99,
		 93,  97,  98,    93,  94,  97,
		 94,  95,  97,    95,  96,  97, //  92 -  99
		100, 101, 104,   101, 105, 104,
		101, 102, 105,   102, 106, 105,
		102, 107, 106,   102, 103, 107,
		103, 104, 107,   100, 104, 103, // 100 - 107
		108, 109, 110,   111, 110, 109, // 108 - 111
		112, 113, 114,   115, 114, 113, // 112 - 115
		116, 117, 118,   119, 118, 117, // 116 - 119
		120, 121, 122,   123, 122, 121, // 120 - 123
		124, 125, 126,   127, 126, 125  // 124 - 127
	};

	const int bayhallNumVertices = sizeof(bayhallVIndex) / sizeof(int);
	for (int i = 0; i < bayhallNumVertices; i++) { bayhall.vertex[i] = (bayhallVertex[bayhallVIndex[i]] / 9.25f) + vec3(0.175f, 0.06f, 0.0f); }

	const int bayhallNumIndices = sizeof(bayhallIndex) / sizeof(int);
	for (int i = 0; i < bayhallNumIndices; i++) { bayhall.index[i] = bayhallIndex[i]; }

	bayhall.init(bayhallNumVertices, bayhallNumIndices, vec3(0.1, 0.1, 0.5), ((vec3(-1.134f, -1.105f, 0.0f) / 9.25f) + vec3(0.175f, 0.06f, 0.0f)), mat4(1.0f), gold, false);

	//------------------------------CAMERA---------------------------------
	camPresetPos[0] = vec3(0.5f, 0.0f, 0.5f);
	camPresetPos[1] = vec3(0.5f, 0.15f, 0.25f);
	camPresetPos[2] = vec3(0.15f, 0.0f, 0.25f);
	camPresetPos[3] = vec3(0.33f, -0.1f, 0.25f);

	POIPresetPos[0] = island.center;
	POIPresetPos[1] = ecdcA.center;
	POIPresetPos[2] = bayhall.center;
	POIPresetPos[3] = ecdcB.center;
	
	Projection = perspective(5.0f, 3.0f / 3.0f, 0.00001f, 1000.0f);
	
	pointOfInterest = island.center;
	cameraLocation  = island.center + vec3(0.0f, 0.0f, 0.25f);
	cameraUp = vec3(1.0f, 0.0f, 0.0f);

	//------------------------------LIGHTS---------------------------------
	light[0].pos   = vec4(0.33f, 0.33f, 0.25f, 1.0f);
	light[0].color = vec3(1.0f, 1.0f, 1.0f);
	light[0].intensity = 5.0f;

	light[1].pos   = vec4(0.5f, 0.5f,  0.5f, 0.0f);
	light[1].color = vec3(1.0f, 1.0f, 1.0f);
	light[1].intensity = 0.1f;
	
	phong = true;

	glEnable(GL_DEPTH_TEST);
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glDepthFunc(GL_LESS);
}

void keyboardCB(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	//cout << "key = " << key << "\n";
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);

	else if (key == GLFW_KEY_W             && action == GLFW_PRESS)   { dir[ 0] = 1; }
	else if (key == GLFW_KEY_S             && action == GLFW_PRESS)   { dir[ 1] = 1; }
	else if (key == GLFW_KEY_A             && action == GLFW_PRESS)   { dir[ 2] = 1; }
	else if (key == GLFW_KEY_D             && action == GLFW_PRESS)   { dir[ 3] = 1; }
	else if (key == GLFW_KEY_LEFT_SHIFT    && action == GLFW_PRESS)   { dir[ 4] = 1; }
	else if (key == GLFW_KEY_LEFT_CONTROL  && action == GLFW_PRESS)   { dir[ 5] = 1; }
																			 
	else if (key == GLFW_KEY_W             && action == GLFW_RELEASE) { dir[ 0] = 0; }
	else if (key == GLFW_KEY_S             && action == GLFW_RELEASE) { dir[ 1] = 0; }
	else if (key == GLFW_KEY_A             && action == GLFW_RELEASE) { dir[ 2] = 0; }
	else if (key == GLFW_KEY_D             && action == GLFW_RELEASE) { dir[ 3] = 0; }
	else if (key == GLFW_KEY_LEFT_SHIFT    && action == GLFW_RELEASE) { dir[ 4] = 0; }
	else if (key == GLFW_KEY_LEFT_CONTROL  && action == GLFW_RELEASE) { dir[ 5] = 0; }
																			 
	else if (key == GLFW_KEY_UP            && action == GLFW_PRESS)   { dir[ 6] = 1; }
	else if (key == GLFW_KEY_DOWN          && action == GLFW_PRESS)   { dir[ 7] = 1; }
	else if (key == GLFW_KEY_LEFT          && action == GLFW_PRESS)   { dir[ 8] = 1; }
	else if (key == GLFW_KEY_RIGHT         && action == GLFW_PRESS)   { dir[ 9] = 1; }
	else if (key == GLFW_KEY_RIGHT_SHIFT   && action == GLFW_PRESS)   { dir[10] = 1; }
	else if (key == GLFW_KEY_RIGHT_CONTROL && action == GLFW_PRESS)   { dir[11] = 1; }

	else if (key == GLFW_KEY_UP		       && action == GLFW_RELEASE) { dir[ 6] = 0; }
	else if (key == GLFW_KEY_DOWN		   && action == GLFW_RELEASE) { dir[ 7] = 0; }
	else if (key == GLFW_KEY_LEFT		   && action == GLFW_RELEASE) { dir[ 8] = 0; }
	else if (key == GLFW_KEY_RIGHT		   && action == GLFW_RELEASE) { dir[ 9] = 0; }
	else if (key == GLFW_KEY_RIGHT_SHIFT   && action == GLFW_RELEASE) { dir[10] = 0; }
	else if (key == GLFW_KEY_RIGHT_CONTROL && action == GLFW_RELEASE) { dir[11] = 0; }

	else if (key == GLFW_KEY_SPACE         && action == GLFW_RELEASE) { changeCamPos = 1; }

	else if (key == GLFW_KEY_TAB           && action == GLFW_RELEASE) { phong = !phong; }
}

void mouseCB(GLFWwindow *window, int button, int action, int mods)
{
	float r = 0.0f, g = 0.0f, b = 0.0f;
	cout << "btn = " << button << "\n";
	if (button == GLFW_MOUSE_BUTTON_1 && action == GLFW_PRESS)
	{
		r = (double)rand() / (double)RAND_MAX;
		g = (double)rand() / (double)RAND_MAX;
		b = (double)rand() / (double)RAND_MAX;
		cout << "r = " << r << " | " << "g = " << g << " | " << "b = " << b << "\n";
		glClearColor(r, g, b, 1.0);
	}
}

void renderWorld()
{
	//---------------------------CHANGE-CAMERA-DIRECTION---------------------------
	if (dir[ 0] == 1) { cameraLocation.x  -= speed; }
	if (dir[ 1] == 1) { cameraLocation.x  += speed; }
	if (dir[ 2] == 1) { cameraLocation.y  -= speed; }
	if (dir[ 3] == 1) { cameraLocation.y  += speed; }
	if (dir[ 4] == 1) { cameraLocation.z  -= speed; }
	if (dir[ 5] == 1) { cameraLocation.z  += speed; }
			 
	if (dir[ 6] == 1) { pointOfInterest.x -= speed; }
	if (dir[ 7] == 1) { pointOfInterest.x += speed; }
	if (dir[ 8] == 1) { pointOfInterest.y -= speed; }
	if (dir[ 9] == 1) { pointOfInterest.y += speed; }
	if (dir[10] == 1) { pointOfInterest.z -= speed; }
	if (dir[11] == 1) { pointOfInterest.z += speed; }

	if (changeCamPos == 1)
	{
		changeCamPos = 0;
		camPresetMode++;
		if (camPresetMode >= 4) { camPresetMode = 0; }
		cameraLocation = camPresetPos[camPresetMode];
		pointOfInterest = POIPresetPos[camPresetMode];
	}

	light[0].pos.x = sin(ang)/3;
	light[0].pos.y = cos(ang)/3;

	ang += 0.0005;
	if (ang > 360) ang = 0;

	View = lookAt(cameraLocation, pointOfInterest, cameraUp);
	PV = Projection * View;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	island.render();
	ground.render();
	cube.render();
	ecdcA.render();
	ecdcB.render();
	bayhall.render();
}

int main()
{
	if (!glfwInit())
	{
		fprintf(stderr, "ERROR: could not start GLFW3\n");
		return 1;
	}

	GLFWwindow* window = glfwCreateWindow(1024, 1024, "COSC 4328 HW 3", NULL, NULL);
	if (!window)
	{
		fprintf(stderr, "ERROR: could not open window with GLFW3\n");
		glfwTerminate();
		return 1;
	}
	glfwMakeContextCurrent(window);

	glewExperimental = GL_TRUE;

	glewInit();

	const GLubyte* renderer = glGetString(GL_RENDERER);
	const GLubyte* version = glGetString(GL_VERSION);
	printf("Renderer: %s\n", renderer);
	printf("OpenGL version supported %s\n", version);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	initialize();
	glfwSetKeyCallback(window, keyboardCB);
	//glfwSetMouseButtonCallback(window, mouseCB);

	while (!glfwWindowShouldClose(window))
	{
		//glfwSetKeyCallback(window, key_callback);
		renderWorld();
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwTerminate();
	return 0;
}