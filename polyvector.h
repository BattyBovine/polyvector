#ifndef POLYVECTOR_H_26f336d6d05611e7abc4cec278b6b50a
#define POLYVECTOR_H_26f336d6d05611e7abc4cec278b6b50a

#include <vector>
#include <map>

#include <core/os/os.h>
#include <scene/3d/visual_instance.h>
#include <scene/resources/surface_tool.h>
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

class PolyVector : public GeometryInstance {
	GDCLASS(PolyVector, GeometryInstance)

public:
	PolyVector();
	~PolyVector();

	bool triangulate_shapes();
	bool render_shapes();

	void set_svg_image(const Ref<RawSVG>&);
	Ref<RawSVG> get_svg_image() const;
	void set_unit_scale(Vector2);
	Vector2 get_unit_scale();
	void set_curve_quality(int);
	int8_t get_curve_quality();
	void set_layer_separation(real_t);
	real_t get_layer_separation();
	void set_offset(Vector2);
	Vector2 get_offset();

	virtual AABB get_aabb() const;
	virtual PoolVector<Face3> get_faces(uint32_t p_usage_flags) const;

protected:
	static void _bind_methods();

private:
	#ifdef POLYVECTOR_DEBUG
	OS *os;
	#endif

	Ref<RawSVG> dataSvgFile;
	//Ref<SurfaceTool> stMeshBuilder;
	Ref<Mesh> mCurrentMesh;
	Ref<SpatialMaterial> smBaseMaterial;
	AABB aabbBounds;

	List<PolyVectorFrame> lFrameData;
	Vector2 v2Dimensions;
	bool bZOrderOffset;
	real_t fLayerDepth;

	uint16_t iFrame;
	Vector2 v2Scale, v2Offset;
	int8_t iCurveQuality;
};

#endif	// POLYVECTOR_H_26f336d6d05611e7abc4cec278b6b50a
