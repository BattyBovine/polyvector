#ifndef POLYVECTOR_H_26f336d6d05611e7abc4cec278b6b50a
#define POLYVECTOR_H_26f336d6d05611e7abc4cec278b6b50a

#include <vector>
#include <map>

#include <core/os/os.h>
#include <scene/3d/immediate_geometry.h>
#include <scene/resources/curve.h>
#include <thirdparty/nanosvg/nanosvg.h>

#include "resource_importer_svg.h"
#include "earcut.hpp/earcut.hpp"

//#define POLYVECTOR_DEBUG

#define POLYVECTOR_MIN_QUALITY 0
#define POLYVECTOR_MAX_QUALITY 9
#define POLYVECTOR_TESSELLATION_MAX_ANGLE 2.0f

using Coord = float;
using Point = Vector2;
namespace mapbox {
namespace util {
template <> struct nth<0, Vector2> { inline static auto get(const Vector2 &v) { return v.x; }; };
template <> struct nth<1, Vector2> { inline static auto get(const Vector2 &v) { return v.y; }; };
}
}

class PolyVector : public ImmediateGeometry {
	GDCLASS(PolyVector,ImmediateGeometry)

public:
	PolyVector();
	~PolyVector();

	bool triangulate_shapes();
	bool render_shapes(uint64_t debugtimer=0);

	void set_svg_image(const Ref<SVGBin>&);
	Ref<SVGBin> get_svg_image() const;
	void set_vector_scale(Vector2);
	Vector2 get_vector_scale();
	void set_curve_quality(int);
	int8_t get_curve_quality();

protected:
	static void _bind_methods();

private:
	#ifdef POLYVECTOR_DEBUG
	OS *os;
	#endif

	Ref<SVGBin> dataSvgFile;
	List<PolyVectorFrame> lFrameData;
	Vector2 v2Dimensions;

	uint16_t iFrame;
	Vector2 v2Scale;
	int8_t iCurveQuality;
};

#endif	// POLYVECTOR_H_26f336d6d05611e7abc4cec278b6b50a
