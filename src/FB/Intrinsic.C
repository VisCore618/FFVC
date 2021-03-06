//##################################################################################
//
// Flow Base class
//
// Copyright (c) 2007-2011 VCAD System Research Program, RIKEN.
// All rights reserved.
//
// Copyright (c) 2011-2014 Institute of Industrial Science, The University of Tokyo.
// All rights reserved.
//
// Copyright (c) 2012-2014 Advanced Institute for Computational Science, RIKEN.
// All rights reserved.
//
//##################################################################################

/** 
 * @file   Intrinsic.C
 * @brief  FlowBase Intrinsic class
 * @author aics
 */

#include "Intrinsic.h"


// #################################################################
// 例題名称の表示
void Intrinsic::printExample(FILE* fp, const int m_id)
{
  if ( !fp )
  {
    stamped_printf("\tFail to write into file\n");
    Exit(0);
  }
  
  string str;
  
  if      ( m_id == id_Polygon ) str = "Polygon";
  else if ( m_id == id_Duct )    str = "Duct";
  else if ( m_id == id_PMT )     str = "Performance Test";
  else if ( m_id == id_Rect )    str = "Rectangular";
  else if ( m_id == id_Cylinder) str = "Cylinder";
  else if ( m_id == id_Step )    str = "Back Step";
  else if ( m_id == id_PPLT2D)   str = "Parallel Plate 2D";
  else if ( m_id == id_Sphere )  str = "Sphere";
  else if ( m_id == id_Jet )     str = "Jet";
  else
  {
    stamped_printf("\tInvalid keyword is described for Exmple definition\n");
    Exit(0);
  }
  
  fprintf(fp,"\n\tChallenge : %s\n\n", str.c_str()); fflush(fp);
  printf    ("\n\tChallenge : %s\n\n", str.c_str());
  
}



// #################################################################
// 例題クラス固有のパラメータの表示
void Intrinsic::printPara(FILE* fp, const Control* R)
{
  if ( !fp ) {
    stamped_printf("\tFail to write into file\n");
    Exit(0);
  }
  // describe method
}


// #################################################################
// 代表パラメータのセット
void Intrinsic::setRefParameter(Control* Cref)
{
  RefL = Cref->RefLength;
  RefV = Cref->RefVelocity;
}


// #################################################################
// モデルIDをsphフォーマット(float)で出力する
void Intrinsic::writeSPH(const int *bcd, const Control* R)
{
  int ix, jx, kx;
  float ox, oy, oz, dx, dy, dz;
  char fname[64];
  
  if ( paraMngr->IsParallel() )
  {
    sprintf( fname, "model_%06d.sph", paraMngr->GetMyRankID() );
  }
  else
  {
    sprintf( fname, "model.sph" );
  }
  
  ofstream ofs(fname, ios::out | ios::binary);
  
  if (!ofs)
  {
    cout << "\tCan't open " << fname << " file" << endl;
    Exit(0);
  }
  
  int imax = size[0];
  int jmax = size[1];
  int kmax = size[2];
  int gd = guide;
  
  ix = imax+2;  // +2 means guide cell
  jx = jmax+2;
  kx = kmax+2;
  
  size_t nx = (size_t)(ix*jx*kx);
  
  dx = (float)pitch[0]*RefL;
  dy = (float)pitch[1]*RefL;
  dz = (float)pitch[2]*RefL;
  
  ox = (float)origin[0]*RefL - dx; // 片側1層分をシフト
  oy = (float)origin[1]*RefL - dy;
  oz = (float)origin[2]*RefL - dz;
  
  float *q = new float[nx];
  
  #pragma omp parallel for firstprivate(imax, jmax, kmax, ix, jx, gd) schedule(static)
  for (int k=0; k<=(kmax+1); k++) {
    for (int j=0; j<=(jmax+1); j++) {
      for (int i=0; i<=(imax+1); i++) {
        size_t l = (size_t)(ix*jx*k + ix*j + i);
        size_t m = _F_IDX_S3D(i, j, k, imax, jmax, kmax, gd);
        q[l] = (float)DECODE_CMP(bcd[m]);
      }
    }
  }
  
  // data property
  int dType  = 1; // float
  int svType = 1; // scalar
  int pad = sizeof(int)*2;
  ofs.write( (char*)&pad, sizeof(int) );
  ofs.write( (char*)&svType, sizeof(int) );
  ofs.write( (char*)&dType,  sizeof(int) );
  ofs.write( (char*)&pad, sizeof(int) );
  
  // voxel size
  pad = sizeof(int)*3;
  ofs.write( (char*)&pad, sizeof(int) );
  ofs.write( (char*)&ix, sizeof(int) );
  ofs.write( (char*)&jx, sizeof(int) );
  ofs.write( (char*)&kx, sizeof(int) );
  ofs.write( (char*)&pad, sizeof(int) );
  
  // original point of domain
  pad = sizeof(float)*3;
  ofs.write( (char*)&pad, sizeof(int) );
  ofs.write( (char*)&ox, sizeof(float) );
  ofs.write( (char*)&oy, sizeof(float) );
  ofs.write( (char*)&oz, sizeof(float) );
  ofs.write( (char*)&pad, sizeof(int) );
  
  // pitch of voxel
  ofs.write( (char*)&pad, sizeof(int) );
  ofs.write( (char*)&dx, sizeof(float) );
  ofs.write( (char*)&dy, sizeof(float) );
  ofs.write( (char*)&dz, sizeof(float) );
  ofs.write( (char*)&pad, sizeof(int) );
  
  // time stamp
  int stp = 0;
  float tm = 0.0;
  pad = sizeof(int)+sizeof(float);
  ofs.write( (char*)&pad, sizeof(int) );
  ofs.write( (char*)&stp, sizeof(int) );
  ofs.write( (char*)&tm, sizeof(float) );
  ofs.write( (char*)&pad, sizeof(int) );
  
  // medium ID
  pad = nx * sizeof(float);
  ofs.write( (char*)&pad, sizeof(int) );
  ofs.write( (char*)q,   pad );
  ofs.write( (char*)&pad, sizeof(int) );
  
  ofs.close();
  
  if (q) { delete [] q; q=NULL; }
}


