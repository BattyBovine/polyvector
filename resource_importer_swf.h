#ifndef RESOURCE_IMPORTER_SVG_H_f6e3a78cd13111e78941cec278b6b50a
#define RESOURCE_IMPORTER_SVG_H_f6e3a78cd13111e78941cec278b6b50a

using N = uint32_t;

#include <vector>
#include <map>
#include <unordered_set>
#include <io/resource_import.h>
#include <scene/3d/spatial.h>
#include <scene/3d/mesh_instance.h>
#include <scene/resources/packed_scene.h>
#include <scene/resources/curve.h>

#include "libshockwave/swfparser.h"
#include "polyvector.h"

#ifdef TOOLS_ENABLED
class ResourceImporterSWF : public ResourceImporter {
	GDCLASS(ResourceImporterSWF, ResourceImporter)
public:
	virtual String get_importer_name() const { return "scene"; }
	virtual String get_visible_name() const { return "PolyVector"; }
	virtual void get_recognized_extensions(List<String> *p_extensions) const { p_extensions->push_back("swf"); }
	virtual String get_save_extension() const { return "scn"; }
	virtual String get_resource_type() const { return "PackedScene"; }
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
	PolyVectorPath verts_to_curve(std::vector<SWF::Point>);
};
#endif	// TOOLS_ENABLED

#endif	// RESOURCE_IMPORTER_SVG_H_f6e3a78cd13111e78941cec278b6b50a
