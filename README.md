# 3D Modern City Simulation with Dynamic Environment

## Overview

This project is a real-time **3D modern city simulation** developed using OpenGL. The simulation demonstrates fundamental and advanced computer graphics concepts including lighting models, hierarchical modeling, texture mapping, procedural animation, Bézier curves, fractals, and multiple camera systems.

The environment contains two cities separated by a river connected by a bridge and includes dynamic lighting, animated vehicles, water simulation, aircraft motion, and interactive camera navigation.

---

## Demo Video

**Watch the project demo:**
https://www.youtube.com/watch?v=YOUR_VIDEO_ID

---

## Key Features

### Scene Environment
- Two city environments placed along the X axis
- River with animated water effects
- Highway bridge connection
- Sky dome with day and night modes
- Garden, park, and urban infrastructure

### Geometry and Modeling
- Cube primitives for buildings and structures
- Cylinders for towers, stadiums, and columns
- Spheres for sky dome, trees, and lighting objects
- Hierarchical modeling for vehicles and complex structures
- Curves and procedural surfaces for advanced shapes

### Lighting System
The project implements a complete lighting system consisting of:

- 1 Directional light (Sun/Moon simulation)
- 8 Point lights (street lighting)
- 12 Spot lights (stadium and vehicle headlights)

Each light supports:
- Ambient component
- Diffuse component (Lambert shading)
- Specular component (Blinn-Phong model)

Lighting components can be toggled independently for analysis.

### Texture System
Multiple texture modes are supported:

- Texture disabled
- Simple texture replacement
- Blended texture mode
- Vertex color mode
- Procedural fragment coloring

Additional texture applications include:
- Animated water textures
- Building textures
- Sky textures
- Vehicle textures

### Animation System
Dynamic animations include:

- Moving highway vehicles
- Ships moving along the river
- Aeroplane following a world path
- Procedural water waves
- Night sky glitter effects
- Traffic control toggle

### Camera and Interaction
The project includes:

- FPS style free camera
- Orbit camera mode
- Field of view zoom
- Preset camera views
- Four viewport diagnostic mode

---

## Technologies Used

### Programming
- C++17

### Graphics
- OpenGL 3.3 Core Profile
- GLSL 330

### Libraries
- GLFW (window and input)
- GLAD (OpenGL loader)
- GLM (mathematics)
- stb_image (texture loading)

### Shaders
- vertexShaderForPhongShading.vs
- fragmentShaderForPhongShading.fs
- vertexShader.vs
- fragmentShader.fs

---

## Controls

### Camera Movement

| Key | Function |
|-----|----------|
| W | Move forward |
| S | Move backward |
| A | Move left |
| D | Move right |
| E | Move upward |
| R | Move downward |
| X | Pitch rotation |
| Y | Yaw rotation |
| Z | Roll rotation |
| SHIFT + X/Y/Z | Reverse rotation direction |
| F | Orbit camera |
| Mouse Wheel | Zoom (FOV change) |

### Preset Views (Press 8)

| Mode | Description |
|------|-------------|
| Isometric | Overview camera |
| Top View | Bird’s eye view |
| Front View | Elevated front camera |
| Normal | Default camera |

### Lighting Controls

| Key | Function |
|-----|----------|
| 1 | Toggle directional light |
| 2 | Toggle point lights |
| 3 | Toggle spot lights |
| 5 | Toggle ambient lighting |
| 6 | Toggle diffuse lighting |
| 7 | Toggle specular lighting |
| L | Toggle day/night mode |

### System Controls

| Key | Function |
|-----|----------|
| 4 | Four viewport mode |
| 9 | Change texture mode |
| T | Toggle traffic movement |
| ESC | Exit program |

---

## Four Viewport Diagnostic Mode

Pressing key **4** divides the screen into four sections:

