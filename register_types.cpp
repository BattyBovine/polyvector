#include "register_types.h"
#include "polyvector.h"
#include "resource_importer_swf.h"


ResourceLoaderJSONVector *resource_loader_jsonvector = NULL;

void register_polyvector_types()
{
	ClassDB::register_class<PolyVector>();
	ClassDB::register_class<JSONVector>();
	
	resource_loader_jsonvector = memnew(ResourceLoaderJSONVector);
	ResourceLoader::add_resource_format_loader(resource_loader_jsonvector);
	
	#ifdef TOOLS_ENABLED
	//Ref<ResourceImporterSVG> rawsvg;
	//rawsvg.instance();
	//ResourceFormatImporter::get_singleton()->add_importer(rawsvg);
	
	Ref<ResourceImporterSWF> swfdata;
	swfdata.instance();
	ResourceFormatImporter::get_singleton()->add_importer(swfdata);
	#endif
}

void unregister_polyvector_types()
{
	if(resource_loader_jsonvector)	memdelete(resource_loader_jsonvector);
}

