# v4d_project
Vulkan4D New Project sample

Clone this repository to create a new game project using the Vulkan4D Engine

# Vulkan4D
Vulkan4D is a revolutionary game engine built from the ground up for Space Games/Simulations and with Vulkan as the sole rendering API, so that we can take full advantage of the new technology. 

## Features
include but not limited to: 
- procedural, full-size planet terrains, planetary systems, a full galaxy to explore.
- triple-precision physics with relative positioning system
- vulkan multithreaded renderer built from the ground up for full RTX support (ray-tracing)
- cross-platform (windows+linux) build tools
- custom SPIR-V shader compiler for GLSL with custom pragmas
- high-performance no-sql database system that takes full advantage of SSD drives
- low-latency networking system that uses both TCP and UDP simultaneously
- complex multiplayer integration tools
- orbital mechanics and newtonian physics
- dynamic sky/space background (actual stars and planets are rendered, no skybox bs)

VR may be implemented as well

## Our five commandments
As a developer for this project or for a resulting game, you will respect our 5 Commandments. 

1. **Thou shall obey physics** 
	*We will not sacrifice nor compromise realism for arcade-style gameplays.*
2. **Thou shall not see "Loading"** 
	*Immersive, seamless transitions, no loading screens, EVER.*
3. **Thou shall not see a skybox** 
	*Actual clouds, stars, planets, etc. are rendered.*
4. **Thou shall be within thy self** 
	*Focused on fully immersive, first person view.*
5. **Thou shall not hit invisible walls** 
	*What you can see, you can touch !*

### Project Structure
- `Core` Compiled into `v4d.dll` and linked into the Project
- `Helpers` Simple-but-useful header-only source files, compiled into anything that is part of V4D
- `Modules` Game functionalities (and plugins/mods) compiled into individual .dll files that are loaded at runtime
- `Libraries` Other libraries used in the project
- `Resources` Icons, Textures, Music, ...
- `Project` App that puts it all together to run the game
- `Tools` Useful tools to help programmers (build scripts, shader compiler, ...)

## License
V4D is going to be Open-source (most likely LGPL), with limited commercial use. 
Developers/companies that contribute to its development may get a free license for commercial use depending on the scale of their contribution (funding or development time). 

