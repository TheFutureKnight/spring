---
layout: default
title: Roster
parent: Lua API
permalink: lua-api/types/Roster
---

# class Roster





Roster

Contains data about a player

[<a href="https://github.com/TheFutureKnight/spring/blob/d7637b4bc0400526485e3a78f121ae0c73f73707/rts/Lua/LuaUnsyncedRead.cpp#L4140-L4152" target="_blank">source</a>]





## fields


### Roster.name

```lua
Roster.name : string
```




### Roster.playerID

```lua
Roster.playerID : integer
```




### Roster.teamID

```lua
Roster.teamID : integer
```




### Roster.allyTeamID

```lua
Roster.allyTeamID : integer
```




### Roster.spectator

```lua
Roster.spectator : boolean
```




### Roster.cpuUsage

```lua
Roster.cpuUsage : number
```



in order to find the progress, use: cpuUsage&0x1 if it's PC or BO, cpuUsage& 0xFE to get path res, (cpuUsage>>8)*1000 for the progress


### Roster.pingTime

```lua
Roster.pingTime : number
```



if -1, the player is pathfinding


