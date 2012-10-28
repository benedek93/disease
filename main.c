#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <SDL.h>

#define NUM_STATES 4
enum { WALL, SUSCEPTIBLE, INFECTED, REMOVED };

unsigned int grid_w = 80;
unsigned int grid_h = 80;
unsigned int cell_size = 4;
unsigned int delay = 60;
float p_infect = 0.2;
float p_remove = 0.2;
float p_reinsert = 0.001;

int *old_grid;
int *new_grid;

void render(SDL_Surface *screen, int dirty);
void step();
int decide(float prob);
void swap_grids() {
    int *tmp_grid = old_grid;
    old_grid = new_grid;
    new_grid = tmp_grid;
}
void print_help();

int main(int argc, char *argv[]) {
    int rc;
    SDL_Surface *screen = NULL;
    int i, j;
    int running = 1;
    SDL_Event event;
    int cursor_state = INFECTED;
    int opt;
    extern char *optarg;

    /* get command-line options: */
    while((opt = getopt(argc, argv, "hg:c:d:p:")) != -1) {
        switch(opt) {
        case 'h':
            print_help();
            return 0;
        case 'g':
            sscanf(optarg, "%ux%u", &grid_w, &grid_h);
            if(grid_w < 3 || grid_h < 3) {
                printf("Grid resolution must be at least 3x3!\n");
                return 1;
            }
            break;
        case 'c':
            sscanf(optarg, "%u", &cell_size);
            if(cell_size < 1) {
                printf("Cell size must be at least 1!\n");
                return 1;
            }
            break;
        case 'd':
            sscanf(optarg, "%u", &delay);
            break;
        case 'p':
            sscanf(optarg, "%f,%f,%f", &p_infect, &p_remove, &p_reinsert);
            break;
        }
    }

    /* seed random generator: */
    srand(time(NULL));

    /* initialize SDL: */
    rc = SDL_Init(SDL_INIT_VIDEO);
    assert(rc != -1);
    SDL_WM_SetCaption("Disease", NULL);
    screen = SDL_SetVideoMode(grid_w*cell_size, grid_h*cell_size, 0,
                              SDL_SWSURFACE);
    assert(screen != NULL);

    /* initialize grid (everything susceptible, except wall border): */
    old_grid = (int *) malloc(grid_w * grid_h * sizeof(int));
    assert(old_grid != NULL);
    new_grid = (int *) malloc(grid_w * grid_h * sizeof(int));
    assert(new_grid != NULL);
    for(j=0; j<grid_h; ++j) {
        for(i=0; i<grid_w; ++i) {
            const int idx = j * grid_w + i;
            if(i == 0 || j == 0 || i == grid_w-1 || j == grid_h-1) {
                old_grid[idx] = WALL;
            } else {
                old_grid[idx] = SUSCEPTIBLE;
            }
        }
    }

    memcpy(new_grid, old_grid, grid_w * grid_h * sizeof(int));

    render(screen, 1);

    /* main loop: */
    while(running) {
        step();
        swap_grids();

        /* handle events: */
        while(SDL_PollEvent(&event)) {
            switch(event.type) {
            case SDL_MOUSEBUTTONDOWN:
                if(event.button.state == SDL_PRESSED) {
                    /* infect clicked cell: */
                    i = event.button.x / cell_size;
                    j = event.button.y / cell_size;
                    if(i > 0 && j > 0 && i < grid_w-1 && j < grid_h-1) {
                        old_grid[i + j*grid_w] = cursor_state;
                    }
                }
                break;
            case SDL_KEYDOWN:
                switch(event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    running = 0;
                    break;
                default:
                    break;
                }
                break;
            case SDL_QUIT:
                running = 0;
                break;
            default:
                break;
            }
        }

        SDL_Delay(delay);

        render(screen, 0);
    }

    return 0;
}

void render(SDL_Surface *screen, int dirty) {
    int i, j;
    SDL_Rect dst;
    dst.w = cell_size;
    dst.h = cell_size;
    Uint32 cols[NUM_STATES];

    /* choose colors: */
    cols[0] = SDL_MapRGB(screen->format, 0, 0, 0);
    cols[1] = SDL_MapRGB(screen->format, 0, 80, 80);
    cols[2] = SDL_MapRGB(screen->format, 255, 128, 0);
    cols[3] = SDL_MapRGB(screen->format, 64, 0, 0);

    /* draw grid: */
    for(j=0; j<grid_h; ++j) {
        for(i=0; i<grid_w; ++i) {
            const int grid_idx = i + j * grid_w;

            if(!dirty && old_grid[grid_idx] == new_grid[grid_idx]) continue;

            dst.x = i * cell_size;
            dst.y = j * cell_size;

            SDL_FillRect(screen, &dst, cols[old_grid[grid_idx]]);
        }
    }

    SDL_Flip(screen);
}

void step() {
    int i, j;

    for(j=1; j<grid_h-1; ++j) {
        for(i=1; i<grid_w-1; ++i) {
            int * const old_grid_ptr = old_grid + i + j * grid_w;
            int * const new_grid_ptr = new_grid + i + j * grid_w;
            int neighbors[NUM_STATES] = {0, 0, 0};

            /* count neighbors, each state separately: */
            ++neighbors[*(old_grid_ptr-1)];
            ++neighbors[*(old_grid_ptr+1)];
            ++neighbors[*(old_grid_ptr-grid_w)];
            ++neighbors[*(old_grid_ptr+grid_w)];

            *new_grid_ptr = *old_grid_ptr;

            /* decide state transition randomly: */
            switch(*old_grid_ptr) {
            case SUSCEPTIBLE:
                if(decide(p_infect * neighbors[INFECTED])) {
                    *new_grid_ptr = INFECTED;
                }
                break;
            case INFECTED:
                if(decide(p_remove)) {
                    *new_grid_ptr = REMOVED;
                }
                break;
            case REMOVED:
                if(decide(p_reinsert * neighbors[SUSCEPTIBLE])) {
                    *new_grid_ptr = SUSCEPTIBLE;
                }
                break;
            default:
                break;
            }
        }
    }
}

/* returns 1 with probability prob, 0 otherwise: */
int decide(float prob) {
    return ((float)rand())/RAND_MAX < prob;
}

void print_help() {
    printf(
      "Usage: disease [options]\n"
      "Options:\n"
      " -h             Print help\n"
      " -g <width>x<height>\n"
      "                Grid resolution (default: 80x80)\n"
      " -c <size>      Graphical Cell size (default: 4)\n"
      " -d <delay>     Pause between steps, in ms (default: 60)\n"
      " -p <infect>,<remove>,<reinsert>\n"
      "                Set probabilities for infection, removal (when infected)\n"
      "                and reinsertion (when removed) (default: 0.2,0.2,0.001).\n"
      "                State transitions also depend on the numbers of neighbors\n"
      "                in the different states.\n"
     );
}
