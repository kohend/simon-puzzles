/*
 * mosaic.c: A puzzle based on a square grid, with some of the tiles
 * having clues as to how many black squares are around them.
 * the purpose of the game is to find what should be on all tiles (black or unmarked)
 * 
 * The game is also known as: ArtMosaico, Count and Darken, Cuenta Y Sombrea, Fill-a-Pix,
 * Fill-In, Komsu Karala, Magipic, Majipiku, Mosaico, Mosaik, Mozaiek, Nampre Puzzle, Nurie-Puzzle, Oekaki-Pix, Voisimage.
 * 
 * Implementation is loosely based on https://github.com/mordechaim/Mosaic, UI interaction is based on
 * the range puzzle in the collection.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>

#include "puzzles.h"

#define DEFAULT_SIZE 5
#define DEFAULT_AGGRESSIVENESS 10
#define SOLVE_MAX_ITERATIONS 200
#define DEBUG_IMAGE 1
#undef DEBUG_IMAGE

/* Getting the coordinates and returning NULL when out of scope 
 * The absurd amount of parentesis is needed to avoid order of operations issues */
#define get_cords(params, array, x, y) (((x) >= 0 && (y) >= 0) && ((x)< params->width && (y)<params->height))? \
     array + ((y)*params->width)+x : NULL;

enum {
    COL_BACKGROUND = 0,
    COL_GRID,
    COL_MARKED = COL_GRID,
    COL_BLANK,
    COL_ERROR,
    COL_LOWLIGHT,
    COL_HIGHLIGHT = COL_ERROR, /* mkhighlight needs it, I don't */
    COL_CURSOR = COL_LOWLIGHT,
    NCOLOURS
};

enum cell_state {
    STATE_UNMARKED = 0,
    STATE_MARKED,
    STATE_BLANK,
    STATE_MARKED_ERROR,
    STATE_BLANK_ERROR,
    STATE_OK_NUM = STATE_MARKED_ERROR
};

struct game_params {
    int width;
    int height;
    int aggressiveness;
    bool advanced;
};

typedef struct board_state board_state;

struct game_state {
    bool cheating;
    int width;
    int height;
    char *cells_contents;
    board_state *board;
};

struct board_state {
    unsigned int references;
    struct board_cell *actual_board;
};

struct board_cell
{
    char clue;
    bool shown;
};

struct solution_cell {
    char cell;
    bool solved;
};

struct desc_cell
{
    char clue;
    bool shown;
    bool value;
    bool full;
    bool empty;
};


static game_params *default_params(void)
{
    game_params *ret = snew(game_params);

    ret->width = DEFAULT_SIZE;
    ret->height = DEFAULT_SIZE;
    ret->advanced = false;
    ret->aggressiveness = DEFAULT_AGGRESSIVENESS;

    return ret;
}

static bool game_fetch_preset(int i, char **name, game_params **params)
{
    const int sizes[5] = {3, 10, 15, 25, 50};
    if (i < 0 || i > 4) {
        return false;
    }
    game_params *res = snew(game_params);
    res->height=sizes[i];
    res->width=sizes[i];
    res->aggressiveness=10;
    res->advanced=false;
    *params=res;
    char *value = snewn(20, char);
    sprintf(value, "Size: %dx%d", sizes[i], sizes[i]);
    *name=value;
    return true;
}

static void free_params(game_params *params)
{
    sfree(params);
}

static game_params *dup_params(const game_params *params)
{
    game_params *ret = snew(game_params);
    *ret = *params;		       /* structure copy */
    return ret;
}

static void decode_params(game_params *params, char const *string)
{
}

static char *encode_params(const game_params *params, bool full)
{
    return dupstr("FIXME");
}

static config_item *game_configure(const game_params *params)
{
    config_item *config = snewn(5, config_item);
    char *value = snewn(12, char);
    config[0].type=C_STRING;
    config[0].name="Height";
    sprintf(value,"%d", params->height);
    config[0].u.string.sval=value;
    value = snewn(12, char);
    config[1].type=C_STRING;
    config[1].name="Width";
    sprintf(value,"%d", params->width);
    config[1].u.string.sval=value;
    value = snewn(12, char);
    config[2].type=C_STRING;
    config[2].name="Agressiveness in hint hiding (higher takes longer to generate)";
    sprintf(value,"%d", params->aggressiveness);
    config[2].u.string.sval=value;
    config[3].name="Advanced (unsupported)";
    config[3].type=C_BOOLEAN;
    config[3].u.boolean.bval = params->advanced;
    config[4].type=C_END;
    return config;
}

