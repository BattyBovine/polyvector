#ifndef RESOURCE_IMPORTER_SVG_H_f6e3a78cd13111e78941cec278b6b50a
#define RESOURCE_IMPORTER_SVG_H_f6e3a78cd13111e78941cec278b6b50a

#include <vector>
#include <io/resource_import.h>
#include <io/resource_loader.h>
#include <io/resource_saver.h>
#include <scene/resources/curve.h>
#include <thirdparty/nanosvg/nanosvg.h>

using N = uint32_t;

//#ifdef TOOLS_ENABLED
class ResourceImporterSVG : public ResourceImporter {
	GDCLASS(ResourceImporterSVG, ResourceImporter)
public:
	virtual String get_importer_name() const { return "RawSVG"; }
	virtual String get_visible_name() const { return "Raw SVG"; }
	virtual void get_recognized_extensions(List<String> *p_extensions) const { p_extensions->push_back("svg"); p_extensions->push_back("svgz"); }
	virtual String get_save_extension() const { return "svgraw"; }
	virtual String get_resource_type() const { return "RawSVG"; }
	virtual bool get_option_visibility(const String &p_option, const Map<StringName, Variant> &p_options) const { return true; }
	virtual int get_preset_count() const { return 0; }
	virtual String get_preset_name(int p_idx) const { return String(); }
	virtual void get_import_options(List<ImportOption> *r_options, int p_preset = 0) const { return; }

	virtual Error import(const String &p_source_file, const String &p_save_path, const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files = NULL);

	ResourceImporterSVG() {}
};
//#endif

class ResourceLoaderSVG : public ResourceFormatLoader {
	bool _is_clockwise(Curve2D*);

public:
	virtual RES load(const String &p_path, const String &p_original_path = "", Error *r_error = NULL);
	virtual void get_recognized_extensions(List<String> *p_extensions) const { p_extensions->push_back("svgraw"); }
	virtual String get_resource_type(const String &p_path) const { return "RawSVG"; }
	virtual bool handles_type(const String &p_type) const { return ( p_type == "RawSVG" ); }
};

struct PolyVectorPath {
	uint32_t id;
	bool closed;
	bool hole;
	Curve2D *curve;
};
struct PolyVectorShape {
	uint32_t id;
	List<PolyVectorPath> paths;
	Color fillcolour;
	Color strokecolour;

	Map<int, std::vector<Vector2> > vertices;
	Map<int, std::vector<N> > indices;
	Map<int, List<PoolVector2Array> > strokes;
};
struct PolyVectorFrame {
	List<PolyVectorShape> shapes;
	Map<int, bool> triangulated;
};

class RawSVG : public Resource {
	GDCLASS(RawSVG, Resource);
	OBJ_SAVE_TYPE(RawSVG);
	RES_BASE_EXTENSION("svgraw");

	Vector2 dimensions;
	List<PolyVectorFrame> frames;

public:
	RawSVG() {}
	void add_frame(PolyVectorFrame &p_data) { this->frames.push_back(p_data); }
	List<PolyVectorFrame> &get_frames() { return this->frames; }
	PolyVectorFrame &get_frame(int i) { return this->frames[i]; }
	void set_dimensions(Vector2 dims) { dimensions = dims; }
	Vector2 &get_dimensions() { return dimensions; }
	PolyVectorFrame &operator[](int i) { return this->frames[i]; }
};

#endif // RESOURCE_IMPORTER_SVG_H_f6e3a78cd13111e78941cec278b6b50a
