#include "register_types.h"
#include "class_db.h"
#include "polyvector.h"

void register_polyvector_types() {
	ClassDB::register_class<PolyVector>();
}

void unregister_polyvector_types() {
	//nothing to do here
}