static game_params *custom_params(const config_item *cfg)
{
    game_params *res = snew(game_params);
    res->height=atol(cfg[0].u.string.sval);
    res->width=atol(cfg[1].u.string.sval);
    res->aggressiveness=atol(cfg[2].u.string.sval);
    res->advanced=cfg[3].u.boolean.bval;
    return res;
}

static const char *validate_params(const game_params *params, bool full)
{
    if (params->advanced && full) {
        return "Cannot generate advanced puzzle";
    }
    if (params->height < 3 || params->width < 3) {
        return "Minimal size is 3x3";
    }
    if (params->height > 50 || params->width > 50) {
        return "Maximum size is 50x50";
    }
    if (params->aggressiveness < 1) {
        return "Aggressiveness must be a positive number";
    }
    return NULL;
}

static bool get_pixel(const game_params *params, const bool *image, const int x, const int y) {
    const bool *pixel;
    pixel = get_cords(params, image, x, y);
    if (pixel) {
        return *pixel;
    }
    return 0;
}

static void populate_cell(const game_params *params, const bool *image, const int x, const int y, bool edge, struct desc_cell *desc) {
    int clue = 0;
    bool xEdge = false;
    bool yEdge = false;
    if (edge) {
        if (x > 0) {
            clue += get_pixel(params, image, x-1, y); 
            if (y > 0) {
                clue += get_pixel(params, image, x-1, y-1);
            }
            if (y < params->height-1) {
                clue += get_pixel(params, image, x-1, y+1);
            }
        } else {
            xEdge = true;
        }

        if (y > 0) {
            clue += get_pixel(params, image, x, y-1);
        } else {
            yEdge = true;
        }
        if (x < params->width-1) {
            clue += get_pixel(params, image, x+1, y);
            if (y>0) {
                clue += get_pixel(params, image, x+1, y-1);
            }
            if (y < params->height-1) {
                clue += get_pixel(params, image, x+1, y+1);
            }
        } else {
            xEdge = true;
        }
        if (y < params->height-1) {
            clue += get_pixel(params, image, x, y+1);
        } else {
            yEdge = true;
        }
    } else {
        clue += get_pixel(params, image, x-1, y-1);
        clue += get_pixel(params, image, x-1, y);
        clue += get_pixel(params, image, x-1, y+1);
        clue += get_pixel(params, image, x, y-1);
        clue += get_pixel(params, image, x, y+1);
        clue += get_pixel(params, image, x+1, y-1);
        clue += get_pixel(params, image, x+1, y);
        clue += get_pixel(params, image, x+1, y+1);
    }
    
    desc->value = get_pixel(params, image, x, y);
    clue += desc->value;
    if (clue == 0) {
        desc->empty = true;
        desc->full = false;
    } else {
        desc->empty = false;
        /* setting the default */
        desc->full = false;
        if (clue == 9) {
            desc->full = true;
        } else if (edge &&
            ((xEdge && yEdge && clue == 4) ||
            ((xEdge || yEdge) && clue == 6))) {
            
            desc->full = true;
        }
    }
    desc->shown = true;
    desc->clue = clue;
}

static void count_around(const game_params *params, struct solution_cell *sol, int x, int y, int *marked, int *blank, int *total) {
    int i, j;
    struct solution_cell *curr = NULL;
    (*total)=0;
    (*blank)=0;
    (*marked)=0;
 
    for (i=-1; i < 2; i++) {
        for (j=-1; j < 2; j++) {
            curr=get_cords(params, sol, x+i, y+j);
            if (curr) {
                (*total)++;
                if (curr->cell == STATE_BLANK) {
                    (*blank)++;
                } else if (curr->cell == STATE_MARKED) {
                    (*marked)++;
                }
            }
        }
    }
}

static void count_around_state(const game_state *state, int x, int y, int *marked, int *blank, int *total) {
    int i, j;
    char *curr = NULL;
    (*total)=0;
    (*blank)=0;
    (*marked)=0;
 
    for (i=-1; i < 2; i++) {
        for (j=-1; j < 2; j++) {
            curr=get_cords(state, state->cells_contents, x+i, y+j);
            if (curr) {
                (*total)++;
                if (*curr == STATE_BLANK) {
                    (*blank)++;
                } else if (*curr == STATE_MARKED) {
                    (*marked)++;
                }
            }
        }
    }
}

