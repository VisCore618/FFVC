#ifndef _IP_DUCT_H_
#define _IP_DUCT_H_

//##################################################################################
//
// FFV-C : Frontflow / violet Cartesian
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
 * @file   IP_Duct.h
 * @brief  IP_Duct class Header
 * @author aics
 */

#include "../FB/Intrinsic.h"
#include "IP_Define.h"

class IP_Duct : public Intrinsic {
protected:
  typedef struct {
    unsigned shape;     ///< 形状
    int direction;      ///< 方向
    REAL_TYPE diameter; ///< 直径
    REAL_TYPE length;   ///< 長さ
  } Driver_property;
  
  Driver_property driver;    ///< ドライバの特性
  
  std::string m_driver;      ///< ドライバ部分のラベル
  std::string m_driver_face; ///< ドライバ指定面のラベル
  
  /** 形状 */
  enum shape_type {
    id_circular = 1,
    id_rectangular
  };
  
public:
  /** コンストラクタ */
  IP_Duct(){
    driver.shape = 0;
    driver.direction = -1;
    driver.diameter = 0.0;
    driver.length = 0.0;
  }
  
  /**　デストラクタ */
  ~IP_Duct() {}

public:

  /**
   * @brief パラメータをロード
   * @param [in] R      Controlクラス
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
   * @brief Ductの計算領域のセルIDを設定する
   * @param [in,out] bcd      BCindex B
   * @param [in]     R        Controlクラスのポインタ
   * @param [in]     G_org    グローバルな原点（無次元）
   * @param [in]     NoMedium 媒質数
   * @param [in]     mat      MediumListクラスのポインタ
   * @param [out]    cut      カット情報
   * @param [out]    bid      境界ID
   */
  virtual void setup(int* bcd, Control* R, REAL_TYPE* G_org, const int NoMedium, const MediumList* mat, float* cut, int* bid);
  
};
#endif // _IP_DUCT_H_
