# PolyVector
PolyVector is a proof-of-concept for importing SVG image files and triangulating them into polygons rendered with the GPU. It is implemented as a node module for Godot Game Engine 3.0.

Shape and path data is stored in memory using a Curve2D object, which are tessellated and triangulated as necessary when the quality level is changed. Animation support, automatic LOD, and many other features are currently planned, as soon as a few known bugs and issues are taken care of.

In order to render colours properly, you will need to apply a Spatial material with the options "Unshaded" and "Use As Albedo" enabled.

[A short feature demonstration is available on YouTube.](https://www.youtube.com/watch?v=9ozzZk03H44)
