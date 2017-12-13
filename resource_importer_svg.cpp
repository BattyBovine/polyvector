#include <core/os/os.h>
#include <io/resource_saver.h>
#include <os/file_access.h>

#include "resource_importer_svg.h"

#ifdef TOOLS_ENABLED
Error ResourceImporterSVG::import(const String &p_source_file, const String &p_save_path, const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files)
{
	FileAccess *svg = FileAccess::open(p_source_file, FileAccess::READ);
	ERR_FAIL_COND_V(!svg, ERR_FILE_CANT_READ);
	FileAccess *svgraw = FileAccess::open(p_save_path + ".svgraw", FileAccess::WRITE);
	ERR_FAIL_COND_V(!svgraw, ERR_FILE_CANT_WRITE);

	size_t xmllen = svg->get_len();
	ERR_FAIL_COND_V(!xmllen, ERR_CANT_OPEN);
	{
		uint8_t *svgdata = new uint8_t[xmllen];
		svg->get_buffer(svgdata, xmllen);
		svgraw->store_buffer(svgdata, xmllen);
		svg->close();
		svgraw->close();
	}
	memdelete(svgraw);
	memdelete(svg);

	return OK;
}
#endif



RES ResourceLoaderSVG::load(const String &p_path, const String &p_original_path, Error *r_error)
{
	if(r_error)	*r_error = ERR_FILE_CANT_OPEN;

	float timer = OS::get_singleton()->get_ticks_usec();
	FileAccess *f = FileAccess::open(p_path, FileAccess::READ);

	FileAccess *svgxml = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V(!svgxml, RES());
	size_t xmllen = svgxml->get_len();
	uint8_t *svgdata = new uint8_t[xmllen];
	svgxml->get_buffer(svgdata, xmllen);
	struct NSVGimage *img = nsvgParse((char*)svgdata, "px", 96);
	ERR_FAIL_COND_V(!img, RES());

	Vector2 dimensions;
	dimensions.x = img->width;
	dimensions.y = img->height;
	PolyVectorFrame framedata;
	uint32_t shape_count = 0;
	for(NSVGshape *shape = img->shapes; shape; shape = shape->next) {
		PolyVectorShape shapedata;
		shapedata.fillcolour.r = ( (shape->fill.color) & 0x000000FF ) / 255.0f;
		shapedata.fillcolour.g = ( (shape->fill.color>>8) & 0x000000FF ) / 255.0f;
		shapedata.fillcolour.b = ( (shape->fill.color>>16) & 0x000000FF ) / 255.0f;
		shapedata.fillcolour.a = ( (shape->fill.color>>24) & 0x000000FF ) / 255.0f;
		shapedata.strokecolour.r = ( (shape->stroke.color) & 0x000000FF ) / 255.0f;
		shapedata.strokecolour.g = ( (shape->stroke.color>>8) & 0x000000FF ) / 255.0f;
		shapedata.strokecolour.b = ( (shape->stroke.color>>16) & 0x000000FF ) / 255.0f;
		shapedata.strokecolour.a = ( (shape->stroke.color>>24) & 0x000000FF ) / 255.0f;
		shapedata.id = shape_count;
		uint32_t path_count = 0;
		for(NSVGpath *path = shape->paths; path; path = path->next) {
			PolyVectorPath pathdata;
			if(path->npts > 0) {
				float *p = &path->pts[0];
				pathdata.curve.add_point(
					Vector2(p[0], -p[1]),
					Vector2(0.0f, 0.0f),
					Vector2(p[2]-p[0], -(p[3]-p[1]))
				);
				for(int i = 0; i < ( path->npts/3 ); i++) {
					p = &path->pts[(i*6)+4];
					pathdata.curve.add_point(
						Vector2(p[2], -p[3]),
						Vector2(p[0]-p[2], -(p[1]-p[3])),
						Vector2(p[4]-p[2], -(p[5]-p[3]))
					);
				}
			}
			pathdata.closed = path->closed;
			pathdata.id = path_count;
			if(path->closed)	pathdata.hole = this->_is_clockwise(pathdata.curve);
			else				pathdata.hole = false;
			shapedata.paths.push_back(pathdata);
			path_count++;
		}
		shapedata.paths.back().hole = false;		// Last shape is always a non-hole
		shapedata.vertices.clear();
		shapedata.indices.clear();
		shapedata.strokes.clear();
		framedata.shapes.push_back(shapedata);
		shape_count++;
	}
	nsvgDelete(img);

	Ref<RawSVG> rawsvg;
	rawsvg.instance();
	rawsvg->add_frame(framedata);
	rawsvg->set_dimensions(dimensions);

	if(r_error)	*r_error = OK;

	return rawsvg;
}

bool ResourceLoaderSVG::_is_clockwise(Curve2D c)
{
	if(c.get_point_count() < 3)	return false;
	N pointcount = c.get_point_count();
	int area = 0;
	Vector2 p0 = c.get_point_position(0);
	Vector2 pn = c.get_point_position(pointcount-1);
	for(N i=1; i<pointcount; i++) {
		Vector2 p1 = c.get_point_position(i);
		Vector2 p2 = c.get_point_position(i-1);
		area += ( p1.x - p2.x ) * ( p1.y + p2.y );
	}
	area += ( p0.x - pn.x ) * ( p0.y + pn.y );
	return ( area >= 0 );
}
