#ifndef _BOMBMAN_H_
#define _BOMBMAN_H_

#ifdef __cplusplus
extern "C" {
#endif

#define GROUP_NUM 2
#define PLAYER_NUM 3

#define GROUP_BLUE 0
#define GROUP_GREEN 1
#define GROUP_NEUTRAL 2
#define GROUP_NAME_LEN 12

#define PLAYER_A 0
#define PLAYER_B 1
#define PLAYER_C 2

#define MAP_ROW 15
#define MAP_COL 15
#define _MAP_ROW 15+2
#define _MAP_COL 15+2

#define BOMB_MAX_PER_PLAYER 3
#define PLAYER_LIFE_VALUE 3

#define HOME_POWER 5

#define BOMB_TTL 7
#define BOMB_POWER 2

#define ROUND_MAX 512

//////////////////////////////////////////////////////////////////////////
/* Define Coord Struct */
typedef struct _Coord {
    int x;
    int y;
}Coord;

/* Object Move Direction */
typedef enum _Operator {
    NOP,  // No OP
    UP,
    DOWN,
    LEFT,
    RIGHT,
    PUT,  // PUT Bomb
}Operator;

/* Object Type */
typedef enum _ObjectType{
    BLANK,
    WOOD,
    STONE,
    HOME,
    BOMB,
}ObjectType, ObjType;

/* Player(BomberMan) Struct */
typedef struct _Player {
    int group;       // group id
    int player;      // player id
    int life_value;  // Life Value
    int bomb_num;    // remainder bomb can put
    Coord pos;
}Player;

/* Bomb Object */
typedef struct _Bomb {
    int group;   // Group ID
    int player;  // Player ID
    int TTL;     // Time To Live
}Bomb;

/* Home Object */
typedef struct _Home {
    int group;  // which group
    int power;  // home energy
}Home;

/* Generic Object */
typedef struct _Object {
    Coord coord;   // Coordinate of Object
    ObjType type;  // BLANK | WOOD | STONE | HOME | BOMB
    union {
        Bomb bomb;
        Home home;
    };  // deferent obj has different captions
}Object;

/* Group struct */
typedef struct _Group {
    int group;
    int score;
    Coord pos;  // home pos
    char name[GROUP_NAME_LEN];
    Player player[PLAYER_NUM];
}Group;

/* Game struct */
typedef struct _Game {
    int grpid;  // group id
    int round;  // remainder rounds
    Group group[GROUP_NUM];
    Object map[MAP_ROW+2][MAP_COL+2];
}Game;

/* API functions */
extern int api_set_group_name(const char *str);

/* AI entry function */
extern void AI(const Game *game, Operator op[PLAYER_NUM]);

/* AI Init function */
extern void Init(void);

#ifdef __cplusplus
}
#endif  /* C++ */

#endif  /* _BOMBERMAN_H_ */
