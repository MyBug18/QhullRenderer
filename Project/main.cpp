#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp> // glm::value_ptr
#include <glm/gtc/matrix_transform.hpp> // glm::translate, glm::rotate, glm::scale, glm::perspective
#include <libqhullcpp/Qhull.h>
#include <libqhullcpp/QhullVertex.h>
#include <libqhullcpp/QhullVertexSet.h>
#include <libqhullcpp/QhullFacetList.h>
#include <libqhullcpp/RboxPoints.h>
#include "Utils.h"

constexpr auto numVAOs = 1;
constexpr auto numVBOs = 1;

GLuint renderingProgram;
GLuint vao[numVAOs];
GLuint vbo[numVBOs];

// variable allocation for display
GLuint mvLoc, projLoc;
glm::mat4 pMat, mvMat;

int verticeSize;
bool isTriangleMode;

// expand vertex index 0, 1, 2 to 0, 1, 1, 2, 2, 0 to draw line correctly
const std::vector<float> toLineVertex(const std::vector<float>& input)
{
	std::vector<float> result;

	if (input.size() % 9 != 0) return result;

	for (size_t i = 0; i < input.size() / 9; i++)
	{
		// index 0
		result.push_back(input[9 * i + 0]);
		result.push_back(input[9 * i + 1]);
		result.push_back(input[9 * i + 2]);

		// index 1
		result.push_back(input[9 * i + 3]);
		result.push_back(input[9 * i + 4]);
		result.push_back(input[9 * i + 5]);

		// index 1
		result.push_back(input[9 * i + 3]);
		result.push_back(input[9 * i + 4]);
		result.push_back(input[9 * i + 5]);

		// index 2
		result.push_back(input[9 * i + 6]);
		result.push_back(input[9 * i + 7]);
		result.push_back(input[9 * i + 8]);

		// index 2
		result.push_back(input[9 * i + 6]);
		result.push_back(input[9 * i + 7]);
		result.push_back(input[9 * i + 8]);

		// index 0
		result.push_back(input[9 * i + 0]);
		result.push_back(input[9 * i + 1]);
		result.push_back(input[9 * i + 2]);
	}

	return result;
}

void setupVertices(const char* rboxCommand)
{
	std::vector<float> randomVerticeVector;

	orgQhull::RboxPoints rbox;
	rbox.appendPoints(rboxCommand);
	orgQhull::Qhull qhull;
	qhull.runQhull(rbox, "");

	auto lst = qhull.facetList().toStdVector();

	for (auto i = 0; i < lst.size(); i++)
	{
		auto vertices = lst[i].vertices().toStdVector();

		auto point0 = vertices[0].point().toStdVector();
		auto point0Vec = glm::vec3(point0[0], point0[1], point0[2]);

		auto point1 = vertices[1].point().toStdVector();
		auto point1Vec = glm::vec3(point1[0], point1[1], point1[2]);

		auto point2 = vertices[2].point().toStdVector();
		auto point2Vec = glm::vec3(point2[0], point2[1], point2[2]);

		auto vec1 = point2Vec - point1Vec;
		auto vec2 = point2Vec - point0Vec;

		auto cross = glm::cross(vec1, vec2);

		auto n = lst[i].getBaseT()->normal;
		auto normal = glm::vec3((float)n[0], (float)n[1], (float)n[2]);

		auto dot = glm::dot(cross, normal);

		if (dot < 0)
		{
			auto temp = std::move(vertices[0]);
			vertices[0] = vertices[1];
			vertices[1] = temp;
		}

		for (auto j = 0; j < vertices.size(); j++)
		{
			auto points = vertices[j].point().toStdVector();

			for (auto k = 0; k < points.size(); k++)
			{
				randomVerticeVector.push_back((float)points[k] * 5);
			}
		}
	}

	if (!isTriangleMode)
	{
		randomVerticeVector = toLineVertex(randomVerticeVector);
	}

	glGenVertexArrays(numVAOs, vao);
	glBindVertexArray(vao[0]);
	glGenBuffers(numVBOs, vbo);

	glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * randomVerticeVector.size(), randomVerticeVector.data(), GL_STATIC_DRAW);
	verticeSize = randomVerticeVector.size() / 3;
}

void init(GLFWwindow* window, const char* rboxCommand)
{
	renderingProgram = Utils::createShaderProgram("vertShader.glsl", "fragShader.glsl");

	int width, height;

	glfwGetFramebufferSize(window, &width, &height);
	pMat = glm::perspective(1.0472f, (float)width / (float)height, 0.1f, 1000.0f);

	setupVertices(rboxCommand);

	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CW);
}

void display(GLFWwindow* window, double currentTime)
{
	glClear(GL_DEPTH_BUFFER_BIT);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(renderingProgram);

	mvLoc = glGetUniformLocation(renderingProgram, "mv_matrix");
	projLoc = glGetUniformLocation(renderingProgram, "proj_matrix");

	mvMat = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -8)) * glm::rotate(glm::mat4(1.0f), (float)currentTime, glm::vec3(1.0f, 1.5f, 0.5f));

	glUniformMatrix4fv(mvLoc, 1, GL_FALSE, glm::value_ptr(mvMat));
	glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(pMat));

	glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	glDrawArrays(isTriangleMode ? GL_TRIANGLES : GL_LINES, 0, verticeSize);
}

std::vector<std::string> parseSetting()
{
	std::vector<std::string> content;

	std::ifstream fileStream("setting.txt", std::ios::in);
	if (!fileStream.is_open())
	{
		content.push_back("true");
		content.push_back("30");
		return content;
	}

	std::string line = "";
	while (!fileStream.eof())
	{
		std::getline(fileStream, line);
		content.push_back(line);
	}

	return content;
}

int main(void)
{
	const auto& settings = parseSetting();
	if (settings.size() != 2) return EXIT_FAILURE;
	isTriangleMode = settings[0] == "true";

	if (!glfwInit()) { exit(EXIT_FAILURE); }
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	auto window = glfwCreateWindow(1280, 720, "yeah", NULL, NULL);
	glfwMakeContextCurrent(window);
	if (glewInit() != GLEW_OK) { exit(EXIT_FAILURE); }
	glfwSwapInterval(1);

	init(window, settings[1].c_str());

	while (!glfwWindowShouldClose(window)) {
		display(window, glfwGetTime());
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	exit(EXIT_SUCCESS);
}