// #################################################################
// 例題のモデルをsvxフォーマットで出力する(体積率とID)
void Intrinsic::writeSVX(REAL_TYPE *vf, int *id, Control* R)
{

  int    sz, ix, jx, kx;
  size_t m, l;
  float  ox, oy, oz, dx, dy, dz;

  char svx_fname[512];

  if ( paraMngr->IsParallel() ) {
    sprintf( svx_fname, "example_%06d.svx", paraMngr->GetMyRankID() );
  } else {
    sprintf( svx_fname, "example.svx" );
  }
  ofstream ofs(svx_fname, ios::out | ios::binary);
  if (!ofs) {
    cout << "\tCan't open " << svx_fname << " file" << endl;
    Exit(0);
  }

  int imax = size[0];
  int jmax = size[1];
  int kmax = size[2];
  int gd = guide;
  
  ix = imax+2;  // +2 means guide cell for IP model
  jx = jmax+2;
  kx = kmax+2;
  
  size_t nx = ix*jx*kx;
  
  dx = (float)pitch[0]*RefL;
  dy = (float)pitch[1]*RefL;
  dz = (float)pitch[2]*RefL;
  ox = (float)origin[0]*RefL - dx; // 片側1層分をシフト
  oy = (float)origin[1]*RefL - dy;
  oz = (float)origin[2]*RefL - dz;
  
  //stamped_printf("example out org(%e %e %e) dimensional\n", ox, oy, oz);

  float *f = new float[nx];
  int   *q = new int[nx];

  for (int k=0; k<=(kmax+1); k++) {
    for (int j=0; j<=(jmax+1); j++) {
      for (int i=0; i<=(imax+1); i++) {
        l = (size_t)(ix*jx*k + ix*j + i);
        m = _F_IDX_S3D(i, j, k, imax, jmax, kmax, gd);
        q[l] = id[m];
        f[l] = (float)vf[m];
      }
    }
  }

  // voxel size
  sz = sizeof(int)*3;
  ofs.write( (char*)&sz, sizeof(int) );
  ofs.write( (char*)&ix, sizeof(int) );
  ofs.write( (char*)&jx, sizeof(int) );
  ofs.write( (char*)&kx, sizeof(int) );
  ofs.write( (char*)&sz, sizeof(int) );

  // original point of domain
  sz = sizeof(float)*3;
  ofs.write( (char*)&sz, sizeof(int) );
  ofs.write( (char*)&ox, sizeof(float) );
  ofs.write( (char*)&oy, sizeof(float) );
  ofs.write( (char*)&oz, sizeof(float) );
  ofs.write( (char*)&sz, sizeof(int) );

  // pitch of voxel
  ofs.write( (char*)&sz, sizeof(int) );
  ofs.write( (char*)&dx, sizeof(float) );
  ofs.write( (char*)&dy, sizeof(float) );
  ofs.write( (char*)&dz, sizeof(float) );
  ofs.write( (char*)&sz, sizeof(int) );

  // type of stored data
  sz = sizeof(int)*1;
  int dtype = 0;
  dtype |= ( 0x1<<0 );  // volume fraction
  dtype |= ( 0x1<<2 );  // medium ID
  ofs.write( (char*)&sz,  sizeof(int) );
  ofs.write( (char*)&dtype, sizeof(int) );
  ofs.write( (char*)&sz,  sizeof(int) );

  // volume fraction
  sz = nx * sizeof(float);
  ofs.write( (char*)&sz, sizeof(int) );
  ofs.write( (char*)f,   sz );
  ofs.write( (char*)&sz, sizeof(int) );

  // medium ID
  sz = nx * sizeof(int);
  ofs.write( (char*)&sz, sizeof(int) );
  ofs.write( (char*)q,   sz );
  ofs.write( (char*)&sz, sizeof(int) );

  ofs.close();

  if (f) { delete [] f; f=NULL; }
  if (q) { delete [] q; q=NULL; }
}


