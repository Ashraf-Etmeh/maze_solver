// #ifndef SOLVER_H
// #define SOLVER_H

// typedef enum Heading
// {
//     NORTH,
//     EAST,
//     SOUTH,
//     WEST
// } Heading;
// typedef enum Action
// {
//     LEFT,
//     FORWARD,
//     RIGHT,
//     IDLE
// } Action;

// Action solver();
// Action leftWallFollower();
// Action floodFill();

// #endif

#ifndef SOLVER_H
#define SOLVER_H

#define GRID_SIZE 16
#define QUEUE_CAPACITY (GRID_SIZE * GRID_SIZE)

#define NORTH 0
#define EAST 1
#define SOUTH 2
#define WEST 3

// Direction bitmasks
#define WALL_N 1
#define WALL_E 2
#define WALL_S 4
#define WALL_W 8

typedef enum Action
{
    LEFT,
    FORWARD,
    RIGHT,
    IDLE
} Action;

// ===== Structs (ONLY here) =====
typedef struct
{
    unsigned char walls; // bitmask of walls (N/E/S/W)
    int distance;        // flood fill distance
    int row;
    int col;
} Cell;

typedef struct Queue
{
    Cell *arr[QUEUE_CAPACITY];
    int front;
    int size;
} Queue;

// Global maze (declared here, defined in solver.c)
extern Cell maze[GRID_SIZE][GRID_SIZE];

// ===== Function prototypes =====
Action solver();
Action leftWallFollower();
void floodFill();

void initSet();
void addWall(int r, int c, int dir);
int isBlank(Cell *cell);
void resetDistances();
void setOuterWalls();

int turnLeftDir(int dir);
int turnRightDir(int dir);

Action planMove(int row, int col, int targetRow, int targetCol, int *dir);

// Queue functions
Queue *createQueue();
Cell *getRear(Queue *queue);
Cell *getFront(Queue *queue);
void enqueue(Queue *queue, Cell *cell);
Cell *dequeue(Queue *queue);
// Debugging functions
#endif
