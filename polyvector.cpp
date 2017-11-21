#include "polyvector.h"

PolyVector::PolyVector()
{
	this->os = OS::get_singleton();

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

		std::vector<PVShape> framedata;
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
			framedata.push_back(shapedata);
			shape_count++;
		}
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

	for(std::vector< std::vector<PVShape> >::iterator f = this->vFrameData.begin(); f != this->vFrameData.end(); f++) {
		std::vector<PVShape> &frame = *f;
		for(std::vector<PVShape>::iterator s = frame.begin(); s != frame.end(); s++) {
			PVShape &shape = *s;
			if(!shape.triangles[this->iCurveQuality].empty())
				continue;
			uint16_t shape_size = shape.size();
			List<TriangulatorPoly> polygons;
			for(uint16_t p = 0; p < shape_size; p++) {
				PoolVector2Array tess = shape[p].curve.tessellate(this->iCurveQuality);
				int tess_size = tess.size();
				TriangulatorPoly poly;
				poly.Init(tess_size);
				Vector2 *tessarray = poly.GetPoints();
				PoolVector2Array::Read tessreader = tess.read();
				for(int i=0; i<tess_size; i++) tessarray[i] = tessreader[i];

				if(!shape[p].closed) {		// If shape is not a closed loop, store as a stroke
					shape.strokes[this->iCurveQuality].push_back(poly);
				} else {					// Otherwise, triangulate
					if(p == shape_size-1) {	// If this is the last path, assume it's a bounding polygon and turn counter-clockwise
						poly.SetHole(false);
						poly.SetOrientation(TRIANGULATOR_CCW);
					} else {				// Anything before the last should be considered a hole and turned clockwise
						poly.SetHole(true);
						poly.SetOrientation(TRIANGULATOR_CW);
					}
					polygons.push_back(poly);
				}
			}
			TriangulatorPartition triangulator;
			List<TriangulatorPoly> noholes, triangles;
			if(polygons.size()>0)	triangulator.Triangulate_EC(&polygons[polygons.size()-1], &triangles);
			//triangulator.RemoveHoles(&polygons, &noholes);
			//triangulator.Triangulate_EC(&noholes, &triangles);
			if(triangles.size()>0)	shape.triangles[this->iCurveQuality].push_back(triangles);
		}
	}
#ifdef POLYVECTOR_DEBUG
	return this->render_shapes(debugtimer);
#else
	return this->render_shapes();
#endif
}

bool PolyVector::render_shapes(uint64_t debugtimer)
{
	this->clear();
	for(std::vector< std::vector<PVShape> >::iterator f = this->vFrameData.begin(); f != this->vFrameData.end(); f++) {
		std::vector<PVShape> &frame = *f;
		for(std::vector<PVShape>::iterator s = frame.begin(); s != frame.end(); s++) {
			PVShape &shape = *s;
			for(List< List<TriangulatorPoly> >::Element *it = shape.triangles[this->iCurveQuality].front(); it; it = it->next()) {
				List<TriangulatorPoly> polygroup = it->get();
				for(List<TriangulatorPoly>::Element *it2 = polygroup.front(); it2; it2 = it2->next()) {
					this->begin(Mesh::PRIMITIVE_LINE_LOOP);
					TriangulatorPoly poly = it2->get();
					//if(poly.GetNumPoints() == 3) {
					//	this->add_vertex(Vector3(poly[0].x*this->v2Scale.x, -(poly[0].y - this->nsvgImage->height)*this->v2Scale.y, 0.0f));
					//	this->add_vertex(Vector3(poly[1].x*this->v2Scale.x, -(poly[1].y - this->nsvgImage->height)*this->v2Scale.y, 0.0f));
					//	this->add_vertex(Vector3(poly[2].x*this->v2Scale.x, -(poly[2].y - this->nsvgImage->height)*this->v2Scale.y, 0.0f));
					//}
					for(int i=0; i<poly.GetNumPoints(); i++)
						this->add_vertex(Vector3(poly[i].x*this->v2Scale.x, -(poly[i].y - this->nsvgImage->height)*this->v2Scale.y, 0.0f));
					this->end();
				}
			}
			for(List<TriangulatorPoly>::Element *it = shape.strokes[this->iCurveQuality].front(); it; it = it->next()) {
				TriangulatorPoly poly = it->get();
				this->begin(Mesh::PRIMITIVE_LINE_STRIP);
				for(int pt = 0; pt < poly.GetNumPoints(); pt++) {
					this->add_vertex(Vector3(poly[pt].x*this->v2Scale.x, -(poly[pt].y - this->nsvgImage->height)*this->v2Scale.y, 0.0f));
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
	this->iDebugTimer = 0;
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
	this->iCurveQuality = CLAMP(t,0,9);
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
