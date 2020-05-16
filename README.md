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
- [x] Hybrid Ray-Tracing Renderer module
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
* Specialized for completely dynamic or procedural worlds with user-created content
* Completely Modular and supports Modding at its core

----

## Renderer

V4D games will allow for up to three modes of rendering : 
- **Basic** : basic rasterization without shadows nor reflections
- **Hybrid** : rasterizaton with ray-traced shadows, reflections and GI
- **Let There Be Light** : 100% Path-Traced, photorealistic graphics

----

## Is Vulkan4D a good choice for your project ?

You should use V4D if : 

* You are making a realistic space game with full size planets
* You are making a large scale open-world MMO
* You want high fidelity graphics with Ray Tracing
* You need double precision physics
* You want your game to support Modding
* Your game world is very dynamic and cannot afford having baked lighting/assets

You should **NOT** use V4D if : 

* You are making a game for Mobile devices
* You are making an Console game
* You are/have level designers who like to drag and drop assets
* You want OSX support
* You want 32-bit support
* You are making a game that needs to run on integrated graphics
* You are making a top-down, 2D or 2.5D game

----

### Requirements

* Vulkan 1.2 capable dedicated GPU

----

## The WHY

Most (if not all) current game engines are optimized for fixed-map games with limited scale where level designers place assets in a scene using an editor, and very often rely on pre-baked graphics. 

This means that other game engines are not meant for Large-scale Procedural Dynamic Open-World games with lots of user-created contents. 

But this is where Vulkan4D shines !

Vulkan4D is also designed especially for realistic large scale space games/simulators. 

----

## Key Features

* Vulkan Renderer with full Ray Tracing support
* Double precision physics system
* Modular system
* Multiplayer integration tools

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

## Building a new Vulkan4D project
(Only supported on Linux at the moment)

### Dependencies
* `openssl` 1.1.0 (and `libssl-dev` package)
* `glslang` >= 8.13
* `gcc` >= 9.3
* `cmake` >= 3.10

### First Build

```bash
# from project parent directory
git clone --recursive git@github.com:Vulkan4D/v4d_project.git
cd v4d_project
tools/cleanbuild.sh
```
Unit tests will run after the build

You may want to make it your own git repository

```bash
# from project directory
tools/initNewGitRepository.sh
git remote add origin <Your Repository Url>
git push -u -f origin master
```

----

## License

Not yet determined. 

V4D is most likely going to be Open-source, with limited commercial use. 
Developers/companies that contribute to its development may get a free/reduced price license for commercial use depending on the scale and usability of their contribution. 
