#include "polyvector.h"

PolyVector::PolyVector()
{
#ifdef POLYVECTOR_DEBUG
	this->os = OS::get_singleton();
#endif
	this->aabbBounds.position = Vector3();
	this->aabbBounds.size = Vector3();

	this->iFrame = 0;
	this->v2Scale = Vector2( 1.0f, 1.0f );
	this->v2Offset = Vector2( 0.0f, 0.0f );
	this->iCurveQuality = 2;
	this->bZOrderOffset = true;
	this->fLayerDepth = 0.0f;

	this->smBaseMaterial.instance();
	this->smBaseMaterial->set_flag(SpatialMaterial::FLAG_USE_VERTEX_LIGHTING, true);
	this->smBaseMaterial->set_flag(SpatialMaterial::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	this->smBaseMaterial->set_flag(SpatialMaterial::FLAG_SRGB_VERTEX_COLOR, true);
	this->smBaseMaterial->set_billboard_mode(SpatialMaterial::BILLBOARD_ENABLED);
	this->smBaseMaterial->set_feature(SpatialMaterial::FEATURE_TRANSPARENT, true);
}

PolyVector::~PolyVector()
{
	this->smBaseMaterial.unref();
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
					for(int i=1; i<tess_size; i++)
						poly.push_back(tessreader[i]);
					polygons.insert(polygons.begin(), poly);
					shape.vertices[this->iCurveQuality].insert(shape.vertices[this->iCurveQuality].begin(), poly.begin(), poly.end());
				}
			}
			if(!polygons.empty())	shape.indices[this->iCurveQuality] = mapbox::earcut<N>(polygons);
		}
		this->lFrameData[this->iFrame].triangulated[this->iCurveQuality] = true;
		#ifdef POLYVECTOR_DEBUG
		if(debugtimer > 0)
			printf("%s triangulated in %.6f seconds\n",
				this->sSvgFile.ascii().get_data(),
				( this->os->get_ticks_usec() - debugtimer ) / 1000000.0L);
		#endif
		return this->render_shapes();
	}
	return false;
}

bool PolyVector::render_shapes()
{
	this->aabbBounds.position = Vector3(this->v2Offset.x, this->v2Offset.y, 0.0f);
	this->aabbBounds.size = Vector3();

	if(this->iFrame < this->lFrameData.size() && this->lFrameData[this->iFrame].triangulated[this->iCurveQuality]) {
		SurfaceTool meshbuilder;
		meshbuilder.begin(Mesh::PRIMITIVE_TRIANGLES);
		float depthoffset = 0.0f;
		for(List<PolyVectorShape>::Element *s = this->lFrameData[this->iFrame].shapes.front(); s; s = s->next()) {
			PolyVectorShape shape = s->get();
			meshbuilder.add_color(shape.fillcolour);
			meshbuilder.add_normal(Vector3(0.0f, 0.0f, 1.0f));
			if(shape.indices[this->iCurveQuality].size() > 0) {
				for(std::vector<N>::reverse_iterator tris = shape.indices[this->iCurveQuality].rbegin();
					tris != shape.indices[this->iCurveQuality].rend();
					tris++) {	// Work through the vector in reverse to make sure the triangles' normals are facing forward
					Vector3 p(
						( shape.vertices[this->iCurveQuality][*tris].x * ( this->v2Scale.x/1000.0f ) ) + this->v2Offset.x,
						( shape.vertices[this->iCurveQuality][*tris].y * ( this->v2Scale.y/1000.0f ) ) + this->v2Offset.y,
						depthoffset);
					meshbuilder.add_vertex(p);
					this->aabbBounds.expand_to(p);
				}
			}
			//for(List<PoolVector2Array>::Element *it = shape.strokes[this->iCurveQuality].front(); it; it = it->next()) {
			//	PoolVector2Array line = it->get();
			//	PoolVector2Array::Read lineread = it->get().read();
			//	this->begin(Mesh::PRIMITIVE_LINE_STRIP);
			//	for(int pt = 0; pt < line.size(); pt++) {
			//		this->set_color(shape.strokecolour);
			//		this->add_vertex(Vector3(
			//			( lineread[pt].x * (this->v2Scale.x/1000.0f) ) + this->v2Offset.x,
			//			( lineread[pt].y * (this->v2Scale.y/1000.0f) ) + this->v2Offset.y,
			//			depthoffset));
			//	}
			//	this->end();
			//}
			if(this->bZOrderOffset)	depthoffset += this->fLayerDepth;
		}
		meshbuilder.set_material(this->smBaseMaterial);
		meshbuilder.index();
		this->mCurrentMesh.unref();
		this->mCurrentMesh = meshbuilder.commit();
		this->set_base(this->mCurrentMesh->get_rid());
	}
	return true;
}



