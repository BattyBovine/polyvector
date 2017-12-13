#include "register_types.h"
#include "polyvector.h"
#include "resource_importer_svg.h"


ResourceLoaderSVG *resource_loader_svg = NULL;

void register_polyvector_types()
{
	ClassDB::register_class<PolyVector>();
	resource_loader_svg = memnew(ResourceLoaderSVG);
	ResourceLoader::add_resource_format_loader(resource_loader_svg);

	#ifdef TOOLS_ENABLED
	Ref<ResourceImporterSVG> rawsvg;
	rawsvg.instance();
	ResourceFormatImporter::get_singleton()->add_importer(rawsvg);
	#endif
}

void unregister_polyvector_types()
{
	if(resource_loader_svg)	memdelete(resource_loader_svg);
}
