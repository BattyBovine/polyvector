#ifndef POLYVECTOR_H_26f336d6d05611e7abc4cec278b6b50a
#define POLYVECTOR_H_26f336d6d05611e7abc4cec278b6b50a

#include <vector>
#include <core/os/os.h>
#include <core/core_string_names.h>
#include <scene/scene_string_names.h>
#include <scene/3d/mesh_instance.h>
#include <scene/resources/primitive_meshes.h>
#include <scene/resources/curve.h>
#include <thirdparty/nanosvg/nanosvg.h>

#include "earcut.hpp/earcut.hpp"

#define POLYVECTOR_DEBUG

#define POLYVECTOR_TESSELLATION_MAX_ANGLE 2.0f

using N = uint32_t;
using Coord = float;
using Point = Vector2;
namespace mapbox {
namespace util {
template <> struct nth<0, Vector2> { inline static auto get(const Vector2 &v) { return v.x; }; };
template <> struct nth<1, Vector2> { inline static auto get(const Vector2 &v) { return v.y; }; };
}
}

struct PolyVectorMatrix
{
	float TranslateX = 0.0f;
	float TranslateY = 0.0f;
	float ScaleX = 1.0f;
	float ScaleY = 1.0f;
	float Skew0 = 0.0f;
	float Skew1 = 0.0f;
};
struct PolyVectorMesh
{
	std::vector<Vector2> vertices;
	std::vector<N> indices;
};
struct PolyVectorPath
{
	bool closed;
	Curve2D curve;
	void operator=(PolyVectorPath in)
	{
		closed=in.closed;
		curve.clear_points();
		for(uint16_t pt=0; pt<in.curve.get_point_count(); pt++)
			curve.add_point(in.curve.get_point_position(pt), in.curve.get_point_in(pt), in.curve.get_point_out(pt));
	}
};
struct PolyVectorShape
{
	PolyVectorPath path;
	List<PolyVectorPath> holes;
	Color fillcolour;
	Color strokecolour;

	Map<uint16_t, List<PoolVector2Array> > strokes;
};
typedef List<PolyVectorShape> PolyVectorCharacter;

class PolyVector : public MeshInstance {
	GDCLASS(PolyVector, MeshInstance)

public:
	PolyVector();
	~PolyVector();

	void set_character(PolyVectorCharacter);
	PolyVectorCharacter get_character();

	void set_curve_quality(int);
	int8_t get_curve_quality();
	void set_unit_scale(real_t);
	real_t get_unit_scale();
	void set_material(Ref<SpatialMaterial>);
	Ref<SpatialMaterial> get_material();

	//virtual AABB get_aabb() const;
	//virtual PoolVector<Face3> get_faces(uint32_t p_usage_flags) const;

	#ifdef POLYVECTOR_DEBUG
	double get_triangulation_time();
	uint32_t get_vertex_count();
	#endif

protected:
	static void _bind_methods();

private:
	void triangulate_mesh(bool force_retri=false);
	void clear_meshes();

	PolyVectorCharacter pvcCharacter;
	Ref<SpatialMaterial> materialSpatial;
	Map<uint16_t, Ref<ArrayMesh> > mapMeshes;
	Vector2 v2Dimensions;
	real_t fUnitScale;

	int32_t iFrame;
	int8_t iCurveQuality;

	#ifdef POLYVECTOR_DEBUG
	OS *os;
	double triangulation_time;
	mutable uint32_t vertex_count;
	#endif
};

#endif	// POLYVECTOR_H_26f336d6d05611e7abc4cec278b6b50a
