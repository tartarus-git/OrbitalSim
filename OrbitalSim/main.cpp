#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <thread>

#define CLASS_NAME TEXT("orbital_sim_window")

// Macro for logging debug output.
#define PRINT(message) OutputDebugString(TEXT(message) "\n")

#define G 10000
#define SPRING_STRENGTH 10
#define SPRING_FRICTION 0.1f

#define VELOCITY_SENSITIVITY 0.05f					// TOOD: Figure out why hovering over this float value gives you a slightly off float value.

#define PARTICLE_RAD 20

#define KEY_O 0x4F
#define KEY_X 0x58

#define NANOSECONDS_PER_SECOND 1000000000

bool shouldGraphicsLoopRun = true;

class Vector2f {
public:
	float x;
	float y;

	Vector2f() = default;

	Vector2f(float x, float y) {
		this->x = x;
		this->y = y;
	}

	void add(Vector2f v) {
		x += v.x;
		y += v.y;
	}

	void sub(Vector2f v) {
		x -= v.x;
		y -= v.y;
	}

	Vector2f operator+(Vector2f v) {
		return Vector2f(x + v.x, y + v.y);
	}

	Vector2f operator-(Vector2f v) {
		return Vector2f(x - v.x, y - v.y);
	}

	void multiply(float factor) {
		x *= factor;
		y *= factor;
	}

	float getAmplitude() {
		return sqrt(x * x + y * y);				// Technically this could have an integer overflow because of the squaring and the adding, but in practice this probably won't ever happen.
	}
};

HDC g;

class Body {
public:
	Vector2f pos;
	Vector2f vel;

	Body() = default;

	Body(Vector2f pos, Vector2f vel) {
		this->pos = pos;
		this->vel = vel;
	}

	void render() {
		Ellipse(g, pos.x - PARTICLE_RAD, pos.y - PARTICLE_RAD, pos.x + PARTICLE_RAD, pos.y + PARTICLE_RAD);
	}

	void update() {
		pos.add(vel);
	}

	Vector2f getGravityVector(Body other) {
		Vector2f diff = other.pos - pos;
		float dist = diff.getAmplitude();
		diff.multiply(G / (dist * dist) / dist);
		return diff;
	}

	void applyGravity(Body& other) {
		Vector2f gravVec = getGravityVector(other);
		vel.add(gravVec);
		other.vel.sub(gravVec);
	}

	Vector2f getCollisionSpringVector(Body other) {
		Vector2f diff = other.pos - pos;
		float dist = diff.getAmplitude();
		diff.multiply((PARTICLE_RAD * 2 - dist) * SPRING_STRENGTH / dist);
		return diff;
	}

	void applyCollisionSpring(Body& other) {
		Vector2f diff = other.pos - pos;
		float dist = diff.getAmplitude();
		if (dist < PARTICLE_RAD * 2) {
			diff.multiply((PARTICLE_RAD * 2 - dist) * SPRING_STRENGTH / dist);
			vel.sub(diff);
			vel.multiply(SPRING_FRICTION);
			other.vel.add(diff);
			other.vel.multiply(SPRING_FRICTION);
		}
	}

	void applyStationaryCollisionSpring(Body other) {
		Vector2f diff = other.pos - pos;
		float dist = diff.getAmplitude();
		if (dist < PARTICLE_RAD * 2) {
			diff.multiply((PARTICLE_RAD * 2 - dist) * SPRING_STRENGTH / dist);
			vel.sub(diff);
			vel.multiply(SPRING_FRICTION);
		}
	}
};

Body mainBody;

bool clearUniverse = false;
bool orbitFlag = false;
bool newBody = false;
bool mouseDown = false;
Vector2f newBodyPos;
Vector2f newBodyVelPos;
Vector2f newBodyVel;

bool resizeFlag = false;

