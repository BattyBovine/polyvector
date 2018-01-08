#ifndef RESOURCE_IMPORTER_SVG_H_f6e3a78cd13111e78941cec278b6b50a
#define RESOURCE_IMPORTER_SVG_H_f6e3a78cd13111e78941cec278b6b50a

using N = uint32_t;
#include "json/src/json.hpp"
using json = nlohmann::json;

#include <vector>
#include <map>
#include <unordered_set>
#include <io/resource_import.h>
#include <io/resource_loader.h>
#include <io/resource_saver.h>
#include <scene/resources/curve.h>
//#include <thirdparty/nanosvg/nanosvg.h>

#include "libshockwave/swfparser.h"

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
	std::list<PolyVectorPath> holes;
	Color fillcolour;
	Color strokecolour;

	Map<int, List<PoolVector2Array> > strokes;
	Map<int, std::vector<Vector2> > vertices;
	Map<int, std::vector<N> > indices;
};
struct PolyVectorFrame
{
	std::list<PolyVectorShape> shapes;
	Map<int, bool> triangulated;
};

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
	struct ShapeRemap
	{
		SWF::ShapeList Shapes;
		std::map<uint16_t,uint16_t> Fills;
		std::map<uint16_t,std::list<uint16_t> > Holes;
	};
	ShapeRemap shape_builder(SWF::ShapeList);
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

//class ResourceLoaderSVG : public ResourceFormatLoader
//{
//	bool inline _is_clockwise(Curve2D);
//
//public:
//	virtual RES load(const String &p_path, const String &p_original_path = "", Error *r_error = NULL);
//	virtual void get_recognized_extensions(List<String> *p_extensions) const { p_extensions->push_back("svgraw"); }
//	virtual String get_resource_type(const String &p_path) const { return "RawSVG"; }
//	virtual bool handles_type(const String &p_type) const { return (p_type=="RawSVG"); }
//};



class JSONVector : public Resource {
	GDCLASS(JSONVector, Resource);
	OBJ_SAVE_TYPE(JSONVector);
	RES_BASE_EXTENSION("jvec");

	//Vector2 dimensions;
	List<PolyVectorFrame> frames;

public:
	JSONVector() {}
	void add_frame(PolyVectorFrame &p_data) { this->frames.push_back(p_data); }
	List<PolyVectorFrame> &get_frames() { return this->frames; }
	PolyVectorFrame &get_frame(int i) { return this->frames[i]; }
	//void set_dimensions(Vector2 dims) { dimensions = dims; }
	//Vector2 &get_dimensions() { return dimensions; }
	PolyVectorFrame operator[](int i) { return this->frames[i]; }
};

//class RawSVG : public Resource {
//	GDCLASS(RawSVG, Resource);
//	OBJ_SAVE_TYPE(RawSVG);
//	RES_BASE_EXTENSION("svgraw");
//
//	Vector2 dimensions;
//	List<PolyVectorFrame> frames;
//
//public:
//	RawSVG() {}
//	void add_frame(PolyVectorFrame &p_data) { this->frames.push_back(p_data); }
//	List<PolyVectorFrame> &get_frames() { return this->frames; }
//	PolyVectorFrame &get_frame(int i) { return this->frames[i]; }
//	void set_dimensions(Vector2 dims) { dimensions = dims; }
//	Vector2 &get_dimensions() { return dimensions; }
//	PolyVectorFrame &operator[](int i) { return this->frames[i]; }
//};

#define PV_JSON_NAME_LIBRARY	"Library"
#define PV_JSON_NAME_CHARACTERS	"Characters"
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
#define PV_JSON_NAME_TRANSFORM	"Transform"

#endif // RESOURCE_IMPORTER_SVG_H_f6e3a78cd13111e78941cec278b6b50a
