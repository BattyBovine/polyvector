#include "register_types.h"
#include "polyvector.h"
#include "resource_importer_svg.h"


ResourceLoaderSVG *resource_loader_svg = NULL;

void register_polyvector_types()
{
	#ifdef TOOLS_ENABLED
	Ref<ResourceImporterSVG> rawsvg;
	rawsvg.instance();
	ResourceFormatImporter::get_singleton()->add_importer(rawsvg);
	#endif
	resource_loader_svg = memnew(ResourceLoaderSVG);
	ResourceLoader::add_resource_format_loader(resource_loader_svg);

	ClassDB::register_class<PolyVector>();
}

void unregister_polyvector_types()
{
	memdelete(resource_loader_svg);
}
