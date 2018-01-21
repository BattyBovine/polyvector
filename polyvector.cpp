#include "polyvector.h"

PolyVector::PolyVector()
{
	this->iFrame = 0;
	this->fUnitScale = 1.0f;
	this->iCurveQuality = 2;

	#ifdef POLYVECTOR_DEBUG
	this->os = OS::get_singleton();
	this->triangulation_time = 0.0L;
	#endif
}

PolyVector::~PolyVector()
{
	this->clear_meshes();
}



void PolyVector::set_character(PolyVectorCharacter c)
{
	this->pvcCharacter = c;
	this->triangulate_mesh();
}

PolyVectorCharacter PolyVector::get_character()
{
	return this->pvcCharacter;
}

void PolyVector::triangulate_mesh(bool force_retri)
{
	if(this->mapMeshes[this->iCurveQuality].is_valid() && !force_retri)	return;

	#ifdef POLYVECTOR_DEBUG
	this->vertex_count = 0;
	this->triangulation_time = 0.0L;
	uint64_t debugtimer = this->os->get_ticks_usec();
	#endif
	
	for(List<PolyVectorShape>::Element *s=this->pvcCharacter.front(); s; s=s->next()) {
		PolyVectorShape &shape = s->get();
		std::vector<Vector2> vertices;
		std::vector<N> indices;
		std::vector<std::vector<Vector2> > polygons;
		PoolVector2Array tess = shape.path.curve.tessellate(this->iCurveQuality, POLYVECTOR_TESSELLATION_MAX_ANGLE);
		if(!shape.path.closed) {	// If shape is not a closed loop, store as a stroke
			continue;
			//shape.strokes[this->iCurveQuality].push_back(tess);
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
		} else {					// Otherwise, triangulate
			uint32_t tess_size = tess.size();
			std::vector<Vector2> poly;
			PoolVector2Array::Read tessreader = tess.read();
			for(uint32_t i=1; i<tess_size; i++)
				poly.push_back(tessreader[i]);
			polygons.push_back(poly);
			vertices.insert(vertices.end(), poly.begin(), poly.end());
			for(List<PolyVectorPath>::Element *h=shape.holes.front(); h; h=h->next()) {
				PolyVectorPath hole = h->get();
				PoolVector2Array holetess = hole.curve.tessellate(this->iCurveQuality, POLYVECTOR_TESSELLATION_MAX_ANGLE);
				uint32_t holetess_size = holetess.size();
				std::vector<Vector2> holepoly;
				PoolVector2Array::Read holetessreader = holetess.read();
				for(uint32_t j=0; j<holetess_size; j++)
					holepoly.push_back(holetessreader[j]);
				polygons.push_back(holepoly);
				vertices.insert(vertices.end(), holepoly.begin(), holepoly.end());
			}
		}
		if(!polygons.empty()) {
			indices = mapbox::earcut<N>(polygons);
			if(indices.size() > 0) {
				Array arr;
				arr.resize(Mesh::ARRAY_MAX);
				PoolVector<Vector3> triangles;
				PoolVector<Vector3> normals;
				PoolVector<Color> colours;
				float unitscale = (this->fUnitScale/1000.0f);
				for(std::vector<N>::reverse_iterator tris=indices.rbegin(); tris!=indices.rend(); tris++) {	// Work through the vector in reverse to make sure the triangles' normals are facing forward
					colours.push_back(shape.fillcolour);
					normals.push_back(Vector3(0.0, 0.0, 1.0));
					triangles.push_back(Vector3(vertices[*tris].x*unitscale, vertices[*tris].y*unitscale, 0.0f));
					#ifdef POLYVECTOR_DEBUG
					this->vertex_count++;
					#endif
				}
				arr[Mesh::ARRAY_VERTEX] = triangles;
				arr[Mesh::ARRAY_NORMAL] = normals;
				arr[Mesh::ARRAY_COLOR] = colours;
				Ref<ArrayMesh> am;
				am.instance();
				//am->connect(CoreStringNames::get_singleton()->changed, this, SceneStringNames::get_singleton()->_mesh_changed);
				am->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arr);
				am->surface_set_material(0, this->materialSpatial);
				this->set_mesh(am);
				if(this->mapMeshes.has(this->iCurveQuality) && this->mapMeshes[this->iCurveQuality].is_valid())
					this->mapMeshes[this->iCurveQuality].unref();
				this->mapMeshes[this->iCurveQuality] = am;
			}
		}
	}

	#ifdef POLYVECTOR_DEBUG
	this->triangulation_time = ((this->os->get_ticks_usec()-debugtimer)/1000000.0L);
	#endif
}

void PolyVector::clear_meshes()
{
	for(Map<uint16_t, Ref<ArrayMesh> >::Element *m=this->mapMeshes.front(); m; m=m->next()) {
		if(m->value().is_valid())
			m->value().unref();
	}
	this->mapMeshes.clear();
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
}
real_t PolyVector::get_unit_scale()
{
	return this->fUnitScale;
}

void PolyVector::set_material(Ref<SpatialMaterial> m)
{
	this->materialSpatial = m;
}
Ref<SpatialMaterial> PolyVector::get_material()
{
	return this->materialSpatial;
}


//AABB PolyVector::get_aabb() const
//{
//	if(this->mapMeshes.has(this->iCurveQuality) && this->mapMeshes[this->iCurveQuality].is_valid())
//		return this->mapMeshes[this->iCurveQuality]->get_aabb();
//	return AABB();
//}
//
//PoolVector<Face3> PolyVector::get_faces(uint32_t p_usage_flags) const
//{
//	if(this->mapMeshes.has(this->iCurveQuality) && this->mapMeshes[this->iCurveQuality].is_valid())
//		return this->mapMeshes[this->iCurveQuality]->get_faces();
//	return PoolVector<Face3>();
//}



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
	
	ClassDB::bind_method(D_METHOD("set_unit_scale"), &PolyVector::set_unit_scale);
	ClassDB::bind_method(D_METHOD("get_unit_scale"), &PolyVector::get_unit_scale);

	#ifdef POLYVECTOR_DEBUG
	ClassDB::bind_method(D_METHOD("get_triangulation_time"), &PolyVector::get_triangulation_time);
	ClassDB::bind_method(D_METHOD("get_vertex_count"), &PolyVector::get_vertex_count);
	#endif
}
