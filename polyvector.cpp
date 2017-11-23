#include "polyvector.h"

PolyVector::PolyVector()
{
#ifdef POLYVECTOR_DEBUG
	this->os = OS::get_singleton();
#endif

	this->sSvgFile = "";
	this->nsvgImage = NULL;
	this->iFrame = 0;
	this->v2Scale = Vector2( 1.0f, 1.0f );
	this->iCurveQuality = 3;
}

PolyVector::~PolyVector()
{
	this->clear();
	nsvgDelete(this->nsvgImage);
}



bool PolyVector::set_svg_image(String filename)
{
	if (filename != this->sSvgFile) {
		#ifdef POLYVECTOR_DEBUG
		uint64_t debugtimer = this->os->get_ticks_usec();
		#endif
		this->sSvgFile = filename;
		this->vFrameData.clear();

		if (this->nsvgImage) nsvgDelete(this->nsvgImage);
		this->nsvgImage = nsvgParseFromFile(this->sSvgFile.ascii(true).get_data(), "px", 96);
		if (this->nsvgImage == NULL)	return false;

		PVFrame framedata;
		uint32_t shape_count = 0;
		for (NSVGshape *shape = this->nsvgImage->shapes; shape; shape = shape->next) {
			PVShape shapedata;
			uint32_t path_count = 0;
			for (NSVGpath *path = shape->paths; path; path = path->next) {
				PVPath pathdata;
				if (path->npts > 0) {
					float *p = &path->pts[0];
					pathdata.curve.add_point(Vector2(p[0], p[1]),
						Vector2(0.0f, 0.0f),
						Vector2(p[2] - p[0], p[3] - p[1]));
					for (int i = 0; i < (path->npts / 3); i++) {
						p = &path->pts[(i * 6) + 4];
						pathdata.curve.add_point(Vector2(p[2], p[3]),
							Vector2(p[0] - p[2], p[1] - p[3]),
							Vector2(p[4] - p[2], p[5] - p[3]));
					}
				}
				pathdata.closed = path->closed;
				pathdata.id = path_count;
				shapedata.paths.push_back(pathdata);
				path_count++;
			}
			shapedata.id = shape_count;
			shapedata.vertices.clear();
			shapedata.indices.clear();
			shapedata.strokes.clear();
			framedata.shapes.push_back(shapedata);
			shape_count++;
		}
		for(int i=POLYVECTOR_MIN_QUALITY; i<=POLYVECTOR_MAX_QUALITY; i++) framedata.triangulated[i] = false;
		this->vFrameData.push_back(framedata);

		#ifdef POLYVECTOR_DEBUG
		printf("%s parsed in %.6f seconds\n",
			   this->sSvgFile.ascii().get_data(),
			   (this->os->get_ticks_usec() - debugtimer) / 1000000.0L);
		#endif
	}

	return this->triangulate_shapes();
}

bool PolyVector::triangulate_shapes()
{
	#ifdef POLYVECTOR_DEBUG
	uint64_t debugtimer = this->os->get_ticks_usec();
	#endif

	if(this->iFrame < this->vFrameData.size() && !this->vFrameData[this->iFrame].triangulated[this->iCurveQuality]) {
		for(std::vector<PVShape>::iterator shape = this->vFrameData[this->iFrame].shapes.begin(); shape != this->vFrameData[this->iFrame].shapes.end(); shape++) {
			uint16_t shape_size = shape->size();
			shape->vertices[this->iCurveQuality].clear();
			std::vector< std::vector<Vector2> > polygons;
			for(std::vector<PVPath>::iterator path = shape->paths.begin(); path != shape->paths.end(); path++) {
				PoolVector2Array tess = path->curve.tessellate(this->iCurveQuality, POLYVECTOR_TESSELLATION_MAX_ANGLE);
				if(!path->closed) {		// If shape is not a closed loop, store as a stroke
					shape->strokes[this->iCurveQuality].push_back(tess);
				} else {				// Otherwise, triangulate
					int tess_size = tess.size();
					std::vector<Vector2> poly;
					PoolVector2Array::Read tessreader = tess.read();
					for(int i=0; i<tess_size-1; i++)
						poly.push_back(tessreader[i]);
					polygons.insert(polygons.begin(), poly);
					shape->vertices[this->iCurveQuality].insert(shape->vertices[this->iCurveQuality].begin(), poly.begin(), poly.end());
				}
			}
			if(!polygons.empty())	shape->indices[this->iCurveQuality] = mapbox::earcut<N>(polygons);
		}
		this->vFrameData[this->iFrame].triangulated[this->iCurveQuality] = true;
	}
	#ifdef POLYVECTOR_DEBUG
	return this->render_shapes(debugtimer);
	#else
	return this->render_shapes();
	#endif
}

