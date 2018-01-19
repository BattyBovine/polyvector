#include "polyvector.h"

PolyVector::PolyVector()
{
	this->iFrame = 0;
	this->fUnitScale = 1.0f;
	this->v2Offset = Vector2(0.0f, 0.0f);
	this->iCurveQuality = 2;
	this->bZOrderOffset = true;
	this->fLayerDepth = 0.0f;

	this->materialDefault.instance();
	this->materialDefault->set_flag(SpatialMaterial::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	this->materialDefault->set_flag(SpatialMaterial::FLAG_SRGB_VERTEX_COLOR, true);
	this->set_material(this->materialDefault);

	#ifdef POLYVECTOR_DEBUG
	this->os = OS::get_singleton();
	this->triangulation_time = 0.0L;
	#endif
}

PolyVector::~PolyVector()
{
	if(!this->materialDefault.is_null())	this->materialDefault.unref();
}



void PolyVector::triangulate_mesh()
{
	#ifdef POLYVECTOR_DEBUG
	this->vertex_count = 0;
	this->triangulation_time = 0.0L;
	uint64_t debugtimer = this->os->get_ticks_usec();
	#endif
	
	for(List<PolyVectorShape>::Element *s=this->lShapes.front(); s; s=s->next()) {
		PolyVectorShape &shape = s->get();
		shape.mesh[this->iCurveQuality].vertices.clear();
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
			shape.mesh[this->iCurveQuality].vertices.insert(shape.mesh[this->iCurveQuality].vertices.end(), poly.begin(), poly.end());
			for(List<PolyVectorPath>::Element *h=shape.holes.front(); h; h=h->next()) {
				PolyVectorPath hole = h->get();
				PoolVector2Array holetess = hole.curve.tessellate(this->iCurveQuality, POLYVECTOR_TESSELLATION_MAX_ANGLE);
				uint32_t holetess_size = holetess.size();
				std::vector<Vector2> holepoly;
				PoolVector2Array::Read holetessreader = holetess.read();
				for(uint32_t j=0; j<holetess_size; j++)
					holepoly.push_back(holetessreader[j]);
				polygons.push_back(holepoly);
				shape.mesh[this->iCurveQuality].vertices.insert(shape.mesh[this->iCurveQuality].vertices.end(), holepoly.begin(), holepoly.end());
			}
		}
		if(!polygons.empty())
			shape.mesh[this->iCurveQuality].indices = mapbox::earcut<N>(polygons);
	}

	#ifdef POLYVECTOR_DEBUG
	this->triangulation_time = ((this->os->get_ticks_usec()-debugtimer)/1000000.0L);
	#endif

	this->_request_update();
}

void PolyVector::_create_mesh_array(Array &p_arr) const
{
	PoolVector<Vector3> vertices;
	PoolVector<Vector3> normals;
	PoolVector<Color> colours;
	float depthoffset = 0.0f;
	for(const List<PolyVectorShape>::Element *s=this->lShapes.front(); s; s=s->next()) {
		PolyVectorShape shape = s->get();
		if(shape.mesh[this->iCurveQuality].indices.size() > 0) {
			float unitscale = (this->fUnitScale/1000.0f);
			for(std::vector<N>::reverse_iterator tris = shape.mesh[this->iCurveQuality].indices.rbegin();
				tris != shape.mesh[this->iCurveQuality].indices.rend();
				tris++) {	// Work through the vector in reverse to make sure the triangles' normals are facing forward
				colours.push_back(shape.fillcolour);
				normals.push_back(Vector3(0.0, 0.0, 1.0));
				vertices.push_back(Vector3(
					(shape.mesh[this->iCurveQuality].vertices[*tris].x * unitscale) + this->v2Offset.x,
					(shape.mesh[this->iCurveQuality].vertices[*tris].y * unitscale) + this->v2Offset.y,
					depthoffset));
				#ifdef POLYVECTOR_DEBUG
				this->vertex_count++;
				#endif
			}
		}
		//for(List<PoolVector2Array>::Element *it = shape.strokes[this->iCurveQuality].front(); it; it = it->next()) {
		//	PoolVector2Array line = it->get();
		//	PoolVector2Array::Read lineread = it->get().read();
		//	this->begin(Mesh::PRIMITIVE_LINE_STRIP);
		//	this->set_color(shape.strokecolour);
		//	this->set_normal(Vector3(0.0, 0.0, 1.0));
		//	for(int pt = 0; pt < line.size(); pt++) {
		//		this->add_vertex(Vector3(
		//			( lineread[pt].x * (this->v2Scale.x/1000.0f) ) + this->v2Offset.x,
		//			( lineread[pt].y * (this->v2Scale.y/1000.0f) ) + this->v2Offset.y,
		//			depthoffset));
		//	}
		//	this->end();
		//}
		if(this->bZOrderOffset)	depthoffset += this->fLayerDepth;
	}
	p_arr[VS::ARRAY_VERTEX] = vertices;
	p_arr[VS::ARRAY_NORMAL] = normals;
	p_arr[VS::ARRAY_COLOR] = colours;
}



void PolyVector::set_curve_quality(int t)
{
	this->iCurveQuality = t;
	this->triangulate_mesh();
}
int8_t PolyVector::get_curve_quality()
{
	return this->iCurveQuality;
}

void PolyVector::set_unit_scale(real_t s)
{
	this->fUnitScale=s;
	this->_request_update();
}
real_t PolyVector::get_unit_scale()
{
	return this->fUnitScale;
}

void PolyVector::set_offset(Vector2 s)
{
	this->v2Offset=s;
	this->_request_update();
}
Vector2 PolyVector::get_offset()
{
	return this->v2Offset;
}

void PolyVector::set_layer_separation(real_t d)
{
	this->fLayerDepth = d;
	this->_request_update();
}
real_t PolyVector::get_layer_separation()
{
	return this->fLayerDepth;
}

void PolyVector::set_material_unshaded(bool t)
{
	this->materialDefault->set_flag(SpatialMaterial::FLAG_UNSHADED, t);
	this->set_material(this->materialDefault);
}
bool PolyVector::get_material_unshaded()
{
	return this->materialDefault->get_flag(SpatialMaterial::FLAG_UNSHADED);
}

void PolyVector::set_billboard(int b)
{
	this->materialDefault->set_billboard_mode((SpatialMaterial::BillboardMode)b);
	this->set_material(this->materialDefault);
}
int PolyVector::get_billboard()
{
	return (this->materialDefault->get_billboard_mode());
}



#ifdef POLYVECTOR_DEBUG
double PolyVector::get_triangulation_time()
{
	return this->triangulation_time;
}
uint32_t PolyVector::get_vertex_count()
{
	return this->vertex_count;
}
#endif



void PolyVector::_bind_methods()
{
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
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "Unit Scale", PROPERTY_HINT_RANGE, "0.0, 1000.0, 1.0"), "set_unit_scale", "get_unit_scale");

	#ifdef POLYVECTOR_DEBUG
	ADD_GROUP("Debug","");
	ClassDB::bind_method(D_METHOD("set_layer_separation"), &PolyVector::set_layer_separation);
	ClassDB::bind_method(D_METHOD("get_layer_separation"), &PolyVector::get_layer_separation);
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "Layer Separation", PROPERTY_HINT_RANGE, "0.0, 1.0, 0.0"), "set_layer_separation", "get_layer_separation");

	ClassDB::bind_method(D_METHOD("get_triangulation_time"), &PolyVector::get_triangulation_time);
	ClassDB::bind_method(D_METHOD("get_vertex_count"), &PolyVector::get_vertex_count);
	#endif
}