static void count_clues_around(const game_params *params,  struct desc_cell *desc, int x, int y, int *clues, int *total) {
    int i, j;
    struct desc_cell *curr = NULL;
    (*total)=0;
    (*clues)=0;
 
    for (i=-1; i < 2; i++) {
        for (j=-1; j < 2; j++) {
            curr=get_cords(params, desc, x+i, y+j);
            if (curr) {
                (*total)++;
                if (curr->shown) {
                    (*clues)++;
                }
            }
        }
    }
}

static void mark_around(const game_params *params, struct solution_cell *sol, int x, int y, int mark) {
    int i, j, marked = 0;
    struct solution_cell *curr;

    for (i=-1; i < 2; i++) {
        for (j=-1; j < 2; j++) {
            curr=get_cords(params, sol, x+i, y+j);
            if (curr) {
                if (curr->cell == STATE_UNMARKED) {
                    curr->cell = mark;
                    marked++;
                }
            }
        }
    }
}

static char solve_cell(const game_params *params, struct desc_cell *desc, struct solution_cell *sol, int x, int y) {
    struct desc_cell *curr=&desc[(y*params->width)+x];
    int marked = 0, total = 0, blank = 0;

    if (sol[(y*params->width)+x].solved) {
        return 1;
    }
    if (curr->full && curr->shown) {
        sol[(y*params->width)+x].solved = true;
        mark_around(params, sol, x, y, STATE_MARKED);
        return 1;
    }
    if (curr->empty && curr->shown)
    {
        sol[(y*params->width)+x].solved = true;
        mark_around(params, sol, x, y, STATE_BLANK);
        return 1;
    }
    count_around(params, sol, x, y, &marked, &blank, &total);
    if (curr->shown) {
        if (!sol[(y*params->width)+x].solved) {
            if (marked == curr->clue) {
                sol[(y*params->width)+x].solved = true;
                mark_around(params, sol, x, y, STATE_BLANK);
            } else if (curr->clue == (total - blank)) {
                sol[(y*params->width)+x].solved = true;
                mark_around(params, sol, x, y, STATE_MARKED);
            } else if (total == marked + blank) {
                return -1;
            } else {
                return 0;
            }
        }
        return 1;
    } else if (total == marked + blank) {
        sol[(y*params->width)+x].solved = true;
        return 1;
    } else {
        return 0;
    }
}

/*static void print_solve(const game_params *params, struct desc_cell *desc, const struct solution_cell *sol) {
    int x,y;
    printf("Current solution:\n");
    for (y=0; y< params->height; y++) {
        for (x=0; x < params->width; x++) {
            printf("M: %d, S: %d, C: %d ",sol[(y*params->width)+x].cell, sol[(y*params->width)+x].solved, desc[(y*params->width)+x].clue);
        }
        printf("\n");
    }
}*/

static bool solve_check(const game_params *params, struct desc_cell *desc) {
    int x,y;
    struct solution_cell *sol = snewn(params->height*params->width, struct solution_cell);
    int solved = 0, iter = 0, curr = 0;

    memset(sol, 0, params->height*params->width * sizeof(struct solution_cell));
    while (solved < params->height*params->width && iter < SOLVE_MAX_ITERATIONS) {
        solved = 0;
        for (y=0; y< params->height; y++) {
            for (x=0; x < params->width; x++) {
                curr = solve_cell(params, desc, sol, x, y);
                if (curr < 0) {
                    iter = SOLVE_MAX_ITERATIONS;
                    printf("error in cell x=%d, y=%d\n", x, y);
                    break;
                }
                solved += curr;
            }
        }
        iter++;
    }
    sfree(sol);
    return solved == params->height*params->width;
}

