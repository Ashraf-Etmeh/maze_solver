#include "solver.h"
#include "API.h"
#include <stdio.h>
#include <stdlib.h>

#define GRID_SIZE 16
#define QUEUE_CAPACITY (GRID_SIZE * GRID_SIZE)

// the grid
#define D_NORTH 1 // 0001
#define D_EAST 2  // 0010
#define D_SOUTH 4 // 0100
#define D_WEST 8  // 1000

// Movement
int dRow[4] = {-1, 0, 1, 0}; // D_NORTH, D_EAST, D_SOUTH, D_WEST
int dCol[4] = {0, 1, 0, -1};
int dirMask[4] = {D_NORTH, D_EAST, D_SOUTH, D_WEST};

// typedef struct
// {
//     unsigned char walls; // bitmask: 1=wall present
//     int distance;        // Manhattan distance to goal
//     int row;
//     int col; // column
// } Cell;

// // queue
// typedef struct Queue
// {
//     Cell *arr[QUEUE_CAPACITY]; // store Cell pointers
//     int front;
//     int size;
// } Queue;

Cell maze[GRID_SIZE][GRID_SIZE];

// some helper functions
int isAccessible(int r1, int c1, int r2, int c2)
{
    if (r2 == r1 - 1 && c2 == c1)
    { // D_NORTH
        return !(maze[r1][c1].walls & D_NORTH) && !(maze[r2][c2].walls & D_SOUTH);
    }
    else if (r2 == r1 + 1 && c2 == c1)
    { // D_SOUTH
        return !(maze[r1][c1].walls & D_SOUTH) && !(maze[r2][c2].walls & D_NORTH);
    }
    else if (r2 == r1 && c2 == c1 - 1)
    { // D_WEST
        return !(maze[r1][c1].walls & D_WEST) && !(maze[r2][c2].walls & D_EAST);
    }
    else if (r2 == r1 && c2 == c1 + 1)
    { // D_EAST
        return !(maze[r1][c1].walls & D_EAST) && !(maze[r2][c2].walls & D_WEST);
    }
    return 0; // Not a neighbor
}

int isBlank(Cell *cell)
{
    return cell->distance == -1; // Assuming -1 indicates blank
}

void resetDistances()
{
    for (int r = 0; r < GRID_SIZE; r++)
    {
        for (int c = 0; c < GRID_SIZE; c++)
        {
            maze[r][c].distance = -1;
        }
    }
}

void setOuterWalls()
{
    for (int i = 0; i < GRID_SIZE; i++)
    {
        // Top row → D_NORTH walls
        maze[0][i].walls = 1;

        // Bottom row → D_SOUTH walls
        maze[GRID_SIZE - 1][i].walls |= 4;

        // Left column → D_WEST walls
        maze[i][0].walls |= 8;

        // Right column → D_EAST walls
        maze[i][GRID_SIZE - 1].walls |= 2;
    }
}

void initSet()
{
    for (int r = 0; r < GRID_SIZE; r++)
    {
        for (int c = 0; c < GRID_SIZE; c++)
        {
            maze[r][c].walls = 0; // No walls known
            maze[r][c].distance = -1;
            maze[r][c].row = r;
            maze[r][c].col = c;
        }
    }

    // set all boundaries and the start default to walls
    setOuterWalls();
    maze[GRID_SIZE - 1][0].walls = 7; // n e s w 0111
}

void addWall(int r, int c, int dir)
{
    maze[r][c].walls |= dir;

    // Update the neighbor in the opposite direction
    int nr = r, nc = c;
    if (dir == D_NORTH)
        nr--;
    if (dir == D_SOUTH)
        nr++;
    if (dir == D_WEST)
        nc--;
    if (dir == D_EAST)
        nc++;

    if (nr >= 0 && nr < GRID_SIZE && nc >= 0 && nc < GRID_SIZE)
    {
        if (dir == D_NORTH)
            maze[nr][nc].walls |= D_SOUTH;
        if (dir == D_SOUTH)
            maze[nr][nc].walls |= D_NORTH;
        if (dir == D_WEST)
            maze[nr][nc].walls |= D_EAST;
        if (dir == D_EAST)
            maze[nr][nc].walls |= D_WEST;
    }
}

