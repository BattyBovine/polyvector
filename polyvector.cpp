#include "polyvector.h"

PolyVector::PolyVector()
{
	this->iFrame = 0;
	this->fUnitScale = 1.0f;
	this->v2Offset = Vector2( 0.0f, 0.0f );
	this->iCurveQuality = 2;
	this->bZOrderOffset = true;
	this->fLayerDepth = 0.0f;

	this->materialDefault.instance();
	this->materialDefault->set_flag(SpatialMaterial::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	this->materialDefault->set_flag(SpatialMaterial::FLAG_SRGB_VERTEX_COLOR, true);
	this->materialDefault->set_cull_mode(SpatialMaterial::CULL_DISABLED);
	this->set_material_override(this->materialDefault);

	#ifdef POLYVECTOR_DEBUG
	this->os = OS::get_singleton();
	this->triangulation_time = 0.0L;
	#endif
}

PolyVector::~PolyVector()
{
	for(MeshDictionaryMap::Element *d=this->mapMeshDictionary.front(); d; d=d->next())
		for(MeshQualityMap::Element *m=d->get().front(); m; m=m->next())
			m->get().unref();
	if(!this->materialDefault.is_null())
		this->materialDefault.unref();
	this->dataVectorFile.unref();
}



void PolyVector::draw_current_frame()
{
	if(this->iFrame >= this->lFrameData.size())	return;
	#ifdef POLYVECTOR_DEBUG
	uint64_t debugtimer = this->os->get_ticks_usec();
	#endif
	for(MeshInstanceMap::Element *m=this->mapMeshDisplay.front(); m; m=m->next())
		m->get()->set_visible(false);
	float layer_separation = 0.0f;
	PolyVectorFrame *framedata = &this->lFrameData[this->iFrame];
	for(PolyVectorFrame::Element *c=framedata->front(); c; c=c->next()) {
		PolyVectorSymbol symbol = c->get();
		if(!this->mapMeshDictionary.has(symbol.id)) {
			MeshQualityMap mqm;
			this->mapMeshDictionary[symbol.id] = mqm;
		}
		if(!this->mapMeshDictionary[symbol.id].has(this->iCurveQuality)) {
			PolyVectorCharacter *pvchar = &this->lDictionaryData[symbol.id];
			Array arr;
			arr.resize(Mesh::ARRAY_MAX);
			PoolVector<Vector3> vertices;
			PoolVector<Vector3> normals;
			PoolVector<Color> colours;

			for(PolyVectorCharacter::Element *s=pvchar->front(); s; s=s->next()) {
				PolyVectorShape &shape = s->get();
				std::vector< std::vector<Vector2> > polygons;
				std::vector<Vector2> tessverts;
				PoolVector<Vector2> tess = shape.path.curve.tessellate(this->iCurveQuality, this->fMaxTessellationAngle);
				//if(!shape.path.closed) {	// If shape is not a closed loop, store as a stroke
				//	shape.strokes[this->iCurveQuality].push_back(tess);
				//} else {					// Otherwise, triangulate
				uint32_t tess_size = tess.size();
				std::vector<Vector2> poly;
				PoolVector<Vector2>::Read tessreader = tess.read();
				for(uint32_t i=1; i<tess_size; i++)
					poly.push_back(tessreader[i]);
				polygons.push_back(poly);
				tessverts.insert(tessverts.end(), poly.begin(), poly.end());
				for(List<PolyVectorPath>::Element *hole=shape.holes.front(); hole; hole=hole->next()) {
					PoolVector<Vector2> holetess = ( hole->get() ).curve.tessellate(this->iCurveQuality, this->fMaxTessellationAngle);
					uint32_t holetess_size = holetess.size();
					std::vector<Vector2> holepoly;
					PoolVector<Vector2>::Read holetessreader = holetess.read();
					for(uint32_t j=0; j<holetess_size; j++)
						holepoly.push_back(holetessreader[j]);
					polygons.push_back(holepoly);
					tessverts.insert(tessverts.end(), holepoly.begin(), holepoly.end());
				}
				//}
				if(!polygons.empty()) {
					std::vector<N> indices = mapbox::earcut<N>(polygons);
					for(std::vector<N>::reverse_iterator tris=indices.rbegin(); tris!=indices.rend(); tris++) {	// Work through the vector in reverse to make sure the triangles' normals are facing forward
						colours.push_back(shape.fillcolour);
						normals.push_back(Vector3(0.0f, 0.0f, 1.0f));
						vertices.push_back(Vector3(tessverts[*tris].x, tessverts[*tris].y, 0.0f));
						#ifdef POLYVECTOR_DEBUG
						this->vertex_count++;
						#endif
					}
				}
			}

			arr[Mesh::ARRAY_VERTEX] = vertices;
			arr[Mesh::ARRAY_NORMAL] = normals;
			arr[Mesh::ARRAY_COLOR] = colours;
			Ref<ArrayMesh> newmesh;
			newmesh.instance();
			newmesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arr);
			newmesh->surface_set_material(0, this->materialDefault);
			this->mapMeshDictionary[symbol.id][this->iCurveQuality] = newmesh;
		}
		if(!this->mapMeshDisplay.has(symbol.depth)) {
			MeshInstance *mi = memnew(MeshInstance);
			this->add_child(mi);
			mi->set_owner(this);
			this->mapMeshDisplay[symbol.depth] = mi;
		}
		MeshInstance *mi = this->mapMeshDisplay[symbol.depth];
		mi->set_mesh(this->mapMeshDictionary[symbol.id][this->iCurveQuality]);
		Transform transform;
		Vector2 offset = this->v2Offset*this->fUnitScale;
		transform.set(
			symbol.matrix.ScaleX*this->fUnitScale,				symbol.matrix.Skew0*this->fUnitScale,					0.0f,
			symbol.matrix.Skew1*this->fUnitScale,				symbol.matrix.ScaleY*this->fUnitScale,					0.0f,
			0.0f,												0.0f,													1.0f,
			symbol.matrix.TranslateX*this->fUnitScale+offset.x,	-symbol.matrix.TranslateY*this->fUnitScale+offset.y,	layer_separation
		);
		mi->set_transform(transform);
		mi->set_visible(true);
		layer_separation += this->fLayerDepth;
	}
	#ifdef POLYVECTOR_DEBUG
	this->triangulation_time = ( ( this->os->get_ticks_usec()-debugtimer )/1000000.0L );
	#endif
}



