Techies AI
===
The lucky winner of Play.cn BomberMan contest  
Name origin: Dota2 Techies

For BomberMan SDK, visit: [http://bomb.play.cn/](http://bomb.play.cn/)

## Strategy

1. Compute each cell's value, 
  * move to most valuable cell
  * cell_value = `func(bombers, bomb_location, cell_object, cell_location)`
  * (In code, least SPV value)
2. Determine PUT bomb or not
  * Can destroy a WOOD on shortest-path
  * Don't PUT around enemy home in the same round. (They deals 1 damage instance)

## Files

##### AI.cpp
Main AI file.
* `#define DEBUG_FLAG (true)`: turn on debug output
* `void Init()`: Initialization, comment it out when testing
* require `-std=c++11`, tested with gcc/clang.

##### runner.cpp
A simple one-step mock of game server, convenient for debugging.

##### BomberMan.h
Reference of SDK's header

## Note
Should be BUGGY! May not work all the time. **:D**  
Code is NOT written in a well-formed CPP style.  
If you can't understand the code, DON'T contact me!

## LICENSE
MIT (C) 2015, wacky6