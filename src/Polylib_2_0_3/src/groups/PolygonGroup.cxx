/*
 * Polylib - Polygon Management Library.
 *
 * Copyright (c) RIKEN, Japan. All right reserved. 2010-
 *
 */
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include "Polylib.h"
//#include "MPIPolylib.h"
#include "polygons/Polygons.h"
#include "polygons/Triangle.h"
#include "polygons/TriMesh.h"
#include "groups/PolygonGroup.h"
#include "file_io/PolylibConfig.h"
#include "file_io/TriMeshIO.h"
#include "file_io/triangle_id.h"

//#define BENCHMARK
//#define DEBUG

using namespace std;
using namespace PolylibNS;

///
/// 他クラスでも使用するXMLタグ
///
const char *PolygonGroup::ATT_NAME_CLASS = "class_name";

///
/// 本クラス内でのみ使用するXMLタグ
///
#define ATT_NAME_NAME		"name"
#define ATT_NAME_PATH		"filepath"
#define ATT_NAME_MOVABLE	"movable"
// ユーザ定義ID追加 2010.10.20
#define ATT_NAME_ID			"id"

/************************************************************************
 *
 * PolygonGroupクラス
 *
 ***********************************************************************/
// public /////////////////////////////////////////////////////////////////////
PolygonGroup::PolygonGroup() {
	m_parent	= 0;
	m_polygons	= new TriMesh();
	m_movable	= false;
	m_need_rebuild = false;
	m_trias_before_move = NULL;
}

// public /////////////////////////////////////////////////////////////////////
PolygonGroup::~PolygonGroup()
{
#ifdef DEBUG
	PL_DBGOSH << "Delete PolygonGroup" << endl;
#endif
	delete m_polygons;
	if( m_trias_before_move ) {
		for( unsigned int i=0; i<m_trias_before_move->size(); i++ ) {
			delete m_trias_before_move->at(i);
		}
		delete m_trias_before_move;
	}
}

// public /////////////////////////////////////////////////////////////////////
POLYLIB_STAT PolygonGroup::init(
	const vector<PrivateTriangle*>	*tri_list, 
	bool							clear
) {
#ifdef DEBUG
	PL_DBGOSH <<"PolygonGroup::init3:clear=" << clear << endl;
#endif
	if (clear == true) {
		m_polygons->init(tri_list);
	}
	return build_polygon_tree();
}

// public /////////////////////////////////////////////////////////////////////
POLYLIB_STAT PolygonGroup::build_group_tree(
	Polylib					*polylib,
	PolygonGroup			*parent,
	const PolylibCfgElem	*elem
) {
#ifdef DEBUG
	PL_DBGOSH << "PolygonGroup::build_group_tree() in." << endl;
#endif
	POLYLIB_STAT 	ret = setup_attribute(polylib, parent, elem);
	if (ret != PLSTAT_OK)		return ret;
	
	const PolylibCfgElem *e = elem->first_element();
	while (e != NULL) {
		const PolylibCfgParam	*att_class;
		PolygonGroup			*pg;
		string					class_name;

		att_class	= e->first_param(ATT_NAME_CLASS);
		class_name	= att_class->get_string_data();
		pg			= polylib->create_polygon_group(class_name);

		// グループリストに追加
		polylib->add_pg_list(pg);	

		if (pg == NULL) {
			PL_ERROSH << "[ERROR]PolygonGroup::build_group_tree():"
					  << "Unknown Class name:" << class_name << "." << endl;
			return PLSTAT_CONFIG_ERROR;
		}

		ret = pg->build_group_tree(polylib, this, e);
		if (ret != PLSTAT_OK) return ret;		

		e = elem->next_element(e);
	}	
	return PLSTAT_OK;
}

