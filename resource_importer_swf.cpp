#include <core/os/os.h>
#include <io/resource_saver.h>
#include <os/file_access.h>

#include "resource_importer_swf.h"

#ifdef TOOLS_ENABLED
Error ResourceImporterSWF::import(const String &p_source_file, const String &p_save_path, const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files)
{
	FileAccess *swf = FileAccess::open(p_source_file, FileAccess::READ);
	ERR_FAIL_COND_V(!swf, ERR_FILE_CANT_READ);
	//FileAccess *pvimport = FileAccess::open(p_save_path + ".jvec", FileAccess::WRITE);
	//ERR_FAIL_COND_V(!pvimport, ERR_FILE_CANT_WRITE);

	size_t xmllen = swf->get_len();
	ERR_FAIL_COND_V(!xmllen, ERR_CANT_OPEN);

	String root_type = p_options["nodes/root_type"];
	Node *scene = Object::cast_to<Node>(ClassDB::instance(root_type));
	scene->set_name(p_options["nodes/root_name"]);

	{
		uint8_t *swfdata = new uint8_t[xmllen];
		swf->get_buffer(swfdata, xmllen);
		ERR_FAIL_COND_V(!swfdata, ERR_INVALID_DATA);
		SWF::Parser *swfparser = new SWF::Parser();
		SWF::Error error = swfparser->parse_swf_data(swfdata, xmllen);
		switch(error) {
			case SWF::Error::SWF_NULL_DATA:
				OS::get_singleton()->alert(String(p_source_file.ascii().get_data())+" contains null data somehow?");
				return Error::ERR_CANT_OPEN;
			case SWF::Error::ZLIB_NOT_COMPILED:
				OS::get_singleton()->alert("zlib compression was not compiled into libshockwave.");
				return Error::ERR_CANT_OPEN;
			case SWF::Error::LZMA_NOT_COMPILED:
				OS::get_singleton()->alert("LZMA compression was not compiled into libshockwave.");
				return Error::ERR_CANT_OPEN;
			case SWF::Error::SWF_FILE_ENCRYPTED:
				OS::get_singleton()->alert(String("Password is required to decrypt %s")+p_source_file.ascii().get_data());
				return Error::ERR_FILE_NO_PERMISSION;
		}

		SWF::Dictionary *dict = swfparser->get_dict();
		std::map<uint16_t, uint16_t> fillstylemap, linestylemap, charactermap;
		std::vector<std::vector<Color> > fillarrays, strokearrays;
		{	// Build the library definitions first
			for(SWF::FillStyleMap::iterator fsm=dict->FillStyles.begin(); fsm!=dict->FillStyles.end(); fsm++) {
				std::vector<Color> fillcolourarray;
				for(SWF::FillStyleArray::iterator fs=fsm->second.begin(); fs!=fsm->second.end(); fs++) {
					SWF::FillStyle fillstyle = *fs;
					fillstylemap[fs-fsm->second.begin()+1] = fillstylemap.size()+1;
					Color fillcolour;
					if(fillstyle.StyleType==SWF::FillStyle::Type::SOLID) {
						fillcolour.r = (fillstyle.Color.r/256.0f);
						fillcolour.g = (fillstyle.Color.g/256.0f);
						fillcolour.b = (fillstyle.Color.b/256.0f);
						fillcolour.a = (fillstyle.Color.a/256.0f);
					}
					fillcolourarray.push_back(fillcolour);
				}
				fillarrays.push_back(fillcolourarray);
			}
			for(SWF::LineStyleMap::iterator lsm=dict->LineStyles.begin(); lsm!=dict->LineStyles.end(); lsm++) {
				std::vector<Color> strokecolourarray;
				for(SWF::LineStyleArray::iterator ls=lsm->second.begin(); ls!=lsm->second.end(); ls++) {
					SWF::LineStyle linestyle = *ls;
					Color strokecolour;
					if(linestyle.Width>0.0f) {
						linestylemap[ls-lsm->second.begin()+1] = linestylemap.size()+1;
						strokecolour.r += (linestyle.Color.r/256.0f);
						strokecolour.g += (linestyle.Color.g/256.0f);
						strokecolour.b += (linestyle.Color.b/256.0f);
						strokecolour.a += (linestyle.Color.a/256.0f);
					}
					strokecolourarray.push_back(strokecolour);
				}
				strokearrays.push_back(strokecolourarray);
			}
			for(SWF::CharacterDict::iterator cd = dict->CharacterList.begin(); cd != dict->CharacterList.end(); cd++) {
				uint16_t characterid = cd->first;
				SWF::Character character = cd->second;
				if(characterid>0) {
					charactermap[characterid] = charactermap.size()+1;
					PolyVector *polyvector = memnew(PolyVector);
					Ref<SpatialMaterial> material;
					material.instance();
					material->set_flag(SpatialMaterial::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
					material->set_flag(SpatialMaterial::FLAG_SRGB_VERTEX_COLOR, true);
					polyvector->set_material(material);
					ShapeRemap remap = this->shape_builder(character.shapes);	// Merge shapes from Flash into solid objects and calculate hole placement and fill rules
					PolyVectorCharacter pvchar;
					for(SWF::ShapeList::iterator s=remap.Shapes.begin(); s!=remap.Shapes.end(); s++) {
						uint16_t shapeno = (s-remap.Shapes.begin());
						if(remap.Fills[shapeno]==0)	continue;
						SWF::Shape swfshape = *s;
						//if(!swfshape.clockwise) {	// Keep enclosing shapes wound clockwise
						//	std::vector<SWF::Vertex> reverseverts;
						//	SWF::Point controlcache;
						//	for(std::vector<SWF::Vertex>::reverse_iterator v=swfshape.vertices.rbegin(); v!=swfshape.vertices.rend(); v++) {
						//		SWF::Point newctrl = controlcache;
						//		controlcache = v->control;
						//		v->control = newctrl;
						//		reverseverts.push_back(*v);
						//	}
						//	swfshape.vertices = reverseverts;
						//}
						PolyVectorShape pvshape;
						if(remap.Fills[shapeno]!=0)
							pvshape.fillcolour = fillarrays[charactermap[characterid]-1][remap.Fills[shapeno]-1];
						if(swfshape.stroke!=0)
							pvshape.strokecolour = strokearrays[charactermap[characterid]][swfshape.stroke-1];
						std::vector<SWF::Point> importedverts;
						for(std::vector<SWF::Vertex>::iterator v=swfshape.vertices.begin(); v!=swfshape.vertices.end(); v++) {
							if(v!=swfshape.vertices.begin())
								importedverts.push_back(v->control);
							if(swfshape.closed && v==(swfshape.vertices.end()-1))
								break;
							importedverts.push_back(v->anchor);
						}
						pvshape.path = this->verts_to_curve(importedverts);
						for(std::list<uint16_t>::iterator h=remap.Holes[shapeno].begin(); h!=remap.Holes[shapeno].end(); h++) {
							uint16_t hole = *h;
							//if(remap.Shapes[hole].clockwise) {	// Keep holes wound counter-clockwise
							//	std::vector<SWF::Vertex> reverseverts;
							//	SWF::Point controlcache;
							//	for(std::vector<SWF::Vertex>::reverse_iterator v=remap.Shapes[hole].vertices.rbegin(); v!=remap.Shapes[hole].vertices.rend(); v++) {
							//		SWF::Point newctrl = controlcache;
							//		controlcache = v->control;
							//		v->control = newctrl;
							//		reverseverts.push_back(*v);
							//	}
							//	remap.Shapes[hole].vertices = reverseverts;
							//}
							std::vector<SWF::Point> importedholeverts;
							for(std::vector<SWF::Vertex>::iterator hv=remap.Shapes[hole].vertices.begin(); hv!=remap.Shapes[hole].vertices.end(); hv++) {
								if(hv!=remap.Shapes[hole].vertices.begin())
									importedholeverts.push_back(hv->control);
								if(remap.Shapes[hole].closed==true && hv==(remap.Shapes[hole].vertices.end()-1))
									break;
								importedholeverts.push_back(hv->anchor);
							}
							pvshape.holes.push_back(this->verts_to_curve(importedholeverts));
						}
						pvchar.push_back(pvshape);
					}
					polyvector->set_unit_scale(float(p_options["vector/unit_scale"]));
					polyvector->set_character(pvchar);
					polyvector->set_name("Symbol"+itos(scene->get_child_count()));
					scene->add_child(polyvector);
					polyvector->set_owner(scene);
				}
			}
		}

		if(swfparser)	delete swfparser;
		if(swfdata)		delete swfdata;
	}
	swf->close();
	memdelete(swf);

	Ref<PackedScene> packer = memnew(PackedScene);
	packer->pack(scene);
	Error err = ResourceSaver::save(p_save_path + ".scn", packer);
	ERR_FAIL_COND_V(err!=OK, err);
	memdelete(scene);

	return OK;
}

void ResourceImporterSWF::get_import_options(List<ImportOption> *r_options, int p_preset) const
{
	r_options->push_back(ImportOption(PropertyInfo(Variant::STRING, "nodes/root_type", PROPERTY_HINT_TYPE_STRING, "Node"), "Spatial"));
	r_options->push_back(ImportOption(PropertyInfo(Variant::STRING, "nodes/root_name"), "PolyVector"));
	r_options->push_back(ImportOption(PropertyInfo(Variant::STRING, "nodes/symbol_prefix"), "Symbol"));

	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "material/unshaded"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "material/billboard", PROPERTY_HINT_ENUM, "Disabled,Enabled,Y-Billboard,Particle"), 0));

	r_options->push_back(ImportOption(PropertyInfo(Variant::REAL, "vector/unit_scale", PROPERTY_HINT_RANGE, "0.0, 1000.0"), 1.0f));
	r_options->push_back(ImportOption(PropertyInfo(Variant::REAL, "vector/layer_separation", PROPERTY_HINT_RANGE, "0.0, 1.0"), 0.0f));
}

