#include "BomberMan.h"
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <cctype>
#include <cstdint>

#define i32 int32_t

#define SET_PLAYER(p,id,_y,_x) {   \
    p.group=1, p.player=id, p.life_value=3, p.bomb_num=3, p.pos.y=_y, p.pos.x=_x; \
}

Game g;
const char* cmap[MAP_ROW+2] = {
    "*****************",
    "*- -*   -   *- -*",
    "*-*  - * * -  *-*",
    "* - - - - - - - *",
    "* -*-- *-* --*- *",
    "*    *  -  *    *",
    "* *  - --- -  *B*",
    "*   *  *-*  *  -*",
    "*X*   -----   *H*",
    "*A  *  *-*  *  -*",
    "*2*  - --- - C* *",
    "*    *  -  *    *",
    "* -*-- *-* --*- *",
    "* - - - - - - - *",
    "*-*  - * * -  *-*",
    "*- -*   -   *- -*",
    "*****************"
};

void cmap2map();
void dumpMap();

int main() {
    cmap2map();
    Operator op[3];
    dumpMap();
    //SET_PLAYER(g.group[1].player[0], 0, 5, 10);
    AI(&g, op);
}

void dumpMap() {
    printf("Game Map:\n   ");
    for (int x=0; x!=MAP_COL+2; ++x)
        printf("%2d ", x);
    putchar('\n');
    for (int y=0; y!=MAP_ROW+2; ++y) {
        printf("%2d ", y);
        for (int x=0; x!=MAP_COL+2; ++x) {
            char bomber = ' ';
            for (int p=0; p!=PLAYER_NUM; ++p) {
                Coord c = g.group[1].player[p].pos;
                if (c.y==y && c.x==x)
                    bomber = 'A'+p;
            }
            switch(g.map[y][x].type) {
            case HOME:
                printf("HQ");
            break;
            case WOOD:
                printf("::");
            break;
            case STONE:
                printf("##");
            break;
            case BOMB:
                printf("%d%c", g.map[y][x].bomb.TTL, bomber);
            break;
            case BLANK:
                printf(" %c", bomber);
            break;
            }
            putchar(' ');
        }
        putchar('\n');
    }
    putchar('\n');
    // char s[65536]; gets(s);
}

void cmap2map() {
    int pId = 0;
    g.grpid = 1;
    g.group[1].group = 1;
    for (int y=0; y!=MAP_ROW+2; ++y) {
        for (int x=0; x!=MAP_COL+2; ++x) {
            Object obj;
            switch(cmap[y][x]) {
            case ' ':
                obj.type = BLANK;
            break;
            case '*':
                obj.type = STONE;
            break;
            case '-':
                obj.type = WOOD;
            break;
            case 'X':
                obj.type = HOME;
                obj.home.group = 0;
            break;
            case 'H':
                obj.type = HOME;
                obj.home.group = 1;
            break;
            case 'A':
            case 'B':
            case 'C':
                pId = cmap[y][x]-'A';
                SET_PLAYER(g.group[1].player[pId], pId, y, x);
                obj.type = BLANK;
            break;
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
                obj.type = BOMB;
                obj.bomb.TTL   = cmap[y][x] - '0';
                obj.bomb.group = 0;
            break;
            }
            g.map[y][x] = obj;
        }
    }
}