void PolyVector::set_vector_image(const Ref<JSONVector> &p_vector)
{
	if(p_vector == this->dataVectorFile)	return;
	this->dataVectorFile = p_vector;
	if(this->dataVectorFile.is_null())
		return;
	this->lFrameData = this->dataVectorFile->get_frames();
	this->lDictionaryData = this->dataVectorFile->get_dictionary();
	this->set_frame(0);
	this->draw_current_frame();
}
Ref<JSONVector> PolyVector::get_vector_image() const
{
	return this->dataVectorFile;
}

void PolyVector::set_frame(uint16_t f)
{
	this->iFrame = CLAMP(f,0,this->lFrameData.size()-1);
	this->draw_current_frame();
}
uint16_t PolyVector::get_frame()
{
	return this->iFrame;
}

void PolyVector::set_curve_quality(int8_t t)
{
	this->iCurveQuality = t;
	this->draw_current_frame();
}
int8_t PolyVector::get_curve_quality()
{
	return this->iCurveQuality;
}

void PolyVector::set_unit_scale(real_t s)
{
	this->fUnitScale=(s/1000.0f);
	this->draw_current_frame();
}
real_t PolyVector::get_unit_scale()
{
	return (this->fUnitScale*1000.0f);
}

void PolyVector::set_offset(Vector2 s)
{
	this->v2Offset=s;
	this->draw_current_frame();
}
Vector2 PolyVector::get_offset()
{
	return this->v2Offset;
}

void PolyVector::set_layer_separation(real_t d)
{
	this->fLayerDepth = d;
	this->draw_current_frame();
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



void PolyVector::set_max_tessellation_angle(real_t f)
{
	this->fMaxTessellationAngle = f;
}
real_t PolyVector::get_max_tessellation_angle()
{
	return this->fMaxTessellationAngle;
}



AABB PolyVector::get_aabb() const
{
	AABB aabbfull;
	for(MeshInstanceMap::Element *mi=this->mapMeshDisplay.front(); mi; mi=mi->next())
		aabbfull.merge_with(mi->get()->get_transformed_aabb());
	return aabbfull;
}

PoolVector<Face3> PolyVector::get_faces(uint32_t p_usage_flags) const
{
	PoolVector<Face3> allfaces;
	for(MeshInstanceMap::Element *mi=this->mapMeshDisplay.front(); mi; mi=mi->next())
		allfaces.append_array(mi->get()->get_mesh()->get_faces());
	return allfaces;
}



#ifdef POLYVECTOR_DEBUG
double PolyVector::get_triangulation_time()
{
	return this->triangulation_time;
}
double PolyVector::get_mesh_update_time()
{
	return this->mesh_update_time;
}
uint32_t PolyVector::get_vertex_count()
{
	return this->vertex_count;
}
#endif



void PolyVector::_bind_methods()
{
	ClassDB::bind_method(D_METHOD("set_vector_image"), &PolyVector::set_vector_image);
	ClassDB::bind_method(D_METHOD("get_vector_image"), &PolyVector::get_vector_image);
	ADD_PROPERTYNZ(PropertyInfo(Variant::OBJECT, "Vector", PROPERTY_HINT_RESOURCE_TYPE, "JSONVector"), "set_vector_image", "get_vector_image");

	ClassDB::bind_method(D_METHOD("set_frame"), &PolyVector::set_frame);
	ClassDB::bind_method(D_METHOD("get_frame"), &PolyVector::get_frame);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "Frame", PROPERTY_HINT_RANGE, "0,65535,1,0"), "set_frame", "get_frame");

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
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "Unit Scale", PROPERTY_HINT_RANGE, "0.0, 1000.0, 1.0, 1.0"), "set_unit_scale", "get_unit_scale");

	ADD_GROUP("Advanced","");
	ClassDB::bind_method(D_METHOD("set_max_tessellation_angle"), &PolyVector::set_max_tessellation_angle);
	ClassDB::bind_method(D_METHOD("get_max_tessellation_angle"), &PolyVector::get_max_tessellation_angle);
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "Max Tessellation Angle", PROPERTY_HINT_RANGE, "1.0, 10.0, 1.0, 4.0"), "set_max_tessellation_angle", "get_max_tessellation_angle");

	#ifdef POLYVECTOR_DEBUG
	ADD_GROUP("Debug","");
	ClassDB::bind_method(D_METHOD("set_layer_separation"), &PolyVector::set_layer_separation);
	ClassDB::bind_method(D_METHOD("get_layer_separation"), &PolyVector::get_layer_separation);
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "Layer Separation", PROPERTY_HINT_RANGE, "0.0, 1.0, 0.0"), "set_layer_separation", "get_layer_separation");

	ClassDB::bind_method(D_METHOD("get_triangulation_time"), &PolyVector::get_triangulation_time);
	ClassDB::bind_method(D_METHOD("get_mesh_update_time"), &PolyVector::get_mesh_update_time);
	ClassDB::bind_method(D_METHOD("get_vertex_count"), &PolyVector::get_vertex_count);
	#endif
}
