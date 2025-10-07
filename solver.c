#include "solver.h"
#include "API.h"
#include <stdio.h>
#include <stdlib.h>

#define GRID_SIZE 16
#define QUEUE_CAPACITY (GRID_SIZE * GRID_SIZE)
// add heading n e s w as 0 1 2 3
#define NORTH 0
#define EAST 1
#define SOUTH 2
#define WEST 3

// the grid
#define WALL_N 1 // 0001
#define WALL_E 2 // 0010
#define WALL_S 4 // 0100
#define WALL_W 8 // 1000

// a small state machine to handle multi-turn moves:
// --- State for multi-turn handling (replace the previous forwardNext static) ---
// pendingTurns = how many 90-degree turns still need to be executed before the final FORWARD
static int pendingTurns = 0;
// pendingTurnIsLeft: 1 => turn left each pending turn, 0 => turn right each pending turn
static int pendingTurnIsLeft = 1;
// once pendingTurns reaches 0 we set forwardNext=1 so next call does forward
static int forwardNext = 0;

// Movement
int dRow[4] = {-1, 0, 1, 0}; // WALL_N, WALL_E, WALL_S, WALL_W
int dCol[4] = {0, 1, 0, -1};
int dirMask[4] = {WALL_N, WALL_E, WALL_S, WALL_W}; // 0:N, 1:E, 2:S, 3:W bitmasks

Cell maze[GRID_SIZE][GRID_SIZE];

// some helper functions
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

void setOuterWalls() /// if you reverse the array to standard like it need to modifiy here
{
    debug_log("Setting outer walls...\n");
    for (int i = 0; i < GRID_SIZE; i++)
    {
        // Top row → WALL_N walls
        maze[0][i].walls |= WALL_N;

        // Bottom row → WALL_S walls
        maze[GRID_SIZE - 1][i].walls |= WALL_S;

        // Left column → WALL_W walls
        maze[i][0].walls |= WALL_W;

        // Right column → WALL_E walls
        maze[i][GRID_SIZE - 1].walls |= WALL_E;
    }
    debug_log("Outer walls completed\n");
}

void initSet()
{
    debug_log("Starting initSet()...\n");
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
    debug_log("Grid initialized, setting outer walls...\n");

    // set all boundaries and the start default to walls
    setOuterWalls();
    debug_log("Outer walls set, setting start position walls...\n");

    debug_log("initSet() completed\n");
}

void addWall(int r, int c, int dir)
{
    unsigned int walls = 0000;
    if (dir == NORTH)
        walls |= WALL_N;
    else if (dir == EAST)
        walls |= WALL_E;
    else if (dir == SOUTH)
        walls |= WALL_S;
    else if (dir == WEST)
        walls |= WALL_W;
    else
    {
        debug_log("Error in addWall: invalid direction");
    }

    maze[r][c].walls |= walls;

    // Update the neighbor in the opposite direction
    int nr = r, nc = c;
    if (dir == NORTH)
        nr--;
    if (dir == SOUTH)
        nr++;
    if (dir == WEST)
        nc--;
    if (dir == EAST)
        nc++;

    if (nr >= 0 && nr < GRID_SIZE && nc >= 0 && nc < GRID_SIZE)
    {
        if (dir == NORTH)
            maze[nr][nc].walls |= WALL_S;
        if (dir == SOUTH)
            maze[nr][nc].walls |= WALL_N;
        if (dir == WEST)
            maze[nr][nc].walls |= WALL_E;
        if (dir == EAST)
            maze[nr][nc].walls |= WALL_W;
    }

    if (maze[r][c].walls & WALL_N)
        API_setWall(c, r, 'n');
    if (maze[r][c].walls & WALL_E)
        API_setWall(c, r, 'e');
    if (maze[r][c].walls & WALL_S)
        API_setWall(c, r, 's');
    if (maze[r][c].walls & WALL_W)
        API_setWall(c, r, 'w');
}

// heading missing with wall directions dir
int turnLeftDir(int heading)
{
    if (heading == NORTH)
        return WEST;
    if (heading == WEST)
        return SOUTH;
    if (heading == SOUTH)
        return EAST;
    if (heading == EAST)
        return NORTH;

    debug_log("Error in turnLeftDir: invalid direction");
    return heading; // should not happen
}