// public /////////////////////////////////////////////////////////////////////
POLYLIB_STAT PolygonGroup::build_polygon_tree()
{
#ifdef BENCHMARK
	double	st1, st2, ut1, ut2, tt1, tt2;
	bool	ret1, ret2;
	ret1 = getrusage_sec(&ut1, &st1, &tt1);
#endif
#ifdef DEBUG
	PL_DBGOSH << "PolygonGroup::build_polygon_tree() in.:" << m_name << endl;
#endif

	//木構造の生成
	POLYLIB_STAT ret = m_polygons->build();
	if (ret != PLSTAT_OK) return ret;

#ifdef DEBUG
	PL_DBGOSH << "PolygonGroup::build_polygon_tree() out." << endl;
#endif
#ifdef BENCHMARK
	ret2 = getrusage_sec(&ut2,&st2,&tt2);
	if (ret1 == false || ret2 == false) {
		PL_DBGOSH << "Reading STL SYS Time Error" << endl;
		PL_DBGOSH << "Reading STL CPU Time Error" << endl;
		PL_DBGOSH << "Reading STL Total Time Error" << endl;
	}
	else{
		cerr.setf(ios::scientific, ios::floatfield);
		PL_DBGOSH << "Reading STL SYS   Time:" << st2 - st1 << endl;
		PL_DBGOSH << "Reading STL CPU   Time:" << ut2 - ut1 << endl;
		PL_DBGOSH << "Reading STL Total Time:" << tt2 - tt1 << endl;
		cerr.unsetf(ios::scientific);
	}
#endif

	return PLSTAT_OK;
}

// public /////////////////////////////////////////////////////////////////////
POLYLIB_STAT PolygonGroup::load_stl_file()
{
#ifdef DEBUG
PL_DBGOSH << "PolygonGroup:load_stl_file():IN" << endl;
#endif
	POLYLIB_STAT ret = m_polygons->import(m_file_name);
	if (ret != PLSTAT_OK) return ret;
	return build_polygon_tree();
}

// public /////////////////////////////////////////////////////////////////////
POLYLIB_STAT PolygonGroup::load_id_file(
	ID_FORMAT	id_format
) {
	// IDはsave_id()関数で一括して出力されるので、ファイル数は必ず1個
	if (m_file_name.size() != 1) {
		PL_ERROSH << "[ERROR]PolygonGroup::load_id_file():Num of files mismatch:" 
				  << m_file_name.size() << endl;
		return PLSTAT_NG;	
	}
	string	fname	= m_file_name.begin()->first;
	int		pos		= fname.find_last_of(".");

	fname.replace(pos + 1, fname.length(), "id");
#ifdef DEBUG
PL_DBGOSH << "load_id_file:" << fname.c_str() << endl;
#endif
	return load_id(m_polygons->get_tri_list(), fname, id_format);
}

// public /////////////////////////////////////////////////////////////////////
POLYLIB_STAT PolygonGroup::save_stl_file(
	string	rank_no,
	string	extend,
	string	format
) {
	char	*fname = mk_stl_fname(rank_no, extend, format);
	return TriMeshIO::save(m_polygons->get_tri_list(), fname, format);
}

// public /////////////////////////////////////////////////////////////////////
POLYLIB_STAT PolygonGroup::save_id_file(
	string		rank_no,
	string		extend,
	ID_FORMAT	id_format
) {
	char	*fname = mk_id_fname(rank_no, extend);
#ifdef DEBUG
PL_DBGOSH <<  "save_id_file:" << fname << endl;
#endif
	return save_id(m_polygons->get_tri_list(), fname, id_format);
}

// public /////////////////////////////////////////////////////////////////////
POLYLIB_STAT PolygonGroup::mk_param_tag(
	xmlNodePtr	elem,
	string		rank_no,
	string		extend,
	string		format
) {
	POLYLIB_STAT	stat;

	// class_nameタグ
	stat = PolylibConfig::mk_param_tag(elem, ATT_NAME_CLASS, 
													get_class_name().c_str());
	if (stat != PLSTAT_OK)		return stat;
	return mk_basic_tag(elem, rank_no, extend, format);
}

// public /////////////////////////////////////////////////////////////////////
POLYLIB_STAT PolygonGroup::move(
	PolylibMoveParams	&params
){
#ifdef DEBUG
PL_DBGOSH <<  "PolygonGroup::move() in." << endl;
#endif
	// virtual用の関数なので中身はない
	return PLSTAT_OK;
}

// public /////////////////////////////////////////////////////////////////////
const vector<PrivateTriangle*>* PolygonGroup::search(
	BBox	*bbox, 
	bool	every
) const {
	return m_polygons->search(bbox, every);
}

// public /////////////////////////////////////////////////////////////////////
POLYLIB_STAT PolygonGroup::search(
	BBox						*bbox, 
	bool						every, 
	vector<PrivateTriangle*>	*tri_list
) const {
	return m_polygons->search(bbox, every, tri_list);
}

