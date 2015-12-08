#include "BomberMan.h"
#include <set>
#include <algorithm>
#include <cstdint>
#include <cctype>
#include <cstdlib>
#include <functional>
using namespace std;

#define i32 int32_t
#define min4(a,b,c,d) (std::min(std::min(a, b), std::min(c, d)))
#define DBG  printf
#define EQU_YX(ix,y,x) (yx==((y)<<16|(x)))
#define YX(y,x) ((y)<<16|(x))
#define gY(yx) ((yx & 0xFFFF0000)>>16)
#define gX(yx) ((yx & 0x0000FFFF))

// remove duplicate Coord
struct CoordSet{
    CoordSet() {}
    void insert(int y, int x) {
        s.insert( YX(y,x) );
    }
    void insert(const Coord c) {
        insert(c.y, c.x);
    }
    bool has(int y, int x) {
        return  s.find(YX(y,x))!=s.end();
    }
    void clear() {
        s.clear();
    }
    unsigned long size() {
        return s.size();
    }
    template<typename F>
    void for_each2(F f) {
        std::for_each(s.cbegin(), s.cend(), [f](const uint32_t yx){
            f( gY(yx), gX(yx) );
        });
    }
    set<uint32_t> s;
};

inline Coord COORD(int y, int x) {
    Coord c;
    c.x=x;
    c.y=y;
    return c;
}


// initialize these in AI()
const Player *player;
Operator *player_op;
long grpid;

bool kamakaze = false;
i32 dts[MAP_ROW+2][MAP_COL+2];    // Distance to Strike
i32 evd[MAP_ROW+2][MAP_COL+2];    // Evade Reference
i32 def[MAP_ROW+2][MAP_COL+2];    // Defence Reference
i32 spath[MAP_ROW+2][MAP_COL+2];  // Shortest Path Reference (YX)
/* LOW 4bit:    shortest-path next cell
 * LOW 8-12bit: is pointed by other cell */

const i32 DTS_MAX  = 0x0000FFFF;
const i32 EVD_MIN  = 0x80000000;
const i32 DEF_MIN  = 0x80000000;
const i32 EVADE_TTL_THRESHOLD = 3;
Object map[MAP_ROW+2][MAP_COL+2];

/* direction delta manipulator
 * MAKE SURE dirs, dirs_op, dirs_off matches !!!!! */
function<void(int&, int&)> dirs[4] = {
    [](int &y, int&x){--y;},
    [](int &y, int&x){++y;},
    [](int &y, int&x){--x;},
    [](int &y, int&x){++x;}
};
Operator dirs_op[4] = {UP, DOWN, LEFT, RIGHT};
int dirs_bit[4] = {0, 1, 2, 3};
int dirs_flood_from[4] = {1, 0, 3, 2};
const char* dirs_str[16] = {
    "  ", "↑",  "↓",  " V",
    "←",  "LU", "LD", "LV",
    "→",  "RU", "RD", "RV",
    " H", "HU", "HD", "HV"
};

Coord enemy_home;
Coord my_home;
CoordSet cs_a, cs_b;
CoordSet *cs, *next_cs;

// Assume map, y, x, s_yx are in the context
#define Type         ( map[y][x].type )
#define TypeYX(y,x)  ( map[y][x].type )
#define DTS(y,x)     ( dts[y][x] ? dts[y][x] : DTS_MAX )
#define BOSET(y,x,t) { if ((y)>0 && (y)<=MAP_ROW && (x)>0 && (x)<=MAP_COL) evd[y][x]=t; }
// WOOD is on Shortest-Path, if neighbour points to it (I guessed :D)
//                           and DTS(WOOD) < DTS(cur)    // checked in decision maker
#define SPATH_WOOD(y,x) ( map[y][x].type==WOOD && (spath[y][x]&0x00000F00) )

template<typename F>
void dumpDecisionMap3(i32 m[MAP_ROW+2][MAP_COL+2], F f) {
    DBG("\n   ");
    for (int x=0; x!=MAP_COL+2; ++x)
        DBG("%2d ", x);
    DBG("\n");
    for (int y=0; y!=MAP_ROW+2; ++y) {
        DBG("%2d ", y);
        for (int x=0; x!=MAP_COL+2; ++x) {
            f(m[y][x],y,x);
            DBG(" ");
        }
        DBG("\n");
    }
    DBG("\n");
    // char s[65536]; gets(s);
}
template<typename F>
void dumpDecisionMap(i32 m[MAP_ROW+2][MAP_COL+2], F f) {
    dumpDecisionMap3(m, [&](i32 v,int,int){ f(v); });
}

void dumpDTS() {
    dumpDecisionMap( dts, [](i32 v){
        if (v==0)
            DBG("  ");
        else if (v>=DTS_MAX)
            DBG("##");
        else
            DBG("%2d", v);
    } );
    //char c[1000]; gets(c);
}

