#include "BomberMan.h"
#include <set>
#include <algorithm>
#include <cstdint>
#include <cctype>
#include <cstdlib>
#include <functional>
#include <ctime>
#include <cstring>
using namespace std;

void Init(void){
	api_set_group_name("Techies");
}

#define DEBUG_FLAG (false)

#define i32 int32_t
#define min4(a,b,c,d) (std::min(std::min(a, b), std::min(c, d)))
#define DBG if(DEBUG_FLAG) printf
#define EQU_YX(yx,y,x) (yx==((y)<<16|(x)))
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
const Player *enemy_player;
Operator *player_op;
CoordSet player_pos;
long grpid;
long game_round;

bool kamakaze = false;
i32 dts[MAP_ROW+2][MAP_COL+2];    // Distance to Strike
i32 evd[MAP_ROW+2][MAP_COL+2];    // Evade Reference
i32 spv[MAP_ROW+2][MAP_COL+2];    // Shortest Path Value (YX)
i32 spath[MAP_ROW+2][MAP_COL+2];  // Shortest Path Reference (YX)
i32 dfh[MAP_ROW+2][MAP_COL+2];    // Distance From Home, backtrack to position before PUT
/* LOW 4bit:    shortest-path next cell
 * LOW 8-12bit: is pointed by other cell */

const i32 DTS_MAX  = 0x0000FFFF;
const i32 SPV_MAX  = 0x0000FFFF;
const i32 EVD_MIN  = 0x80000000;
const i32 EVD_MAX  = 0x7FFFFFFF;
const i32 DFH_MAX  = 0x7FFFFFFF;
const i32 BOMB_STACK_FACTOR = 1;
const i32 CLUSTER_DIST = 1;
const i32 CLUSTER_FACTOR = 2;
const i32 PLAYER_B_STAGE_2_ROUND = 6;
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
#define BIT_UP     (1<<0)
#define BIT_DOWN   (1<<1)
#define BIT_LEFT   (1<<2)
#define BIT_RIGHT  (1<<3)
#define BIT_VERT   (BIT_UP   | BIT_DOWN)
#define BIT_HORZ   (BIT_LEFT | BIT_RIGHT)
#define BIT_ANY    (BIT_VERT | BIT_HORZ)
#define BIT_PUT    (1<<8)
int dirs_bit[4] = {0, 1, 2, 3};
const char* dirs_str[16] = {
    "⊚ ", "↑ ", "↓ ", " V",
    "← ", "LU", "LD", "LV",
    "→ ", "RU", "RD", "RV",
    " H", "HU", "HD", "HV"
};

Coord enemy;
Coord my;
CoordSet cs_a, cs_b;
CoordSet *cs, *next_cs;
bool bomb_at_home;