static void hide_clues(const game_params *params, struct desc_cell *desc, random_state *rs){
    int to_hideX, to_hideY, hidden, shown, total, tries = 0, x, y;
    bool solveable = false;
    struct desc_cell *curr;

    printf("Hiding clues\n");
    while (!solveable && tries < 1000) {
        for (hidden=0;hidden < ((params->height * params->width) * params->aggressiveness)/10;hidden++) {
            to_hideX=random_upto(rs, params->width);
            to_hideY=random_upto(rs, params->height);
            count_clues_around(params, desc, to_hideX, to_hideY, &shown, &total);
            if (shown > 1) {
                curr = get_cords(params, desc, to_hideX, to_hideY);
                curr->shown=false;
            }
            solveable=solve_check(params, desc);
            if (!solveable) {
                curr->shown=true;
            }
        }
        solveable=solve_check(params, desc);
        if (solveable) {
            break;
        }
        tries++;
        for (y=0; y< params->height; y++) {
            for (x=0; x < params->width; x++) {
                curr = get_cords(params, desc, x, y);
                curr->shown=true;
            }
        }
    }
}

static bool start_point_check(size_t size, struct desc_cell *desc) {
    int i;
    for (i=0; i < size; i++)
    {
        if (desc[i].empty || desc[i].full){
            return true;
        }
    }
    return false;
}

static void generate_image(const game_params *params, random_state *rs, bool *image) {
    int x,y;
    for (y=0; y< params->height; y++) {
        for (x=0; x < params->width; x++) {
            image[(y*params->width)+x]=random_bits(rs, 1);
        }
    }
}

static char *new_game_desc(const game_params *params, random_state *rs,
			   char **aux, bool interactive)
{
    bool *image = snewn(params->height*params->width, bool);
    bool valid = false;
    char *desc_string = snewn((params->height*params->width*4)+1, char);

    struct desc_cell* desc=snewn(params->height*params->width, struct desc_cell);    
    int x,y, location_in_str;

    while (!valid) {
        generate_image(params, rs, image);
#ifdef DEBUG_IMAGE
        image[0] = 1;
        image[1] = 1;
        image[2] = 0;
        image[3] = 1;
        image[4] = 1;
        image[5] = 0;
        image[6] = 0;
        image[7] = 0;
        image[8] = 0;
#endif

        for (y=0; y< params->height; y++) {
            for (x=0; x < params->width; x++) {
                populate_cell(params, image, x, y, x*y == 0 || y == params->height - 1 || x == params->width -1, &desc[(y*params->width)+x]);
            }
        }
        valid = start_point_check((params->height-1) * (params->width-1), desc);
        if (!valid) {
            printf("Not valid, regenerating.\n");
        } else {
            valid = solve_check(params, desc);
            if (!valid) {
                printf("Couldn't solve, regenerating.");
            } else {
                hide_clues(params, desc, rs);
            }
        }
    }
    location_in_str = 0;
    for (y=0; y< params->height; y++) {
        for (x=0; x < params->width; x++) {
            if (desc[(y*params->width)+x].shown) {
                printf("%d(%d)", desc[(y*params->width)+x].value, desc[(y*params->width)+x].clue);
                sprintf(desc_string + location_in_str, "%d", desc[(y*params->width)+x].clue);
            } else {
                printf("%d( )", desc[(y*params->width)+x].value);
                sprintf(desc_string + location_in_str, " ");
            }
            location_in_str+=1;
        }
        printf("\n");
    }

    return desc_string;
}



static const char *validate_desc(const game_params *params, const char *desc)
{
    return NULL;
}

static game_state *new_game(midend *me, const game_params *params,
                            const char *desc)
{
    game_state *state = snew(game_state);
    char *curr_desc = dupstr(desc);
    char *desc_base = curr_desc;

    state->cheating = false;
    state->height = params->height;
    state->width = params->width;
    state->cells_contents = snewn(params->height*params->width, char);
    memset(state->cells_contents, 0, params->height*params->width* sizeof(char));
    state->board = snew(board_state);
    state->board->references = 1;
    state->board->actual_board = snewn(params->height*params->width, struct board_cell);

    while (*curr_desc != '\0') {
        if (*curr_desc >= '0' && *curr_desc <= '9'){
            state->board->actual_board[curr_desc-desc_base].shown = true;
            state->board->actual_board[curr_desc-desc_base].clue = *curr_desc - '0';
        } else {
            state->board->actual_board[curr_desc-desc_base].shown = false;
            state->board->actual_board[curr_desc-desc_base].clue = -1;
        }
        curr_desc++;
    }

    sfree(desc_base);
    return state;
}

static game_state *dup_game(const game_state *state)
{
    game_state *ret = snew(game_state);

    ret->cheating = state->cheating;
    ret->width = state->width;
    ret->height = state->height;
    ret->cells_contents = snewn(state->height*state->width, char);
    memcpy(ret->cells_contents, state->cells_contents, state->height*state->width * sizeof(char));
    ret->board = state->board;
    ret->board->references++;

    return ret;
}

