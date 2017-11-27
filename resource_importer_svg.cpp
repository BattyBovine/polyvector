#include "io/resource_saver.h"
#include "os/file_access.h"

#include "resource_importer_svg.h"

//#ifdef TOOLS_ENABLED
Error ResourceImporterSVG::import(const String &p_source_file, const String &p_save_path, const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files)
{
	FileAccess *svgxml = FileAccess::open(p_source_file, FileAccess::READ);
	ERR_FAIL_COND_V(!svgxml, ERR_FILE_CANT_READ);
	size_t xmllen = svgxml->get_len();
	uint8_t *svgdata = new uint8_t[xmllen];
	svgxml->get_buffer(svgdata, xmllen);
	struct NSVGimage *img = nsvgParse((char*)svgdata, "px", 96);
	ERR_FAIL_COND_V(!img, ERR_CANT_OPEN);
	FileAccess *svgbin = FileAccess::open(p_save_path + ".svgbin", FileAccess::WRITE);
	ERR_FAIL_COND_V(!svgbin, ERR_FILE_CANT_WRITE);

	svgbin->store_float(img->width);
	svgbin->store_float(img->height);
	for(NSVGshape *shape = img->shapes; shape; shape = shape->next) {
		for(NSVGpath *path = shape->paths; path; path = path->next) {
			svgbin->store_8(path->closed);
			if(path->npts > 0) {
				float *p = &path->pts[0];
				svgbin->store_float(p[0]);		svgbin->store_float(p[1]);
				svgbin->store_float(0.0f);		svgbin->store_float(0.0f);
				svgbin->store_float(p[2]-p[0]);	svgbin->store_float(p[3]-p[1]);
				for(int i = 0; i < (path->npts/3); i++) {
					p = &path->pts[(i*6)+4];
					svgbin->store_float(p[2]);		svgbin->store_float(p[3]);
					svgbin->store_float(p[0]-p[2]);	svgbin->store_float(p[1]-p[3]);
					svgbin->store_float(p[4]-p[2]);	svgbin->store_float(p[5]-p[3]);
				}
			}
			svgbin->store_string("ENDP");
		}
		svgbin->store_string("ENDS");
	}
	svgbin->store_string("ENDV");
	svgbin->close();
	nsvgDelete(img);
	memdelete(svgbin);

	return OK;
}
//#endif



RES ResourceLoaderSVG::load(const String &p_path, const String &p_original_path, Error *r_error)
{
	if(r_error)	*r_error = ERR_FILE_CANT_OPEN;

	FileAccess *f = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V(!f, RES());

	Vector2 dimensions;
	dimensions.x = f->get_float();
	dimensions.y = f->get_float();

	PolyVectorFrame framedata;
	uint32_t shape_count = 0;
	uint8_t code[4];
	size_t filepos = f->get_position();
	f->get_buffer(code, 4);
	while(!f->eof_reached() && !(code[0]=='E' && code[1]=='N' && code[2]=='D' && code[3]=='V')) {
		f->seek(filepos);
		PolyVectorShape shapedata;
		uint32_t path_count = 0;
		while(!f->eof_reached() && !(code[0]=='E' && code[1]=='N' && code[2]=='D' && code[3]=='S' )) {
			f->seek(filepos);
			PolyVectorPath pathdata;
			pathdata.closed = f->get_8();
			filepos = f->get_position();
			Vector2 point, ctrl1, ctrl2;
			Curve2D *curve = new Curve2D();
			while(!f->eof_reached() && !(code[0]=='E' && code[1]=='N' && code[2]=='D' && code[3]=='P' )) {
				f->seek(filepos);
				point.x = f->get_float();
				point.y = -f->get_float();
				ctrl1.x = f->get_float();
				ctrl1.y = -f->get_float();
				ctrl2.x = f->get_float();
				ctrl2.y = -f->get_float();
				curve->add_point(point, ctrl1, ctrl2);
				filepos = f->get_position();
				f->get_buffer(code, 4);
			}
			pathdata.id = path_count;
			pathdata.curve = curve;
			shapedata.paths.push_back(pathdata);
			path_count++;
			filepos = f->get_position();
			f->get_buffer(code, 4);
		}
		shapedata.id = shape_count;
		shapedata.vertices.clear();
		shapedata.indices.clear();
		shapedata.strokes.clear();
		framedata.shapes.push_back(shapedata);
		shape_count++;
		filepos = f->get_position();
		f->get_buffer(code, 4);
	}
	f->close();
	memdelete(f);

	Ref<SVGBin> rawsvg;
	rawsvg.instance();
	rawsvg->add_frame(framedata);
	rawsvg->set_dimensions(dimensions);

	if(r_error)	*r_error = OK;

	return rawsvg;
}
