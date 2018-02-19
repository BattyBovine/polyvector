#include <core/os/os.h>
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
						fillstyledef[PV_JSON_NAME_COLOUR] += 255;
						fillstyledef[PV_JSON_NAME_COLOUR] += 0;
						fillstyledef[PV_JSON_NAME_COLOUR] += 255;
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
					SWFPolygonList shapes = this->shape_builder(character.shapes);	// Merge shapes from Flash into solid objects and calculate hole placement and fill rules
					for(SWFPolygonList::iterator shape=shapes.begin(); shape!=shapes.end(); shape++) {
						//uint16_t shapeno = (s-remap.Shapes.cbegin());
						json shapeout;
						if(shape->polygon.layer>0)
							shapeout[PV_JSON_NAME_LAYER] = shape->polygon.layer;
						shapeout[PV_JSON_NAME_FILL] = shape->polygon.fill0;
						//shapeout[PV_JSON_NAME_STROKE] = shape.stroke;
						shapeout[PV_JSON_NAME_CLOSED] = shape->polygon.closed;
						if(shape->area<0)
							this->points_reverse(&shape->polygon);
						for(std::vector<SWF::Vertex>::iterator v=shape->polygon.vertices.begin(); v!=shape->polygon.vertices.end(); v++) {
							if(v!=shape->polygon.vertices.begin()) {
								shapeout[PV_JSON_NAME_VERTICES] += bool(p_options["binary"]) ? v->control.x  : double(round(v->control.x*100.0f)/100.0L);
								shapeout[PV_JSON_NAME_VERTICES] += bool(p_options["binary"]) ? -v->control.y : double(round(-v->control.y*100.0f)/100.0L);
							}
							if(shapeout[PV_JSON_NAME_CLOSED]==true && v==(shape->polygon.vertices.end()-1))
								break;
							shapeout[PV_JSON_NAME_VERTICES] += bool(p_options["binary"]) ? v->anchor.x  : double(round(v->anchor.x*100.0f)/100.0L);
							shapeout[PV_JSON_NAME_VERTICES] += bool(p_options["binary"]) ? -v->anchor.y : double(round(-v->anchor.y*100.0f)/100.0L);
						}
						for(std::list<uint16_t>::iterator h=shape->children.begin(); h!=shape->children.end(); h++) {
							shapeout[PV_JSON_NAME_HOLES] += (*h);
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
							charout[PV_JSON_NAME_TRANSFORM] += bool(p_options["binary"]) ? -dl->second.transform.RotateSkew0 : double(round(-dl->second.transform.RotateSkew0*100) / 100.0L);
							charout[PV_JSON_NAME_TRANSFORM] += bool(p_options["binary"]) ? -dl->second.transform.RotateSkew1 : double(round(-dl->second.transform.RotateSkew1*100) / 100.0L);
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

ResourceImporterSWF::SWFPolygonList ResourceImporterSWF::shape_builder(SWF::ShapeList sl)
{
	SWFPolygonList shapeparts;
	shapeparts.reserve(sl.size());

	for(SWF::ShapeList::iterator s=sl.begin(); s!=sl.end(); s++) {	// Collect already closed shapes first
		if(s->closed) {
			SWFPolygon sp;
			sp.polygon = *s;
			sp.area = this->shape_area(sp.polygon);
			if(sp.area < 0)			sp.polygon.fill1 = sp.polygon.fill0;	// Use left fill if the shape is wound counter-clockwise
			else if(sp.area > 0)	sp.polygon.fill0 = sp.polygon.fill1;	// Use right fill if the shape is wound clockwise
			else continue;
			shapeparts.push_back(sp);
		}
	}

	std::sort(shapeparts.begin(), shapeparts.end(),							// Sort from smallest to largest
		[](const SWFPolygon &a, const SWFPolygon &b) { return abs(a.area) < abs(b.area); });

	std::map<SWFPolygonList::iterator, std::list<SWF::ShapeList::iterator> > childsegments;
	std::set<SWFPolygonList::iterator> discardedpolygons;
	std::set<SWF::ShapeList::iterator> discardedsegments;
	for(SWFPolygonList::iterator outer=shapeparts.begin(); outer!=shapeparts.end(); outer++) {		// For every closed polygon...
		for(SWFPolygonList::iterator inner=shapeparts.begin(); inner!=shapeparts.end(); inner++) {	// ...find the child polygons (i.e. holes)...
			if(outer==inner ||
				!inner->polygon.closed ||
				discardedpolygons.find(inner)!=discardedpolygons.end() ||
				!this->shape_contains_point(inner->polygon.vertices.front().anchor, outer->polygon))
				continue;
			outer->children.push_back(inner-shapeparts.begin());
			discardedpolygons.insert(inner);
		}
		for(SWF::ShapeList::iterator inner=sl.begin(); inner!=sl.end(); inner++) {					// ...and then inner line segments
			if(inner->closed ||
				discardedsegments.find(inner)!=discardedsegments.end() ||
				!this->shape_contains_point(inner->vertices.front().anchor, outer->polygon))
				continue;
			childsegments[outer].push_back(inner);
			discardedsegments.insert(inner);
		}
	}

	std::set<SWF::ShapeList::iterator> left, right;	// Shapes that get used to satisfy fill rules are discarded here
	for(SWFPolygonList::iterator parent=shapeparts.begin(); parent!=shapeparts.end(); parent++) {	// Merge line segments into complete shapes
		for(std::list<SWF::ShapeList::iterator>::iterator l=childsegments[parent].begin(); l!=childsegments[parent].end(); l++) {
			SWF::ShapeList::iterator line = *l;
			if(left.find(line)==left.end() && line->fill0!=parent->polygon.fill0) {		// Check to the left of the shape first (counter-clockwise)
				SWFPolygon sp;
				sp.polygon = *line;
				this->find_connected_shapes(&sp.polygon, line, false, &left, &right, &childsegments[parent]);
				if(sp.polygon.closed) {
					sp.area = this->shape_area(sp.polygon);
					sp.polygon.fill1 = sp.polygon.fill0;
					sp.parent = (parent-shapeparts.begin());
					parent->children.push_back(shapeparts.size());
					shapeparts.push_back(sp);
				}
			}
			if(right.find(line)==right.end() && line->fill1!=parent->polygon.fill0) {	// Then to the right (clockwise)
				SWFPolygon sp;
				sp.polygon = *line;
				this->find_connected_shapes(&sp.polygon, line, true, &left, &right, &childsegments[parent]);
				if(sp.polygon.closed) {
					sp.area = this->shape_area(sp.polygon);
					sp.polygon.fill0 = sp.polygon.fill1;
					sp.parent = (parent-shapeparts.begin());
					parent->children.push_back(shapeparts.size());
					shapeparts.push_back(sp);
				}
			}
		}
	}

	SWFPolygonList shapeadd;
	std::list<SWF::ShapeList::iterator> outersegments;
	for(SWF::ShapeList::iterator line=sl.begin(); line!=sl.end(); line++) {	// If line segments that have no parents exist at this point, these are our actual outside edges
		if(!line->closed && discardedsegments.find(line)==discardedsegments.end())
			outersegments.push_back(line);
	}
	for(std::list<SWF::ShapeList::iterator>::iterator l=outersegments.begin(); l!=outersegments.end(); l++) {	// Make complete shapes from these segments
		SWF::ShapeList::iterator line = *l;
		if(!line->closed && discardedsegments.find(line)==discardedsegments.end()) {
			if(left.find(line)==left.end() &&
				line->fill0!=0) {		// Check to the left of the shape first (counter-clockwise)
				SWFPolygon sp;
				sp.polygon = *line;
				this->find_connected_shapes(&sp.polygon, line, false, &left, &right, &outersegments);
				if(sp.polygon.closed) {
					sp.area = this->shape_area(sp.polygon);
					sp.polygon.fill1 = sp.polygon.fill0;
					shapeadd.push_back(sp);
				}
			}
			if(right.find(line)==right.end() &&
				line->fill1!=0) {	// Then to the right (clockwise)
				SWFPolygon sp;
				sp.polygon = *line;
				this->find_connected_shapes(&sp.polygon, line, true, &left, &right, &outersegments);
				if(sp.polygon.closed) {
					sp.area = this->shape_area(sp.polygon);
					sp.polygon.fill0 = sp.polygon.fill1;
					shapeadd.push_back(sp);
				}
			}
		}
	}
	for(SWFPolygonList::iterator outer=shapeadd.begin(); outer!=shapeadd.end(); outer++) {	// Find this shape's children in our original list
		for(SWFPolygonList::iterator inner=shapeparts.begin(); inner!=shapeparts.end(); inner++) {
			if(this->shape_contains_point(inner->polygon.vertices.front().anchor, outer->polygon) &&
				inner->parent<0) {	// Only add a child if the found shape has no parent already
				inner->parent = (outer-shapeadd.begin())+shapeparts.size();	// Set the new outer shape as the child shape's parent (assuming we'll later be appending the new shapes to the old list)
				outer->children.push_back(inner-shapeparts.begin());
			}
		}
	}
	shapeparts.insert(shapeparts.end(), shapeadd.begin(), shapeadd.end());	// Finally, merge our newly-found shapes to the previous list

	return shapeparts;
}

bool ResourceImporterSWF::shape_contains_point(SWF::Point innervertex, SWF::Shape outershape)
{
	uint16_t outervertexcount = outershape.vertices.size();
	SWF::Vertex *outervertices = &outershape.vertices[0];
	bool contained = false;
	for(uint16_t outeredge=1; outeredge<outervertexcount; outeredge++) {
		if(( outervertices[outeredge].anchor.y>innervertex.y )!=( outervertices[outeredge-1].anchor.y>innervertex.y ) &&
			innervertex.x<( outervertices[outeredge-1].anchor.x-outervertices[outeredge].anchor.x )*( innervertex.y-outervertices[outeredge].anchor.y )/( outervertices[outeredge-1].anchor.y-outervertices[outeredge].anchor.y )+outervertices[outeredge].anchor.x)
			contained = !contained;
	}
	return contained;
}

void ResourceImporterSWF::find_connected_shapes(SWF::Shape *buildshape, SWF::ShapeList::iterator s, bool clockwise, std::set<SWF::ShapeList::iterator> *leftused, std::set<SWF::ShapeList::iterator> *rightused, std::list<SWF::ShapeList::iterator> *sl)
{
	std::list<SWF::ShapeList::iterator>::iterator ns;
	for(ns=sl->begin(); ns!=sl->end(); ns++) {
		SWF::ShapeList::iterator next_shape = *ns;
		if(next_shape==s || next_shape->closed)
			continue;
		if(this->points_equal(buildshape->vertices.back(), next_shape->vertices.back())) {	// If attached in reverse, compare opposite fills
			if((!clockwise && buildshape->fill0==next_shape->fill1) ||
				(clockwise && buildshape->fill1==next_shape->fill0))
				break;
		} else if(this->points_equal(buildshape->vertices.back(), next_shape->vertices.front())) {	// If attached in sequence, compare matching fills
			if((!clockwise && buildshape->fill0==next_shape->fill0) ||
				(clockwise && buildshape->fill1==next_shape->fill1))
				break;
		}
		else	continue;	// If the shape isn't attached, skip
	}
	if(ns==sl->end())	// If no connected shapes were found, this is the end
		return;
	SWF::ShapeList::iterator next_shape = *ns;
	SWF::Shape mergeshape = *next_shape;
	if(this->points_equal(buildshape->vertices.back(), mergeshape.vertices.back())) {
		this->points_reverse(&mergeshape);
		if(clockwise)
			leftused->insert(next_shape);
		else
			rightused->insert(next_shape);
	} else {
		if(clockwise)
			rightused->insert(next_shape);
		else
			leftused->insert(next_shape);
	}
	buildshape->vertices.insert(buildshape->vertices.end(), mergeshape.vertices.begin()+1, mergeshape.vertices.end());

	if(this->points_equal(buildshape->vertices.back(), buildshape->vertices.front()))
		buildshape->closed = true;
	else
		this->find_connected_shapes(buildshape, next_shape, clockwise, leftused, rightused, sl);
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
	uint16_t fill1 = s->fill0;
	s->fill0 = s->fill1;
	s->fill1 = fill1;
}

inline float ResourceImporterSWF::shape_area(SWF::Shape s)
{
	if(s.vertices.size()<3)
		return 0.0f;
	if(!s.closed)
		s.vertices.push_back(s.vertices.front());
	size_t vsize = s.vertices.size();
	SWF::Vertex *varray = &s.vertices[0];
	float area = 0.0f;
	for(uint16_t i=1; i<vsize; i++)
		area += ((varray[i-1].anchor.x * varray[i].anchor.y) - (varray[i].anchor.x * varray[i-1].anchor.y));
	return (area / 2);
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
		std::vector<uint8_t> msgpak(jsonstring, jsonstring+jsonlength);
		jsondata = json::from_msgpack(msgpak);
	} catch(const json::parse_error&) {
		jsondata = json::parse(jsonstring, jsonstring+jsonlength);
		try {	// If the data could not be parsed as MessagePack-encoded JSON, it's probably plain text
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
			if(!jshape[PV_JSON_NAME_LAYER].is_null())
				pvshape.layer = jshape[PV_JSON_NAME_LAYER];
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
			for(json::iterator jhv=jshape[PV_JSON_NAME_HOLES].begin(); jhv!=jshape[PV_JSON_NAME_HOLES].end(); jhv++)
				pvshape.holes.push_back(*jhv);
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
		Vector2 anchor(jverts[0], jverts[1]);
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
					quadcontrol.y = vert;
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
					anchor.y = vert;
					inctrldelta = (quadcontrol-anchor)*(2.0f/3.0f);
					break;
				}
			}
		}
		if(pvpath.closed) {
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
