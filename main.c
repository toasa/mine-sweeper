#include <ncurses.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define MINE_PERCENTAGE 10

struct cell {
    bool has_mine;
    bool is_revealed;
    bool is_flagged;
    int n_neighbor_mines;
};

struct board {
    int n_row;
    int n_col;

    int n_mines;
    int n_flagged;

    char top_msg[256];

    struct cell *body;
};

enum CELL_TYPE {
    CELL_MINE,
    CELL_FLAG,
    CELL_HIDE,
    CELL_NUM,
    CELL_BLANK,
};

struct cell_info {
    int fg_color;
    int bg_color;
    char c;
};

struct cell_info cell_infos[] = {
    [CELL_MINE] = {.fg_color = COLOR_RED, .bg_color = COLOR_WHITE, .c = '*'},
    [CELL_FLAG] = {.fg_color = COLOR_GREEN, .bg_color = COLOR_WHITE, .c = '!'},
    [CELL_HIDE] = {.fg_color = COLOR_BLACK, .bg_color = COLOR_WHITE, .c = '.'},
    [CELL_NUM] = {.fg_color = COLOR_BLUE, .bg_color = COLOR_WHITE, .c = '0'},
    [CELL_BLANK] = {.fg_color = COLOR_WHITE, .bg_color = COLOR_WHITE, .c = ' '},
};

struct cell *get_cell(const struct board *b, int row, int col) {
    return &b->body[row * b->n_col + col];
}

bool is_valid_cell(const struct board *b, int row, int col) {
    return 0 < row && row < b->n_row && 0 <= col && col < b->n_col;
}

void print_debug_board_mines(struct board *b) {
    for (int r = 1; r < b->n_row; r++) {
        for (int c = 0; c < b->n_col; c++) {
            struct cell *cell = get_cell(b, r, c);
            printf(" %c", cell->has_mine ? '*' : '.');
        }
        puts("");
    }
}

void print_debug_board_neighbors(struct board *b) {
    for (int r = 1; r < b->n_row; r++) {
        for (int c = 0; c < b->n_col; c++) {
            struct cell *cell = get_cell(b, r, c);
            printf(" %d", cell->n_neighbor_mines);
        }
        puts("");
    }
}

void draw_board(struct board *b) {
    mvprintw(0, 0, "                               "); // TODO
    mvprintw(0, 0, "%s", b->top_msg);

    for (int r = 1; r < b->n_row; r++) {
        for (int c = 0; c < b->n_col; c++) {
            struct cell *cell = get_cell(b, r, c);

            int cell_type = CELL_BLANK;
            int n = 0;
            char cell_char = ' ';
            if (cell->is_flagged)
                cell_type = CELL_FLAG;
            else if (!cell->is_revealed)
                cell_type = CELL_HIDE;
            else if (cell->has_mine)
                cell_type = CELL_MINE;
            else if (0 < cell->n_neighbor_mines) {
                cell_type = CELL_NUM;
                n = cell->n_neighbor_mines;
            }

            attrset(COLOR_PAIR(cell_type));
            mvprintw(r, c * 2, " %c", cell_infos[cell_type].c + n);
            attroff(COLOR_PAIR(cell_type));
        }
    }

    refresh();
}

bool reveil_cell(struct board *b, int row, int col) {
    if (!is_valid_cell(b, row, col))
        return true;

    struct cell *cell = get_cell(b, row, col);
    if (cell->is_revealed || cell->is_flagged)
        return true;

    cell->is_revealed = true;

    if (cell->has_mine)
        return false;

    if (cell->n_neighbor_mines == 0) {
        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                if (is_valid_cell(b, row + dr, col + dc) &&
                    !get_cell(b, row + dr, col + dc)->is_revealed)
                    reveil_cell(b, row + dr, col + dc);
            }
        }
    }

    return true;
}

void toggle_flag(struct board *b, int row, int col) {
    if (!is_valid_cell(b, row, col))
        return;

    struct cell *cell = get_cell(b, row, col);
    if (!cell->is_flagged && cell->is_revealed)
        return;

    if (cell->is_flagged) {
        cell->is_flagged = false;
        b->n_flagged--;
    } else {
        cell->is_flagged = true;
        b->n_flagged++;
    }
}

