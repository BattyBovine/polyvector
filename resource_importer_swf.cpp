﻿#include <core/os/os.h>
#include <io/resource_saver.h>
#include <os/file_access.h>

#include "resource_importer_swf.h"

#ifdef TOOLS_ENABLED
//Error ResourceImporterSVG::import(const String &p_source_file, const String &p_save_path, const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files)
//{
//	FileAccess *svg = FileAccess::open(p_source_file, FileAccess::READ);
//	ERR_FAIL_COND_V(!svg, ERR_FILE_CANT_READ);
//	FileAccess *svgraw = FileAccess::open(p_save_path + ".svgraw", FileAccess::WRITE);
//	ERR_FAIL_COND_V(!svgraw, ERR_FILE_CANT_WRITE);
//
//	size_t xmllen = svg->get_len();
//	ERR_FAIL_COND_V(!xmllen, ERR_CANT_OPEN);
//	{
//		uint8_t *svgdata = new uint8_t[xmllen];
//		svg->get_buffer(svgdata, xmllen);
//		svgraw->store_buffer(svgdata, xmllen);
//	}
//	svgraw->close();
//	memdelete(svgraw);
//	svg->close();
//	memdelete(svg);
//
//	return OK;
//}

Error ResourceImporterSWF::import(const String &p_source_file, const String &p_save_path, const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files)
{
	FileAccess *swf = FileAccess::open(p_source_file, FileAccess::READ);
	ERR_FAIL_COND_V(!swf, ERR_FILE_CANT_READ);

	size_t xmllen = swf->get_len();
	ERR_FAIL_COND_V(!xmllen, ERR_CANT_OPEN);
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
		json root;
		std::map<uint16_t, uint16_t> fillstylemap, linestylemap, charactermap;
		{	// Build the library definitions first
			for(SWF::FillStyleMap::iterator fsm=dict->FillStyles.begin(); fsm!=dict->FillStyles.end(); fsm++) {
				json fillstylearray;
				for(SWF::FillStyleArray::iterator fs=fsm->second.begin(); fs!=fsm->second.end(); fs++) {
					SWF::FillStyle fillstyle = *fs;
					fillstylemap[fs-fsm->second.begin()+1] = fillstylemap.size()+1;
					json fillstyledef;
					if(fillstyle.StyleType==SWF::FillStyle::Type::SOLID) {
						fillstyledef[PV_JSON_NAME_COLOUR] += fillstyle.Color.r;
						fillstyledef[PV_JSON_NAME_COLOUR] += fillstyle.Color.g;
						fillstyledef[PV_JSON_NAME_COLOUR] += fillstyle.Color.b;
						if(fillstyle.Color.a < 255)
							fillstyledef[PV_JSON_NAME_COLOUR] += fillstyle.Color.a;
					} else {	// Placeholder for unsupported fill types
						fillstyledef[PV_JSON_NAME_COLOUR] += 0;
						fillstyledef[PV_JSON_NAME_COLOUR] += 0;
						fillstyledef[PV_JSON_NAME_COLOUR] += 0;
					}
					fillstylearray += fillstyledef;
				}
				root[PV_JSON_NAME_LIBRARY][PV_JSON_NAME_FILLSTYLES] += fillstylearray;
			}
			//for(SWF::LineStyleMap::iterator lsm=dict->LineStyles.begin(); lsm!=dict->LineStyles.end(); lsm++) {
			//	json linestylearray;
			//	for(SWF::LineStyleArray::iterator ls=lsm->second.begin(); ls!=lsm->second.end(); ls++) {
			//		SWF::LineStyle linestyle = *ls;
			//		if(linestyle.Width>0.0f) {
			//			linestylemap[ls-lsm->second.begin()+1] = linestylemap.size()+1;
			//			json linestyledef;
			//			linestyledef[PV_JSON_NAME_LINEWIDTH] = bool(p_options["binary"]) ? linestyle.Width : double(round(linestyle.Width*100)/100.0L);
			//			linestyledef[PV_JSON_NAME_COLOUR] += linestyle.Color.r;
			//			linestyledef[PV_JSON_NAME_COLOUR] += linestyle.Color.g;
			//			linestyledef[PV_JSON_NAME_COLOUR] += linestyle.Color.b;
			//			if(linestyle.Color.a < 255)
			//				linestyledef[PV_JSON_NAME_COLOUR] += linestyle.Color.a;
			//			linestylearray += linestyledef;
			//		}
			//	}
			//	root[PV_JSON_NAME_LIBRARY][PV_JSON_NAME_LINESTYLES] += linestylearray;
			//}
			for(SWF::CharacterDict::iterator cd = dict->CharacterList.begin(); cd != dict->CharacterList.end(); cd++) {
				uint16_t characterid = cd->first;
				SWF::Character character = cd->second;
				if(characterid>0) {
					charactermap[characterid] = charactermap.size()+1;
					json characterdef;
					ShapeRemap remap = this->shape_builder(character.shapes);	// Merge shapes from Flash into solid objects and calculate hole placement and fill rules
					for(SWF::ShapeList::iterator s=remap.Shapes.begin(); s!=remap.Shapes.end(); s++) {
						uint16_t shapeno = (s-remap.Shapes.begin());
						if(remap.Fills[shapeno]==0)	continue;	// Skip holes; we handle those along with the parent shape
						SWF::Shape shape = *s;
						json shapeout;
						shapeout[PV_JSON_NAME_FILL] = remap.Fills[shapeno];
						//shapeout[PV_JSON_NAME_STROKE] = shape.stroke;
						shapeout[PV_JSON_NAME_CLOSED] = shape.closed;
						if(shape.winding == SWF::Shape::Winding::COUNTERCLOCKWISE) {	// Keep enclosing shapes wound clockwise
							std::vector<SWF::Vertex> reverseverts;
							SWF::Point controlcache;
							for(std::vector<SWF::Vertex>::reverse_iterator v=shape.vertices.rbegin(); v!=shape.vertices.rend(); v++) {
								SWF::Point newctrl = controlcache;
								controlcache = v->control;
								v->control = newctrl;
								reverseverts.push_back(*v);
							}
							shape.vertices = reverseverts;
						}
						for(std::vector<SWF::Vertex>::iterator v=shape.vertices.begin(); v!=shape.vertices.end(); v++) {
							if(v!=shape.vertices.begin()) {
								shapeout[PV_JSON_NAME_VERTICES] += bool(p_options["binary"]) ? v->control.x : double(round(v->control.x*100.0f)/100.0L);
								shapeout[PV_JSON_NAME_VERTICES] += bool(p_options["binary"]) ? v->control.y : double(round(v->control.y*100.0f)/100.0L);
							}
							if(shapeout[PV_JSON_NAME_CLOSED]==true && v==(shape.vertices.end()-1))
								break;
							shapeout[PV_JSON_NAME_VERTICES] += bool(p_options["binary"]) ? v->anchor.x : double(round(v->anchor.x*100.0f)/100.0L);
							shapeout[PV_JSON_NAME_VERTICES] += bool(p_options["binary"]) ? v->anchor.y : double(round(v->anchor.y*100.0f)/100.0L);
						}
						for(std::list<uint16_t>::iterator h=remap.Holes[shapeno].begin(); h!=remap.Holes[shapeno].end(); h++) {
							uint16_t hole = *h;
							json holeverts;
							if(remap.Shapes[hole].winding == SWF::Shape::Winding::CLOCKWISE) {	// Keep holes wound counter-clockwise
								std::vector<SWF::Vertex> reverseverts;
								SWF::Point controlcache;
								for(std::vector<SWF::Vertex>::reverse_iterator v=remap.Shapes[hole].vertices.rbegin(); v!=remap.Shapes[hole].vertices.rend(); v++) {
									SWF::Point newctrl = controlcache;
									controlcache = v->control;
									v->control = newctrl;
									reverseverts.push_back(*v);
								}
								remap.Shapes[hole].vertices = reverseverts;
							}
							for(std::vector<SWF::Vertex>::iterator hv=remap.Shapes[hole].vertices.begin(); hv!=remap.Shapes[hole].vertices.end(); hv++) {
								if(hv!=remap.Shapes[hole].vertices.begin()) {
									holeverts += bool(p_options["binary"]) ? hv->control.x : double(round(hv->control.x*100.0f)/100.0L);
									holeverts += bool(p_options["binary"]) ? hv->control.y : double(round(hv->control.y*100.0f)/100.0L);
								}
								if(remap.Shapes[hole].closed==true && hv==(remap.Shapes[hole].vertices.end()-1))
									break;
								holeverts += bool(p_options["binary"]) ? hv->anchor.x : double(round(hv->anchor.x*100.0f)/100.0L);
								holeverts += bool(p_options["binary"]) ? hv->anchor.y : double(round(hv->anchor.y*100.0f)/100.0L);
							}
							shapeout[PV_JSON_NAME_HOLES] += holeverts;
						}
						characterdef += shapeout;
					}
					root[PV_JSON_NAME_LIBRARY][PV_JSON_NAME_CHARACTERS] += characterdef;
				}
			}
		}
		for(SWF::FrameList::iterator f=dict->Frames.begin(); f!=dict->Frames.end(); f++) {
			SWF::DisplayList framedisplist = *f;
			json jdisplaylist;
			for(SWF::DisplayList::iterator dl=framedisplist.begin(); dl!=framedisplist.end(); dl++) {
				if(dl->second.id>0) {
					json charout;
					charout[PV_JSON_NAME_ID] = charactermap[dl->second.id-1];
					charout[PV_JSON_NAME_DEPTH] = dl->first;
					charout[PV_JSON_NAME_TRANSFORM] += bool(p_options["binary"]) ? dl->second.transform.TranslateX  : double(round(dl->second.transform.TranslateX*100) / 100.0L);
					charout[PV_JSON_NAME_TRANSFORM] += bool(p_options["binary"]) ? dl->second.transform.TranslateY  : double(round(dl->second.transform.TranslateY*100) / 100.0L);
					if((round(dl->second.transform.ScaleX*100)!=100.0f || round(dl->second.transform.ScaleY*100)!=100.0f) ||	// Only add the scale and rotate transformation values if they are different from the default
						(round(dl->second.transform.RotateSkew0*100)!=0.0f || round(dl->second.transform.RotateSkew1*100)!=0.0f)) {	// If the rotation value is different but scale is not, store the scale value anyway
						charout[PV_JSON_NAME_TRANSFORM] += bool(p_options["binary"]) ? dl->second.transform.ScaleX : double(round(dl->second.transform.ScaleX*100) / 100.0L);
						charout[PV_JSON_NAME_TRANSFORM] += bool(p_options["binary"]) ? dl->second.transform.ScaleY : double(round(dl->second.transform.ScaleY*100) / 100.0L);
						if(round(dl->second.transform.RotateSkew0*100)!=0.0f || round(dl->second.transform.RotateSkew1*100)!=0.0f) {
							charout[PV_JSON_NAME_TRANSFORM] += bool(p_options["binary"]) ? dl->second.transform.RotateSkew0 : double(round(dl->second.transform.RotateSkew0*100) / 100.0L);
							charout[PV_JSON_NAME_TRANSFORM] += bool(p_options["binary"]) ? dl->second.transform.RotateSkew1 : double(round(dl->second.transform.RotateSkew1*100) / 100.0L);
						}
					}
					jdisplaylist += charout;
				}
			}
			if(jdisplaylist.size()>0) root[PV_JSON_NAME_FRAMES] += jdisplaylist;
		}
		root[PV_JSON_NAME_FPS] = bool(p_options["binary"]) ? swfparser->get_properties()->framerate : double(round(swfparser->get_properties()->framerate*100) / 100.0L);

		FileAccess *pvimport = FileAccess::open(p_save_path + ".jvec", FileAccess::WRITE);
		ERR_FAIL_COND_V(!pvimport, ERR_FILE_CANT_WRITE);
		if(bool(p_options["binary"])) {
			std::vector<uint8_t> jsonout = json::to_msgpack(root);
			pvimport->store_buffer(jsonout.data(), jsonout.size());
		} else {
			std::string out = root.dump(bool(p_options["prettify_text"])?2:-1);
			pvimport->store_buffer((const uint8_t*)out.c_str(), out.size());
		}
		pvimport->close();
		memdelete(pvimport);

		if(swfparser)	delete swfparser;
		if(swfdata)		delete swfdata;
	}
	swf->close();
	memdelete(swf);

	return OK;
}

