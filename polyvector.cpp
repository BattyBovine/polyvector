#include "polyvector.h"

PolyVector::PolyVector()
{
#ifdef POLYVECTOR_DEBUG
	this->os = OS::get_singleton();
#endif
	this->iFrame = 0;
	this->v2Scale = Vector2( 1.0f, 1.0f );
	this->v2Offset = Vector2( 0.0f, 0.0f );
	this->iCurveQuality = 2;
	this->bZOrderOffset = true;
	this->fLayerDepth = 0.0f;

	this->materialDefault.instance();
	this->materialDefault->set_flag(SpatialMaterial::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	this->materialDefault->set_flag(SpatialMaterial::FLAG_SRGB_VERTEX_COLOR, true);
	this->set_material_override(this->materialDefault);
}

PolyVector::~PolyVector()
{
	if(!this->materialDefault.is_null())	this->materialDefault.unref();
	this->clear();
}



bool PolyVector::triangulate_shapes()
{
	#ifdef POLYVECTOR_DEBUG
	uint64_t debugtimer = this->os->get_ticks_usec();
	#endif

	if(this->iFrame < this->lFrameData.size() && !this->lFrameData[this->iFrame].triangulated[this->iCurveQuality]) {
		for(std::list<PolyVectorShape>::iterator s=this->lFrameData[this->iFrame].shapes.begin(); s!=this->lFrameData[this->iFrame].shapes.end(); s++) {
			PolyVectorShape &shape = *s;
			shape.vertices[this->iCurveQuality].clear();
			std::vector< std::vector<Vector2> > polygons;
			PoolVector2Array tess = shape.path.curve.tessellate(this->iCurveQuality, POLYVECTOR_TESSELLATION_MAX_ANGLE);
			if(!shape.path.closed) {	// If shape is not a closed loop, store as a stroke
				shape.strokes[this->iCurveQuality].push_back(tess);
			} else {					// Otherwise, triangulate
				uint32_t tess_size = tess.size();
				std::vector<Vector2> poly;
				PoolVector2Array::Read tessreader = tess.read();
				for(uint32_t i=1; i<tess_size; i++)
					poly.push_back(tessreader[i]);
				polygons.push_back(poly);
				shape.vertices[this->iCurveQuality].insert(shape.vertices[this->iCurveQuality].end(), poly.begin(), poly.end());
				for(std::list<PolyVectorPath>::iterator hole=shape.holes.begin(); hole!=shape.holes.end(); hole++) {
					PoolVector2Array holetess = hole->curve.tessellate(this->iCurveQuality, POLYVECTOR_TESSELLATION_MAX_ANGLE);
					uint32_t holetess_size = holetess.size();
					std::vector<Vector2> holepoly;
					PoolVector2Array::Read holetessreader = holetess.read();
					for(uint32_t j=0; j<holetess_size; j++)
						holepoly.push_back(holetessreader[j]);
					polygons.push_back(holepoly);
					shape.vertices[this->iCurveQuality].insert(shape.vertices[this->iCurveQuality].end(), holepoly.begin(), holepoly.end());
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
	}
	return this->render_shapes();
}

bool PolyVector::render_shapes()
{
	if(this->iFrame < this->lFrameData.size() && this->lFrameData[this->iFrame].triangulated[this->iCurveQuality]) {
		this->clear();
		float depthoffset = 0.0f;
		for(std::list<PolyVectorShape>::iterator s=this->lFrameData[this->iFrame].shapes.begin(); s!=this->lFrameData[this->iFrame].shapes.end(); s++) {
			PolyVectorShape shape = *s;
			if(shape.indices[this->iCurveQuality].size() >= 0) {
				this->begin(Mesh::PRIMITIVE_TRIANGLES);
				for(std::vector<N>::reverse_iterator tris = shape.indices[this->iCurveQuality].rbegin();
					tris != shape.indices[this->iCurveQuality].rend();
					tris++) {	// Work through the vector in reverse to make sure the triangles' normals are facing forward
					this->set_color(shape.fillcolour);
					this->set_normal(Vector3(0.0, 0.0, 1.0));
					this->add_vertex(Vector3(
						( shape.vertices[this->iCurveQuality][*tris].x * (this->v2Scale.x/1000.0f) ) + this->v2Offset.x,
						( shape.vertices[this->iCurveQuality][*tris].y * (this->v2Scale.y/1000.0f) ) + this->v2Offset.y,
						depthoffset));
				}
				this->end();
			}
			for(List<PoolVector2Array>::Element *it = shape.strokes[this->iCurveQuality].front(); it; it = it->next()) {
				PoolVector2Array line = it->get();
				PoolVector2Array::Read lineread = it->get().read();
				this->begin(Mesh::PRIMITIVE_LINE_STRIP);
				for(int pt = 0; pt < line.size(); pt++) {
					this->set_color(shape.strokecolour);
					this->set_normal(Vector3(0.0, 0.0, 1.0));
					this->add_vertex(Vector3(
						( lineread[pt].x * (this->v2Scale.x/1000.0f) ) + this->v2Offset.x,
						( lineread[pt].y * (this->v2Scale.y/1000.0f) ) + this->v2Offset.y,
						depthoffset));
				}
				this->end();
			}
			if(this->bZOrderOffset)	depthoffset += this->fLayerDepth;
		}
	}
	return true;
}



void PolyVector::set_vector_image(const Ref<JSONVector> &p_vector)
{
	if(p_vector == this->dataVectorFile)	return;
	this->dataVectorFile = p_vector;
	if(this->dataVectorFile.is_null())
		return;
	this->lFrameData = this->dataVectorFile->get_frames();
	//this->v2Dimensions = this->dataVectorFile->get_dimensions();
	this->triangulate_shapes();
}
Ref<JSONVector> PolyVector::get_vector_image() const
{
	return this->dataVectorFile;
}

void PolyVector::set_frame(uint16_t f)
{
	this->iFrame = CLAMP(f,0,this->lFrameData.size());
	this->triangulate_shapes();
}
uint16_t PolyVector::get_frame()
{
	return this->iFrame;
}

void PolyVector::set_curve_quality(int t)
{
	this->iCurveQuality = t;
	this->triangulate_shapes();
}
int8_t PolyVector::get_curve_quality()
{
	return this->iCurveQuality;
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

void PolyVector::set_offset(Vector2 s)
{
	this->v2Offset=s;
	this->render_shapes();
}
Vector2 PolyVector::get_offset()
{
	return this->v2Offset;
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

void PolyVector::set_material_unshaded(bool t)
{
	this->materialDefault->set_flag(SpatialMaterial::FLAG_UNSHADED, t);
	this->set_material_override(this->materialDefault);
}
bool PolyVector::get_material_unshaded()
{
	return this->materialDefault->get_flag(SpatialMaterial::FLAG_UNSHADED);
}

void PolyVector::set_billboard(int b)
{
	this->materialDefault->set_billboard_mode((SpatialMaterial::BillboardMode)b);
	this->set_material_override(this->materialDefault);
}
int PolyVector::get_billboard()
{
	return (this->materialDefault->get_billboard_mode());
}



void PolyVector::_bind_methods()
{
	ClassDB::bind_method(D_METHOD("set_vector_image"), &PolyVector::set_vector_image);
	ClassDB::bind_method(D_METHOD("get_vector_image"), &PolyVector::get_vector_image);
	ADD_PROPERTYNZ(PropertyInfo(Variant::OBJECT, "Vector", PROPERTY_HINT_RESOURCE_TYPE, "JSONVector"), "set_vector_image", "get_vector_image");

	ClassDB::bind_method(D_METHOD("set_frame"), &PolyVector::set_frame);
	ClassDB::bind_method(D_METHOD("get_frame"), &PolyVector::get_frame);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "Frame"), "set_frame", "get_frame");

	ClassDB::bind_method(D_METHOD("set_curve_quality"), &PolyVector::set_curve_quality);
	ClassDB::bind_method(D_METHOD("get_curve_quality"), &PolyVector::get_curve_quality);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "Curve Quality", PROPERTY_HINT_RANGE, "0,9,1,2"), "set_curve_quality", "get_curve_quality");

	ADD_GROUP("Display","");
	ClassDB::bind_method(D_METHOD("set_material_unshaded"), &PolyVector::set_material_unshaded);
	ClassDB::bind_method(D_METHOD("get_material_unshaded"), &PolyVector::get_material_unshaded);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "Unshaded"), "set_material_unshaded", "get_material_unshaded");

	ClassDB::bind_method(D_METHOD("set_billboard"), &PolyVector::set_billboard);
	ClassDB::bind_method(D_METHOD("get_billboard"), &PolyVector::get_billboard);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "Billboard", PROPERTY_HINT_ENUM, "Disabled,Enabled,Y-Billboard,Particle"), "set_billboard", "get_billboard");

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