// public /////////////////////////////////////////////////////////////////////
const vector<PrivateTriangle*>* PolygonGroup::linear_search(
	BBox	*bbox, 
	bool	every
) const {
	return m_polygons->linear_search(bbox, every);
}

// public /////////////////////////////////////////////////////////////////////
POLYLIB_STAT PolygonGroup::linear_search(
	BBox						*bbox, 
	bool						every, 
	vector<PrivateTriangle*>	*tri_list
) const {
	return m_polygons->linear_search(bbox, every, tri_list);
}

// public /////////////////////////////////////////////////////////////////////
string PolygonGroup::acq_fullpath() {
	if (m_parent_path.empty() == true)	return m_name;
	else								return m_parent_path + "/" + m_name;
}

// public /////////////////////////////////////////////////////////////////////
string PolygonGroup::acq_file_name() {
	string							fnames;
	map<string, string>::iterator	it;
	for (it = m_file_name.begin(); it != m_file_name.end(); it++) {
		if (it == m_file_name.begin()) {
			fnames = it->first;
		}
		else {
			fnames.append(",");
			fnames.append(it->first);
		}
	}
	return fnames;
}

// public /////////////////////////////////////////////////////////////////////
const std::vector<PrivateTriangle*>*
PolygonGroup::search_outbounded(
	BBox neibour_bbox,
	vector<int> *exclude_tria_ids
)
{
#ifdef DEBUG
	PL_DBGOSH << "PolygonGroup::search_outbounded() in. " << endl;
#endif
	vector<PrivateTriangle*> *p_trias;

	// 除外IDリストを昇順ソート
	std::sort( exclude_tria_ids->begin(), exclude_tria_ids->end() );

	// 隣接PE領域(ガイドセル含)に懸かる三角形を検索
	p_trias = (vector<PrivateTriangle*>*)search( &neibour_bbox, false );
#ifdef DEBUG
PL_DBGOSH << "p_trias org num:" << p_trias->size() << endl;
#endif

	// 検索結果から除外対象を除く
	vector<PrivateTriangle*>::iterator itr;
	for( itr=p_trias->begin(); itr!=p_trias->end(); ) {
		int id = (*itr)->get_id();
		if( std::binary_search(exclude_tria_ids->begin(),
							   exclude_tria_ids->end(), id) ) {
			itr = p_trias->erase(itr);
		}
		else {
			itr++;
		}
	}
#ifdef DEBUG
PL_DBGOSH << "p_trias ret num:" << p_trias->size() << endl;
#endif
	return p_trias;
}

// public /////////////////////////////////////////////////////////////////////
POLYLIB_STAT
PolygonGroup::add_triangles(
	vector<PrivateTriangle*> *tri_list
)
{
#ifdef DEBUG
	PL_DBGOSH << "PolygonGroup::add_triangles() in. " << endl;
#endif
	if( tri_list==NULL || tri_list->size()==0 ) {
		return PLSTAT_OK;
	}

	m_polygons->add( tri_list );

	// KD木要再構築フラグを立てる
	m_need_rebuild = true;

	return PLSTAT_OK;
}

// public /////////////////////////////////////////////////////////////////////
POLYLIB_STAT
PolygonGroup::rebuild_polygons()
{
#ifdef DEBUG
	PL_DBGOSH << "PolygonGroup::rebuild_polygons() in. " << endl;
#endif
	// 不要な再構築は行わない
	if( !m_need_rebuild ) {
#ifdef DEBUG
		PL_DBGOSH << "PolygonGroup::rebuild_polygons() didnot need rebuild." << endl;
#endif
		return PLSTAT_OK;
	}

	POLYLIB_STAT ret = build_polygon_tree();
	m_need_rebuild = false;
	return ret;
}

