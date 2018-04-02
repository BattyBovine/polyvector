#ifndef RESOURCE_IMPORTER_SWF_H_f6e3a78cd13111e78941cec278b6b50a
#define RESOURCE_IMPORTER_SWF_H_f6e3a78cd13111e78941cec278b6b50a

using N = uint32_t;
#include "json/src/json.hpp"
using json = nlohmann::json;

#include <vector>
#include <map>
#include <set>
#include <io/resource_import.h>
#include <io/resource_loader.h>
#include <io/resource_saver.h>
#include <scene/resources/curve.h>
#include <scene/3d/mesh_instance.h>

#include "libshockwave/swfparser.h"

#define RISWF_SHAPE_AREA_THRESHOLD 0.1f

struct PolyVectorMatrix
{
	float TranslateX = 0.0f;
	float TranslateY = 0.0f;
	float ScaleX = 1.0f;
	float ScaleY = 1.0f;
	float Skew0 = 0.0f;
	float Skew1 = 0.0f;
};
struct PolyVectorColourTransform
{
	float RedAdd = 0.0f;
	float GreenAdd = 0.0f;
	float BlueAdd = 0.0f;
	float AlphaAdd = 0.0f;
	float RedMultiplier = 1.0f;
	float GreenMultiplier = 1.0f;
	float BlueMultiplier = 1.0f;
	float AlphaMultiplier = 1.0f;
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
	~PolyVectorShape()
	{
		if(fillcolour==NULL)	delete fillcolour;
		if(strokecolour==NULL)	delete strokecolour;
	}
	uint8_t layer;
	PolyVectorPath path;
	List<uint16_t> holes;
	Color *fillcolour = NULL;
	Color *strokecolour = NULL;

	Map<uint16_t, List<PoolVector2Array> > strokes;
};
typedef List<PolyVectorShape> PolyVectorCharacter;

struct PolyVectorSymbol
{
	~PolyVectorSymbol() { if(tint==NULL) delete tint; }
	uint16_t id = 0;
	uint16_t depth = 0;
	PolyVectorMatrix matrix;
	PolyVectorColourTransform *tint = NULL;
};
typedef List<PolyVectorSymbol> PolyVectorFrame;

typedef Map<uint16_t, Ref<ArrayMesh> > MeshQualityMap;
typedef Map<uint16_t, MeshQualityMap> MeshDictionaryMap;
typedef Map<uint16_t, MeshInstance*> MeshInstanceMap;

#ifdef TOOLS_ENABLED
//class ResourceImporterSVG : public ResourceImporter {
//	GDCLASS(ResourceImporterSVG, ResourceImporter)
//public:
//	virtual String get_importer_name() const { return "RawSVG"; }
//	virtual String get_visible_name() const { return "Raw SVG"; }
//	virtual void get_recognized_extensions(List<String> *p_extensions) const { p_extensions->push_back("svg"); p_extensions->push_back("svgz"); }
//	virtual String get_save_extension() const { return "svgraw"; }
//	virtual String get_resource_type() const { return "RawSVG"; }
//	virtual bool get_option_visibility(const String &p_option, const Map<StringName, Variant> &p_options) const { return true; }
//	virtual int get_preset_count() const { return 0; }
//	virtual String get_preset_name(int p_idx) const { return String(); }
//	virtual void get_import_options(List<ImportOption> *r_options, int p_preset = 0) const { return; }
//
//	virtual Error import(const String &p_source_file, const String &p_save_path, const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files = NULL);
//
//	ResourceImporterSVG() {}
//};

class ResourceImporterSWF : public ResourceImporter {
	GDCLASS(ResourceImporterSWF, ResourceImporter)
public:
	virtual String get_importer_name() const { return "JSONVector"; }
	virtual String get_visible_name() const { return "PolyVector"; }
	virtual void get_recognized_extensions(List<String> *p_extensions) const { p_extensions->push_back("swf"); }
	virtual String get_save_extension() const { return "jvec"; }
	virtual String get_resource_type() const { return "JSONVector"; }
	virtual bool get_option_visibility(const String&, const Map<StringName, Variant>&) const { return true; }
	virtual int get_preset_count() const { return 0; }
	virtual String get_preset_name(int p_idx) const { return String(); }
	virtual void get_import_options(List<ImportOption> *r_options, int p_preset = 0) const;