ResourceImporterSWF::ShapeRemap ResourceImporterSWF::shape_builder(SWF::ShapeList sl)
{
	// Stores the results of shape merges, parents and fill rule checks
	ShapeRemap remap;

	// Merge lines to form enclosed shapes wherever possible
	std::map<uint16_t,std::unordered_set<uint16_t> > mergemap;
	for(SWF::ShapeList::iterator s=sl.begin(); s!=sl.end(); s++) {
		if(s->closed) {
			remap.Shapes.push_back(*s);
		} else {
			for(SWF::ShapeList::iterator s2=(s+1); s2!=sl.end(); s2++) {
				if(s2->closed)	continue;
				if((round(s->vertices.back().anchor.x*100.0f)==round(s2->vertices.front().anchor.x*100.0f) &&	// Match vertices at each end of two open shapes to see if they fit
					round(s->vertices.back().anchor.y*100.0f)==round(s2->vertices.front().anchor.y*100.0f))) {
					SWF::Shape mergedshape = *s;
					if(round(s->vertices.front().anchor.x*100.0f) == round(s2->vertices.back().anchor.x*100.0f) &&	// Shape is closed if the new merge will make a complete shape
						round(s->vertices.front().anchor.y*100.0f) == round(s2->vertices.back().anchor.y*100.0f))
						mergedshape.closed = true;
					mergedshape.vertices.insert(mergedshape.vertices.end(), s2->vertices.begin()+1, s2->vertices.end());	// Remove the first endpoint from shape 2 while merging
					uint16_t newshapeno = remap.Shapes.size();
					mergemap[newshapeno].insert(s-sl.begin());
					mergemap[newshapeno].insert(s2-sl.begin());
					remap.Shapes.push_back(mergedshape);
				}
			}
		}
	}
	// Iterate over the shape merges and calculate fill rules for the new shapes
	for(uint16_t shapeno=0; shapeno<mergemap.size(); shapeno++) {
		if(remap.Fills[shapeno]!=0)	continue;
		for(std::unordered_set<uint16_t>::iterator m=mergemap[shapeno].begin(); m!=mergemap[shapeno].end(); m++) {
			uint16_t mergerecord = *m;
			if(remap.Shapes[shapeno].fill0==0)	remap.Shapes[shapeno].fill0 = sl[mergerecord].fill0;	// Also merge old fills into new shape for later checks
			if(remap.Shapes[shapeno].fill1==0)	remap.Shapes[shapeno].fill1 = sl[mergerecord].fill1;
			if(sl[mergerecord].clockwise) {
				if(sl[mergerecord].fill1!=0) {
					remap.Fills[shapeno] = sl[mergerecord].fill1;
					break;
				} else if(sl[mergerecord].fill1!=0) {
					remap.Fills[shapeno] = sl[mergerecord].fill1;
					break;
				}
			} else {
				if(sl[mergerecord].fill0!=0) {
					remap.Fills[shapeno] = sl[mergerecord].fill0;
					break;
				} else if(sl[mergerecord].fill0!=0) {
					remap.Fills[shapeno] = sl[mergerecord].fill0;
					break;
				}
			}
		}
	}

	// Calculate bounding boxes for each new shape so we can do size comparisons and sorting
	std::vector<SWF::Rect> aabb;
	for(SWF::ShapeList::iterator s=remap.Shapes.begin(); s!=remap.Shapes.end(); s++) {
		SWF::Rect bb;
		for(std::vector<SWF::Vertex>::iterator v=s->vertices.begin(); v!=s->vertices.end(); v++) {
			if(v==s->vertices.begin()) {
				bb.xmin = bb.xmax = v->anchor.x;
				bb.ymin = bb.ymax = v->anchor.y;
				continue;
			}
			if(bb.xmin>v->anchor.x)	bb.xmin=v->anchor.x;
			if(bb.xmax<v->anchor.x)	bb.xmax=v->anchor.x;
			if(bb.ymin>v->anchor.y)	bb.ymin=v->anchor.y;
			if(bb.ymax<v->anchor.y)	bb.ymax=v->anchor.y;
		}
		aabb.push_back(bb);
	}
	// Find shapes that are enclosed within other shapes
	std::map<uint16_t,int32_t> parentshapes;
	for(uint16_t testshape=0; testshape<remap.Shapes.size(); testshape++) {
		std::list<uint16_t> enclosingshapes;
		for(uint16_t containershape=0; containershape<remap.Shapes.size(); containershape++) {
			if(containershape==testshape || !remap.Shapes[containershape].closed)	continue;
			bool contained = false;
			// Check all points, since we're not calculating curves for the shape, and any one point in another means it's guaranteed to be fully inside
			for(uint16_t v=0; v<remap.Shapes[testshape].vertices.size(); v++) {
				SWF::Point testpoint = remap.Shapes[testshape].vertices[v].anchor;
				uint16_t vcount = remap.Shapes[containershape].vertices.size();
				SWF::Vertex *varray = &remap.Shapes[containershape].vertices[0];
				for(uint16_t v=1; v<vcount; v++) {
					if(( ( varray[v-1].anchor.y>testpoint.y ) != ( varray[v].anchor.y>testpoint.y ) ) &&
						( testpoint.x<( varray[v-1].anchor.x-varray[v].anchor.x )*( testpoint.y-varray[v].anchor.y ) / ( varray[v-1].anchor.y-varray[v].anchor.y )+varray[v].anchor.x )) {
						contained = !contained;
					}
				}
				if(contained)	break;
			}
			if(contained)	enclosingshapes.push_back(containershape);
		}
		// Find the smallest shape that encloses our test shape; this is the parent
		SWF::Rect smallestparent;
		int32_t parentid = -1;
		for(std::list<uint16_t>::iterator s=enclosingshapes.begin(); s!=enclosingshapes.end(); s++) {
			uint16_t parent = *s;
			if(s==enclosingshapes.begin()) {
				smallestparent = aabb[parent];
				parentid = parent;
				continue;
			}
			if((aabb[parent].xmax-aabb[parent].xmin)<(smallestparent.xmax-smallestparent.xmin) ||
				(aabb[parent].ymax-aabb[parent].ymin)<(smallestparent.ymax-smallestparent.ymin)) {
				smallestparent = aabb[parent];
				parentid = parent;
			}
		}
		parentshapes[testshape] = parentid;
		// Satisfy fill rules for the containing shape's parent if necessary
		// Additionally, if this shape has no fill rules for itself at this point, it's a hole
		if(parentid<0)	continue;
		if(remap.Shapes[testshape].clockwise) {
			if(remap.Fills[parentid]==0 && remap.Shapes[testshape].fill0!=0)	remap.Fills[parentid] = remap.Shapes[testshape].fill0;
			if(remap.Shapes[testshape].fill1==0)	remap.Holes[parentid].push_back(testshape);
		} else {
			if(remap.Fills[parentid]==0 && remap.Shapes[testshape].fill1!=0)	remap.Fills[parentid] = remap.Shapes[testshape].fill1;
			if(remap.Shapes[testshape].fill0==0)	remap.Holes[parentid].push_back(testshape);
		}
	}

	// Do the final fill checks for shapes that still have none
	for(uint16_t shapeno=0; shapeno<remap.Shapes.size(); shapeno++) {
		if(remap.Fills[shapeno]==0) {
			if(remap.Shapes[shapeno].clockwise) {
				if(remap.Shapes[shapeno].fill1!=0)	remap.Fills[shapeno] = remap.Shapes[shapeno].fill1;
			} else {
				if(remap.Shapes[shapeno].fill0!=0)	remap.Fills[shapeno] = remap.Shapes[shapeno].fill0;
			}
		}
	}

	return remap;
}