// public /////////////////////////////////////////////////////////////////////
POLYLIB_STAT PolygonGroup::show_group_info(
	int irank
) {
	ostringstream	oss;
	string			rank;

	if (irank < 0) {
		rank = "";
	}
	else{
		oss << setw(3) << setfill('0') << "rank(" << irank << "): ";
		rank = oss.str();
	}
	PL_DBGOSH << "PolygonGroup::show_group_info::rank:" << rank << endl;

	if (m_name.empty() == false) {
		PL_DBGOSH << "  polygon group name: " << m_name << endl;
	}
	else {
		PL_DBGOSH << "  polygon group name: empty." << endl;
	}

	if (m_parent_path.empty() == false) {
		PL_DBGOSH << "  parent polygon group name: " << m_parent_path << endl;
	}
	else {
		PL_DBGOSH << "  parent polygon group name: empty." << endl;
	}

	if (m_file_name.size() > 0) {
		map<string, string>::iterator it = m_file_name.begin();
		for (; it != m_file_name.end(); it++) 
			PL_DBGOSH << "  file name: " << (*it).first << endl;
	}
	else {
		PL_DBGOSH << "  file name: empty." << endl;
	}

	if (m_polygons == NULL) {
		PL_ERROSH << "[ERROR]PolygonGroup::show_group_info():Polygon is nothing:"
				  << endl;
		return PLSTAT_POLYGON_NOT_EXIST;
	}
	if (m_polygons->get_tri_list() == 0) {
		PL_ERROSH << "[ERROR]PolygonGroup::show_group_info():Triangle is nothing:"
				  << endl;
		return PLSTAT_TRIANGLE_NOT_EXIST;
	}

	vector<PrivateTriangle*>* tmp_list = m_polygons->get_tri_list();

	PL_DBGOSH << "  triangle list size: " << tmp_list->size() << endl;
	PL_DBGOSH << "  vertex vector list: " << endl;
	vector<PrivateTriangle*>::iterator it;
	for (it = tmp_list->begin(); it != tmp_list->end(); it++) {
		Vec3f *vtx = (*it)->get_vertex();
		for (int i=0; i<3; i++) {
			PL_DBGOSH << "    id:" << i		   << " x:" << vtx[i][0] 
				 << " y:"	  << vtx[i][1] << " z:" << vtx[i][2] << endl;
		}
	}

	PL_DBGOSH << "  normal vector list: " << endl;
	for (it = tmp_list->begin(); it != tmp_list->end(); it++) {
		Vec3f vtx = (*it)->get_normal();
		PL_DBGOSH << "    x:" << vtx[0] << " y:" << vtx[1] << " z:" << vtx[2] <<endl;
	}

	PL_DBGOSH << "  triangle area list: " << endl;
	for (it = tmp_list->begin(); it != tmp_list->end(); it++) {
		PL_DBGOSH << "    area:" << (*it)->get_area() << endl;
	}
	return PLSTAT_OK;
}

// add keno 20120331
float PolygonGroup::get_group_area( void ) {
  
  float m_area=0.0, a;
  
	vector<PrivateTriangle*>* tmp_list = m_polygons->get_tri_list();
  
	vector<PrivateTriangle*>::iterator it;
  
	for (it = tmp_list->begin(); it != tmp_list->end(); it++) {
    a = (*it)->get_area();
		m_area += a;
	}
	return m_area;
}

int PolygonGroup::get_group_num_tria( void ) {
  
	vector<PrivateTriangle*>* tmp_list = m_polygons->get_tri_list();
  
	return (int)tmp_list->size();
}// add keno 20120331

