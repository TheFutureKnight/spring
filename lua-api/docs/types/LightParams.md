---
layout: default
title: LightParams
parent: Lua API
permalink: lua-api/types/LightParams
---

# class LightParams





Parameters for lighting

[<a href="https://github.com/TheFutureKnight/spring/blob/f0c829c6e793112ab93b1ec454b2e6f2a1767d4c/rts/Lua/LuaUnsyncedCtrl.cpp#L1567-L1593" target="_blank">source</a>]

---



## fields
---

### LightParams.ambientColor
---
```lua
LightParams.ambientColor : { green: number,blue: number,red: number }
```




### LightParams.ambientDecayRate
---
```lua
LightParams.ambientDecayRate : { ambientGreenDecay: number,ambientBlueDecay: number,ambientRedDecay: number }
```



Per-frame decay of `ambientColor` (spread over TTL frames)



### LightParams.decayFunctionType
---
```lua
LightParams.decayFunctionType : { ambientDecayType: number,specularDecayType: number,diffuseDecayType: number }
```



If value is `0.0` then the `*DecayRate` values will be interpreted as linear, otherwise exponential.



### LightParams.diffuseColor
---
```lua
LightParams.diffuseColor : { red: number,green: number,blue: number }
```




### LightParams.diffuseDecayRate
---
```lua
LightParams.diffuseDecayRate : { diffuseBlueDecay: number,diffuseRedDecay: number,diffuseGreenDecay: number }
```



Per-frame decay of `diffuseColor` (spread over TTL frames)



### LightParams.direction
---
```lua
LightParams.direction : { dx: number,dy: number,dz: number }
```




### LightParams.fov
---
```lua
LightParams.fov : number
```




### LightParams.ignoreLOS
---
```lua
LightParams.ignoreLOS : boolean
```




### LightParams.intensityWeight
---
```lua
LightParams.intensityWeight : { specularWeight: number,ambientWeight: number,diffuseWeight: number }
```




### LightParams.position
---
```lua
LightParams.position : { pz: number,py: number,px: number }
```




### LightParams.priority
---
```lua
LightParams.priority : number
```




### LightParams.radius
---
```lua
LightParams.radius : number
```




### LightParams.specularColor
---
```lua
LightParams.specularColor : { blue: number,red: number,green: number }
```




### LightParams.specularDecayRate
---
```lua
LightParams.specularDecayRate : { specularGreenDecay: number,specularBlueDecay: number,specularRedDecay: number }
```



Per-frame decay of `specularColor` (spread over TTL frames)


### LightParams.ttl
---
```lua
LightParams.ttl : number
```




