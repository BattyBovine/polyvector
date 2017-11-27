#include "polyvector.h"

PolyVector::PolyVector()
{
#ifdef POLYVECTOR_DEBUG
	this->os = OS::get_singleton();
#endif

	this->iFrame = 0;
	this->v2Scale = Vector2( 1.0f, 1.0f );
	this->iCurveQuality = 3;
}

PolyVector::~PolyVector()
{
	this->clear();
}



bool PolyVector::triangulate_shapes()
{
	#ifdef POLYVECTOR_DEBUG
	uint64_t debugtimer = this->os->get_ticks_usec();
	#endif

	if(this->iFrame < this->lFrameData.size() && !this->lFrameData[this->iFrame].triangulated[this->iCurveQuality]) {
		for(List<PolyVectorShape>::Element *s = this->lFrameData[this->iFrame].shapes.front(); s; s = s->next()) {
			PolyVectorShape &shape = s->get();
			shape.vertices[this->iCurveQuality].clear();
			std::vector< std::vector<Vector2> > polygons;
			for(List<PolyVectorPath>::Element *p = shape.paths.front(); p; p = p->next()) {
				PolyVectorPath path = p->get();
				PoolVector2Array tess = path.curve->tessellate(this->iCurveQuality, POLYVECTOR_TESSELLATION_MAX_ANGLE);
				if(!path.closed) {		// If shape is not a closed loop, store as a stroke
					shape.strokes[this->iCurveQuality].push_back(tess);
				} else {				// Otherwise, triangulate
					int tess_size = tess.size();
					std::vector<Vector2> poly;
					PoolVector2Array::Read tessreader = tess.read();
					for(int i=0; i<tess_size-1; i++)
						poly.push_back(tessreader[i]);
					polygons.insert(polygons.begin(), poly);
					shape.vertices[this->iCurveQuality].insert(shape.vertices[this->iCurveQuality].begin(), poly.begin(), poly.end());
				}
			}
			if(!polygons.empty())	shape.indices[this->iCurveQuality] = mapbox::earcut<N>(polygons);
		}
		this->lFrameData[this->iFrame].triangulated[this->iCurveQuality] = true;
		#ifdef POLYVECTOR_DEBUG
		return this->render_shapes(debugtimer);
		#else
		return this->render_shapes();
		#endif
	} else {
		return false;
	}
}

bool PolyVector::render_shapes(uint64_t debugtimer)
{
	if(this->iFrame < this->lFrameData.size() && this->lFrameData[this->iFrame].triangulated[this->iCurveQuality]) {
		this->clear();
		for(List<PolyVectorShape>::Element *s = this->lFrameData[this->iFrame].shapes.front(); s; s = s->next()) {
			PolyVectorShape shape = s->get();
			if(shape.indices[this->iCurveQuality].size() > 0) {
				this->begin(Mesh::PRIMITIVE_TRIANGLES);
				this->set_color(Color(1.0f, 1.0f, 0.0f));
				for(std::vector<N>::reverse_iterator tris = shape.indices[this->iCurveQuality].rbegin();
					tris != shape.indices[this->iCurveQuality].rend();
					tris++) {	// Work through the vector in reverse to make sure the triangles' normals are facing forward
					this->add_vertex(Vector3(shape.vertices[this->iCurveQuality][*tris].x * this->v2Scale.x,
						shape.vertices[this->iCurveQuality][*tris].y * this->v2Scale.y,
						0.0f));
				}
				this->end();
			}
			for(List<PoolVector2Array>::Element *it = shape.strokes[this->iCurveQuality].front(); it; it = it->next()) {
				PoolVector2Array line = it->get();
				PoolVector2Array::Read lineread = it->get().read();
				this->begin(Mesh::PRIMITIVE_LINE_STRIP);
				for(int pt = 0; pt < line.size(); pt++) {
					this->add_vertex(Vector3(lineread[pt].x*this->v2Scale.x, lineread[pt].y*this->v2Scale.y, 0.0f));
				}
				this->end();
			}
		}
	} else {
		return this->triangulate_shapes();
	}
	#ifdef POLYVECTOR_DEBUG
	if(debugtimer > 0)
		printf("%s triangulated and rendered in %.6f seconds\n",
			   this->sSvgFile.ascii().get_data(),
			   (this->os->get_ticks_usec() - debugtimer) / 1000000.0L);
	#endif
	return true;
}



void PolyVector::set_svg_image(const Ref<SVGBin> &p_svg)
{
	if(p_svg == this->dataSvgFile)	return;
	this->dataSvgFile = p_svg;
	if(this->dataSvgFile.is_null()) {
		this->clear();
		return;
	}
	this->lFrameData = this->dataSvgFile->get_frames();
	this->v2Dimensions = this->dataSvgFile->get_dimensions();
	this->triangulate_shapes();
}
Ref<SVGBin> PolyVector::get_svg_image() const
{
	return this->dataSvgFile;
}

void PolyVector::set_vector_scale(Vector2 s)
{
	this->v2Scale=s;
	this->render_shapes();
}
Vector2 PolyVector::get_vector_scale()
{
	return this->v2Scale;
}

void PolyVector::set_curve_quality(int t)
{
	this->iCurveQuality = CLAMP(t, POLYVECTOR_MIN_QUALITY, POLYVECTOR_MAX_QUALITY);
	this->render_shapes();
}
int8_t PolyVector::get_curve_quality()
{
	return this->iCurveQuality;
}



void PolyVector::_bind_methods()
{
	ClassDB::bind_method(D_METHOD("set_svg_image"), &PolyVector::set_svg_image);
	ClassDB::bind_method(D_METHOD("get_svg_image"), &PolyVector::get_svg_image);
	ADD_PROPERTYNZ(PropertyInfo(Variant::OBJECT, "SVG", PROPERTY_HINT_RESOURCE_TYPE, "BinarySVG"), "set_svg_image", "get_svg_image");

	ClassDB::bind_method(D_METHOD("set_vector_scale"), &PolyVector::set_vector_scale);
	ClassDB::bind_method(D_METHOD("get_vector_scale"), &PolyVector::get_vector_scale);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "Vector Prescale"), "set_vector_scale", "get_vector_scale");

	ClassDB::bind_method(D_METHOD("set_curve_quality"), &PolyVector::set_curve_quality);
	ClassDB::bind_method(D_METHOD("get_curve_quality"), &PolyVector::get_curve_quality);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "Curve Quality"), "set_curve_quality", "get_curve_quality");
}