void dumpEvd() {
    dumpDecisionMap( evd, [](i32 v){
        if (v) DBG("%2d", v);
        else   DBG("  ");
    });
}

void dumpSPath() {
    dumpDecisionMap3( spath, [](i32 bit_d, int y, int x)->void{
        if (Type == STONE)
            DBG("%s", "##");
        else if ( (bit_d&0x0000000F00) && Type==WOOD)
            DBG("%s", "::");
        else
            DBG("%s", dirs_str[bit_d&15]);
    });
}

static auto OpStr = [](Operator op) {
    if (op==NOP)   return "NOOP ";
    if (op==UP)    return "UP   ";
    if (op==DOWN)  return "DOWN ";
    if (op==LEFT)  return "LEFT ";
    if (op==RIGHT) return "RIGHT";
    if (op==PUT)   return "PUT  ";
    return "ERROR";
};

// fill `evd`
void detonate(int _y, int _x, int _ttl=-1, CoordSet* detonated=0) {
    #define BO(y,x) (evd[y][x] ? evd[y][x] : 0x0000FFFF)
    if (!detonated) {
        auto detonated = new CoordSet();
        detonate(_y, _x, _ttl, detonated);
        delete detonated;
    }else{
        i32 ttl = _ttl==-1 ? map[_y][_x].bomb.TTL : _ttl;
        DBG("detonating %d, %d\n", _y, _x);
        if (detonated->has(_y, _x)) return;
        evd[_y][_x] = std::min(ttl, BO(_y,_x));
        detonated->insert(_y, _x);
        for (int d=0; d!=4; ++d) {
            int y=_y, x=_x;
            for (int i=0; i!=BOMB_POWER; ++i) {
                dirs[d](y, x);
                if (Type == STONE)
                    break;
                evd[y][x] = std::min(ttl, BO(y,x));
                if (Type == BOMB)
                    detonate(y, x, ttl, detonated);
                if (Type == WOOD || Type == HOME)
                    break;
            }
            //dumpOverlay();
        }
    }
}

void flood_dts(int y, int x) {
    i32 _dts = DTS_MAX;       // calculated dts for cell [y,x]
    i32 dts_min_4 = DTS_MAX;  // i32, dts
    i32 s_cell = 0;           // YX
    DBG("%2d, %2d   ", y, x);
    // type of flood source
    ObjectType src_type = TypeYX(y,x);
    for (int d=0; d!=4; ++d) {
        int ny=y, nx=x;
        dirs[d](ny, nx);
        if (TypeYX(ny,nx)==STONE) continue;
        switch (src_type) {
        case HOME:
            _dts = DTS(y, x);
        break;
        case STONE:
        break;
        case WOOD:
            if (evd[y][x])
                _dts = DTS(y, x) + 2 + evd[y][x];
            else
                _dts = DTS(y, x) + 2 + BOMB_TTL;
        break;
        case BLANK:
            _dts = DTS(y, x)+1;
        break;
        case BOMB:
            _dts = DTS(y, x) + map[y][x].bomb.TTL;
        break;
        }
        DBG("_dts: %2d\n", _dts);
        if (dts[ny][nx]==0 || _dts <= dts[ny][nx]) {
            if (dts[ny][nx]==0 || _dts<dts[ny][nx]) {
                dts[ny][nx] = _dts;
                spath[ny][nx] = 0;
                if (TypeYX(ny,nx)!=STONE)
                    next_cs->insert(ny, nx);
            }
            spath[ny][nx] |= 1<<dirs_bit[dirs_flood_from[d]];
        }
    }
}

void calc_decision_points() {
    memset(dts, sizeof(dts), 0);
    memset(evd, sizeof(evd), 0);
    memset(def, sizeof(def), 0);
    memset(evd, sizeof(evd), 0);
    memset(spath, sizeof(spath), 0);
    cs      = &cs_a;
    next_cs = &cs_b;
    for (int y=0; y!=MAP_ROW+2; ++y) {
        dts[y][0] = DTS_MAX;
        dts[y][MAP_COL+1] = DTS_MAX;
    }
    for (int x=0; x!=MAP_COL+2; ++x) {
        dts[0][x] = DTS_MAX;
        dts[MAP_ROW+1][x] = DTS_MAX;
    }

    for (int y=1; y<=MAP_ROW; ++y) {
        for (int x=1; x<=MAP_COL; ++x) {
            if (map[y][x].type == HOME) {
                if (map[y][x].home.group != grpid)
                    enemy_home = COORD(y, x);
                if (map[y][x].home.group == grpid)
                    my_home = COORD(y, x);
            }
            if (map[y][x].type == BOMB) {
                detonate(y, x);
            }
        }
    }

    /* take care for positions that can bomb enemy_home directly!
     * their dts is 1 */
    cs->insert(enemy_home);
    dts[enemy_home.y][enemy_home.x] = 1;
    while (cs->size()) {   // begin flood
        next_cs->clear();
        cs->for_each2( [&](int y, int x){
            flood_dts(y, x);
        });
        //dumpDTS();
        std::swap(cs, next_cs);
    }

    // flood SPath's cell reference
    cs->clear();
    cs->insert(my_home);
    while (cs->size()) {
        next_cs->clear();
        cs->for_each2( [&](int y, int x){
            for (int d=0; d!=4; ++d)
                if (spath[y][x] & (1<<dirs_bit[d])) {
                    int sy=y, sx=x;
                    dirs[d](sy, sx);
                    spath[sy][sx] |= 1<<(8+dirs_bit[d]);
                    next_cs->insert(sy, sx);
                }
        });
        std::swap(cs, next_cs);
    }
    dumpEvd();
    dumpDTS();
    dumpSPath();
}

