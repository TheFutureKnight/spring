---
layout: default
title: CreateCommandOptions
parent: Lua API
permalink: lua-api/types/CreateCommandOptions
---

# alias CreateCommandOptions
---



```lua
(alias) CreateCommandOptions = (CommandOptionName[]|table<CommandOptionName,boolean>|CommandOptionBit|integer)
    | CommandOptionName[] -- An array of option names.
    | table<...> -- A map of command names to booleans, considered held when `true`.
    | CommandOptionBit -- A specific integer value for a command option.
    | integer -- A bit mask combination of `CommandOptionBit` values. Pass `0` for no options.

```




[<a href="https://github.com/TheFutureKnight/spring/blob/d7637b4bc0400526485e3a78f121ae0c73f73707/rts/Lua/LuaUtils.cpp#L999-L1005" target="_blank">source</a>]

