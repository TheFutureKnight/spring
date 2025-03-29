---
layout: default
title: SendToUnsynced
parent: Lua API
permalink: lua-api/globals/SendToUnsynced
---

# global SendToUnsynced


```lua
function SendToUnsynced(...: (nil|boolean|number|string))
```
@param `...` - Arguments. Typically the first argument is the name of a function to call.







Invoke `UnsyncedCallins:RecvFromSynced` callin with the given arguments.

[<a href="https://github.com/TheFutureKnight/spring/blob/d7637b4bc0400526485e3a78f121ae0c73f73707/rts/Lua/LuaHandleSynced.cpp#L1961-L1967" target="_blank">source</a>]
 See: UnsyncedCallins:RecvFromSynced


