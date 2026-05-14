#include "raylib.h"
#include <cmath>
#include <vector>
#include <queue>
#include <algorithm>

struct IVector2
{
    int x, y;

    bool operator==(const IVector2& other) const { return x == other.x && y == other.y; }
    bool operator!=(const IVector2& other) const { return !(*this == other); }

    IVector2 operator+(const IVector2& other) const { return { x + other.x, y + other.y }; }
    IVector2 operator-(const IVector2& other) const { return { x - other.x, y - other.y }; }

    operator Vector2() const { return { static_cast<float>(x), static_cast<float>(y) }; }
};

enum class TileType
{
    None = 0,
    Start,
    End,
    Obstacle,
    Unvisited,
    Open,
    Closed,
    Path
};

struct Tile
{
    TileType type;
    IVector2 position;
};

static constexpr int g_TileSize { 32 };
static constexpr int g_GridWidth { 32 };
static constexpr int g_GridHeight { 24 };
static constexpr int g_GridGap { 2 };
static Tile g_TileMap[g_GridWidth][g_GridHeight];
struct RenderCommand
{
    IVector2 position;
    TileType tileType;
};

enum class AppState
{
    Initialise,
    SelectingStart,
    SelectingEnd,
    Ready,
    Replaying,
    Finish
};

struct State
{
    IVector2 startPos;
    IVector2 targetPos;

    int replayIndex { 0 };
    double lastStepTime { 0.0 };
    double stepInterval { 0.005 };
    std::vector<RenderCommand> renderCommands;

    AppState appState { AppState::Initialise };
};

std::vector<IVector2> FindPath(IVector2 start, IVector2 target, State& state)
{
    struct Node
    {
        int cost;
        int heuristic;
        IVector2 position;

        bool operator>(const Node& other) const
        {
            if (cost == other.cost)
            {
                return heuristic > other.heuristic;
            }
            return cost > other.cost;
        }
    };

    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> frontier;
    frontier.push({ 0, 0, {start.x, start.y} });

    bool reached[g_GridWidth][g_GridHeight] = {};
    reached[start.x][start.y] = true;

    Tile* cameFrom[g_GridWidth][g_GridHeight] = {};

    int costSoFar[g_GridWidth][g_GridHeight] = {};
    costSoFar[start.x][start.y] = 0;

    auto Heuristic = [](IVector2 start, IVector2 target)
    {
        IVector2 delta { std::abs(start.x - target.x), std::abs(start.y - target.y) };
        return 10 * (delta.x + delta.y) + (-6) * std::min(delta.x, delta.y);
    };

    while (!frontier.empty())
    {
        Node current { frontier.top() };
        frontier.pop();

        RenderCommand renderCommand {};
        renderCommand.position = current.position;
        renderCommand.tileType = TileType::Closed;
        state.renderCommands.push_back(renderCommand);

        if (current.position.x == target.x &&
            current.position.y == target.y)
        {
            std::vector<IVector2> path;
            Tile* step { &g_TileMap[target.x][target.y] };

            while (step)
            {
                path.push_back(step->position);
                step = cameFrom[step->position.x][step->position.y];
            }

            std::reverse(path.begin(), path.end());
            return path;
        }

        // Offset from current node for 8 surrounding nodes
        std::vector<IVector2> delta { {0, -1}, {1, -1,}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1} };

        for (int i { 0 }; i < 8; i++)
        {
            int neighbourX { current.position.x + delta[i].x };
            int neighbourY { current.position.y + delta[i].y};

            if (neighbourX < 0 || neighbourX >= g_GridWidth ||
                neighbourY < 0 || neighbourY >= g_GridHeight)
            {
                continue;
            }

            if (delta[i].x && delta[i].y)
            {
                if (g_TileMap[current.position.x + delta[i].x][current.position.y].type == TileType::Obstacle ||
                    g_TileMap[current.position.x][current.position.y + delta[i].y].type == TileType::Obstacle)
                {
                    continue;
                }
            }

            Tile& neighbour { g_TileMap[neighbourX][neighbourY] };

            if (neighbour.type == TileType::Obstacle)
            {
                continue;
            }

            int moveCost { (delta[i].x && delta[i].y) ? 14 : 10 };
            int newCost { costSoFar[current.position.x][current.position.y] + moveCost };

            if (!reached[neighbourX][neighbourY] || newCost < costSoFar[neighbourX][neighbourY])
            {
                costSoFar[neighbourX][neighbourY] = newCost;
                int heuristic { Heuristic({ neighbourX, neighbourY }, target) };
                int cost { newCost + heuristic };
                frontier.push({ cost, heuristic, {neighbourX, neighbourY} });
                reached[neighbourX][neighbourY] = true;
                cameFrom[neighbourX][neighbourY] = &g_TileMap[current.position.x][current.position.y];

                RenderCommand renderCommand {};
                renderCommand.position = { neighbourX, neighbourY };
                renderCommand.tileType = TileType::Open;
                state.renderCommands.push_back(renderCommand);
            }
        }
    }

    return{};
}

