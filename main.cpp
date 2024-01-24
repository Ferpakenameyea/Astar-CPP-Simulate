#include <vector>
#include <thread>
#include <chrono>
#include <iostream>
#include <sstream>
#include <queue>

#include <Windows.h>
#include <graphics.h>

#include "func.h"
#include "map"

static MapMesh map(50, 50);

const int canvasWidth = 900;
const int canvasHeight = 900;

const int blockWidth = canvasWidth / map.width;
const int blockHeight = canvasHeight / map.height;
class PathFindingTask;
PathFindingTask* currentTask = NULL;

class DrawTask {
private:
	const int FPS = 60;
	std::thread* renderThread = NULL;

	void Draw() {
		initgraph(canvasWidth, canvasHeight, SHOWCONSOLE);
		setbkcolor(WHITE);
		cleardevice();
		BeginBatchDraw();
		while (TRUE) {
			cleardevice();
			OnDraw();
			FlushBatchDraw();
			std::this_thread::sleep_for(std::chrono::milliseconds(1000 / FPS));
		}
		EndBatchDraw();
	}

	void OnDraw() {
		map.OnDrawCanvas(canvasWidth, canvasHeight);
	}

public:
	void Launch() {
		this->renderThread = new std::thread([this]() {
			this->Draw();
			});

		this->renderThread->detach();
	}

	~DrawTask() {
		delete this->renderThread;
	}
};

class PathFindingTask {
private:
	struct PathBlock {
		int x;
		int y;
		int cost;
		int possibleCost;
		static const int weightA = 1;
		static const int weightB = 1;

		struct CompareLess {
			bool operator()(const PathBlock& a, const PathBlock& b) {
				return (weightA * a.cost + weightB * a.possibleCost) > (weightA * b.cost + weightB * b.possibleCost);
			}
		};

		PathBlock operator=(const PathBlock& other) {
			this->x = other.x;
			this->y = other.y;
			this->cost = other.cost;
			this->possibleCost = other.possibleCost;
			return *this;
		}
	};

	bool running = false;
	
public:
	const bool IsRunning() const {
		return this->running;
	}

	void Run(const std::pair<int, int> from, const std::pair<int, int> to, const int delay_micro) {
		std::thread([this, from, to, delay_micro]() {
			this->_Run(from, to, delay_micro);
			}).detach();
	}

	void _Run(const std::pair<int, int> from, const std::pair<int, int> to, const int delay_micro) {
		if (this->running) {
			return;
		}
		this->running = true;
		std::priority_queue <PathBlock, std::vector<PathBlock>, PathBlock::CompareLess> heap;
		const auto& startPos = map.GetStartPos();
		const auto& destPos = map.GetDestPos();
		auto func = [destPos](const int x, const int y) -> const int {
			return std::abs(destPos.first - x) + std::abs(destPos.second - y);
			};

		PathBlock start = { startPos.first, startPos.second, 0, func(startPos.first, startPos.second) };

		heap.push(start);
		while (!heap.empty()) {
			const auto top = heap.top();
			heap.pop();

			if (destPos.first == top.x && destPos.second == top.y) {
				std::cout << "found!" << std::endl;
				break;
			}

			if (map.InMapRange(top.x + 1, top.y) && !map.Visited(top.x + 1, top.y) && map.Walkable(top.x + 1, top.y)) {
				heap.push({ top.x + 1, top.y, top.cost + 1, func(top.x + 1, top.y) });
				map.SetParent(top.x + 1, top.y, LEFT);
				map.MarkVisit(top.x + 1, top.y);
			}
			if (map.InMapRange(top.x - 1, top.y) && !map.Visited(top.x - 1, top.y) && map.Walkable(top.x - 1, top.y)) {
				heap.push({ top.x - 1, top.y, top.cost + 1, func(top.x - 1, top.y) });
				map.SetParent(top.x - 1, top.y, RIGHT);
				map.MarkVisit(top.x - 1, top.y);
			}
			if (map.InMapRange(top.x, top.y + 1) && !map.Visited(top.x, top.y + 1) && map.Walkable(top.x, top.y + 1)) {
				heap.push({ top.x, top.y + 1, top.cost + 1, func(top.x, top.y + 1) });
				map.SetParent(top.x, top.y + 1, UP);
				map.MarkVisit(top.x, top.y + 1);
			}
			if (map.InMapRange(top.x, top.y - 1) && !map.Visited(top.x, top.y - 1) && map.Walkable(top.x, top.y - 1)) {
				heap.push({ top.x, top.y - 1, top.cost + 1, func(top.x, top.y - 1) });
				map.SetParent(top.x, top.y - 1, DOWN);
				map.MarkVisit(top.x, top.y - 1);
			}
			std::this_thread::sleep_for(std::chrono::microseconds(delay_micro));
		}

		map.ShowPath(1);
		this->running = false;
	}
};

