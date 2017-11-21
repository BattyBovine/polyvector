#ifndef POLYVECTOR_H
#define POLYVECTOR_H

#include <vector>
#include <algorithm>
#include <core/os/os.h>
#include <scene/3d/immediate_geometry.h>
#include <scene/resources/curve.h>
#include <thirdparty/nanosvg/nanosvg.h>
#include <thirdparty/misc/triangulator.h>

class PolyVector : public ImmediateGeometry {
	GDCLASS(PolyVector,ImmediateGeometry)

private:
	struct PVPath {
		uint32_t id;
		Curve2D curve;
		Color colour;
		bool closed;
	};
	struct PVShape {
		std::vector<PVPath> paths;
		PVPath &operator[](int i) { return paths[i]; }
		size_t size() { return paths.size(); }
		Map<int, List< List<TriangulatorPoly> > > triangles;
		Map<int, List<TriangulatorPoly> > strokes;

		uint32_t id;
		Color colour;
	};

public:
	PolyVector();
	~PolyVector();

	bool set_svg_image(String);
	bool triangulate_shapes();
	bool render_shapes(uint64_t debugtimer=0);

	String get_svg_image();
	void set_vector_scale(Vector2);
	Vector2 get_vector_scale();
	void set_curve_quality(int);
	int8_t get_curve_quality();

protected:
	static void _bind_methods();

private:
	OS *os;
	String sSvgFile;
	struct NSVGimage *nsvgImage;

	std::vector< std::vector<PVShape> > vFrameData;
	uint16_t iFrame;
	Vector2 v2Scale;
	int8_t iCurveQuality;
};

#endif