static void free_game(game_state *state)
{
    sfree(state->cells_contents);
    state->cells_contents = NULL;
    if (state->board->references <= 1) {
        sfree(state->board);
        state->board = NULL;
    } else {
        state->board->references--;
    }
    sfree(state);
}

static char *solve_game(const game_state *state, const game_state *currstate,
                        const char *aux, const char **error)
{

    return NULL;
}

static bool game_can_format_as_text_now(const game_params *params)
{
    return true;
}

static char *game_text_format(const game_state *state)
{
    return NULL;
}

static game_ui *new_ui(const game_state *state)
{
    return NULL;
}

static void free_ui(game_ui *ui)
{
}

static char *encode_ui(const game_ui *ui)
{
    return NULL;
}

static void decode_ui(game_ui *ui, const char *encoding)
{
}

static void game_changed_state(game_ui *ui, const game_state *oldstate,
                               const game_state *newstate)
{
}

struct game_drawstate {
    int tilesize;
    int FIXME;
};

static char *interpret_move(const game_state *state, game_ui *ui,
                            const game_drawstate *ds,
                            int x, int y, int button)
{
    int gameX, gameY;
    char move_type;
    char move_desc[15] = "";
    char *ret = NULL;
    if (button == LEFT_BUTTON || button == RIGHT_BUTTON) {
        gameX=x/ds->tilesize;
        gameY=y/ds->tilesize;
        if (button == RIGHT_BUTTON) {
            /* Right button toggles twice */
            move_type = 'T';
        } else {
            move_type = 't';
        }
        sprintf(move_desc, "%c%d,%d", move_type, gameX, gameY);
        if (gameX >= 0 && gameY >= 0 && gameX < state->width && gameY < state->height) {
            ret = dupstr(move_desc);
        }
    }
    return ret;
}

static game_state *execute_move(const game_state *state, const char *move)
{
    game_state *new_state = dup_game(state);
    int i = 0, x, y, marked, total, blank;
    char *comma, *cell;
    struct board_cell *curr;
    char coordinate[12];
    int steps = 1;
    if (move[0] == 't' || move[0] == 'T') {
        if (move[0] == 'T') {
            steps++;
        }
        i++;
        comma=strchr(move + i, ',');
        if (comma != NULL) {
            strncpy(coordinate, move + i, comma - move -i);
            x = atol(coordinate);
            strcpy(coordinate, comma + 1);
            y = atol(coordinate);
            cell = get_cords(new_state, new_state->cells_contents, x, y);
            if (*cell >= STATE_OK_NUM) {
                *cell -= 2;
            }
            *cell = (*cell + steps) % STATE_OK_NUM;
            curr = get_cords(state, state->board->actual_board, x, y);
            if (*cell != STATE_UNMARKED && curr->shown) {
                count_around_state(new_state, x, y, &marked, &blank, &total);
                if (marked > curr->clue || curr->clue > (total-blank)) {
                    *cell += 2;
                }
            }
        }
    }
    return new_state;
}

/* ----------------------------------------------------------------------
 * Drawing routines.
 */

static void game_compute_size(const game_params *params, int tilesize,
                              int *x, int *y)
{
    *x = (params->width+1) * tilesize;
    *y = (params->height+1) * tilesize;
}

static void game_set_size(drawing *dr, game_drawstate *ds,
                          const game_params *params, int tilesize)
{
    ds->tilesize = tilesize;
}

#define COLOUR(ret, i, r, g, b) \
   ((ret[3*(i)+0] = (r)), (ret[3*(i)+1] = (g)), (ret[3*(i)+2] = (b)))

static float *game_colours(frontend *fe, int *ncolours)
{
    float *ret = snewn(3 * NCOLOURS, float);

   /* frontend_default_colour(fe, &ret[COL_BACKGROUND * 3]);*/
    COLOUR(ret, COL_GRID,  0.0F, 0.0F, 0.0F);
    COLOUR(ret, COL_ERROR, 1.0F, 0.0F, 0.0F);
    COLOUR(ret, COL_BLANK,  1.0F, 1.0F, 1.0F);
    COLOUR(ret, COL_BACKGROUND,  0.8F, 0.8F, 0.8F);
    *ncolours = NCOLOURS;
    return ret;
}