// Assume map, y, x, s_yx are in the context
#define Type         ( map[y][x].type )
#define TypeYX(y,x)  ( map[y][x].type )
#define DTS(y,x)     ( dts[y][x] ? dts[y][x] : DTS_MAX )

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
void dumpDecisionMap33(i32 m[MAP_ROW+2][MAP_COL+2], F f) {
    DBG("\n   ");
    for (int x=0; x!=MAP_COL+2; ++x)
        DBG("%3d ", x);
    DBG("\n");
    for (int y=0; y!=MAP_ROW+2; ++y) {
        DBG("%3d ", y);
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
        if (v==0){
            DBG("  ");
        }else if (v>=DTS_MAX){
            DBG("##");
        }else{
            DBG("%2d", v);
        }
    } );
    //char c[1000]; gets(c);
}
void dumpDFH() {
    dumpDecisionMap( dfh, [](i32 v){
        if (v==0){
            DBG("##");
        }else if (v>=DTS_MAX){
            DBG("##");
        }else{
            DBG("%2d", v);
        }
    } );
    //char c[1000]; gets(c);
}
void dumpEvd() {
    dumpDecisionMap( evd, [](i32 v){
        if (v) {
            DBG("%2d", v);
        }else{
            DBG("  ");
        }
    });
}
void dumpSPV() {
    dumpDecisionMap3( spv, [](i32 spv, int y, int x){
        if (Type == STONE){
            DBG("%s", "##");
        }else{
            DBG("%2d", spv);
        }
    });
}
void dumpSPath() {
    dumpDecisionMap33( spath, [](i32 bit_d, int y, int x){
        if (Type == STONE){
            DBG("%s", "###");
        }else if (Type==WOOD){
            DBG("%c%s", 'X', dirs_str[bit_d&15]);
        }else if (Type==BOMB){
            DBG("%c%s", '0'+map[y][x].bomb.TTL, dirs_str[bit_d&15]);
        }else if (Type==HOME){
            DBG("%s%s", "H", dirs_str[bit_d&15]);
        }else{
            DBG(".%s", dirs_str[bit_d&15]);
        }
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
void detonate(int _y, int _x, CoordSet* detonated=0) {
    static i32 min_ttl;
    if (!detonated) {
        min_ttl = 65535;
        auto detonated = new CoordSet();
        detonate(_y, _x, detonated);
        // detonate all bombs!
        detonated->for_each2( [&](int dy, int dx){
            evd[dy][dx] = evd[dy][dx] + BOMB_STACK_FACTOR*(BOMB_POWER) /* +min_ttl */;
            for (int d=0; d!=4; ++d) {
                int y=dy, x=dx;
                for (int i=0; i!=BOMB_POWER; ++i) {
                    dirs[d](y, x);
                    if (Type==STONE || Type==HOME || Type==WOOD)
                        break;
                    evd[y][x] = min(evd[y][x]?evd[y][x]:EVD_MAX, evd[y][x]+BOMB_STACK_FACTOR*(BOMB_POWER-i)+min_ttl);
                }
            }
        } );
        delete detonated;
    }else{
        if (detonated->has(_y, _x)) return;
        detonated->insert(_y, _x);
        min_ttl = min(min_ttl, map[_y][_x].bomb.TTL);
        for (int d=0; d!=4; ++d) {
            int y=_y, x=_x;
            for (int i=0; i!=BOMB_POWER; ++i) {
                dirs[d](y, x);
                if (Type == STONE || Type == HOME || Type == WOOD)
                    break;
                if (Type == BOMB)
                    detonated->insert(y, x);
            }
        }
    }
}

void flood_dts(int y, int x) {
    i32 _dts = DTS_MAX;       // calculated dts for cell [y,x]
    //DBG("%2d, %2d   ", y, x);
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
        //DBG("_dts: %2d\n", _dts);
        if ( _dts <= DTS(ny,nx) ) {
            dts[ny][nx] = _dts;
            if (TypeYX(ny,nx)!=STONE)
                next_cs->insert(ny, nx);
        }
    }
}

void flood_dfh(int y, int x) {
    for (int d=0; d!=4; ++d) {
        int ny=y, nx=x;
        dirs[d](ny, nx);
        if (TypeYX(ny,nx)==STONE) continue;
        if (dfh[ny][nx]==0) {
            dfh[ny][nx] = dfh[y][x]+(TypeYX(ny,nx)==WOOD?3:1);
            next_cs->insert(ny,nx);
        }
    }
}

void avoid_cluster() {
    for (int p=0; p!=3; ++p) {
        int pY = player[p].pos.y;
        int pX = player[p].pos.x;
        evd[pY][pX] += CLUSTER_FACTOR*(CLUSTER_DIST+1);
        for (int d=0; d!=4; ++d) {
            int y=pY, x=pX;
            for (int i=0; i!=CLUSTER_DIST; ++i) {
                dirs[d](y,x);
                if (Type==BLANK || Type==WOOD)
                    evd[y][x] += CLUSTER_FACTOR*(CLUSTER_DIST-i);
            }
        }
    }
}

void calc_decision_points() {
    memset(dts, sizeof(dts), 0);
    memset(evd, sizeof(evd), 0);
    memset(spv, sizeof(evd), 0);
    memset(dfh, sizeof(dfh), 0);
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
                    enemy = COORD(y, x);
                if (map[y][x].home.group == grpid)
                    my = COORD(y, x);
            }
            if (map[y][x].type == BOMB) {
                detonate(y, x);
            }
        }
    }
    
    avoid_cluster();

    // flood DTS
    /* take care for positions that can bomb enemy directly!
     * their dts is 1 */
    cs->insert(enemy);
    dts[enemy.y][enemy.x] = 1;
    for (int d=0; d!=4; ++d) {
        int ny=enemy.y, nx=enemy.x;
        for (int i=0; i!=BOMB_POWER; ++i) {
            dirs[d](ny,nx);
            if (TypeYX(ny,nx)==BLANK)
                dts[ny][nx] = 1;
            else
                break;
        }
    }
    while (cs->size()) {
        next_cs->clear();
        cs->for_each2( [&](int y, int x){
            flood_dts(y, x);
        });
        //dumpDTS();
        std::swap(cs, next_cs);
    }

    // flood DFH
    cs->insert(my);
    dfh[my.y][my.x] = 1;
    while (cs->size()) {
        next_cs->clear();
        cs->for_each2( [&](int y, int x){
            flood_dfh(y, x);
        });
        std::swap(cs, next_cs);
    }
    
    // overlay evd
    for (int y=0; y!=MAP_ROW+2; ++y)
        for (int x=0; x!=MAP_COL+2; ++x)
            spv[y][x] = dts[y][x] + evd[y][x];
           
    // calculate SPath
    for (int y=0; y!=MAP_ROW+2; ++y)
        for (int x=0; x!=MAP_COL+2; ++x) {
            if (Type==STONE || Type==HOME) continue;
            #define SPV(y,x) (spv[y][x] ? spv[y][x] : SPV_MAX)
            i32 min_spv = SPV(y,x);
            for (int d=0; d!=4; ++d) {
                int ny=y, nx=x;
                dirs[d](ny,nx);
                if (SPV(ny,nx) <= min_spv &&
                    (TypeYX(ny,nx)==WOOD || TypeYX(ny,nx)==BLANK)
                ) {
                    if (SPV(ny,nx) < min_spv)
                        spath[y][x] = 0;
                    min_spv = SPV(ny,nx);
                    spath[y][x] |= (1<<dirs_bit[d]);
                }
            }
        }
        
    /* Don't walk into a WOOD in SPath, if we have alternatives */
    for (int y=0; y!=MAP_ROW+2; ++y)
        for (int x=0; x!=MAP_COL+2; ++x) {
            int num_of_choice = 0;
            int num_of_wood   = 0;
            i32 wood_mask     = 0;
            for (int d=0; d!=4; ++d) {
                if ( (spath[y][x] & (1<<dirs_bit[d])) == (1<<dirs_bit[d]) ) {
                    num_of_choice++;
                    int ny=y, nx=x;
                    dirs[d](ny,nx);
                    if (TypeYX(ny,nx)==WOOD) {
                        num_of_wood++;
                        wood_mask |= (1<<dirs_bit[d]);
                    }
                }
                if (num_of_choice > num_of_wood) {
                    spath[y][x] &= ~wood_mask;
                }
            }
        }

    DBG("SPath\n");
    dumpSPath();
    // flood SPath's cell reference, remove non-BLANK target's SPath bit
    for (int y=0; y!=MAP_ROW+2; ++y)
        for (int x=0; x!=MAP_COL+2; ++x) {
            for (int d=0; d!=4; ++d) {
                if (spath[y][x] & (1<<dirs_bit[d])) {
                    int ny=y, nx=x;
                    dirs[d](ny, nx);
                    spath[ny][nx] |= (1<<(8+dirs_bit[d]));
                    if (TypeYX(ny,nx)!=BLANK)
                        spath[y][x] &= ~(1<<dirs_bit[d]);
                }
            }
        }
        
    //dumpEvd();
    DBG("DTS\n");
    dumpDTS();
    DBG("SPV\n");
    dumpSPV();
    DBG("SPV Path\n");
    dumpSPath();
    //dumpDFH();
}

