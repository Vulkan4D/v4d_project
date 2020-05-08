![Banner](res/banner2.png)

# v4d_project

Vulkan4D **DEV STATUS** : `in active development`

- [x] CMAKE cross-platform build system from linux host
- [x] Core helpers
- [x] Core IO utilities
- [x] Core Config utilities
- [x] Core Multithreading utilities
- [x] Core Cryptographic utilities
- [x] Core Data Streaming utilities
- [x] Core Networking utilities
- [x] Mudular structure
- [x] Sample Module and documentation
- [x] Core Graphics utilities (Vulkan API)
- [~] Hybrid Ray-Tracing Renderer module
- [ ] Physics utilities
- [ ] In-game coding framework (XenonCode)
- [ ] Audio utilities
- [ ] Demo game (Galaxy4D)
- [ ] Launcher template
- [ ] Build system for Visual Studio on Windows host
- [ ] Complete Engine documentation
- [ ] Editor Tools
- [ ] Licensing

Here's the Discord Server for Vulkan4D related discussions : 
https://discord.gg/5aY3ZBW

----

## What is Vulkan4D ?

**Vulkan4D** is a game engine meant for large scale open worlds with high fidelity graphics. 

* Build games for 64-bit Windows and Linux platforms
* Developed from the ground up for optimal use of the Vulkan API and ray tracing
* Completely Modular and supports Modding at its core

V4D games will have three modes of rendering : 
- **Basic** : basic rasterization without shadows nor reflections (any vulkan 1.2 capable GPU)
- **Hybrid** : rasterizaton with ray-traced shadows (GTX10+ series GPU)
- **Let There Be Light** : 100% Ray-Traced, photorealistic graphics (requires RTX or RDNA2 GPU)

----

## Is Vulkan4D any good for your project ?

You should use V4D if : 

* You are making a realistic space game with full size planets
* You are making a large scale open-world MMO
* You want high fidelity graphics with Ray Tracing
* You need double precision physics
* You want your game to support Modding

You should **NOT** use V4D if : 

* You are making a game for Mobile devices
* You are a level designer who like to drag and drop assets
* You are making an XBOX game
* You want OSX support
* You want 32-bit support
* You are making a game that needs to run on integrated graphics
* You are making a top-down, 2D or 2.5D game

----

## The WHY

Most (if not all) game engines are optimized for fixed-map games with very limited physics and scale where level designers place assets in a scene using an editor. 
They are not meant for Large-scale Procedural Open-World games. 

When trying to build space games using popular game engines, we quickly fall into problems that the engine was not intended to solve, and have to do lots of workarounds, which takes most of our development time and affects performance, maintainability and stability of the resulting game. 

Also, most game engines do not implement Vulkan nor RTX from the ground up, and these features do not work well with existing technologies, causing poor performance, not taking advantage of these new technologies. 

Vulkan4D is not only the answer to all possible issues developing space games, but also future-proof, implementing new technologies and taking advantage of all the most recent hardware. 

V4D is thought from the ground up and optimized for multi-threading, easily taking full advantage of a 16-cores CPU, also achieving fully multi-threaded Rendering !

The aim of this engine is at games that require very large scales, complex physics and native Ray Tracing implementation. 

In other words, the holy grail of an engine for realistic multiplayer space games. 

----

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

----

## License

Not yet determined. 

V4D is most likely going to be Open-source, with limited commercial use. 
Developers/companies that contribute to its development may get a free/reduced price license for commercial use depending on the scale and usability of their contribution. 