void graphicsLoop(HWND hWnd);

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_KEYDOWN:
		switch (wParam) {
		case KEY_O:
			orbitFlag = !orbitFlag;
			return 0;
		case KEY_X:
			clearUniverse = true;
			return 0;
		}
		break;
	case WM_LBUTTONDOWN:
		newBodyPos = Vector2f(lParam & 0x0000FFFF, lParam >> 16);
		if (orbitFlag) {
			Vector2f diff = newBodyPos - mainBody.pos;
			float dist = diff.getAmplitude();
			diff = Vector2f(-diff.y, diff.x);
			diff.multiply(sqrt(G / dist) / dist);
			newBodyVel = diff;
			newBody = true;
			return 0;
		}
		newBodyVelPos = newBodyPos;
		mouseDown = true;
		return 0;
	case WM_LBUTTONUP:
		if (mouseDown) {
			newBodyVel = newBodyVelPos - newBodyPos;
			newBodyVel.multiply(VELOCITY_SENSITIVITY);
			mouseDown = false;
			newBody = true;
		}
		return 0;
	case WM_MOUSEMOVE:
		if (mouseDown) {
			newBodyVelPos = Vector2f(lParam & 0x0000FFFF, lParam >> 16);
		}
		return 0;
	case WM_EXITSIZEMOVE:
		resizeFlag = true;
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

#ifdef UNICODE
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, wchar_t* lpCmdLine, int nCmdShow) {
	PRINT("Started program from wWinMain entry point (UNICODE).");
#else
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char* lpCmdLine, int nCmdShow) {
	PRINT("Started program from WinMain entry point (ANSI).");
#endif

	WNDCLASS windowClass = { };
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = hInstance;
	windowClass.lpszClassName = CLASS_NAME;
	windowClass.hCursor = LoadCursor(hInstance, IDC_ARROW);				// TODO: You gotta solve this weird mouse shit that happens.

	PRINT("Registering the window class...");
	RegisterClass(&windowClass);

	PRINT("Creating the window...");
	HWND hWnd = CreateWindowEx(0, CLASS_NAME, TEXT("Orbital Simulation"), WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);

	// Check if the window creation was sucessful.
	if (hWnd == NULL) {
		PRINT("Error encountered while creating the window, terminating...");
		UnregisterClass(CLASS_NAME, hInstance);
		return 0;
	}

	PRINT("Showing the window...");
	ShowWindow(hWnd, nCmdShow);

	PRINT("Starting the graphics thread...");
	std::thread graphicsThread(graphicsLoop, hWnd);

	PRINT("Running the message loop...");
	MSG msg = { };
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	shouldGraphicsLoopRun = false;
	graphicsThread.join();
}

template <typename T>
class LinkedList {				// TODO: It's sad, but you have to stop using LinkedList's because they cause too many cache misses. Use std::vector instead I think.
public:
	LinkedList* child;
	T element;

	int length = 0;

	LinkedList() = default;

	void iterate(void(*action)(LinkedList* element)) {
		LinkedList* current = this;
		while (true) {
			action(current);
			if (current->length == 1) { break; }
			current = current->child;
		}
	}

	void add(T element) {
		LinkedList* current = this;
		while (current->length) {
			if (current->length == 1) { current->child = new LinkedList(); }
			current->length++;
			current = current->child;
		}
		current->length++;
		current->element = element;
	}

	void release() {
		if (length > 1) {
			child->release();
			delete child;
		}
		length = 0;
	}

	void clear() { release(); }

	~LinkedList() { release(); }
};

class FrameManager {
	std::chrono::high_resolution_clock::time_point frameStart;
	int nsPerFrame;

public:
	FrameManager(int FPS) {
		nsPerFrame = NANOSECONDS_PER_SECOND / FPS;
	}

	void start() {
		frameStart = std::chrono::high_resolution_clock::now();
	}

	std::chrono::high_resolution_clock::duration measure() {
		return std::chrono::high_resolution_clock::now() - frameStart;
	}
	
	void delay() {
		long long frameDuration = measure().count();
		if (frameDuration < nsPerFrame) {
			std::this_thread::sleep_for(std::chrono::nanoseconds(nsPerFrame - frameDuration));
		}
	}
};

//Global variables which are used to hold values in lambda functions.
LinkedList<Body>* tempI;