// protected //////////////////////////////////////////////////////////////////
POLYLIB_STAT PolygonGroup::setup_attribute (
	Polylib					*polylib,
	PolygonGroup			*parent, 
	const PolylibCfgElem	*elem
) {
	const PolylibCfgParam *att_name = elem->first_param(ATT_NAME_NAME);
	const PolylibCfgParam *att_file = elem->first_param(ATT_NAME_PATH);
	// ユーザ定義ID追加 2010.10.20
	const PolylibCfgParam *att_id	= elem->first_param(ATT_NAME_ID);
	if (att_name == NULL) {
		PL_ERROSH << "[ERROR]PolygonGroup::setup_attribute():Paramete not found." 
				  << endl;
		return PLSTAT_CONFIG_ERROR;
	}

	// moveメソッドにより移動するグループか?
	if (this->whoami() == this->get_class_name()) {
		// 基本クラスの場合はmovableの設定は不要
	}
	else {
		const PolylibCfgParam *att_mov = elem->first_param(ATT_NAME_MOVABLE);
		if (att_mov == NULL) {
			PL_ERROSH << "[ERROR]PolygonGroup::setup_attribute():Movable not found."
				 << endl;
			return PLSTAT_CONFIG_ERROR;
		}
		if (att_mov->get_string_data() == "true")   m_movable = true;
	}

	// グループ名が重複していないか確認
	string	pg_name = att_name->get_string_data();
	string	parent_path = "";
	if (parent != NULL)		parent_path = parent->acq_fullpath();
	POLYLIB_STAT ret = polylib->check_group_name(pg_name, parent_path);
	if (ret != PLSTAT_OK)		return ret;

	// STLファイルのファイル名を設定 (リーフ以外はファイル名は未設定)
	while (att_file != NULL) {
		string fname = att_file->get_string_data();
		if (fname == "") {
			PL_ERROSH << "[ERROR]PolygonGroup::setup_attribute():File name not"
					  << " found." << endl;
			return PLSTAT_CONFIG_ERROR;
		}

		// STLファイルの拡張子をチェック
		string format = TriMeshIO::input_file_format(fname);
		if (format.empty()) {
			PL_ERROSH << "[ERROR]PolygonGroup::setup_attribute():Unknown"
					  << "extention: fname=" << fname << endl;
			return PLSTAT_UNKNOWN_STL_FORMAT;
		}

		m_file_name.insert(map<string, string>::value_type(fname, format));

		att_file = elem->next_param(att_file, ATT_NAME_PATH);
	}

	// 親を設定
	if (parent != NULL)	{
		m_parent		= parent;
		m_parent_path	= parent->acq_fullpath();
		parent->add_children(this);
	}

	// その他の属性を設定
	m_name = att_name->get_string_data();
	m_internal_id = create_global_id();
	// ユーザ定義ID追加 2010.10.20
	if (att_id == NULL) m_id = 0;
	else				m_id = att_id->get_int_data();
	
	return PLSTAT_OK;
}

// protected //////////////////////////////////////////////////////////////////
POLYLIB_STAT
PolygonGroup::init_check_leaped()
{
#ifdef DEBUG
	PL_DBGOSH << "PolygonGroup::init_check_leaped() in. " << endl;
#endif

	vector<PrivateTriangle*>* p_trias = get_triangles();

	// 動かないポリゴングループならば何もしないで終了
	if( !m_movable || p_trias==NULL || p_trias->size()==0 ) return PLSTAT_OK;

	// move後と比較するために三角形ポリゴンリストのディープコピーを保存
	m_trias_before_move = new vector<PrivateTriangle*>;
	for( unsigned int i=0; i<p_trias->size(); i++ ) {
		m_trias_before_move->push_back( new PrivateTriangle(*(p_trias->at(i))) );
	}
	return PLSTAT_OK;
}

// protected //////////////////////////////////////////////////////////////////
POLYLIB_STAT
PolygonGroup::check_leaped(
	Vec3f origin,
	Vec3f cell_size
)
{
#ifdef DEBUG
	PL_DBGOSH << "PolygonGroup::check_leaped() in. " << endl;
#endif
	unsigned int i, j;
	vector<PrivateTriangle*>* p_trias = get_triangles();

	// 動かないポリゴングループならば何もしないで終了
	if( !m_movable || p_trias==NULL || p_trias->size()==0 ) return PLSTAT_OK;

	// move前の三角形と座標を比較。
	for( i=0; i<p_trias->size(); i++ ) {

		for( j=0; j<3; j++ ) {
			// 隣接セルより遠方へ移動してたらcerrにメッセージを出力。
			if( is_far( origin, cell_size, p_trias->at(i)->get_vertex()[j],
						m_trias_before_move->at(i)->get_vertex()[j]        ) ) {
				PL_ERROSH << "[ERROR]PolygonGroup::check_leaped():Leaped Vertex"
						  << " Detected. GroupID:" << m_internal_id
						  << " TriaID:" << p_trias->at(i)->get_id()
						  << " before:(" << m_trias_before_move->at(i)->get_vertex()[j]
						  << ") after:(" << p_trias->at(i)->get_vertex()[j]
						  << ")" << endl;
			}
		}
		
		// 移動前三角形インスタンスはもう不要なので削除
		delete m_trias_before_move->at(i);
	}

	// あとしまつ
	delete m_trias_before_move;

	return PLSTAT_OK;
}

