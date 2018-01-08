# PolyVector
PolyVector is a proof-of-concept for importing SWF animations and triangulating them into polygons rendered with the GPU. It is implemented as a node module for Godot Game Engine 3.0.

Shape and path data is stored in memory using a Curve2D object, which are tessellated and triangulated as necessary when the quality level is changed. Animation support, automatic LOD, and many other features are currently planned.

[A short feature demonstration is available on YouTube.](https://www.youtube.com/watch?v=9ozzZk03H44)
