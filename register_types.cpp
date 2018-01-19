#include "register_types.h"
#include "polyvector.h"
#include "resource_importer_swf.h"

void register_polyvector_types()
{
	ClassDB::register_class<PolyVector>();
	#ifdef TOOLS_ENABLED
	Ref<ResourceImporterSWF> swfdata;
	swfdata.instance();
	ResourceFormatImporter::get_singleton()->add_importer(swfdata);
	#endif
}

void unregister_polyvector_types(){}