// #################################################################
// 例題のモデルをsvxフォーマットで出力する(ID)
void Intrinsic::writeSVX(const int* bcd, Control* R)
{
  
  int   sz, ix, jx, kx;
  size_t m, l;
  float ox, oy, oz, dx, dy, dz;
  
  char svx_fname[512];
  
  if ( paraMngr->IsParallel() ) {
    sprintf( svx_fname, "example_%06d.svx", paraMngr->GetMyRankID() );
  } else {
    sprintf( svx_fname, "example.svx" );
  }
  ofstream ofs(svx_fname, ios::out | ios::binary);
  if (!ofs) {
    cout << "\tCan't open " << svx_fname << " file" << endl;
    Exit(0);
  }
  
  int imax = size[0];
  int jmax = size[1];
  int kmax = size[2];
  int gd = guide;
  
  ix = imax+2;  // +2 means guide cell for IP model
  jx = jmax+2;
  kx = kmax+2;
  
  size_t nx = (size_t)(ix*jx*kx);
  
  dx = (float)pitch[0]*RefL;
  dy = (float)pitch[1]*RefL;
  dz = (float)pitch[2]*RefL;
  ox = (float)origin[0]*RefL - dx; // 片側1層分をシフト
  oy = (float)origin[1]*RefL - dy;
  oz = (float)origin[2]*RefL - dz;
  
  //stamped_printf("example out org(%e %e %e) dimensional\n", ox, oy, oz);
  
  int   *q = new int[nx];
  
  for (int k=0; k<=(kmax+1); k++) {
    for (int j=0; j<=(jmax+1); j++) {
      for (int i=0; i<=(imax+1); i++) {
        l = (size_t)(ix*jx*k + ix*j + i);
        m = _F_IDX_S3D(i, j, k, imax, jmax, kmax, gd);
        q[l] = DECODE_CMP(bcd[m]);
      }
    }
  }
  
  // voxel size
  sz = sizeof(int)*3;
  ofs.write( (char*)&sz, sizeof(int) );
  ofs.write( (char*)&ix, sizeof(int) );
  ofs.write( (char*)&jx, sizeof(int) );
  ofs.write( (char*)&kx, sizeof(int) );
  ofs.write( (char*)&sz, sizeof(int) );
  
  // original point of domain
  sz = sizeof(float)*3;
  ofs.write( (char*)&sz, sizeof(int) );
  ofs.write( (char*)&ox, sizeof(float) );
  ofs.write( (char*)&oy, sizeof(float) );
  ofs.write( (char*)&oz, sizeof(float) );
  ofs.write( (char*)&sz, sizeof(int) );
  
  // pitch of voxel
  ofs.write( (char*)&sz, sizeof(int) );
  ofs.write( (char*)&dx, sizeof(float) );
  ofs.write( (char*)&dy, sizeof(float) );
  ofs.write( (char*)&dz, sizeof(float) );
  ofs.write( (char*)&sz, sizeof(int) );
  
  // type of stored data
  sz = sizeof(int)*1;
  int dtype = 0;
  dtype |= ( 0x1<<2 );  // medium ID
  ofs.write( (char*)&sz,  sizeof(int) );
  ofs.write( (char*)&dtype, sizeof(int) );
  ofs.write( (char*)&sz,  sizeof(int) );
  
  // medium ID
  sz = nx * sizeof(int);
  ofs.write( (char*)&sz, sizeof(int) );
  ofs.write( (char*)q,   sz );
  ofs.write( (char*)&sz, sizeof(int) );
  
  ofs.close();
  
  if (q) { delete [] q; q=NULL; }
}