void place_mines(struct board *b) {
    b->n_mines = b->n_row * b->n_col * MINE_PERCENTAGE / 100;

    srand(time(NULL));

    int placed = 0;
    while (placed < b->n_mines) {
        int r = rand() % b->n_row;
        int c = rand() % b->n_col;

        struct cell *cell = get_cell(b, r, c);

        if (!cell->has_mine) {
            cell->has_mine = 1;
            placed++;
        }
    }
}

int count_neighbor_mines(const struct board *b, int row, int col) {
    int count = 0;

    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            if (dr == 0 && dc == 0)
                continue;

            int r = row + dr;
            int c = col + dc;

            if (is_valid_cell(b, r, c) && get_cell(b, r, c)->has_mine)
                count++;
        }
    }

    return count;
}

void calc_neighbor_mines(const struct board *b) {
    for (int r = 1; r < b->n_row; r++) {
        for (int c = 0; c < b->n_col; c++) {
            struct cell *cell = get_cell(b, r, c);
            cell->n_neighbor_mines = count_neighbor_mines(b, r, c);
        }
    }
}

bool check_game_win(const struct board *b) {
    for (int r = 1; r < b->n_row; r++) {
        for (int c = 0; c < b->n_col; c++) {
            struct cell *cell = get_cell(b, r, c);
            if (!cell->has_mine && !cell->is_revealed)
                return false;
        }
    }

    return true;
}

int init_ncurses(void) {
    initscr();
    cbreak();             // 行バッファリングを無効
    noecho();             // 入力文字を非表示
    keypad(stdscr, TRUE); // マウスでの操作を有効
    curs_set(0);          // カーソル非表示

    if (mousemask(BUTTON1_CLICKED | BUTTON_SHIFT, NULL) == 0) {
        fprintf(stderr, "Error: Mouse support not available\n");
        return -1;
    }

    start_color();
    for (int i = 0; i < sizeof(cell_infos) / sizeof(cell_infos[0]); i++) {
        struct cell_info t = cell_infos[i];
        init_pair(i, t.fg_color, t.bg_color);
    }

    return 0;
}

struct board *init_board(void) {
    struct board *b = calloc(1, sizeof(struct board));
    if (b == NULL) {
        perror("calloc");
        return NULL;
    }

    sprintf(b->top_msg, "Press Q(uit)");

    // Get terminal window size.
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        perror("ioctl");
        return NULL;
    }
    b->n_row = ws.ws_row;
    b->n_col = ws.ws_col / 2;

    b->body = calloc(b->n_col * b->n_row, sizeof(struct cell));
    if (b->body == NULL) {
        perror("calloc");
        return NULL;
    }

    place_mines(b);
    calc_neighbor_mines(b);

    return b;
}

struct board *init(void) {
    init_ncurses();

    return init_board();
}

void deinit(void) {
    attrset(A_NORMAL);
    endwin();
}

int main() {
    struct board *b = init();

    // print_debug_board_mines(b);
    // print_debug_board_neighbors(b);

    bool game_won = false;
    bool game_over = false;
    while (!game_over && !game_won) {
        draw_board(b);

        int c = getch();
        if (c == 'q')
            goto end;

        MEVENT e;
        if (c == KEY_MOUSE && getmouse(&e) == OK) {
            if (e.bstate & BUTTON1_CLICKED) {
                int row = e.y;
                int col = e.x / 2;
                if (e.bstate & BUTTON_SHIFT) {
                    toggle_flag(b, row, col);
                } else {
                    if (!reveil_cell(b, row, col))
                        game_over = true;
                }
            }
        }

        sprintf(b->top_msg, "[%d/%d]", b->n_flagged, b->n_mines);

        if (!game_over && check_game_win(b))
            game_won = true;
    };

    if (game_won)
        sprintf(b->top_msg, "Game won!");
    else
        sprintf(b->top_msg, "Game is over!");

    draw_board(b); // 最終盤面を表示

    refresh();
    getch();

end:

    deinit();
}