int turnRightDir(int heading)
{
    if (heading == NORTH)
        return EAST;
    if (heading == EAST)
        return SOUTH;
    if (heading == SOUTH)
        return WEST;
    if (heading == WEST)
        return NORTH;

    debug_log("Error in turnRightDir: invalid direction");
    return heading; // should not happen
}

// planMove: decide how to move from (row,col) facing *dir* towards (targetRow,targetCol).
// dir is passed by pointer so we can update it if needed.
Action planMove(int row, int col, int targetRow, int targetCol, int *heading)
{
    // Determine which direction target cell lies in
    // heading missing with dir walls
    int desiredHeading;

    if (targetRow == row - 1 && targetCol == col) // move upward
        desiredHeading = NORTH;
    else if (targetRow == row + 1 && targetCol == col) // move downward
        desiredHeading = SOUTH;
    else if (targetRow == row && targetCol == col + 1) // move right
        desiredHeading = EAST;
    else if (targetRow == row && targetCol == col - 1) // move left
        desiredHeading = WEST;
    else
    {
        // here the debug_log doesn't do anything because it is a simulation so I replaced it with a debug_log down here
        // char buf[128];
        // debug_log(buf, sizeof(buf),
        //           "DEBUG: row=%d col=%d heading=%d targetRow=%d targetCol=%d",
        //           row, col, *heading, targetRow, targetCol);
        // debug_log(buf);

        debug_log("return next move IDLE");
        return IDLE; // Not a valid neighbor
    }

    // If already facing the right direction → move forward
    if (*heading == desiredHeading)
        return FORWARD;

    // Check if turning left gets us there
    else if (turnLeftDir(*heading) == desiredHeading)
        return LEFT;

    // Check if turning right gets us there
    else if (turnRightDir(*heading) == desiredHeading)
        return RIGHT;
    // else if (turnLeftDir(turnLeftDir(*heading)) == desiredHeading) // untageld
    //     return pending = 2;
    else
    {
        // Otherwise it's the opposite direction.
        // We'll plan two left turns (could pick right; they both take 2 turns).
        // Set pendingTurns so solver will perform two turns across successive solver() calls,
        // then set forwardNext so the following call will move forward.
        pendingTurns = 2;
        pendingTurnIsLeft = 1; // use left turns for the 180°
        // Return one left now — solver() will execute one left turn immediately.
        return LEFT;
    }
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

// debuging functions

// detect walls → update maze → re-flood → pick loWALL_W-distance neighbor → move

static int initialized = 0;

Action solver()
{
    // starting row and column & direction:
    // static as local for just inside this function
    // initialize only once
    static int row = GRID_SIZE - 1;
    static int col = 0;
    // Heading missid with wall directions dir
    static int heading = NORTH; // start facing North

    // for checking:
    // return leftWallFollower();

    // if we should immediately move forward (after finishing turns)
    if (forwardNext)
    {
        forwardNext = 0;
        debug_log("Executing pending FORWARD\n");

        // advance position according to heading
        row += dRow[heading];
        col += dCol[heading];

        return FORWARD;
    }

    // If there are pending turns, perform one turn now (do not prematurely FORWARD)
    if (pendingTurns > 0)
    {
        if (pendingTurnIsLeft)
        {
            // perform one left turn
            pendingTurns--;
            // update our internal direction to reflect the turn
            // NOTE: dir must be accessible; we'll declare dir static below if not already
            // We'll update dir here (see static dir declaration below)
            // Return LEFT action so caller will execute one 90° left turn physically.
            if (pendingTurns == 0)
                forwardNext = 1; // after this turn sequence, next call should go forward
            heading = turnLeftDir(heading);
            return LEFT;
        }
        else
        {
            pendingTurns--;
            if (pendingTurns == 0)
                forwardNext = 1;
            heading = turnRightDir(heading);
            return RIGHT;
        }
    }

    if (!initialized)
    {
        // 1-> Set all cells except goal to “blank state”:
        initSet();
        floodFill();
        initialized = 1;
        debug_log("Init...");
    }

    int wallsChanged = 0;

    // Check walls around and update maze
    if (API_wallFront())
    {
        addWall(row, col, heading);
        debug_log("wallFront...");
        wallsChanged = 1;
    }

    if (API_wallLeft())
    {
        addWall(row, col, turnLeftDir(heading));
        debug_log("wall left...");
        wallsChanged = 1;
    }

    if (API_wallRight())
    {
        addWall(row, col, turnRightDir(heading));
        debug_log("wall right...");
        wallsChanged = 1;
    }

    // If walls changed -> reflood
    if (wallsChanged)
    {
        floodFill();
        debug_log("wall changed -> reFlooded...");
    }

    // ADD DEBUG OUTPUT HERE

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
        int opposite = (i + 2) % 4;
        if ((maze[row][col].walls & dirMask[i]) ||
            (maze[nx][ny].walls & dirMask[opposite]))
        {
            debug_log("NOT accessible\n");
            continue;
        }
        else
        {
            if (maze[nx][ny].distance < bestDist)
            {
                bestDist = maze[nx][ny].distance;
                bestRow = nx;
                bestCol = ny;
                debug_log("found next cell...");
            }
            debug_log("\n");
        }
    }

    // now  plan the move to (bestRow, bestCol)
    Action act = planMove(row, col, bestRow, bestCol, &heading);
    debug_log("paln next move...");
    debug_log("Action decided is: ");

    if (act == FORWARD)
        debug_log("FORWARD\n");
    else if (act == LEFT)
        debug_log("LEFT\n");
    else if (act == RIGHT)
        debug_log("RIGHT\n");
    else
        debug_log("IDLE\n");

    // update our internal state after movement
    if (act == FORWARD)
    {
        row = bestRow;
        col = bestCol;
    }
    else if (act == LEFT)
    {
        // perform single left turn now (the physical turn will be executed by caller)

        heading = turnLeftDir(heading);
    }
    else if (act == RIGHT)
    {
        heading = turnRightDir(heading);
    }
    else
    {
        debug_log("Action: IDLE - This shouldn't happen!\n");
    }
    debug_log("update state ,,...");

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
    debug_log("Starting floodFill()...\n"); // form # here is correct to
    // reset only distances, keep walls
    resetDistances();
    debug_log("Distances reset\n");
    Queue *queue = createQueue(); // to # here

    // Set goal cell(s) value to 0 and add to queue:
    int goal = GRID_SIZE / 2;

    maze[goal][goal].distance = 0;
    maze[goal - 1][goal].distance = 0;
    maze[goal][goal - 1].distance = 0;
    maze[goal - 1][goal - 1].distance = 0;
    debug_log("Goal distances set to 0\n");

    if (!queue)
    {
        debug_log("ERROR: Failed to create queue!\n");
        return;
    }
    debug_log("Queue created\n");

    enqueue(queue, &maze[goal][goal]); // Add goal cell's info to queue
    enqueue(queue, &maze[goal - 1][goal]);
    enqueue(queue, &maze[goal][goal - 1]);
    enqueue(queue, &maze[goal - 1][goal - 1]);

    int iterations = 0;
    // While queue is not empty:
    while (queue->size > 0)
    {
        iterations++;
        if (iterations > GRID_SIZE * GRID_SIZE * 2)
        {
            debug_log("ERROR: Too many iterations in floodFill! Breaking to avoid infinite loop.\n");
            break;
        }

        if (iterations % 50 == 0)
        {
            debug_log("floodFill iteration  mod50 == 0...\n");
        }

        // i- Take front cell in queue “out of line” for consideration:
        Cell *current = dequeue(queue);
        if (!current)
        {
            debug_log("ERROR: dequeue returned NULL!\n");
            break;
        }

        API_clearText(current->row, current->col); // clear previous text

        // ii- Set all blank and accessible neighbors to front cell’s value + 1:
        for (int i = 0; i < 4; i++)
        {
            int nr = current->row + dRow[i]; // neighbor row
            int nc = current->col + dCol[i]; // neighbor col

            // Check bounds
            if (nr < 0 || nr >= GRID_SIZE || nc < 0 || nc >= GRID_SIZE)
                continue;

            // Check accessible and blank
            int opposite = (i + 2) % 4; // opposite direction index
            if ((current->walls & dirMask[i]) ||
                (maze[nr][nc].walls & dirMask[opposite]))
            {
                continue; // wall blocks movement
            }

            // check if nieghbor is blank (unvisited)
            if (isBlank(&maze[nr][nc]))
            {
                maze[nr][nc].distance = current->distance + 1;
                API_setText(nr, nc, maze[nr][nc].distance); // for debugging in simulator
                // Add neighbor to queue
                enqueue(queue, &maze[nr][nc]);
            }
        }
    } // iv- Else, continue!:

    free(queue);

    // return IDLE;
}