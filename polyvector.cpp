#include "polyvector.h"

PolyVector::PolyVector()
{
	this->fTime = 0.0f;
	this->fUnitScale = 1.0f;
	this->v2Offset = Vector2( 0.0f, 0.0f );
	this->iCurveQuality = 2;
	this->bZOrderOffset = true;
	this->fLayerDepth = 0.0f;

	this->materialDefault.instance();
	this->materialDefault->set_flag(SpatialMaterial::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	this->materialDefault->set_flag(SpatialMaterial::FLAG_SRGB_VERTEX_COLOR, true);
	this->materialDefault->set_cull_mode(SpatialMaterial::CULL_DISABLED);

	#ifdef POLYVECTOR_DEBUG
	this->bDebugWireframe = false;
	this->materialDebug.instance();
	this->materialDefault->set_flag(SpatialMaterial::FLAG_UNSHADED, true);
	this->os = OS::get_singleton();
	this->dTriangulationTime = 0.0L;
	this->dMeshUpdateTime = 0.0L;
	#endif
}

PolyVector::~PolyVector()
{
	this->clear_mesh_data();
	if(!this->materialDefault.is_null())
		this->materialDefault.unref();
	this->dataVectorFile.unref();
}



void PolyVector::draw_current_frame()
{
	if(this->dataVectorFile.is_null())	return;
	uint16_t frameno = CLAMP((this->fFps*this->fTime), 0, this->lFrameData.size()-1);
	#ifdef POLYVECTOR_DEBUG
	uint64_t debugtimer = this->os->get_ticks_usec();
	#endif
	for(MeshInstanceMap::Element *m=this->mapMeshDisplay.front(); m; m=m->next())
		m->get()->set_visible(false);
	float layer_separation = 0.0f;
	PolyVectorFrame *framedata = &this->lFrameData[frameno];
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
			#ifdef POLYVECTOR_DEBUG
			Array wireframearr;
			wireframearr.resize(Mesh::ARRAY_MAX);
			PoolVector<Vector2> wireframevertices;
			#endif

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
				for(uint32_t i=1; i<tess_size; i++) {
					poly.push_back(tessreader[i]);
					#ifdef POLYVECTOR_DEBUG
					if(this->bDebugWireframe) {
						wireframevertices.push_back(tessreader[i-1]);
						wireframevertices.push_back(tessreader[i]);
					}
					#endif
				}
				polygons.push_back(poly);
				tessverts.insert(tessverts.end(), poly.begin(), poly.end());
				for(List<uint16_t>::Element *hole=shape.holes.front(); hole; hole=hole->next()) {
					PoolVector<Vector2> holetess = (*pvchar)[hole->get()].path.curve.tessellate(this->iCurveQuality, this->fMaxTessellationAngle);
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

			#ifdef POLYVECTOR_DEBUG
			if(this->bDebugWireframe) {
				wireframearr[Mesh::ARRAY_VERTEX] = wireframevertices;
				newmesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, wireframearr);
				newmesh->surface_set_material(newmesh->get_surface_count()-1, this->materialDebug);
			}
			#endif

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
			symbol.matrix.ScaleX*this->fUnitScale,				symbol.matrix.Skew1*this->fUnitScale,					0.0f,
			symbol.matrix.Skew0*this->fUnitScale,				symbol.matrix.ScaleY*this->fUnitScale,					0.0f,
			0.0f,												0.0f,													1.0f,
			symbol.matrix.TranslateX*this->fUnitScale+offset.x,	-symbol.matrix.TranslateY*this->fUnitScale+offset.y,	layer_separation
		);
		mi->set_transform(transform);
		mi->set_visible(true);
		layer_separation += this->fLayerDepth;
	}
	#ifdef POLYVECTOR_DEBUG
	this->dTriangulationTime = ( ( this->os->get_ticks_usec()-debugtimer )/1000000.0L );
	#endif
}

void PolyVector::clear_mesh_data()
{
	for(MeshDictionaryMap::Element *d=this->mapMeshDictionary.front(); d; d=d->next()) {
		for(MeshQualityMap::Element *m=d->get().front(); m; m=m->next())
			m->get().unref();
		d->get().clear();
	}
}

void PolyVector::clear_mesh_instances()
{
	int32_t childcount = this->get_child_count();
	for(int32_t i=childcount-1; i>=0; i--) {
		MeshInstance *mi = Node::cast_to<MeshInstance>(this->get_child(i));
		if(mi) this->remove_child(mi);
	}
	for(MeshInstanceMap::Element *mim=this->mapMeshDisplay.front(); mim; mim=mim->next())
		memdelete<MeshInstance>(mim->get());
	this->mapMeshDisplay.clear();
}