int turnLeftDir(int dir)
{
    if (dir == D_NORTH)
        return D_WEST;
    if (dir == D_WEST)
        return D_SOUTH;
    if (dir == D_SOUTH)
        return D_EAST;
    if (dir == D_EAST)
        return D_NORTH;
    return dir; // should not happen
}

int turnRightDir(int dir)
{
    if (dir == D_NORTH)
        return D_EAST;
    if (dir == D_EAST)
        return D_SOUTH;
    if (dir == D_SOUTH)
        return D_WEST;
    if (dir == D_WEST)
        return D_NORTH;
    return dir; // should not happen
}

// planMove: decide how to move from (row,col) facing *dir* towards (targetRow,targetCol).
// dir is passed by pointer so we can update it if needed.
Action planMove(int row, int col, int targetRow, int targetCol, int *dir)
{
    // Determine which direction target cell lies in
    int desiredDir;

    if (targetRow == row - 1 && targetCol == col)
        desiredDir = D_NORTH;
    else if (targetRow == row + 1 && targetCol == col)
        desiredDir = D_SOUTH;
    else if (targetRow == row && targetCol == col + 1)
        desiredDir = D_EAST;
    else if (targetRow == row && targetCol == col - 1)
        desiredDir = D_WEST;
    else
        return IDLE; // Not a valid neighbor

    // If already facing the right direction → move forward
    if (*dir == desiredDir)
        return FORWARD;

    // Check if turning left gets us there
    if (turnLeftDir(*dir) == desiredDir)
        return LEFT;

    // Check if turning right gets us there
    if (turnRightDir(*dir) == desiredDir)
        return RIGHT;

    // Otherwise, it's the opposite direction → turn left twice
    // You could add a special case here for "turn around"
    return LEFT; // caller logic already handles turning + forwardNext
}

Queue *createQueue()
{
    Queue *queue = (Queue *)malloc(sizeof(Queue));
    queue->front = 0;
    queue->size = 0;
    return queue;
}

Cell *getRear(Queue *queue)
{
    if (queue->size == 0)
    {
        return NULL; // Queue is empty
    }
    int rear = (queue->front + queue->size - 1) % QUEUE_CAPACITY;
    return queue->arr[rear];
}

Cell *getFront(Queue *queue)
{
    if (queue->size == 0)
    {
        return NULL; // Queue is empty
    }
    return queue->arr[queue->front];
}

void enqueue(Queue *queue, Cell *cell)
{
    if (queue->size == QUEUE_CAPACITY)
        return;
    int rear = (queue->front + queue->size) % QUEUE_CAPACITY;
    queue->arr[rear] = cell;
    queue->size++;
}

Cell *dequeue(Queue *queue)
{
    if (queue->size == 0)
        return NULL; // Queue is empty
    Cell *res = queue->arr[queue->front];
    queue->front = (queue->front + 1) % QUEUE_CAPACITY;
    queue->size--;
    return res;
}

// detect walls → update maze → re-flood → pick loD_WEST-distance neighbor → move

