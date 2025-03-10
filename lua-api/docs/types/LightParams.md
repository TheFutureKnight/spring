---
layout: default
title: LightParams
parent: Lua API
permalink: lua-api/types/LightParams
---

# class LightParams





Parameters for lighting

[<a href="https://github.com/TheFutureKnight/spring/blob/95d591b7c91f26313b58187692bd4485b39cb050/rts/Lua/LuaUnsyncedCtrl.cpp#L1567-L1593" target="_blank">source</a>]

---



## fields
---

### LightParams.ambientColor
---
```lua
LightParams.ambientColor : { red: number,blue: number,green: number }
```




### LightParams.ambientDecayRate
---
```lua
LightParams.ambientDecayRate : { ambientRedDecay: number,ambientGreenDecay: number,ambientBlueDecay: number }
```



Per-frame decay of `ambientColor` (spread over TTL frames)



### LightParams.decayFunctionType
---
```lua
LightParams.decayFunctionType : { diffuseDecayType: number,ambientDecayType: number,specularDecayType: number }
```



If value is `0.0` then the `*DecayRate` values will be interpreted as linear, otherwise exponential.



### LightParams.diffuseColor
---
```lua
LightParams.diffuseColor : { green: number,red: number,blue: number }
```




### LightParams.diffuseDecayRate
---
```lua
LightParams.diffuseDecayRate : { diffuseGreenDecay: number,diffuseBlueDecay: number,diffuseRedDecay: number }
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
LightParams.intensityWeight : { diffuseWeight: number,specularWeight: number,ambientWeight: number }
```




### LightParams.position
---
```lua
LightParams.position : { pz: number,px: number,py: number }
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
LightParams.specularColor : { green: number,blue: number,red: number }
```




### LightParams.specularDecayRate
---
```lua
LightParams.specularDecayRate : { specularBlueDecay: number,specularRedDecay: number,specularGreenDecay: number }
```



Per-frame decay of `specularColor` (spread over TTL frames)


### LightParams.ttl
---
```lua
LightParams.ttl : number
```