bool PolyVector::render_shapes(uint64_t debugtimer)
{
	if(this->iFrame < this->vFrameData.size() && this->vFrameData[this->iFrame].triangulated[this->iCurveQuality]) {
		this->clear();
		for(std::vector<PVShape>::iterator shape = this->vFrameData[this->iFrame].shapes.begin(); shape != this->vFrameData[this->iFrame].shapes.end(); shape++) {
			if(shape->indices[this->iCurveQuality].size() > 0) {
				this->begin(Mesh::PRIMITIVE_TRIANGLES);
				this->set_color(Color(1.0f, 1.0f, 0.0f));
				for(std::vector<N>::iterator tris = shape->indices[this->iCurveQuality].begin(); tris != shape->indices[this->iCurveQuality].end(); tris++) {
					this->add_vertex(Vector3(shape->vertices[this->iCurveQuality][*tris].x * this->v2Scale.x,
						-(shape->vertices[this->iCurveQuality][*tris].y-this->nsvgImage->height) * this->v2Scale.y,
						0.0f));
				}
				this->end();
			}
			for(List<PoolVector2Array>::Element *it = shape->strokes[this->iCurveQuality].front(); it; it = it->next()) {
				PoolVector2Array line = it->get();
				PoolVector2Array::Read lineread = it->get().read();
				this->begin(Mesh::PRIMITIVE_LINE_STRIP);
				for(int pt = 0; pt < line.size(); pt++) {
					this->add_vertex(Vector3(lineread[pt].x*this->v2Scale.x, -(lineread[pt].y - this->nsvgImage->height)*this->v2Scale.y, 0.0f));
				}
				this->end();
			}
		}
	}
	#ifdef POLYVECTOR_DEBUG
	if(debugtimer > 0)
		printf("%s triangulated and rendered in %.6f seconds\n",
			   this->sSvgFile.ascii().get_data(),
			   (this->os->get_ticks_usec() - debugtimer) / 1000000.0L);
	#endif
	return true;
}



String PolyVector::get_svg_image()
{
	return this->sSvgFile;
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
	this->triangulate_shapes();
}
int8_t PolyVector::get_curve_quality()
{
	return this->iCurveQuality;
}



void PolyVector::_bind_methods()
{
	ClassDB::bind_method(D_METHOD("set_svg_image"), &PolyVector::set_svg_image);
	ClassDB::bind_method(D_METHOD("get_svg_image"), &PolyVector::get_svg_image);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "SVG"), "set_svg_image", "get_svg_image");

	ClassDB::bind_method(D_METHOD("set_vector_scale"), &PolyVector::set_vector_scale);
	ClassDB::bind_method(D_METHOD("get_vector_scale"), &PolyVector::get_vector_scale);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "Vector Prescale"), "set_vector_scale", "get_vector_scale");

	ClassDB::bind_method(D_METHOD("set_curve_quality"), &PolyVector::set_curve_quality);
	ClassDB::bind_method(D_METHOD("get_curve_quality"), &PolyVector::get_curve_quality);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "Curve Quality"), "set_curve_quality", "get_curve_quality");
}