void PolyVector::set_vector_image(const Ref<JSONVector> &p_vector)
{
	if(this->dataVectorFile.is_valid())
		this->dataVectorFile.unref();
	this->dataVectorFile = p_vector;
	if(this->dataVectorFile.is_null())
		return;
	this->fFps = this->dataVectorFile->get_fps();
	this->lFrameData = this->dataVectorFile->get_frames();
	this->lDictionaryData = this->dataVectorFile->get_dictionary();

	this->clear_mesh_instances();
	this->clear_mesh_data();

	AnimationPlayer *animplayer = NULL;
	int32_t childcount = this->get_child_count();
	for(int32_t i=0; i<childcount; i++) {
		animplayer = Node::cast_to<AnimationPlayer>(this->get_child(i));
		if(animplayer)	break;
	}

	this->set_time(0.0f);
}
Ref<JSONVector> PolyVector::get_vector_image() const
{
	return this->dataVectorFile;
}

void PolyVector::set_time(real_t f)
{
	this->fTime = f;
	this->draw_current_frame();
}
real_t PolyVector::get_time()
{
	return this->fTime;
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
}
bool PolyVector::get_material_unshaded()
{
	return this->materialDefault->get_flag(SpatialMaterial::FLAG_UNSHADED);
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
void PolyVector::set_debug_wireframe(bool b)
{
	this->bDebugWireframe = b;
	this->clear_mesh_instances();
	this->clear_mesh_data();
	this->draw_current_frame();
}

bool PolyVector::get_debug_wireframe()
{
	return this->bDebugWireframe;
}

double PolyVector::get_triangulation_time()
{
	return this->dTriangulationTime;
}
double PolyVector::get_mesh_update_time()
{
	return this->dMeshUpdateTime;
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
	ADD_PROPERTYNZ(PropertyInfo(Variant::OBJECT, "vector", PROPERTY_HINT_RESOURCE_TYPE, "JSONVector"), "set_vector_image", "get_vector_image");


	ADD_GROUP("Display","");
	ClassDB::bind_method(D_METHOD("set_time"), &PolyVector::set_time);
	ClassDB::bind_method(D_METHOD("get_time"), &PolyVector::get_time);
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "timecode", PROPERTY_HINT_RANGE, "0.0, 1000.0, 0.01, 0.0"), "set_time", "get_time");

	ClassDB::bind_method(D_METHOD("set_curve_quality"), &PolyVector::set_curve_quality);
	ClassDB::bind_method(D_METHOD("get_curve_quality"), &PolyVector::get_curve_quality);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "curve_quality", PROPERTY_HINT_RANGE, "0,10,1,2"), "set_curve_quality", "get_curve_quality");

	ClassDB::bind_method(D_METHOD("set_material_unshaded"), &PolyVector::set_material_unshaded);
	ClassDB::bind_method(D_METHOD("get_material_unshaded"), &PolyVector::get_material_unshaded);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "unshaded"), "set_material_unshaded", "get_material_unshaded");


	ADD_GROUP("Adjustments","");
	ClassDB::bind_method(D_METHOD("set_offset"), &PolyVector::set_offset);
	ClassDB::bind_method(D_METHOD("get_offset"), &PolyVector::get_offset);
	ADD_PROPERTYNZ(PropertyInfo(Variant::VECTOR2, "offset"), "set_offset", "get_offset");

	ClassDB::bind_method(D_METHOD("set_unit_scale"), &PolyVector::set_unit_scale);
	ClassDB::bind_method(D_METHOD("get_unit_scale"), &PolyVector::get_unit_scale);
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "unit_scale", PROPERTY_HINT_RANGE, "0.0, 1000.0, 1.0, 1.0"), "set_unit_scale", "get_unit_scale");

	ClassDB::bind_method(D_METHOD("set_layer_separation"), &PolyVector::set_layer_separation);
	ClassDB::bind_method(D_METHOD("get_layer_separation"), &PolyVector::get_layer_separation);
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "layer_separation", PROPERTY_HINT_RANGE, "0.0, 1.0, 0.0"), "set_layer_separation", "get_layer_separation");


	ADD_GROUP("Advanced","");
	ClassDB::bind_method(D_METHOD("set_max_tessellation_angle"), &PolyVector::set_max_tessellation_angle);
	ClassDB::bind_method(D_METHOD("get_max_tessellation_angle"), &PolyVector::get_max_tessellation_angle);
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "max_tessellation_angle", PROPERTY_HINT_RANGE, "1.0, 10.0, 1.0, 4.0"), "set_max_tessellation_angle", "get_max_tessellation_angle");


	#ifdef POLYVECTOR_DEBUG
	ADD_GROUP("Debug", "");
	ClassDB::bind_method(D_METHOD("set_debug_wireframe"), &PolyVector::set_debug_wireframe);
	ClassDB::bind_method(D_METHOD("get_debug_wireframe"), &PolyVector::get_debug_wireframe);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_wireframe"), "set_debug_wireframe", "get_debug_wireframe");

	ClassDB::bind_method(D_METHOD("get_triangulation_time"), &PolyVector::get_triangulation_time);
	ClassDB::bind_method(D_METHOD("get_mesh_update_time"), &PolyVector::get_mesh_update_time);
	ClassDB::bind_method(D_METHOD("get_vertex_count"), &PolyVector::get_vertex_count);
	#endif
}