Operator normal_decide(Player p) {
    int pid = p.player;
    /* normally, bomber choose a position with least DTS
     * if put a bomb in current position can destroy a WOOD with less DTS, place bomb
     */
    DBG("Normal decision\n");
    
    #define PRESERVED_WOOD(Y,X) (abs((X)-my.x)<=BOMB_POWER && abs((Y)-my.y)<=BOMB_POWER)
    #define STRIKE_POS(Y,X) (     \
           ( abs((X)-enemy.x)<=BOMB_POWER && Y==enemy.y )    \
        || ( abs((Y)-enemy.y)<=BOMB_POWER && X==enemy.x )    \
    )
    // WOOD is on Shortest-Path, if neighbour points to it (I guessed :D)
    //                           and DTS(WOOD) < DTS(cur)    // checked in decision maker
    #define SPATH_WOOD(y,x) ( map[y][x].type==WOOD && (spath[y][x]&0x00000F00) )
    int pY = p.pos.y, pX = p.pos.x;
    i32 cur_dts = dts[pY][pX];
    i32 opt_move = spath[pY][pX] & 15;   // move options
    /* check Op-Put, if we can destroy a WOOD on Shortest-Path
     *                  and WOOD is not a barrier (sheilds HQ from bombs, Preserved WOOD) */
    for (int d=0; d!=4; ++d) {
        int y=p.pos.y, x=p.pos.x;
        for (int i=0; i!=BOMB_POWER; ++i) {
            dirs[d](y, x);
            bool blocked = Type==STONE || Type==BOMB;
            if (blocked) break;
            /*
            DBG("ckk PUT: %2d, %2d:  %s %s %s %s %s,  ", y, x,
                        SPATH_WOOD(y,x)?"SPWOOD":"      ",
                        !PRESERVED_WOOD(y,x)?"CTBLO":"     ",
                        DTS(y,x)<cur_dts?"DTS<Cur":"       ",
                        !evd[y][x]?"NBOMB":"     ",
                        STRIKE_POS(pY,pX)?"STRIKE":"      ");
            */
            if (SPATH_WOOD(y,x) && !PRESERVED_WOOD(y,x)
                && DTS(y,x)<cur_dts
                && (evd[y][x]==0 || STRIKE_POS(pY,pX)))
            {
                //DBG("SHOULD_PUT");
                opt_move |= BIT_PUT;
            }
            //DBG("\n");
        }
    }
    
    if (DTS(pY,pX)==1) 
        opt_move |= BIT_PUT;

    // We just put a bomb, choose direction so we can go to a safe position!!
    if (TypeYX(pY,pX)==BOMB) {
        DBG("Bomb Backtrack %c\n", 'A'+p.player);
        for (int d=0; d!=4; ++d) {
            int ny=pY, nx=pX;
            dirs[d](ny,nx);
            if (TypeYX(ny,nx)==BLANK && dfh[ny][nx]<dfh[pY][pX]) {
                opt_move = (1<<dirs_bit[d]);
            }
        }
    }

    // consider move away from home, if round < 5
    // a hack to Jammed Initial position for Bomber B/C, around the preserved woods
    if ((opt_move&15)==0 && game_round<5) {
        DBG("Patch starting round NOOP\n");
        for (int d=0; d!=4; ++d) {
            int ny=pY, nx=pX;
            dirs[d](ny,nx);
            if (TypeYX(ny,nx)==BLANK && dfh[ny][nx]>dfh[pY][pX])
                opt_move = (1<<dirs_bit[d]);
        }
    }

    // check Op-Move
    DBG("Acceptable Op: %x\n", opt_move);
    #define isOP(bit)  ((opt_move & BIT_##bit) == BIT_##bit)
    if ( TypeYX(pY,pX)!=BOMB && isOP(PUT) ) {
        // avoid placing two bombs that bombs enemy home in the same round!
        if (STRIKE_POS(pY,pX)) {
            if (!bomb_at_home) {
                bomb_at_home = true;
                return PUT;
            }else{
                return NOP;
            }
        }
        return PUT;
    }
    if ( isOP(ANY) )
        return pid%2 ? (pid%2 ? LEFT : RIGHT) : (pid%2 ? UP : DOWN);
    if ( isOP(HORZ) ) {
        if (isOP(UP))   return UP;
        if (isOP(DOWN)) return DOWN;
        return pid%2 ? LEFT : RIGHT;
    }
    if ( isOP(VERT) ) {
        if (isOP(LEFT))  return LEFT;
        if (isOP(RIGHT)) return RIGHT;
        if (p.player==PLAYER_B)
            return rand()%2 ? UP : DOWN;
        return pid%2 ? UP : DOWN;
    }
    if (isOP(LEFT))  return LEFT;
    if (isOP(RIGHT)) return RIGHT;
    if (isOP(UP))    return UP;
    if (isOP(DOWN))  return DOWN;

    return NOP;
}