PolyVectorPath ResourceImporterSWF::verts_to_curve(std::vector<SWF::Point> verts)
{
	PolyVectorPath pvpath;
	if(verts.size()>1) {
		Vector2 inctrldelta, outctrldelta, quadcontrol;
		Vector2 anchor(verts[0].x, -verts[0].y);
		Vector2 firstanchor = anchor;
		for(std::vector<SWF::Point>::iterator vi=verts.begin()+1; vi!=verts.end(); vi++) {
			SWF::Point vert = *vi;
			switch(((vi-verts.begin()+1)%2)) {
				case 0:
				{
					quadcontrol = Vector2(vert.x,-vert.y);
					outctrldelta = (quadcontrol-anchor)*(2.0f/3.0f);
					pvpath.curve.add_point(anchor, inctrldelta, outctrldelta);
					break;
				}
				case 1:
				{
					anchor = Vector2(vert.x,-vert.y);
					inctrldelta = (quadcontrol-anchor)*(2.0f/3.0f);
					break;
				}
			}
		}
		if(pvpath.closed) {
			//pvpath.curve.add_point(anchor, inctrldelta, outctrldelta);
			inctrldelta = (quadcontrol-firstanchor)*(2.0f/3.0f);
			pvpath.curve.add_point(firstanchor, inctrldelta, Vector2(0,0));
		}
	}
	return pvpath;
}
#endif