Action solver()
{
    // return leftWallFollower();
    static int initialized = 0;
    static int forwardNext = 0; // to handle the case of turning then moving forward

    if (forwardNext)
    {
        forwardNext = 0;
        return FORWARD;
    }

    // starting row and column & direction:
    static int row = GRID_SIZE - 1;
    static int col = 0;
    static int dir = D_NORTH; // start facing D_NORTH

    if (!initialized)
    {
        // 1-> Set all cells except goal to “blank state”:
        initSet();
        floodFill();
        initialized = 1;
    }

    // Check walls around and update maze
    if (API_wallFront())
    {
        addWall(row, col, dir);
    }

    if (API_wallLeft())
    {
        addWall(row, col, turnLeftDir(dir));
    }

    if (API_wallRight())
    {
        addWall(row, col, turnRightDir(dir));
    }

    // If walls changed -> reflood
    floodFill();

    // Choose next move = neighbor with lowest distance
    int bestRow = row, bestCol = col;
    int bestDist = maze[row][col].distance;

    for (int i = 0; i < 4; i++)
    {
        int nx = row + dRow[i];
        int ny = col + dCol[i];

        // Check bounds
        if (nx < 0 || nx >= GRID_SIZE || ny < 0 || ny >= GRID_SIZE)
            continue;

        // Check accessible
        if (isAccessible(row, col, nx, ny))
        {
            if (maze[nx][ny].distance < bestDist)
            {
                bestDist = maze[nx][ny].distance;
                bestRow = nx;
                bestCol = ny;
            }
        }
    }

    // now  plan the move to (bestRow, bestCol)
    Action act = planMove(row, col, bestRow, bestCol, &dir);

    // update our internal state after movement
    if (act == FORWARD)
    {
        row = bestRow;
        col = bestCol;
    }
    else if (act == LEFT)
    {
        dir = turnLeftDir(dir);
        forwardNext = 1;
    }
    else if (act == RIGHT)
    {
        dir = turnRightDir(dir);
        forwardNext = 1;
    }

    return act;
}

// This is an example of a simple left wall following algorithm.
Action leftWallFollower()
{
    if (API_wallFront())
    {
        if (API_wallLeft())
        {
            return RIGHT;
        }
        return LEFT;
    }
    return FORWARD;
}

// Put your implementation of floodfill here!
void floodFill()
{
    resetDistances();

    // Set goal cell(s) value to 0 and add to queue:
    int goal = GRID_SIZE / 2;
    maze[goal][goal].distance = 0;
    maze[goal - 1][goal].distance = 0;
    maze[goal][goal - 1].distance = 0;
    maze[goal - 1][goal - 1].distance = 0;
    Queue *queue = createQueue();
    enqueue(queue, &maze[goal][goal]); // Add goal cell's info to queue
    enqueue(queue, &maze[goal - 1][goal]);
    enqueue(queue, &maze[goal][goal - 1]);
    enqueue(queue, &maze[goal - 1][goal - 1]);

    // While queue is not empty:
    while (queue->size > 0)
    {
        // i- Take front cell in queue “out of line” for consideration:
        Cell *current = dequeue(queue);
        // ii- Set all blank and accessible neighbors to front cell’s value + 1:
        for (int i = 0; i < 4; i++)
        {
            int nx = current->row + dRow[i]; // neighbor row
            int ny = current->col + dCol[i]; // neighbor col

            // Check bounds
            if (nx < 0 || nx >= GRID_SIZE || ny < 0 || ny >= GRID_SIZE)
                continue;

            // Check accessible and blank
            if (isAccessible(current->row, current->col, nx, ny) &&
                isBlank(&maze[nx][ny]))
            {

                maze[nx][ny].distance = current->distance + 1;

                // Add neighbor to queue
                enqueue(queue, &maze[nx][ny]);
            }
        }
    } // iv- Else, continue!:

    free(queue);

    // return IDLE;
}

// simple test for queue functionality
// int main()
// {
//     Queue *q = createQueue();
//     enqueue(q, 10);
//     printf("%d %d\n", getFront(q), getRear(q));
//     enqueue(q, 20);
//     printf("%d %d\n", getFront(q), getRear(q));
//     enqueue(q, 30);
//     printf("%d %d\n", getFront(q), getRear(q));
//     enqueue(q, 40);
//     printf("%d %d\n", getFront(q), getRear(q));
//     dequeue(q);
//     printf("%d %d\n", getFront(q), getRear(q));
//     dequeue(q);
//     printf("%d %d\n", getFront(q), getRear(q));
//     enqueue(q, 50);
//     printf("%d %d\n", getFront(q), getRear(q));
//     free(q->arr);
//     free(q);
//     return 0;
// }