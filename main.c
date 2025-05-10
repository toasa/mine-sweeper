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
    int n_neighbor_mines;
};

struct board {
    int n_row;
    int n_col;

    struct cell *body;
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
            printf("%c", cell->has_mine ? '*' : '.');
        }
        puts("");
    }
}

void print_debug_board_neighbors(struct board *b) {
    for (int r = 1; r < b->n_row; r++) {
        for (int c = 0; c < b->n_col; c++) {
            struct cell *cell = get_cell(b, r, c);
            printf("%d", cell->n_neighbor_mines);
        }
        puts("");
    }
}

void draw_board(struct board *b) {
    mvprintw(0, 0, "Press Q(uit)");
    for (int r = 1; r < b->n_row; r++) {
        for (int c = 0; c < b->n_col; c++) {
            struct cell *cell = get_cell(b, r, c);

            char cell_char = ' ';
            if (!cell->is_revealed)
                cell_char = '.';
            else if (cell->has_mine)
                cell_char = '*';
            else if (0 < cell->n_neighbor_mines)
                cell_char = '0' + cell->n_neighbor_mines;

            mvprintw(r, c, "%c", cell_char);
        }
    }

    refresh();
}

bool reveil_cell(struct board *b, int row, int col) {
    if (!is_valid_cell(b, row, col))
        return true;

    struct cell *cell = get_cell(b, row, col);
    if (cell->is_revealed)
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

void place_mines(struct board *b) {
    int total_mine = b->n_row * b->n_col * MINE_PERCENTAGE / 100;

    srand(time(NULL));

    int placed = 0;
    while (placed < total_mine) {
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
            if (!cell->has_mine)
                cell->n_neighbor_mines = count_neighbor_mines(b, r, c);
        }
    }
}

int init_ncurses(void) {
    initscr();
    cbreak();             // 行バッファリングを無効
    noecho();             // 入力文字を非表示
    keypad(stdscr, TRUE); // マウスでの操作を有効
    curs_set(0);          // カーソル非表示

    if (mousemask(BUTTON1_CLICKED, NULL) == 0) {
        fprintf(stderr, "Error: Mouse support not available\n");
        return -1;
    }

    return 0;
}

struct board *init_board(void) {
    struct board *b = calloc(1, sizeof(struct board));
    if (b == NULL) {
        perror("calloc");
        return NULL;
    }

    // Get terminal window size.
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        perror("ioctl");
        return NULL;
    }
    b->n_row = ws.ws_row;
    b->n_col = ws.ws_col;

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

void deinit(void) { endwin(); }

int main() {
    struct board *b = init();

    // print_debug_board_mines(b);
    // print_debug_board_neighbors(b);

    bool game_over = false;
    while (!game_over) {
        draw_board(b);

        int c = getch();
        if (c == 'q')
            goto end;

        MEVENT e;
        if (c == KEY_MOUSE && getmouse(&e) == OK) {
            if (!reveil_cell(b, e.y, e.x))
                game_over = true;
        }
    };

    draw_board(b); // 最終盤面を表示

    if (game_over)
        mvprintw(0, 0, "Game is over!");

    refresh();
    getch();

end:

    deinit();
}
