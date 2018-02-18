#ifndef POLYVECTOR_H_26f336d6d05611e7abc4cec278b6b50a
#define POLYVECTOR_H_26f336d6d05611e7abc4cec278b6b50a

#include <vector>
#include <map>

#include <core/os/os.h>
#include <scene/3d/mesh_instance.h>
#include <scene/animation/animation_player.h>
#include <scene/resources/curve.h>
#include <thirdparty/nanosvg/nanosvg.h>

#include "resource_importer_swf.h"
#include "earcut.hpp/earcut.hpp"

#define POLYVECTOR_DEBUG

using Coord = float;
using Point = Vector2;
namespace mapbox {
namespace util {
template <> struct nth<0, Vector2> { inline static auto get(const Vector2 &v) { return v.x; }; };
template <> struct nth<1, Vector2> { inline static auto get(const Vector2 &v) { return v.y; }; };
}
}

typedef Map<uint16_t, Ref<ArrayMesh> > MeshQualityMap;
typedef Map<uint16_t, MeshQualityMap> MeshDictionaryMap;
typedef Map<uint16_t, MeshInstance*> MeshInstanceMap;



class PolyVector : public VisualInstance {
	GDCLASS(PolyVector, VisualInstance)

public:
	PolyVector();
	~PolyVector();

	void draw_current_frame();
	void clear_mesh_data();
	void clear_mesh_instances();

	void set_vector_image(const Ref<JSONVector>&);
	Ref<JSONVector> get_vector_image() const;
	void set_time(real_t);
	real_t get_time();
	void set_curve_quality(int8_t);
	int8_t get_curve_quality();
	void set_unit_scale(real_t);
	real_t get_unit_scale();
	void set_offset(Vector2);
	Vector2 get_offset();
	void set_layer_separation(real_t);
	real_t get_layer_separation();
	void set_material_unshaded(bool);
	bool get_material_unshaded();

	void set_max_tessellation_angle(real_t);
	real_t get_max_tessellation_angle();

	virtual AABB get_aabb() const;
	virtual PoolVector<Face3> get_faces(uint32_t p_usage_flags) const;

	#ifdef POLYVECTOR_DEBUG
	void set_debug_wireframe(bool);
	bool get_debug_wireframe();

	double get_triangulation_time();
	double get_mesh_update_time();
	uint32_t get_vertex_count();
	#endif

protected:
	static void _bind_methods();

private:
	Ref<JSONVector> dataVectorFile;
	Ref<SpatialMaterial> materialDefault;
	MeshDictionaryMap mapMeshDictionary;
	MeshInstanceMap mapMeshDisplay;
	Vector2 v2Dimensions;
	bool bZOrderOffset;
	real_t fLayerDepth;
	real_t fUnitScale;

	List<PolyVectorFrame> lFrameData;
	List<PolyVectorCharacter> lDictionaryData;
	real_t fTime;
	real_t fFps;
	Vector2 v2Offset;
	int8_t iCurveQuality;

	real_t fMaxTessellationAngle;

	#ifdef POLYVECTOR_DEBUG
	OS *os;
	bool bDebugWireframe;
	Ref<SpatialMaterial> materialDebug;
	double dTriangulationTime;
	double dMeshUpdateTime;
	uint32_t vertex_count;
	#endif
};

#endif	// POLYVECTOR_H_26f336d6d05611e7abc4cec278b6b50a
