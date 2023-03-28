#define GLM_ENABLE_EXPERIMENTAL
#include "gameLayer.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include "platformInput.h"
#include "imgui.h"
#include <iostream>
#include <sstream>
#include "imfilebrowser.h"
#include <gl2d/gl2d.h>
#include <random>
#include <raudio.h>

#undef max
#undef min


gl2d::Renderer2D renderer;


float carPos = 0;
float carPosX = 0.5;

struct TrackSegment
{
	float curvature = 0;
	float distance = 0;
	float trackSize = 0.7f;
	float edgeSize = 0.08f;

	void generateNewSegment()
	{
		std::mt19937 r(std::random_device{}());
		std::uniform_int_distribution<int> curvature(-2, 2);
		std::uniform_real_distribution<float> distance(50.f, 200.f);
		std::uniform_real_distribution<float> trackSize(0.4f, 0.7f);
		std::uniform_real_distribution<float> edgeSize(0.f, 0.1f);

		this->curvature = curvature(r)/2.f;
		this->distance = distance(r);
		this->trackSize = trackSize(r);
		this->edgeSize = edgeSize(r);
	}
};

TrackSegment currentSegment;
float currentCurvature = 0;
float currentSize = 0;
float currentEdge = 0;

Music music;

bool initGame()
{
	gl2d::init();

	renderer.create();

	currentSegment.generateNewSegment();
	currentSegment.curvature = 0;
	currentCurvature = currentSegment.curvature;
	currentSize = currentSegment.trackSize;
	currentEdge = currentSegment.edgeSize;

	music = LoadMusicStream(RESOURCES_PATH "dejavu.ogg");
	music.looping = true;
	PlayMusicStream(music);
	SetMusicVolume(music, 0.4);
	

	return true;
}

const int W = 160;
const int H = 100;
const int PIXEL = 8;

glm::vec3 screen[100][160] = {};
glm::vec3 &getScreen(int x, int y) { return screen[y][x]; }

glm::vec3 stub = {};
glm::vec3 &getScreenSafe(int x, int y)
{
	if (x < 0 || y < 0 || x >= W || y >= H)
	{
		stub = {};
		return stub;
	}
	else
	{
		return screen[y][x];
	}
}

#pragma region drawcar

const char *carDataMiddle =
"    x     x    "
"    xxxxxxx    "
"       o       "
"     XXXXX     "
"OOO  XXXXX  OOO"
"OOOXXXXXXXXXOOO"
"OOO  XXXXX  OOO";

const char *carDataLeft =
"      x     x  "
"      xxxxxxx  "
"         o     "
"      XXXXX    "
"OOO    XXXXXOOO"
" OOOXXXXXXXXXOO"
"  OOO   XXXXXOO";

const char *carDataRight =
"  x     x      "
"  xxxxxxx      "
"     o         "
"    XXXXX      "
"OOOXXXXX    OOO"
"OOXXXXXXXXXOOO "
"OOXXXXX  OOO   ";

void drawCar(int x, int y, int dir)
{
	int carW = 15;
	int carH = 7;

	int cornerX = x - carW / 2;
	int cornerY = y - carH / 2;

	const char *carData = carDataMiddle;
	if (dir < 0)
	{
		carData = carDataLeft;
	}
	else if (dir > 0)
	{
		carData = carDataRight;
	}

	for (int j = 0; j < carH; j++)
		for (int i = 0; i < carW; i++)
		{
			if (carData[j * carW + i] != ' ')
			{
				if (carData[j * carW + i] == 'O')
				{
					getScreenSafe(i + cornerX, j + cornerY) = glm::vec3(0.1, 0.1, 0.1);
				}
				else if (carData[j * carW + i] == 'X')
				{
					getScreenSafe(i + cornerX, j + cornerY) = glm::vec3(0.8, 0.1, 0.1);
				}
				else if (carData[j * carW + i] == 'o')
				{
					getScreenSafe(i + cornerX, j + cornerY) = glm::vec3(0.4, 0.3, 0.3);
				}
				else if (carData[j * carW + i] == 'x')
				{
					getScreenSafe(i + cornerX, j + cornerY) = glm::vec3(0.9, 0.3, 0.3);
				}

			}
		}
}

#pragma endregion

float speed = 0;
float mountainsFaze = 0;
float mountainsFaze2 = 0;

