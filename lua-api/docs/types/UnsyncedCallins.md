---
layout: default
title: UnsyncedCallins
parent: Lua API
permalink: lua-api/types/UnsyncedCallins
---

# class UnsyncedCallins





[<a href="https://github.com/TheFutureKnight/spring/blob/d7637b4bc0400526485e3a78f121ae0c73f73707/rts/Lua/LuaHandleSynced.cpp#L166-L169" target="_blank">source</a>]

Functions called by the Engine (Unsynced).



## methods


### UnsyncedCallins.RecvFromSynced

```lua
function UnsyncedCallins.RecvFromSynced(...: any)
```





Receives data sent via `SendToUnsynced` callout.

[<a href="https://github.com/TheFutureKnight/spring/blob/d7637b4bc0400526485e3a78f121ae0c73f73707/rts/Lua/LuaHandleSynced.cpp#L177-L181" target="_blank">source</a>]


### UnsyncedCallins.DrawUnit

```lua
function UnsyncedCallins.DrawUnit(
  unitID: integer,
  drawMode: number
) -> suppressEngineDraw boolean
```





For custom rendering of units

[<a href="https://github.com/TheFutureKnight/spring/blob/d7637b4bc0400526485e3a78f121ae0c73f73707/rts/Lua/LuaHandleSynced.cpp#L208-L214" target="_blank">source</a>]


### UnsyncedCallins.DrawFeature

```lua
function UnsyncedCallins.DrawFeature(
  featureID: integer,
  drawMode: number
) -> suppressEngineDraw boolean
```





For custom rendering of features

[<a href="https://github.com/TheFutureKnight/spring/blob/d7637b4bc0400526485e3a78f121ae0c73f73707/rts/Lua/LuaHandleSynced.cpp#L244-L250" target="_blank">source</a>]


### UnsyncedCallins.DrawShield

```lua
function UnsyncedCallins.DrawShield(
  featureID: integer,
  weaponID: integer,
  drawMode: number
) -> suppressEngineDraw boolean
```





For custom rendering of shields.

[<a href="https://github.com/TheFutureKnight/spring/blob/d7637b4bc0400526485e3a78f121ae0c73f73707/rts/Lua/LuaHandleSynced.cpp#L279-L286" target="_blank">source</a>]


### UnsyncedCallins.DrawProjectile

```lua
function UnsyncedCallins.DrawProjectile(
  projectileID: integer,
  drawMode: number
) -> suppressEngineDraw boolean
```





For custom rendering of weapon (& other) projectiles

[<a href="https://github.com/TheFutureKnight/spring/blob/d7637b4bc0400526485e3a78f121ae0c73f73707/rts/Lua/LuaHandleSynced.cpp#L317-L323" target="_blank">source</a>]


### UnsyncedCallins.DrawMaterial

```lua
function UnsyncedCallins.DrawMaterial(
  uuid: number,
  drawMode: number
) -> suppressEngineDraw boolean
```





[<a href="https://github.com/TheFutureKnight/spring/blob/d7637b4bc0400526485e3a78f121ae0c73f73707/rts/Lua/LuaHandleSynced.cpp#L354-L360" target="_blank">source</a>]