int main()
{
    // Initialise
    const int screenWidth { g_GridWidth * g_TileSize };
    const int screenHeight { g_GridHeight * g_TileSize };

    InitWindow(screenWidth, screenHeight, "A* Visualiser | Start/End: Left Click | Obstacle: Hold F | Run: Space");
    SetTargetFPS(60);

    State state;

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(WHITE);

        if (state.appState == AppState::SelectingStart || 
            state.appState == AppState::SelectingEnd || 
            state.appState == AppState::Ready)
        {
            if (IsKeyDown(KEY_F))
            {
                Vector2 mousePosition { GetMousePosition() };
                IVector2 tilePosition { static_cast<int>(mousePosition.x / g_TileSize), static_cast<int>(mousePosition.y / g_TileSize) };

                if (tilePosition.x >= 0 && tilePosition.x < g_GridWidth && 
                    tilePosition.y >= 0 && tilePosition.y < g_GridHeight)
                {
                    if (g_TileMap[tilePosition.x][tilePosition.y].type != TileType::Start &&
                        g_TileMap[tilePosition.x][tilePosition.y].type != TileType::End)
                    {
                        g_TileMap[tilePosition.x][tilePosition.y].type = TileType::Obstacle;
                    }
                }
            }
        }

        switch (state.appState)
        {
        case AppState::Initialise:
        {
            // Initialise grid
            for (int y { 0 }; y < g_GridHeight; y++)
            {
                for (int x { 0 }; x < g_GridWidth; x++)
                {
                    g_TileMap[x][y].type = TileType::Unvisited;
                    g_TileMap[x][y].position = { x, y };
                }
            }

            state.renderCommands.clear();
            state.appState = AppState::SelectingStart;
        }
        break;

        case AppState::SelectingStart:
        {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                Vector2 mousePosition { GetMousePosition() };
                IVector2 tilePosition { static_cast<int>(mousePosition.x / g_TileSize), static_cast<int>(mousePosition.y / g_TileSize) };

                if (tilePosition.x >= 0 && tilePosition.x < g_GridWidth && 
                    tilePosition.y >= 0 && tilePosition.y < g_GridHeight)
                {
                    if (g_TileMap[tilePosition.x][tilePosition.y].type != TileType::Obstacle)
                    {
                        state.startPos = tilePosition;
                        g_TileMap[tilePosition.x][tilePosition.y].type = TileType::Start;
                        state.appState = AppState::SelectingEnd;
                    }
                }
            }
        }
        break;

        case AppState::SelectingEnd:
        {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                Vector2 mousePosition { GetMousePosition() };
                IVector2 tilePosition { static_cast<int>(mousePosition.x / g_TileSize), static_cast<int>(mousePosition.y / g_TileSize) };

                if (tilePosition.x >= 0 && tilePosition.x < g_GridWidth && 
                    tilePosition.y >= 0 && tilePosition.y < g_GridHeight)
                {
                    if (g_TileMap[tilePosition.x][tilePosition.y].type != TileType::Obstacle &&
                        g_TileMap[tilePosition.x][tilePosition.y].type != TileType::Start)
                    {
                        state.targetPos = tilePosition;
                        g_TileMap[tilePosition.x][tilePosition.y].type = TileType::End;
                        state.appState = AppState::Ready;
                    }
                }
            }
        }
        break;

        case AppState::Ready:
        {
            if (IsKeyPressed(KEY_SPACE))
            {
                std::vector<IVector2> path { FindPath(state.startPos, state.targetPos, state) };

                for (const auto& tilePosition : path)
                {
                    if (g_TileMap[tilePosition.x][tilePosition.y].type == TileType::Start || 
                        g_TileMap[tilePosition.x][tilePosition.y].type == TileType::End)
                    {
                        continue;
                    }
                    state.renderCommands.push_back({ tilePosition, TileType::Path });
                }

                state.replayIndex = 0;
                state.lastStepTime = GetTime();
                state.appState = AppState::Replaying;
            }
        }
        break;

        case AppState::Replaying:
        {
            if (state.replayIndex < static_cast<int>(state.renderCommands.size()))
            {
                if (GetTime() - state.lastStepTime >= state.stepInterval)
                {
                    state.lastStepTime = GetTime();
                    const RenderCommand command { state.renderCommands[state.replayIndex++] };

                    TileType current { g_TileMap[command.position.x][command.position.y].type };
                    if (current != TileType::Start && current != TileType::End)
                    {
                        g_TileMap[command.position.x][command.position.y].type = command.tileType;
                    }
                }
            }
            else
            {
                state.appState = AppState::Finish;
            }
        }
        break;

        case AppState::Finish:
        {
            if (IsKeyPressed(KEY_SPACE))
            {
                state.appState = AppState::Initialise;
            }
        }
        break;
        }


        // Draw
        {
            for (int y { 0 }; y < g_GridHeight; y++)
            {
                for (int x { 0 }; x < g_GridWidth; x++)
                {
                    Color colour { MAGENTA };
                    Tile tile { g_TileMap[x][y] };
                    switch (tile.type)
                    {
                    case TileType::Start:
                    {
                        colour = DARKGREEN;
                    }
                    break;

                    case TileType::End:
                    {
                        colour = DARKBLUE;
                    }
                    break;

                    break;

                    case TileType::Unvisited:
                    {
                        colour = GRAY;
                    }
                    break;

                    case TileType::Open:
                    {
                        colour = GREEN;
                    }
                    break;

                    case TileType::Closed:
                    {
                        colour = SKYBLUE;
                    }
                    break;

                    case TileType::Obstacle:
                    {
                        colour = BLACK;
                    }
                    break;

                    case TileType::Path:
                    {
                        colour = YELLOW;
                    }
                    break;

                    }
                    DrawRectangle(x * g_TileSize + g_GridGap / 2, y * g_TileSize + g_GridGap / 2, g_TileSize - g_GridGap, g_TileSize - g_GridGap, colour);
                }
            }
        }

        EndDrawing();
    }
        CloseWindow();
        return 0;
    
}