class UserControlTask {
private:
	const int tickRate = 300;
	std::thread* controlThread = NULL;
	void OnTick() {
		bool fillButtonDown = GetAsyncKeyState(VK_LBUTTON);
		bool clearButtonDown = GetAsyncKeyState(VK_RBUTTON);
		const MOUSEMSG mouse = GetMouseMsg();
		if (currentTask != NULL && currentTask->IsRunning()) {
			return;
		}

		if (fillButtonDown) {
			const int mouseX = (mouse.x) / blockWidth;
			const int mouseY = (mouse.y) / blockHeight;
			if (map.InMapRange(mouseX, mouseY)) {
				map.FillBarricade(mouseX, mouseY);
			}
		}
		else if (clearButtonDown) {
			const int mouseX = (mouse.x) / blockWidth;
			const int mouseY = (mouse.y) / blockHeight;
			if (map.InMapRange(mouseX, mouseY)) {
				map.ClearBarricade(mouseX, mouseY);
			}
		}
	}

public:
	void Launch() {
		this->controlThread = new std::thread([this]() {
			while (TRUE) {
				this->OnTick();
				std::this_thread::sleep_for(std::chrono::milliseconds(1000 / tickRate));
			}
			});
		this->controlThread->detach();
	}

	~UserControlTask() {
		delete this->controlThread;
	}
};

class Console {
private:
	std::string input;
	std::string token;
	std::vector<std::string> tokens;
	
public:
	void RunConsole() {
		while (true) {
			std::getline(std::cin, input);
			if (currentTask != NULL && currentTask->IsRunning()) {
				std::cout << "Console is disabled when running!" << std::endl;
				continue;
			}
			
			std::stringstream stringstream(input);
			
			while (std::getline(stringstream, token, ' ')) {
				tokens.push_back(token);
			}
			if (tokens.size() == 0) {
				continue;
			}
			if (tokens[0] == "clear") {
				if (tokens.size() != 1) {
					std::cout << "Wrong parameters! [help]: reset" << std::endl;
					tokens.clear();
					continue;
				}

				map.Clear();
			} else if (tokens[0] == "reset") {
				if (tokens.size() != 1) {
					std::cout << "Wrong parameters! [help]: reset" << std::endl;
					tokens.clear();
					continue;
				}

				map.Reset();
			} else if (tokens[0] == "setstart") {
				if (tokens.size() != 3) {
					std::cout << "Wrong parameters! [help]: setstart [x] [y]" << std::endl;
					tokens.clear();
					continue;
				}
				const int x = std::stoi(tokens[1]);
				const int y = std::stoi(tokens[2]);
				const int returnCode = map.SetStartingPoint(x, y);
				if (returnCode == -1) {
					std::cout << "Given position is out of range!" << std::endl;
				}
				else if (returnCode == -2) {
					std::cout << "You can't create a starting point in a wall!" << std::endl;
				}
			} else if (tokens[0] == "setdest") {
				if (tokens.size() != 3) {
					std::cout << "Wrong parameters! [help]: setdest [x] [y]" << std::endl;
					tokens.clear();
					continue;
				}
				const int x = std::stoi(tokens[1]);
				const int y = std::stoi(tokens[2]);
				const int returnCode = map.SetDestPoint(x, y);
				if (returnCode == -1) {
					std::cout << "Given position is out of range!" << std::endl;
				}
				else if (returnCode == -2) {
					std::cout << "You can't create a dest point in a wall!" << std::endl;
				}
			}
			else if (tokens[0] == "run") {
				if (tokens.size() != 1) {
					std::cout << "Wrong parameters! [help]: run" << std::endl;
					tokens.clear();
					continue;
				}
				if (!map.Ready()) {
					std::cout << "Map is not ready to run! You need to assign both start point and dest point to run!" << std::endl;
					tokens.clear();
					continue;
				}
				if (currentTask != NULL) {
					delete currentTask;
				}

				currentTask = new PathFindingTask();
				currentTask->Run(map.GetStartPos(), map.GetDestPos(), 5);
			}
			else {
				std::cout << "Unknown command: " + tokens[0] << std::endl;
			}

			tokens.clear();
		}
	}
};

int main() {
	DrawTask renderTask;
	renderTask.Launch();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	UserControlTask controlTask;
	controlTask.Launch();
	Console console;

	console.RunConsole();
}