void ResourceImporterSWF::get_import_options(List<ImportOption> *r_options, int p_preset) const
{
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "binary"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "prettify_text"), false));
}

ResourceImporterSWF::ShapeRemap ResourceImporterSWF::shape_builder(SWF::ShapeList sl)
{
	// Stores the results of shape merges, parents and fill rule checks
	ShapeRemap remap;

	// Merge all connected lines with compatible fills into complete shapes
	List<List<SWF::ShapeList::iterator> > shapeparts;
	for(SWF::ShapeList::iterator s=sl.begin(); s!=sl.end(); s++) {
		SWF::Shape buildshape = *s;
		if(buildshape.closed) {
			remap.Shapes.push_back(buildshape);
		} else {
			List<SWF::ShapeList::iterator> linesegments;
			this->find_connected_shapes(&buildshape, &linesegments, s, &sl);
			remap.Shapes.push_back(buildshape);
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
		if(remap.Shapes[testshape].winding == SWF::Shape::Winding::CLOCKWISE) {
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
			if(remap.Shapes[shapeno].winding == SWF::Shape::Winding::CLOCKWISE) {
				if(remap.Shapes[shapeno].fill1!=0)
					remap.Fills[shapeno] = remap.Shapes[shapeno].fill1;
			} else {
				if(remap.Shapes[shapeno].fill0!=0)
					remap.Fills[shapeno] = remap.Shapes[shapeno].fill0;
			}
		}
	}

	return remap;
}

void ResourceImporterSWF::find_connected_shapes(SWF::Shape *buildshape, List<SWF::ShapeList::iterator> *linesegments, SWF::ShapeList::iterator s, SWF::ShapeList *sl)
{
	uint16_t shapeno = s-sl->begin();
	SWF::ShapeList::iterator smallest_shape = sl->end();
	real_t winding_angle = (buildshape->winding==SWF::Shape::Winding::CLOCKWISE) ? 0.0f : Math_INF;
	Vector2 C(buildshape->vertices.back().anchor.x, buildshape->vertices.back().anchor.y);	// Calculate the angle of the intersection between the two lines
	Vector2 A((buildshape->vertices.rbegin()+1)->anchor.x, (buildshape->vertices.rbegin()+1)->anchor.y);
	Vector2 a = (A-C).normalized();
	for(SWF::ShapeList::iterator s2=sl->begin(); s2!=sl->end(); s2++) {
		uint16_t shape2no = s2-sl->begin();
		if(s2==s || s2->closed || linesegments->find(s2))
			continue;
		Vector2 B;
		if(this->points_equal(buildshape->vertices.back(), s2->vertices.back()))	// If attached in reverse, get the last line
			B = Vector2((s2->vertices.rbegin()+1)->anchor.x, (s2->vertices.rbegin()+1)->anchor.y);
		else if(this->points_equal(buildshape->vertices.back(), s2->vertices.front()))	// If attached in sequence, get the first line
			B = Vector2(s2->vertices[1].anchor.x, s2->vertices[1].anchor.y);
		else	continue;	// If the shape isn't actually attached, skip
		Vector2 b = (B-C).normalized();
		
		real_t Ø = atan2(b.y, b.x) - atan2(a.y, a.x);
		if(Ø<0)	Ø += Math_TAU;
		if(buildshape->winding==SWF::Shape::Winding::CLOCKWISE && Ø>winding_angle) {
			winding_angle = Ø;
			smallest_shape = s2;
		} else if(buildshape->winding==SWF::Shape::Winding::COUNTERCLOCKWISE && Ø<winding_angle) {
			winding_angle = Ø;
			smallest_shape = s2;
		} else if(buildshape->winding==SWF::Shape::Winding::NONE) {
			if(buildshape->fill1!=0 && Ø>winding_angle) {	// If there is a fill1 defined, assume clockwise rotation
				buildshape->winding = SWF::Shape::Winding::CLOCKWISE;
				winding_angle = Ø;
				smallest_shape = s2;
			} else if(Ø<winding_angle) {	// Otherwise, assume counter-clockwise rotation
				buildshape->winding = SWF::Shape::Winding::COUNTERCLOCKWISE;
				winding_angle = Ø;
				smallest_shape = s2;
			}
		}
	}
	if(smallest_shape==sl->end())	// If no connected shapes were found, this is the end
		return;
	linesegments->push_back(smallest_shape);
	SWF::Shape mergeshape = *smallest_shape;
	if(this->points_equal(buildshape->vertices.back(), mergeshape.vertices.back()))
		this->points_reverse(&mergeshape);
	if(buildshape->fill0==0)	buildshape->fill0 = mergeshape.fill0;
	if(buildshape->fill1==0)	buildshape->fill1 = mergeshape.fill1;
	buildshape->vertices.insert(buildshape->vertices.end(), mergeshape.vertices.begin()+1, mergeshape.vertices.end());

	if(this->points_equal(buildshape->vertices.back(), buildshape->vertices.front()))
		buildshape->closed = true;
	else
		this->find_connected_shapes(buildshape, linesegments, smallest_shape, sl);
}

inline bool ResourceImporterSWF::points_equal(SWF::Vertex &a, SWF::Vertex &b)
{
	return (
		int32_t(round(a.anchor.x*20.0f))==int32_t(round(b.anchor.x*20.0f)) &&
		int32_t(round(a.anchor.y*20.0f))==int32_t(round(b.anchor.y*20.0f))
		);
}

inline void ResourceImporterSWF::points_reverse(SWF::Shape *s)
{
	std::vector<SWF::Vertex> reverseverts;
	SWF::Point controlcache;
	for(std::vector<SWF::Vertex>::reverse_iterator v=s->vertices.rbegin(); v!=s->vertices.rend(); v++) {
		SWF::Point newctrl = controlcache;
		controlcache = v->control;
		v->control = newctrl;
		reverseverts.push_back(*v);
	}
	s->vertices = reverseverts;
	s->winding = (s->winding==SWF::Shape::Winding::CLOCKWISE) ? SWF::Shape::Winding::COUNTERCLOCKWISE : SWF::Shape::Winding::CLOCKWISE;
	uint16_t fill1 = s->fill0;
	s->fill0 = s->fill1;
	s->fill1 = fill1;
}
#endif



RES ResourceLoaderJSONVector::load(const String &p_path, const String &p_original_path, Error *r_error)
{
	if(r_error)	*r_error = ERR_FILE_CANT_OPEN;
	FileAccess *polyvector = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V(!polyvector, RES());
	size_t jsonlength = polyvector->get_len();
	uint8_t *jsonstring = new uint8_t[jsonlength];
	polyvector->get_buffer(jsonstring, jsonlength);
	polyvector->close();
	memdelete(polyvector);

	json jsondata;
	try {
		jsondata = json::parse(jsonstring, jsonstring+jsonlength);
	} catch(const json::parse_error&) {
		try {	// If the data could not be parsed as a string, it might be MessagePack-encoded
			std::vector<uint8_t> msgpak(jsonstring, jsonstring+jsonlength);
			jsondata = json::from_msgpack(msgpak);
		} catch(const json::parse_error &e) {
			OS::get_singleton()->alert(String("JSON error: ")+e.what(), "JSON Error");
		}
	}
	delete[] jsonstring;

	Ref<JSONVector> vectordata;
	vectordata.instance();

	// Load library data
	json jchardict = jsondata[PV_JSON_NAME_LIBRARY][PV_JSON_NAME_CHARACTERS];
	for(json::iterator jci=jchardict.begin(); jci!=jchardict.end(); jci++) {
		json jchar = *jci;
		uint16_t characterid = jci-jchardict.begin();
		PolyVectorCharacter pvchar;
		for(json::iterator jsi=jchar.begin(); jsi!=jchar.end(); jsi++) {
			json jshape = *jsi;
			uint16_t jshapefill = jshape[PV_JSON_NAME_FILL];
			PolyVectorShape pvshape;
			if(jshapefill>0) {
				json jcolour = jsondata[PV_JSON_NAME_LIBRARY][PV_JSON_NAME_FILLSTYLES][characterid][jshapefill-1][PV_JSON_NAME_COLOUR];
				pvshape.fillcolour = Color(jcolour[0]/255.0f, jcolour[1]/255.0f, jcolour[2]/255.0f);
				if(jcolour.size()>3)	pvshape.fillcolour.a = jcolour[3]/255.0f;
			}
			//uint16_t jshapestroke = jshape[PV_JSON_NAME_STROKE];
			//if(jshapestroke>0) {
			//	json jcolour = jsondata[PV_JSON_NAME_LIBRARY][PV_JSON_NAME_LINESTYLES][characterid][jshapestroke-1][PV_JSON_NAME_COLOUR];
			//	pvshape.strokecolour = Color(jcolour[0]/255.0f, jcolour[1]/255.0f, jcolour[2]/255.0f);
			//	if(jcolour.size()>3)	pvshape.strokecolour.a = jcolour[3]/255.0f;
			//}
			PolyVectorPath pvpath = this->verts_to_curve(jshape[PV_JSON_NAME_VERTICES]);
			pvpath.closed = jshape[PV_JSON_NAME_CLOSED];
			pvshape.path = pvpath;
			for(json::iterator jhv=jshape[PV_JSON_NAME_HOLES].begin(); jhv!=jshape[PV_JSON_NAME_HOLES].end(); jhv++) {
				pvshape.holes.push_back(this->verts_to_curve(*jhv));
			}
			pvchar.push_back(pvshape);
		}
		vectordata->add_character(pvchar);
	}

	// Load frame data
	for(json::iterator jfi=jsondata[PV_JSON_NAME_FRAMES].begin(); jfi!=jsondata[PV_JSON_NAME_FRAMES].end(); jfi++) {
		json jdisplaylist = *jfi;
		PolyVectorFrame frame;
		for(json::iterator jdli=jdisplaylist.begin(); jdli!=jdisplaylist.end(); jdli++) {
			json jdisplayitem = *jdli;
			PolyVectorSymbol pvom;
			pvom.depth = jdisplayitem[PV_JSON_NAME_DEPTH];
			pvom.id = jdisplayitem[PV_JSON_NAME_ID];
			if(jdisplayitem[PV_JSON_NAME_TRANSFORM].size()>=2) {
				pvom.matrix.TranslateX = jdisplayitem[PV_JSON_NAME_TRANSFORM][0];
				pvom.matrix.TranslateY = jdisplayitem[PV_JSON_NAME_TRANSFORM][1];
			}
			if(jdisplayitem[PV_JSON_NAME_TRANSFORM].size()>=4) {
				pvom.matrix.ScaleX = jdisplayitem[PV_JSON_NAME_TRANSFORM][2];
				pvom.matrix.ScaleY = jdisplayitem[PV_JSON_NAME_TRANSFORM][3];
			}
			if(jdisplayitem[PV_JSON_NAME_TRANSFORM].size()>=6) {
				pvom.matrix.Skew0 = jdisplayitem[PV_JSON_NAME_TRANSFORM][4];
				pvom.matrix.Skew1 = jdisplayitem[PV_JSON_NAME_TRANSFORM][5];
			}
			frame.push_back(pvom);
		}
		vectordata->add_frame(frame);
	}

	vectordata->set_fps(jsondata[PV_JSON_NAME_FPS]);

	if(r_error)	*r_error = OK;

	return vectordata;
}

PolyVectorPath ResourceLoaderJSONVector::verts_to_curve(json jverts)
{
	PolyVectorPath pvpath;
	if(jverts.size()>2) {
		Vector2 inctrldelta, outctrldelta, quadcontrol;
		Vector2 anchor(float(jverts[0]), -float(jverts[1]));
		Vector2 firstanchor = anchor;
		for(json::iterator jvi=jverts.begin()+2; jvi!=jverts.end(); jvi++) {
			float vert = *jvi;
			switch(((jvi-jverts.begin()+2)%4)) {
				case 0:
				{
					quadcontrol.x = vert;
					break;
				}
				case 1:
				{
					quadcontrol.y = -vert;
					outctrldelta = (quadcontrol-anchor)*(2.0f/3.0f);
					pvpath.curve.add_point(anchor, inctrldelta, outctrldelta);
					break;
				}
				case 2:
				{
					anchor.x = vert;
					break;
				}
				case 3:
				{
					anchor.y = -vert;
					inctrldelta = (quadcontrol-anchor)*(2.0f/3.0f);
					break;
				}
			}
		}
		if(pvpath.closed) {
			pvpath.curve.add_point(anchor, inctrldelta, outctrldelta);
			inctrldelta = (quadcontrol-firstanchor)*(2.0f/3.0f);
			pvpath.curve.add_point(firstanchor, inctrldelta, Vector2(0,0));
		}
	}
	return pvpath;
}



//RES ResourceLoaderSVG::load(const String &p_path, const String &p_original_path, Error *r_error)
//{
//	if(r_error)	*r_error = ERR_FILE_CANT_OPEN;
//	FileAccess *svgxml = FileAccess::open(p_path, FileAccess::READ);
//	ERR_FAIL_COND_V(!svgxml, RES());
//	size_t xmllen = svgxml->get_len();
//	uint8_t *svgdata = new uint8_t[xmllen];
//	svgxml->get_buffer(svgdata, xmllen);
//	struct NSVGimage *img = nsvgParse((char*)svgdata, "px", 96);
//	ERR_FAIL_COND_V(!img, RES());
//
//	Vector2 dimensions;
//	dimensions.x = img->width;
//	dimensions.y = img->height;
//	PolyVectorFrame framedata;
//	uint32_t shape_count = 0;
//	for(NSVGshape *shape = img->shapes; shape; shape = shape->next) {
//		PolyVectorShape shapedata;
//		shapedata.fillcolour.r = ( ( shape->fill.color ) & 0x000000FF ) / 255.0f;
//		shapedata.fillcolour.g = ( ( shape->fill.color>>8 ) & 0x000000FF ) / 255.0f;
//		shapedata.fillcolour.b = ( ( shape->fill.color>>16 ) & 0x000000FF ) / 255.0f;
//		shapedata.fillcolour.a = ( ( shape->fill.color>>24 ) & 0x000000FF ) / 255.0f;
//		shapedata.strokecolour.r = ( ( shape->stroke.color ) & 0x000000FF ) / 255.0f;
//		shapedata.strokecolour.g = ( ( shape->stroke.color>>8 ) & 0x000000FF ) / 255.0f;
//		shapedata.strokecolour.b = ( ( shape->stroke.color>>16 ) & 0x000000FF ) / 255.0f;
//		shapedata.strokecolour.a = ( ( shape->stroke.color>>24 ) & 0x000000FF ) / 255.0f;
//		shapedata.id = shape_count;
//		uint32_t path_count = 0;
//		for(NSVGpath *path = shape->paths; path; path = path->next) {
//			PolyVectorPath pathdata;
//			if(path->npts > 0) {
//				float *p = &path->pts[0];
//				pathdata.curve.add_point(
//					Vector2(p[0], -p[1]),
//					Vector2(0.0f, 0.0f),
//					Vector2(p[2]-p[0], -( p[3]-p[1] ))
//				);
//				for(int i = 0; i < ( path->npts/3 ); i++) {
//					p = &path->pts[( i*6 )+4];
//					pathdata.curve.add_point(
//						Vector2(p[2], -p[3]),
//						Vector2(p[0]-p[2], -( p[1]-p[3] )),
//						Vector2(p[4]-p[2], -( p[5]-p[3] ))
//					);
//				}
//			}
//			pathdata.closed = path->closed;
//			pathdata.id = path_count;
//			if(path->closed)	pathdata.hole = this->_is_clockwise(pathdata.curve);
//			else				pathdata.hole = false;
//			shapedata.paths.push_back(pathdata);
//			path_count++;
//		}
//		shapedata.paths.back().hole = false;		// Last shape is always a non-hole
//		shapedata.vertices.clear();
//		shapedata.indices.clear();
//		shapedata.strokes.clear();
//		framedata.shapes.push_back(shapedata);
//		shape_count++;
//	}
//	nsvgDelete(img);
//
//	Ref<RawSVG> rawsvg;
//	rawsvg.instance();
//	rawsvg->add_frame(framedata);
//	rawsvg->set_dimensions(dimensions);
//
//	if(r_error)	*r_error = OK;
//
//	return rawsvg;
//}
//
//bool inline ResourceLoaderSVG::_is_clockwise(Curve2D c)
//{
//	if(c.get_point_count() < 3)	return false;
//	N pointcount = c.get_point_count();
//	int area = 0;
//	Vector2 p0 = c.get_point_position(0);
//	Vector2 pn = c.get_point_position(pointcount-1);
//	for(N i=1; i<pointcount; i++) {
//		Vector2 p1 = c.get_point_position(i);
//		Vector2 p2 = c.get_point_position(i-1);
//		area += ( p1.x - p2.x ) * ( p1.y + p2.y );
//	}
//	area += ( p0.x - pn.x ) * ( p0.y + pn.y );
//	return ( area >= 0 );
//}