// protected //////////////////////////////////////////////////////////////////
bool
PolygonGroup::is_far(
	Vec3f origin,
	Vec3f cell_size,
	Vec3f pos1,
	Vec3f pos2
)
{
	for( int i=0; i<3; i++ ) {
		// pos1所属ボクセルの起点座標を求める
		float p;
		float dist = pos1[i] - origin[i];
		if( dist >= 0 ) {
			p = origin[i] + ((int(dist / cell_size[i])) * cell_size[i]);
		}
		else {
			if( fmodf(dist,cell_size[i]) == 0 ) {
				p = origin[i] + ((int(dist / cell_size[i])) * cell_size[i]);
			} else {
				p = origin[i] + ((int(dist / cell_size[i]) - 1) * cell_size[i]);
			}
		}

		// 隣接ボクセルまで含んだ領域のmin/max
		float min = p - cell_size[i];
		float max = p + cell_size[i] * 2;

		// pos2がmin-max間に含まれなければ真
		if( pos2[i] < min || pos2[i] > max ) return true;
	}
	return false;
}
// protected //////////////////////////////////////////////////////////////////
POLYLIB_STAT PolygonGroup::mk_basic_tag(
	xmlNodePtr	elem,
	string		rank_no,
	string		extend,
	string		format
) {
	POLYLIB_STAT	stat;

	// movableタグ (基本クラスじゃなければ作成)
	if (this->whoami() != this->get_class_name()) {
		stat = PolylibConfig::mk_param_tag(elem, ATT_NAME_MOVABLE,
										m_movable == true ? "true" : "false");
		if (stat != PLSTAT_OK)  return PLSTAT_NG;
	}

	// nameタグ
	stat = PolylibConfig::mk_param_tag(elem, ATT_NAME_NAME, m_name.c_str());
	if (stat != PLSTAT_OK)	return PLSTAT_NG;

	// filepathタグ
	// リーフの場合であれば、ポリゴン数が0でもファイルは作成する
	// ポリゴン数が0の場合はファイル未作成に変更 2010.10.19
	if (this->get_children().size() == 0 && m_polygons->triangles_num() != 0) {
		// mapの最初にあるファイル名を流用してSTLファイル名を作成
		char	*fname = mk_stl_fname(rank_no, extend, format);
		stat = PolylibConfig::mk_param_tag(elem, ATT_NAME_PATH, fname);
		if (stat != PLSTAT_OK)	return PLSTAT_NG;
	}

	// cutlib用ID追加 2010.10.20
	stat = PolylibConfig::mk_param_tag(elem, ATT_NAME_ID, m_id);
	if (stat != PLSTAT_OK)	return PLSTAT_NG;

	return PLSTAT_OK;
}

// protected //////////////////////////////////////////////////////////////////
char *PolygonGroup::mk_stl_fname(
	string		rank_no,
	string		extend,
	string		format
) {
	char		fname1[1024];
	char		*prefix;
	static char	fname2[1024];

	// グループ名のフルパスを取得して、/を_に置き換え
	strcpy(fname1, acq_fullpath().c_str());
	for (int i = 0; i < (int)strlen(fname1); i++) {
		if (fname1[i] == '/')	fname1[i] = '_';
	}

	if (format == TriMeshIO::FMT_STL_A || format == TriMeshIO::FMT_STL_AA) {
		prefix = "stla";
	}
	else {
		prefix = "stlb";
	}

	if (rank_no == "") {
		sprintf(fname2, "%s_%s.%s", fname1, extend.c_str(), prefix);
	}
	else {
		sprintf(fname2, "%s_%s_%s.%s", fname1, rank_no.c_str(), extend.c_str(), 
																		prefix);
	}
	return fname2;
}

// protected //////////////////////////////////////////////////////////////////
char *PolygonGroup::mk_id_fname(
	string		rank_no,
	string		extend
) {
	char		fname1[1024];
	static char	fname2[1024];

	// グループ名のフルパスを取得して、/を_に置き換え
	strcpy(fname1, acq_fullpath().c_str());
	for (int i = 0; i < (int)strlen(fname1); i++) {
		if (fname1[i] == '/')	fname1[i] = '_';
	}

	if (rank_no == "") {
		sprintf(fname2, "%s_%s.id", fname1, extend.c_str());
	}
	else {
		sprintf(fname2, "%s_%s_%s.id", fname1, rank_no.c_str(), extend.c_str());
	}
	return fname2;
}

// protected //////////////////////////////////////////////////////////////////
int PolygonGroup::create_global_id() {
	static int global_id = 0;
	return global_id++;
}
