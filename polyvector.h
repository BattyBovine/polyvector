#ifndef POLYVECTOR_H_26f336d6d05611e7abc4cec278b6b50a
#define POLYVECTOR_H_26f336d6d05611e7abc4cec278b6b50a

#include <vector>
#include <map>

#include <core/os/os.h>
#include <scene/3d/immediate_geometry.h>
#include <scene/resources/curve.h>
#include <thirdparty/nanosvg/nanosvg.h>

#include "resource_importer_svg_swf.h"
#include "earcut.hpp/earcut.hpp"

#define POLYVECTOR_DEBUG

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
	GDCLASS(PolyVector, ImmediateGeometry)

public:
	PolyVector();
	~PolyVector();

	bool triangulate_shapes();
	bool render_shapes();

	void set_vector_image(const Ref<JSONVector>&);
	Ref<JSONVector> get_vector_image() const;
	void set_frame(int32_t);
	uint16_t get_frame();
	void set_curve_quality(int);
	int8_t get_curve_quality();
	void set_unit_scale(real_t);
	real_t get_unit_scale();
	void set_offset(Vector2);
	Vector2 get_offset();
	void set_layer_separation(real_t);
	real_t get_layer_separation();
	void set_material_unshaded(bool);
	bool get_material_unshaded();
	void set_billboard(int);
	int get_billboard();

	#ifdef POLYVECTOR_DEBUG
	double get_triangulation_time();
	double get_mesh_update_time();
	uint32_t get_vertex_count();
	#endif

protected:
	static void _bind_methods();

private:
	Ref<JSONVector> dataVectorFile;
	Ref<SpatialMaterial> materialDefault;
	List<PolyVectorFrame> lFrameData;
	Vector2 v2Dimensions;
	bool bZOrderOffset;
	real_t fLayerDepth;
	real_t fUnitScale;

	int32_t iFrame;
	Vector2 v2Offset;
	int8_t iCurveQuality;

	#ifdef POLYVECTOR_DEBUG
	OS *os;
	double triangulation_time;
	double mesh_update_time;
	uint32_t vertex_count;
	#endif
};

#endif	// POLYVECTOR_H_26f336d6d05611e7abc4cec278b6b50a