void graphicsLoop(HWND hWnd) {
	PRINT("Started graphics thread. Setting up...");

	HDC hDC = GetDC(hWnd);
	if (!hDC) {
		PRINT("DC creation failed, shutting down...");
		PostQuitMessage(0);
		return;
	}

	// Get bounds of window.
	RECT clientBounds;
	GetClientRect(hWnd, &clientBounds);

	// Create a seconds DC for double-buffering.
	g = CreateCompatibleDC(hDC);
	HBITMAP bmp = CreateCompatibleBitmap(hDC, clientBounds.right, clientBounds.bottom);
	SelectObject(g, bmp);

	HGDIOBJ backgroundBrush = GetStockObject(BLACK_BRUSH);
	HGDIOBJ backgroundPen = GetStockObject(BLACK_PEN);

	HPEN mainBodyPen = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
	HBRUSH mainBodyBrush = CreateSolidBrush(RGB(255, 0, 0));
	HPEN bodyPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 255));
	HBRUSH bodyBrush = CreateSolidBrush(RGB(0, 0, 255));
	HPEN velocityPen = CreatePen(PS_SOLID, 5, RGB(0, 255, 0));

	// Scene setup.
	mainBody = Body(Vector2f(clientBounds.right / 2, clientBounds.bottom / 2), Vector2f(0, 0));
	LinkedList<Body> universe;

	FrameManager mainFrameManager(120);

	while (shouldGraphicsLoopRun) {
		mainFrameManager.start();

		// Background
		SelectObject(g, backgroundBrush);
		SelectObject(g, backgroundPen);
		Rectangle(g, clientBounds.left, clientBounds.top, clientBounds.right, clientBounds.bottom);

		// Bodies
		SelectObject(g, mainBodyBrush);
		SelectObject(g, mainBodyPen);
		mainBody.render();
		if (universe.length) {
			SelectObject(g, bodyBrush);
			SelectObject(g, bodyPen);
			universe.iterate([](LinkedList<Body>* i) {
				i->element.render();
			});
			universe.iterate([](LinkedList<Body>* i) {
				i->element.vel.add(i->element.getGravityVector(mainBody));
				i->element.applyStationaryCollisionSpring(mainBody);
				if (i->length == 1) { return; }
				tempI = i;
				i->child->iterate([](LinkedList<Body>* j) {
					tempI->element.applyGravity(j->element);
					tempI->element.applyCollisionSpring(j->element);
				});
			});
			universe.iterate([](LinkedList<Body>* i) {
				i->element.update();
			});
		}

		// Mouse stuff.

		// Draw velocity line if need be.
		if (mouseDown) {
			SelectObject(g, velocityPen);
			MoveToEx(g, newBodyPos.x, newBodyPos.y, NULL);
			LineTo(g, newBodyVelPos.x, newBodyVelPos.y);
		}

		// Check if new bodies need to be added.
		if (newBody) {
			universe.add(Body(newBodyPos, newBodyVel));
			newBody = false;
		}

		// Draw the buffer onto the screen.
		BitBlt(hDC, clientBounds.left, clientBounds.top, clientBounds.right, clientBounds.bottom,
			g, clientBounds.left, clientBounds.top, SRCCOPY);

		if (clearUniverse) {
			universe.clear();
			clearUniverse = false;
		}

		if (resizeFlag) {
			GetClientRect(hWnd, &clientBounds);
			mainBody.pos.x = clientBounds.right / 2;
			mainBody.pos.y = clientBounds.bottom / 2;
			DeleteObject(bmp);
			bmp = CreateCompatibleBitmap(hDC, clientBounds.right, clientBounds.bottom);
			SelectObject(g, bmp);
			resizeFlag = false;
		}

		mainFrameManager.delay();
	}

	DeleteObject(mainBodyPen);
	DeleteObject(mainBodyBrush);
	DeleteObject(bodyPen);
	DeleteObject(bodyBrush);
	DeleteObject(velocityPen);
	DeleteDC(g);
	DeleteObject(bmp);
	ReleaseDC(hWnd, hDC);
}