bool gameLogic(float deltaTime)
{
	UpdateMusicStream(music);

#pragma region init stuff
	int w = 0; int h = 0;
	w= platform::getWindowSizeX();
	h = platform::getWindowSizeY();
	
	glViewport(0, 0, w, h);
	glClear(GL_COLOR_BUFFER_BIT);

	renderer.updateWindowMetrics(w, h);

	memset(screen, 0, sizeof(screen));

#pragma endregion

#pragma region input

	int movingDir = 0;
	if (platform::isKeyHeld(platform::Button::Left))
	{
		movingDir += -1;
	}
	else if (platform::isKeyHeld(platform::Button::Right))
	{
		movingDir += 1;
	}
	else
	{
		movingDir = 0;
	}

	if (platform::isKeyHeld(platform::Button::Up))
	{
		speed += deltaTime * 6;	
	}
	else if(platform::isKeyHeld(platform::Button::Down))
	{
		speed -= deltaTime * 5;
	}
	else
	{
		speed -= deltaTime * 2;
	}


	speed = glm::clamp(speed, 0.f, 50.f);



	carPos += speed * deltaTime;
	currentSegment.distance -= speed * deltaTime;

	if (movingDir < 0)
	{
		carPosX -= 1.f/(W * 2) * 40 * deltaTime;
	}
	else if (movingDir > 0)
	{
		carPosX += 1.f / (W * 2) * 40 * deltaTime;
	}




	if (currentSegment.distance < 0) { currentSegment.generateNewSegment(); }

#pragma endregion



	for (int y = 0; y < H/2; y++)
	{
		for (int x = 0; x < W; x++)
		{
			float normalizedY = y / (H / 2.f);

			if (normalizedY > 0.80f && (1 - normalizedY) < 0.2 * std::abs(std::sin((float(x) * 3.1415926 * 1 / W) + mountainsFaze2)) 
				||
				normalizedY > 0.95f
				)
			{
				getScreen(x, y) = glm::vec3(63, 64, 58) / 255.f;
			}else
			if(normalizedY > 0.5f && (1-normalizedY) < 0.5 * std::sin((float(x) * 3.1415926 * 2 / W) + mountainsFaze))
			{
				getScreen(x, y) = glm::vec3(64, 48, 39)/255.f;
			}
			else
			{
				getScreen(x, y) = glm::vec3(0.3, 0.4, 0.9) * 0.5f + glm::vec3(0.3, 0.4, 0.9) * 0.5f * (floor(normalizedY*10)/10);
			}

		}
	}

	const glm::vec3 grassColor1 = glm::vec3(0.2, 0.7, 0.3);
	const glm::vec3 grassColor2 = glm::vec3(0.15, 0.5, 0.25);
	const glm::vec3 trackColor = glm::vec3(0.2, 0.2, 0.22);
	const glm::vec3 borderColor1 = glm::vec3(0.9, 0.9, 0.9);
	const glm::vec3 borderColor2 = glm::vec3(0.9, 0.1, 0.1);

	currentCurvature += (currentSegment.curvature - currentCurvature) * speed * deltaTime * 0.5;
	currentSize += (currentSegment.trackSize - currentSize) * speed * deltaTime * 0.5;
	currentEdge += (currentSegment.edgeSize - currentEdge) * speed * deltaTime * 0.5;

	mountainsFaze += speed * deltaTime * currentCurvature * 0.008;
	mountainsFaze2 += speed * deltaTime * currentCurvature * 0.02;

	carPosX -= currentCurvature * (speed / (W * 2)) * deltaTime;

	carPosX = std::min(carPosX, 1.f);
	carPosX = std::max(carPosX, 0.f);

	const float trackSize = currentSize;
	const float edgeSize = currentEdge;

	for (int y = H/2; y < H; y++)
	{
		float fy = (y-H/2) / (float)(H/2 - 1);
		fy = 1 - fy;

		for (int x = 0; x < W; x++)
		{
			float trackMiddle = 0.5 + fy * fy * currentCurvature * 0.5;
			float fx = x / (float)(W-1);
			
			float perspective = ((float)y - (H / 2.f)) / (H - 2.f);  // 0 -> 1
			perspective = perspective*3 + 0.1f; //0.1 -> 3.1

			const float trackSize2 = trackSize * perspective;
			const float edgeSize2 = edgeSize * trackSize2;

			glm::vec3 grassColor = std::sin(fy * fy * 3.1415926f * 10 + carPos) > 0 ? grassColor1 : grassColor2;
			glm::vec3 borderColor = std::sin(fy * fy * 3.1415926f * 22 + carPos) > 0 ? borderColor1 : borderColor2;

			if (fx < trackMiddle - trackSize2 / 2.f)
			{
				getScreen(x, y) = grassColor;
			}
			else if (fx < trackMiddle + trackSize2 / 2.f)
			{
				if (fx < trackMiddle - trackSize2 / 2.f + edgeSize2)
				{
					getScreen(x, y) = borderColor;
				}
				else if (fx < trackMiddle + trackSize2 / 2.f - edgeSize2)
				{
					getScreen(x, y) = trackColor;
				}
				else
				{
					getScreen(x, y) = borderColor;
				}
			}
			else
			{
				getScreen(x, y) = grassColor;
			}
			
		}
	}

	if(speed > 5)
	{
		float trackMiddle = 0.5;
		if (carPosX < trackMiddle - trackSize / 2.f)
		{
			speed -= deltaTime * 10;
		}
		else if (carPosX < trackMiddle + trackSize / 2.f)
		{
			if (carPosX < trackMiddle - trackSize / 2.f + edgeSize)
			{
				speed -= deltaTime * 7;
			}
			else if (carPosX < trackMiddle + trackSize / 2.f - edgeSize)
			{
				//track
			}
			else
			{
				speed -= deltaTime * 7;

			}
		}
		else
		{
			speed -= deltaTime * 10;
		}
	}

	if (movingDir < 0)
	{
		drawCar(carPosX * W, H - 8, -1);
	}
	else if (movingDir > 0)
	{
		drawCar(carPosX * W, H - 8, 1);
	}
	else
	{
		drawCar(carPosX * W, H - 8, 0);
	}

#pragma region rendering
	
	for (int y = 0; y < H; y++)
	{
		for (int x = 0; x < W; x++)
		{
			renderer.renderRectangle(glm::vec4{x,y,1,1}*(float)PIXEL, glm::vec4(getScreen(x, y), 1));
		}
	}
	renderer.flush();
#pragma endregion

	return true;
#pragma endregion

}

void closeGame()
{

}