static game_drawstate *game_new_drawstate(drawing *dr, const game_state *state)
{
    struct game_drawstate *ds = snew(struct game_drawstate);

    ds->tilesize = 0;
    ds->FIXME = 0;

    return ds;
}

static void game_free_drawstate(drawing *dr, game_drawstate *ds)
{
    sfree(ds);
}

static void draw_cell(drawing *dr, game_drawstate *ds,
                    const game_state *state,
                    int x, int y) {
    const int ts = ds->tilesize;
    int startX = x * ts, startY = y * ts;
    int color, text_color = COL_GRID;
    
    char *cell_p = get_cords(state, state->cells_contents, x, y);
    char cell = *cell_p;
    if (cell != STATE_MARKED && cell != STATE_MARKED_ERROR) {
        draw_rect_outline(dr, startX, startY, ts, ts, COL_GRID);
    } else {
        draw_rect(dr, startX, startY, ts, ts, COL_GRID);
    }

    if (cell == STATE_MARKED || cell == STATE_MARKED_ERROR) {
        color = COL_MARKED;
    } else if (cell == STATE_BLANK || cell == STATE_BLANK_ERROR) {
        color = COL_BLANK;
    } else {
        color = COL_BACKGROUND;
    }
    
    draw_rect(dr, startX+1, startY+1, ts-2, ts-2, color);
    if (color == COL_GRID) {
        text_color = COL_BACKGROUND;
    }
    if (cell >= STATE_OK_NUM) {
        text_color = COL_ERROR;
    }
    struct board_cell *curr = NULL;
    char clue[3];
    curr = get_cords(state, state->board->actual_board, x, y);
    if (curr && curr->shown) {
        sprintf(clue, "%d", curr->clue);
        draw_text(dr, startX + ts/2, startY + ts/2, 1, ts * 3/5,
        ALIGN_VCENTRE | ALIGN_HCENTRE, text_color, clue);
    }
}

static void game_redraw(drawing *dr, game_drawstate *ds,
                        const game_state *oldstate, const game_state *state,
                        int dir, const game_ui *ui,
                        float animtime, float flashtime)
{
    /*
     * The initial contents of the window are not guaranteed and
     * can vary with front ends. To be on the safe side, all games
     * should start by drawing a big background-colour rectangle
     * covering the whole window.
     */
    int x, y;
    draw_rect(dr, 0, 0, (state->width+1)*ds->tilesize, (state->height+1)*ds->tilesize, COL_BACKGROUND);
    for (y=0;y<state->height;y++) {
        for (x=0;x<state->height;x++) {
            draw_cell(dr, ds, state, x, y);
        }
    }
    draw_update(dr, 0, 0, (state->width+1)*ds->tilesize, (state->height+1)*ds->tilesize);
}

static float game_anim_length(const game_state *oldstate,
                              const game_state *newstate, int dir, game_ui *ui)
{
    return 0.0F;
}

static float game_flash_length(const game_state *oldstate,
                               const game_state *newstate, int dir, game_ui *ui)
{
    return 0.0F;
}

static int game_status(const game_state *state)
{
    return 0;
}

static bool game_timing_state(const game_state *state, game_ui *ui)
{
    return true;
}

static void game_print_size(const game_params *params, float *x, float *y)
{
}

static void game_print(drawing *dr, const game_state *state, int tilesize)
{
}

#ifdef COMBINED
#define thegame mosaic
#endif

const struct game thegame = {
    "Mosaic", NULL, NULL,
    default_params,
    game_fetch_preset, NULL,
    decode_params,
    encode_params,
    free_params,
    dup_params,
    true, game_configure, custom_params,
    validate_params,
    new_game_desc,
    validate_desc,
    new_game,
    dup_game,
    free_game,
    false, solve_game,
    false, game_can_format_as_text_now, game_text_format,
    new_ui,
    free_ui,
    encode_ui,
    decode_ui,
    NULL, /* game_request_keys */
    game_changed_state,
    interpret_move,
    execute_move,
    20 /* FIXME */, game_compute_size, game_set_size,
    game_colours,
    game_new_drawstate,
    game_free_drawstate,
    game_redraw,
    game_anim_length,
    game_flash_length,
    game_status,
    false, false, game_print_size, game_print,
    false,			       /* wants_statusbar */
    false, game_timing_state,
    0,				       /* flags */
};