	virtual Error import(const String &p_source_file, const String &p_save_path, const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files = NULL);

	ResourceImporterSWF() {}

private:
	struct SWFPolygon;
	typedef std::vector<SWFPolygon> SWFPolygonList;
	struct SWFPolygon
	{
		SWF::Shape polygon;
		float area = 0.0f;
		uint16_t fill;
		uint16_t stroke;
		bool has_parent = false;
		std::list<uint16_t> children;
	};
	SWFPolygonList shape_builder(SWF::ShapeList);
	bool shape_contains_point(SWF::Point, SWF::Shape);
	void find_connected_shapes(SWF::Shape*, SWF::ShapeList::iterator, bool, std::set<SWF::ShapeList::iterator>*, std::set<SWF::ShapeList::iterator>*, std::list<SWF::ShapeList::iterator>*);
	inline bool points_equal(SWF::Vertex&, SWF::Vertex&);
	inline void points_reverse(SWF::Shape*);
	inline float shape_area(SWF::ShapeList::iterator i) { return this->shape_area(*i); }
	inline float shape_area(SWF::Shape);
	inline bool shape_area_too_small(float a) { return (abs(a)<RISWF_SHAPE_AREA_THRESHOLD); }
};
#endif

class ResourceLoaderJSONVector : public ResourceFormatLoader
{
public:
	virtual RES load(const String &p_path, const String &p_original_path = "", Error *r_error = NULL);
	virtual void get_recognized_extensions(List<String> *p_extensions) const { p_extensions->push_back("jvec"); }
	virtual String get_resource_type(const String &p_path) const { return "JSONVector"; }
	virtual bool handles_type(const String &p_type) const { return (p_type=="JSONVector"); }
private:
	PolyVectorPath verts_to_curve(json);
};

class JSONVector : public Resource
{
	GDCLASS(JSONVector, Resource);
	OBJ_SAVE_TYPE(JSONVector);
	RES_BASE_EXTENSION("jvec");

	real_t fps;
	Vector2 dimensions;
	List<PolyVectorCharacter> dictionary;
	List<PolyVectorFrame> frames;

	MeshDictionaryMap mapMeshDictionary;

public:
	JSONVector() {}
	void add_character(PolyVectorCharacter p_data) { this->dictionary.push_back(p_data); }
	PolyVectorCharacter get_character(uint16_t i) { return this->dictionary[i]; }
	List<PolyVectorCharacter> get_dictionary() { return this->dictionary; }

	void add_frame(PolyVectorFrame p_data) { this->frames.push_back(p_data); }
	PolyVectorFrame get_frame(uint16_t i) { return this->frames[i]; }
	List<PolyVectorFrame> get_frames() { return this->frames; }

	void set_fps(real_t f) { this->fps = f; }
	real_t get_fps() { return this->fps; }
	void set_dimensions(Vector2 d) { this->dimensions = d; }
	Vector2 get_dimensions() { return this->dimensions; }

	MeshDictionaryMap &get_mesh_dictionary() { return this->mapMeshDictionary; }
};

#define PV_JSON_NAME_FPS		"FPS"
#define PV_JSON_NAME_DIMS		"Dimensions"
#define PV_JSON_NAME_LIBRARY	"Library"
#define PV_JSON_NAME_CHARACTERS	"Characters"
#define PV_JSON_NAME_LAYER		"Layer"
#define PV_JSON_NAME_FILL		"Fill"
#define PV_JSON_NAME_STROKE		"Stroke"
#define PV_JSON_NAME_CLOSED		"Closed"
#define PV_JSON_NAME_VERTICES	"Vertices"
#define PV_JSON_NAME_HOLES		"Holes"
#define PV_JSON_NAME_FILLSTYLES	"Fill Styles"
#define PV_JSON_NAME_COLOUR		"Colour"
#define PV_JSON_NAME_LINESTYLES	"Line Styles"
#define PV_JSON_NAME_LINEWIDTH	"Line Width"
#define PV_JSON_NAME_FRAMES		"Frames"
#define PV_JSON_NAME_ID			"ID"
#define PV_JSON_NAME_DEPTH		"Depth"
#define PV_JSON_NAME_TRANSFORM	"Transform"
#define PV_JSON_NAME_CXFORM		"CXform"

#endif // RESOURCE_IMPORTER_SWF_H_f6e3a78cd13111e78941cec278b6b50a