| Section | Display |
|---------|---------|
| Top Left | Full lighting |
| Top Right | Ambient only |
| Bottom Left | Diffuse only |
| Bottom Right | Directional lighting only |

This mode demonstrates viewport transformations and lighting component separation.

---

## Viewing and Projection

### View Matrix
Camera is implemented using:

glm::lookAt()

Used for:
- Free camera
- Preset camera views
- Split viewport cameras

### Projection Matrix

Custom perspective projection:

myPerspective(fov, aspect, near, far)

Implements a standard perspective projection matrix.

### Model Transformations

Objects use standard transformation order:

Translate → Rotate → Scale

Applied per object.

---

## Scene Objects

### Basic Primitives

Cubes:
- Buildings
- Bridge structures
- Vehicles
- Road barriers

Cylinders:
- Towers
- Stadium structures
- Tree trunks
- Ship hulls

Spheres:
- Sky dome
- Tree leaves
- Light bulbs
- Tower tops

---

## Major Structures

### Burj Khalifa Style Tower
Features:
- Stepped tower design
- Cylinder stacking
- Wing extensions
- Spire structure
- Night emissive lighting

### Cricket Stadium
Features:
- Circular seating design
- Flood lighting
- Textured field

### Eiffel Style Tower
Features:
- Multi-level lattice structure
- Cross bracing
- Observation platforms

### Jamuna Future Park Inspired Mall
Features:
- Bézier curved facade
- Column grid
- Parking ramp ruled surfaces
- Window systems

### Bridge Structure
Features:
- Textured road deck
- Suspension cables
- Parabolic cable sag
- Support towers

### Vehicles
Includes:
- Cars
- Buses
- Trucks
- Ambulance

Uses hierarchical transformations.

### Ships
Includes:
- Cargo ships
- Passenger ships

Features:
- Bézier hull profiles
- River path animation

### Aeroplane
Features:
- Textured model
- Continuous motion path
- Time based animation

### Trees
Features:
- Recursive fractal branching
- Procedural generation
- Leaf spheres

---

## Mathematical Concepts Used

### Cubic Bézier Curve

Used for:
- Ship hull shapes
- Bridge elevation
- Mall facade curves

Equation:

B(t) = (1−t)³P0 + 3(1−t)²tP1 + 3(1−t)t²P2 + t³P3

Implemented in:

bezier3(P0,P1,P2,P3,t)

---

### Parabolic Cable Equation

Used for bridge cable sag:

y(t) = ytop − 4s t(1−t)

Provides smooth cable curvature.

---

### Ruled Surfaces

Used for parking ramps.

Constructed using:
- Linear interpolation
- Connected cube segments

---

### Fractal Trees

Recursive subdivision:

Each branch generates three child branches until depth limit.

---

### Water Wave Equation

Vertex displacement uses sinusoidal waves:

Δy = Ax sin(kx x + ωt)
   + Az sin(kz z − ωt)
   + Ac cos(kx x − kz z + ωt)

---

## Animation Summary

| Object | Animation Method |
|--------|------------------|
| Vehicles | Position update over time |
| Ships | River path translation |
| Aeroplane | Time based world movement |
| Water | Vertex wave displacement |
| Sky | Procedural glitter effect |

---

## Texture Modes

| Mode | Description |
|------|-------------|
| 0 | No texture |
| 1 | Simple texture |
| 2 | Blended texture |
| 3 | Vertex color |
| 4 | Procedural fragment color |

---

## Screenshots

<div align="center">

<table>

<tr>
<td align="center"><b>Bridge overview</b><br><img src="3D Modern City Final Version/screenshots/1_bridge_view.png" width="240"></td>
<td align="center"><b>Isometric preset</b><br><img src="3D Modern City Final Version/screenshots/2_iso_metric_view.png" width="240"></td>
<td align="center"><b>Top bird view</b><br><img src="3D Modern City Final Version/screenshots/3_birds_view.png" width="240"></td>
</tr>

