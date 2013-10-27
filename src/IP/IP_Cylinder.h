#ifndef _IP_CYL_H_
#define _IP_CYL_H_

//##################################################################################
//
// FFV-C : Frontflow / violet Cartesian
//
// Copyright (c) 2007-2011 VCAD System Research Program, RIKEN.
// All rights reserved.
//
// Copyright (c) 2011-2013 Institute of Industrial Science, The University of Tokyo.
// All rights reserved.
//
// Copyright (c) 2012-2013 Advanced Institute for Computational Science, RIKEN.
// All rights reserved.
//
//##################################################################################

/** 
 * @file   IP_Cylinder.h
 * @brief  IP_Cylinder class Header
 * @author kero
 */

#include "../FB/Intrinsic.h"
#include "IP_Define.h"

class IP_Cylinder : public Intrinsic {
protected:

  REAL_TYPE width;           ///< 流路の幅
  REAL_TYPE height;          ///< ドライバ部の高さ
  REAL_TYPE drv_length;      ///< ドライバの長さ
  int drv_mode;              ///< ドライバのON/OFF

  std::string m_driver;      ///< ドライバ部分のラベル
  std::string m_driver_face; ///< ドライバ指定面のラベル
  
public:
  /** コンストラクタ */
  IP_Cylinder(){
    width  = 0.0;
    height = 0.0;
    drv_length = 0.0;
  }
  
  /**　デストラクタ */
  ~IP_Cylinder() {}

public:

  /**
   * @brief パラメータをロード
   * @apram [in] R      Controlクラス
   * @param [in] tpCntl テキストパーサクラス
   * @return true-成功, false-エラー
   */
  virtual bool getTP(Control* R, TextParser* tpCntl);
  
  
  /**
   * @brief パラメータの表示
   * @param [in] fp ファイルポインタ
   * @param [in] R  コントロールクラスのポインタ
   */
  virtual void printPara(FILE* fp, const Control* R);

  
  /**
   * @brief Cylinderの計算領域のセルIDを設定する
   * @param [in,out] bcd   　　BCindex B
   * @param [in]     R     　　Controlクラスのポインタ
   * @param [in]     G_org 　　グローバルな原点（無次元）
   * @param [in]     NoMedium 媒質数
   * @param [in]     mat   　　MediumListクラスのポインタ
   * @param [out]    cut      カット情報
   * @param [out]    bid      境界ID
   */
  virtual void setup(int* bcd, Control* R, REAL_TYPE* G_org, const int NoMedium, const MediumList* mat, float* cut, int* bid);
  
};
#endif // _IP_CYL_H_