void AI(const Game *game, Operator op[PLAYER_NUM]) {
    srand(time(0));
    DBG("\n\nRound: %d\n", ROUND_MAX - game->round);
    // initialize global variables
    grpid = game->grpid;
    player_op = op;
    player = game->group[grpid].player;
    enemy_player = game->group[grpid==1?0:1].player;
    game_round  = ROUND_MAX - game->round;
    memcpy(map, game->map, sizeof(map));
    for (int p=0; p!=PLAYER_NUM; ++p)
        player_pos.insert(player[p].pos.y, player[p].pos.x);

    // figure out decision points (SPV, SPath)
    calc_decision_points();
    
    // decide if we should put bombs!
    // avoid place 2 bombs placed near enemy in same round
    // because: 2 bombs detonates at the same time only cause 1 HP loss
    bomb_at_home = false; 
    int alive_count = 0;
    for (int np=0; np!=PLAYER_NUM; ++np) {
        op[np] = normal_decide(player[np]);
        if (player[np].life_value>0) 
            alive_count++;
        DBG("Decided: %c, From %2d, %2d, %s\n", 'A'+np, player[np].pos.y, player[np].pos.x, OpStr(op[np]));
    }
    
    // if A or C is at Y==8, X=[enemy-2], backtrack(up/down), or we end up in infinite loop!
    if (   player[PLAYER_A].pos.y==(MAP_ROW+2)/2 
        && abs(player[PLAYER_A].pos.x-enemy.x)==2
    ){
        int aY = player[PLAYER_A].pos.y;
        int aX = player[PLAYER_A].pos.x;
        if (TypeYX(aY-1, aX)==BLANK)
            op[PLAYER_A] = UP;
    }
    if (   player[PLAYER_C].pos.y==(MAP_ROW+2)/2
        && abs(player[PLAYER_C].pos.x-enemy.x)==2
    ){
        int cY = player[PLAYER_C].pos.y;
        int cX = player[PLAYER_C].pos.x;
        if (TypeYX(cY+1, cX)==BLANK)
            op[PLAYER_C] = DOWN;
    }
    
    // Halt B's operation after initial startup
    // or B will block A,C's offensive path, resulting in a infinite loop
    // TODO: issues with avoid_cluster() algorithm.
    if (game_round > PLAYER_B_STAGE_2_ROUND) {
        if (alive_count>=3) {
            // keep moving B to preserved wood, if we have A,C on offensive
            // let's buy HQ some time if enemy bombs it.
            int bY = player[PLAYER_B].pos.y;
            int bX = player[PLAYER_B].pos.x;
            int dltX = 0;
            if (bX - my.x > 0) {        // Move left
                op[PLAYER_B] = LEFT;
                dltX = -1;
            }else if (my.x - bX > 0) {  // Move right
                op[PLAYER_B] = RIGHT;
                dltX = 1;
            }else {
                op[PLAYER_B] = NOP;
            }
            // don't walk into WOOD/STONE, in case Benchmarker gets insane. 
            if (TypeYX(bX+dltX, bY)==WOOD && TypeYX(bX+dltX, bY)==STONE) {
                op[PLAYER_B] = NOP; 
            }
        }
    }
}
