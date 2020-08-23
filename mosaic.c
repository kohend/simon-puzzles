/*
 * mosaic.c [FIXME]: Template defining the null game (in which no
 * moves are permitted and nothing is ever drawn). This file exists
 * solely as a basis for constructing new game definitions - it
 * helps to have something which will compile from the word go and
 * merely doesn't _do_ very much yet.
 * 
 * Parts labelled FIXME actually want _removing_ (e.g. the dummy
 * field in each of the required data structures, and this entire
 * comment itself) when converting this source file into one
 * describing a real game.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>

#include "puzzles.h"

#define DEFAULT_SIZE 5
#define SOLVE_MAX_ITERATIONS 200
#define DEBUG_IMAGE 1
#undef DEBUG_IMAGE

/* Getting the coordinates and returning NULL when out of scope 
 * The absurd amount of parentesis is needed to avoid order of operations issues */
#define get_cords(params, array, x, y) (((x) >= 0 && (y) >= 0) && ((x)< params->width && (y)<params->height))? \
     array + ((y)*params->width)+x : NULL;

enum {
    COL_BACKGROUND,
    COL_FILLED,
    COL_BLANK,
    NCOLOURS
};

enum cell_state {
    STATE_UNMARKED = 0,
    STATE_MARKED,
    STATE_BLANK
};

struct game_params {
    int width;
    int height;
    bool advanced;
};

struct game_state {
    int FIXME;
    enum cell_data * cells;
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

    return ret;
}

static bool game_fetch_preset(int i, char **name, game_params **params)
{
    return false;
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
    config_item *config = snewn(4, config_item);
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
    config[2].name="Advanced (unsupported)";
    config[2].type=C_BOOLEAN;
    config[2].u.boolean.bval = params->advanced;
    config[3].type=C_END;
    return config;
}

static game_params *custom_params(const config_item *cfg)
{
    game_params *res = snew(game_params);
    res->height=atol(cfg[0].u.string.sval);
    res->width=atol(cfg[1].u.string.sval);
    res->advanced=cfg[2].u.boolean.bval;
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
    /*print_solve(params, desc, sol);*/
    sfree(sol);
    return solved == params->height*params->width;
}

static void hide_clues(const game_params *params, struct desc_cell *desc, random_state *rs){
    int to_hideX, to_hideY, hidden, shown, total, tries = 0, x, y;
    bool solveable = false;
    struct desc_cell *curr;

    printf("Hiding clues\n");
    while (!solveable && tries < 10) {
        for (hidden=0;hidden < (params->height * params->width);hidden++) {
            to_hideX=random_upto(rs, params->width);
            to_hideY=random_upto(rs, params->height);
            count_clues_around(params, desc, to_hideX, to_hideY, &shown, &total);
            if (shown > 1) {
                curr = get_cords(params, desc, to_hideX, to_hideY);
                curr->shown=false;
            }
        }
        solveable=solve_check(params, desc);
        if (solveable) {
            break;
        }
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

    struct desc_cell* desc=snewn(params->height*params->width, struct desc_cell);    
    int x,y;

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
    for (y=0; y< params->height; y++) {
        for (x=0; x < params->width; x++) {
            if (desc[(y*params->width)+x].shown) {
                printf("%d(%d)", desc[(y*params->width)+x].value, desc[(y*params->width)+x].clue);
            } else {
                printf("%d( )", desc[(y*params->width)+x].value);
            }
        }
        printf("\n");
    }

    return dupstr("FIXME");
}

static const char *validate_desc(const game_params *params, const char *desc)
{
    return NULL;
}

static game_state *new_game(midend *me, const game_params *params,
                            const char *desc)
{
    game_state *state = snew(game_state);

    state->FIXME = 0;

    return state;
}

static game_state *dup_game(const game_state *state)
{
    game_state *ret = snew(game_state);

    ret->FIXME = state->FIXME;

    return ret;
}

static void free_game(game_state *state)
{
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
    return NULL;
}

static game_state *execute_move(const game_state *state, const char *move)
{
    return NULL;
}

/* ----------------------------------------------------------------------
 * Drawing routines.
 */

static void game_compute_size(const game_params *params, int tilesize,
                              int *x, int *y)
{
    *x = *y = 10 * tilesize;	       /* FIXME */
}

static void game_set_size(drawing *dr, game_drawstate *ds,
                          const game_params *params, int tilesize)
{
    ds->tilesize = tilesize;
}

static float *game_colours(frontend *fe, int *ncolours)
{
    float *ret = snewn(3 * NCOLOURS, float);

    frontend_default_colour(fe, &ret[COL_BACKGROUND * 3]);

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
    draw_rect(dr, 0, 0, 10*ds->tilesize, 10*ds->tilesize, COL_BACKGROUND);
    draw_update(dr, 0, 0, 10*ds->tilesize, 10*ds->tilesize);
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