void PolyVector::set_svg_image(const Ref<RawSVG> &p_svg)
{
	if(p_svg == this->dataSvgFile)	return;
	this->dataSvgFile = p_svg;
	if(this->dataSvgFile.is_null()) {
		return;
	}
	this->lFrameData = this->dataSvgFile->get_frames();
	this->v2Dimensions = this->dataSvgFile->get_dimensions();
	this->triangulate_shapes();
}
Ref<RawSVG> PolyVector::get_svg_image() const
{
	return this->dataSvgFile;
}

void PolyVector::set_unit_scale(Vector2 s)
{
	this->v2Scale=s;
	this->render_shapes();
}
Vector2 PolyVector::get_unit_scale()
{
	return this->v2Scale;
}

void PolyVector::set_curve_quality(int t)
{
	this->iCurveQuality = CLAMP(t, POLYVECTOR_MIN_QUALITY, POLYVECTOR_MAX_QUALITY);
	this->triangulate_shapes();
	this->render_shapes();
}
int8_t PolyVector::get_curve_quality()
{
	return this->iCurveQuality;
}

void PolyVector::set_layer_separation(real_t d)
{
	this->fLayerDepth = d;
	this->render_shapes();
}
real_t PolyVector::get_layer_separation()
{
	return this->fLayerDepth;
}

void PolyVector::set_offset(Vector2 s)
{
	this->v2Offset=s;
	this->render_shapes();
}
Vector2 PolyVector::get_offset()
{
	return this->v2Offset;
}

AABB PolyVector::get_aabb() const
{
	return this->aabbBounds;
}

PoolVector<Face3> PolyVector::get_faces(uint32_t p_usage_flags) const
{
	if(!( p_usage_flags & ( FACES_SOLID | FACES_ENCLOSING ) ) ||
		this->mCurrentMesh.is_null())	return PoolVector<Face3>();
	return this->mCurrentMesh->get_faces();
}



void PolyVector::_bind_methods()
{
	ClassDB::bind_method(D_METHOD("set_svg_image"), &PolyVector::set_svg_image);
	ClassDB::bind_method(D_METHOD("get_svg_image"), &PolyVector::get_svg_image);
	ADD_PROPERTYNZ(PropertyInfo(Variant::OBJECT, "SVG", PROPERTY_HINT_RESOURCE_TYPE, "RawSVG"), "set_svg_image", "get_svg_image");

	ClassDB::bind_method(D_METHOD("set_curve_quality"), &PolyVector::set_curve_quality);
	ClassDB::bind_method(D_METHOD("get_curve_quality"), &PolyVector::get_curve_quality);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "Curve Quality"), "set_curve_quality", "get_curve_quality");

	ADD_GROUP("Adjustments","");
	ClassDB::bind_method(D_METHOD("set_offset"), &PolyVector::set_offset);
	ClassDB::bind_method(D_METHOD("get_offset"), &PolyVector::get_offset);
	ADD_PROPERTYNZ(PropertyInfo(Variant::VECTOR2, "Offset"), "set_offset", "get_offset");

	ClassDB::bind_method(D_METHOD("set_unit_scale"), &PolyVector::set_unit_scale);
	ClassDB::bind_method(D_METHOD("get_unit_scale"), &PolyVector::get_unit_scale);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "Unit Scale"), "set_unit_scale", "get_unit_scale");

	ClassDB::bind_method(D_METHOD("set_layer_separation"), &PolyVector::set_layer_separation);
	ClassDB::bind_method(D_METHOD("get_layer_separation"), &PolyVector::get_layer_separation);
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "Layer Separation", PROPERTY_HINT_RANGE, "0.0, 1.0, 0.0"), "set_layer_separation", "get_layer_separation");
}