Operator normal_decide(Player p) {
    /* normally, bomber choose a position with least DTS
     * if put a bomb in current position can destroy a WOOD with less DTS, place bomb
     */
    DBG("Normal decision\n");
    set<Operator> acc_moves;
    #define LESS_DTS(x,y,d) ( y>0 && y<=MAP_ROW && x>0 && x<=MAP_COL && dts[x][y] && dts[y][x]<(d) )
    #define PRESERVED_WOOD(Y,X) (abs((X)-my_home.x)<=BOMB_POWER && abs((Y)-my_home.y)<=BOMB_POWER)
    #define STRIKE_POS(Y,X) (abs((X)-enemy_home.x)<=BOMB_POWER && abs((Y)-enemy_home.y)<=BOMB_POWER)
    int pY = p.pos.y, pX = p.pos.x;
    i32 cur_dts = dts[pY][pX];
    /* check Op-Put, if we can destroy a WOOD on Shortest-Path
     *                  and WOOD is not a barrier (sheilds HQ from bombs, Preserved WOOD) */
    for (int d=0; d!=4; ++d) {
        int y=p.pos.y, x=p.pos.x;
        for (int i=0; i!=BOMB_POWER; ++i) {
            dirs[d](y, x);
            DBG("ckk PUT: %2d, %2d:  %s %s %s %s %s,  ", y, x,
                        SPATH_WOOD(y,x)?"SPWOOD":"      ",
                        !PRESERVED_WOOD(y,x)?"CTBLO":"     ",
                        DTS(y,x)<cur_dts?"DTS<Cur":"       ",
                        !evd[y][x]?"NBOMB":"     ",
                        STRIKE_POS(pY,pX)?"STRIKE":"      ");
            if (SPATH_WOOD(y,x) && !PRESERVED_WOOD(y,x)
                && DTS(y,x)<cur_dts
                && (evd[y][x]==0 || STRIKE_POS(pY,pX)))
            {
                DBG("SHOULD_PUT");
                acc_moves.insert(PUT);
            }
            DBG("\n");
        }
    }

    // check Op-Move
    for (int d=0; d!=4; ++d) {
        int y=p.pos.y, x=p.pos.x;
        dirs[d](y, x);
        if (LESS_DTS(x, y, cur_dts) && TypeYX(y,x)==BLANK) {
            if (!kamakaze && (evd[y][x]==0 || evd[y][x]> EVADE_TTL_THRESHOLD)) {
                // We are not Japanese, so value our life, don't suicide
                acc_moves.insert(dirs_op[d]);
            }
        }
    }

    std::for_each(acc_moves.begin(), acc_moves.end(), [](Operator op){
        DBG("%s\n", OpStr(op));
    });
    // choose non-opposite, put whenever possible
    if (acc_moves.count(PUT)) {
        return PUT;
    }
    if (acc_moves.count(LEFT) && acc_moves.count(RIGHT)) {
        if (acc_moves.count(UP))   return UP;
        if (acc_moves.count(DOWN)) return DOWN;
        return LEFT;
    } else if (acc_moves.count(UP) && acc_moves.count(DOWN)) {
        if (acc_moves.count(LEFT))  return LEFT;
        if (acc_moves.count(RIGHT)) return RIGHT;
        return UP;
    } else {
        return acc_moves.begin()!=acc_moves.end() ? *acc_moves.begin() : NOP;
    }
    return NOP;
}

Operator kamakaze_decide(Player p) {
    return NOP;
}

void AI(const Game *game, Operator op[PLAYER_NUM]) {
    // initialize global variables
    grpid = game->grpid;
    player_op = op;
    player = game->group[grpid].player;
    memcpy(map, game->map, sizeof(map));

    // flood DTS
    calc_decision_points();
    // decide if we should put bombs!
    for (int np=0; np!=PLAYER_NUM; ++np) {
        op[np] = kamakaze
                 ? kamakaze_decide(player[np])
                 : normal_decide(player[np]);
        DBG("Decided: %c, From %2d, %2d, %s\n", 'A'+np, player[np].pos.y, player[np].pos.x, OpStr(op[np]));
    }
}