<tr>
<td align="center"><b>Front view</b><br><img src="3D Modern City Final Version/screenshots/4_front_view.png" width="240"></td>
<td align="center"><b>Street view</b><br><img src="3D Modern City Final Version/screenshots/5_inside_view.png" width="240"></td>
<td align="center"><b>Tower structure</b><br><img src="3D Modern City Final Version/screenshots/6_burj_khalifa.png" width="240"></td>
</tr>

<tr>
<td align="center"><b>Cricket stadium</b><br><img src="3D Modern City Final Version/screenshots/7_cricket_stadium.png" width="240"></td>
<td align="center"><b>Lattice tower</b><br><img src="3D Modern City Final Version/screenshots/8_eye_fell_tower.png" width="240"></td>
<td align="center"><b>Fractal trees</b><br><img src="3D Modern City Final Version/screenshots/9_trees.png" width="240"></td>
</tr>

<tr>
<td align="center"><b>River</b><br><img src="3D Modern City Final Version/screenshots/10_river.png" width="240"></td>
<td align="center"><b>Passenger ship</b><br><img src="3D Modern City Final Version/screenshots/11_passenger_ship.png" width="240"></td>
<td align="center"><b>Cargo ship</b><br><img src="3D Modern City Final Version/screenshots/12_cargo_ship.png" width="240"></td>
</tr>

<tr>
<td align="center"><b>Aeroplane</b><br><img src="3D Modern City Final Version/screenshots/13_plane.png" width="240"></td>
<td align="center"><b>Bridge cables</b><br><img src="3D Modern City Final Version/screenshots/14_catenary_cable.png" width="240"></td>
<td align="center"><b>Day mode</b><br><img src="3D Modern City Final Version/screenshots/15_day.png" width="240"></td>
</tr>

<tr>
<td align="center"><b>Night mode</b><br><img src="3D Modern City Final Version/screenshots/16_night.png" width="240"></td>
<td align="center"><b>No texture</b><br><img src="3D Modern City Final Version/screenshots/17_no_texture.png" width="240"></td>
<td align="center"><b>Simple texture</b><br><img src="3D Modern City Final Version/screenshots/18_simple_texture.png" width="240"></td>
</tr>

<tr>
<td align="center"><b>Blended texture</b><br><img src="3D Modern City Final Version/screenshots/19_blended_texture.png" width="240"></td>
<td align="center"><b>Vertex color</b><br><img src="3D Modern City Final Version/screenshots/20_vertex.png" width="240"></td>
<td align="center"><b>Procedural color</b><br><img src="3D Modern City Final Version/screenshots/21_fragment.png" width="240"></td>
</tr>

<tr>
<td align="center"><b>Directional lighting</b><br><img src="3D Modern City Final Version/screenshots/22_directional_light.png" width="240"></td>
<td align="center"><b>Point lights</b><br><img src="3D Modern City Final Version/screenshots/23_toggle_point_light.png" width="240"></td>
<td align="center"><b>Spot lights</b><br><img src="3D Modern City Final Version/screenshots/24_spot_light.png" width="240"></td>
</tr>

<tr>
<td align="center"><b>Viewport mode</b><br><img src="3D Modern City Final Version/screenshots/25_viewport_tarnsformation.png" width="240"></td>
<td align="center"><b>Ambient only</b><br><img src="3D Modern City Final Version/screenshots/26_ambient_only_view.png" width="240"></td>
<td align="center"><b>Diffuse only</b><br><img src="3D Modern City Final Version/screenshots/27_diffuse_only_view.png" width="240"></td>
</tr>

</table>

</div>

---

## Conclusion

This project demonstrates practical implementation of core computer graphics techniques including lighting models, transformations, hierarchical modeling, procedural animation, texture mapping, curves, and fractal geometry within a unified real-time OpenGL simulation.

The project can serve as a reference implementation for modern OpenGL scene construction and interactive graphics programming.
