# v4d_project
Vulkan4D New Project sample

Clone this repository to create a new game project using the Vulkan4D Engine

# Vulkan4D
Vulkan4D is a revolutionary game engine built from the ground up for Space Games/Simulations and with Vulkan as the sole rendering API, so that we can take full advantage of the new technology. 

## Features (will include but not limited to) : 
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

1. Thou shall obey physics. We do not sacrifice nor compromise realism for arcade-style gameplays.  
2. Thou shall not hit invisible walls. If it looks accessible, then it is. 
3. Thou shall not see a skybox. Actual clouds, stars, etc.  
4. Thou shall not see "Loading". Immersive, seamless transitions, no loading screens, EVER.  
5. Thou shall be within thy self. Focused on fully immersive, first person view. 

## Project Structure
- `Core Modules and Helpers` Compiled into `v4d.dll` and linked into the Launcher
- `Systems` Game functionalities (and mods) compiled into individual .dll files that are loaded at runtime
- `Libraries` Other libraries used in the project
- `Resources` Icons, Textures, Music, ...
- `Launcher` App that puts it all together to run the game

## License
V4D is going to be Open-source, with limited commercial use. 
Developers/companies that contribute to its development may get a free license for commercial use depending on the scale of their contribution (funding or development time). 

