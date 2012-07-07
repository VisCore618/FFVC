// #################################################################
//
// FFV : Frontflow / violet
//
// Copyright (c) All right reserved. 2012
//
// Institute of Industrial Science, The University of Tokyo, Japan. 
//
// #################################################################

/** 
 * @file   NS_FS_E_Binary.C
 * @brief  FFV Class
 * @author kero
 */

#include "ffv.h"


// Fractional Step法でNavier-Stokes方程式を解く．バイナリ近似．
void FFV::NS_FS_E_Binary()
{
  int s_length = (size[0]+2*guide) * (size[1]+2*guide) * (size[2]+2*guide);
  int v_length = s_length * 3;
  
  
  // local variables
  REAL_TYPE dt = deltaT;               /// 時間積分幅
  REAL_TYPE dh = (REAL_TYPE)deltaX;    /// 空間幅
  double flop;                         /// 浮動小数演算数
  REAL_TYPE convergence=0.0;           /// 定常収束モニター量
  REAL_TYPE coef = deltaX/dt;          /// Poissonソース項の係数
  REAL_TYPE Re = C.Reynolds;           /// レイノルズ数
  REAL_TYPE rei = C.getRcpReynolds();  /// レイノルズ数の逆数
  REAL_TYPE b2 = 0.0;                  /// 反復解法での定数項ベクトルのノルム
  REAL_TYPE half = 0.5;                /// 定数                          
  REAL_TYPE comm_size;                 /// 通信面1面あたりの通信量
  int wall_prof = C.Mode.Wall_profile; /// 壁面条件（slip/noslip）
  int cnv_scheme = C.CnvScheme;        /// 対流項スキーム
  REAL_TYPE clear_value = 0.0;
  
  
  // 境界処理用
  REAL_TYPE* m_buf = NULL;
  REAL_TYPE* m_tmp = NULL;
  m_buf = new REAL_TYPE [C.NoBC*2];
  m_tmp = new REAL_TYPE [C.NoBC*2];
  
  comm_size = count_comm_size(size, 1);
  
  int v_mode=0;
  
  ItrCtl* ICp = &IC[ItrCtl::ic_prs_pr];  /// 圧力のPoisson反復
  ItrCtl* ICv = &IC[ItrCtl::ic_vis_cn];  /// 粘性項のCrank-Nicolson反復

  
  // point Data
  // d_v   セルセンタ速度 v^n -> v^{n+1}
  // d_v0  セルセンタ速度 v^nの保持
  // d_vc  疑似速度ベクトル
  // d_wv  ワーク　陰解法の時の疑似速度ベクトル，射影ステップの境界条件
  // d_p   圧力 p^n -> p^{n+1}
  // d_p0  圧力 p^nの保持
  // d_ws  Poissonのソース項0　速度境界を考慮
  // d_sq Poissonのソース項1　反復毎に変化するソース項，摩擦速度，発散値, div(u)の値を保持，出力のところまでは再利用しないこと
  // d_bcd IDのビットフラグ
  // d_bcp 圧力のビットフラグ
  // d_bcv 速度のビットフラグ
  // d_wo  ワーク　壁関数利用時のWSS，ベクトル出力時のテンポラリ
  // d_cvf コンポーネントの体積率
  // d_t0  温度 t^n 
  // d_vt  LES計算の渦粘性係数
  // d_ab0 Adams-Bashforth用のワーク
  
  
  
  // >>> Fractional step section
  TIMING_start(tm_frctnl_stp_sct); 
  
  // >>> Fractional step sub-section 1
  TIMING_start(tm_frctnl_stp_sct_1); 
  
  
  // n stepの値を保持 >> In use (d_v0, d_p0)
  TIMING_start(tm_copy_array);
  fb_copy_real_(d_p0, d_p, &s_length);
  fb_copy_real_(d_v0, d_v, &v_length);
  TIMING_stop(tm_copy_array, 0.0, 2);
  

  // 壁関数指定時の摩擦速度の計算 src0をテンポラリのワークとして利用
  if ( C.Mode.Wall_profile == Control::Log_Law ) 
  {
    TIMING_start(tm_WallFunc);
    flop = 0.0;
    friction_velocity_(d_ws, size, &guide, &dh, &Re, d_v0, d_bcp, range_Yp, range_Ut, v00, &flop);
    TIMING_stop(tm_WallFunc, flop);
  }

  TIMING_stop(tm_frctnl_stp_sct_1, 0.0);
  // <<< Fractional step subsection 1


  
  // >>> Fractional step sub-section 2
  TIMING_start(tm_frctnl_stp_sct_2);
  
  // 対流項と粘性項の評価 >> In use (dc_vc, dc_wv)
  switch (C.AlgorithmF) {
    case Control::Flow_FS_EE_EE:
    case Control::Flow_FS_AB2:
      TIMING_start(tm_pseudo_vec);     
      
      flop = 0.0;
      v_mode = (C.Mode.Wall_profile == Control::Log_Law) ? 2 : 1;

      if ( C.LES.Calc == ON ) {
        //pvec_les_(vc, sz, &guide, dh, (int*)&C.CnvScheme, v00, &rei, v0, vf, (int*)bcv, vt, &flop);
        Exit(0);
      }
      else {
        pvec_muscl_(d_vc, size, &guide, &dh, &cnv_scheme, v00, &rei, d_v0, d_bcv, d_bcp, &v_mode, d_ws, &wall_prof, d_bcd, d_cvf, &flop); 
      }
      TIMING_stop(tm_pseudo_vec, flop);

      TIMING_start(tm_pvec_flux);      
      flop = 0.0;
      BC.mod_Pvec_Flux(d_vc, d_v0, d_bcv, CurrentTime, &C, v_mode, v00, flop);
      TIMING_stop(tm_pvec_flux, flop);
      break;
      
    case Control::Flow_FS_AB_CN:
      TIMING_start(tm_pseudo_vec);
      flop = 0.0;
      v_mode = 0;
      if ( C.LES.Calc == ON ) {
        //pvec_les_(wv, sz, &guide, &dh, (int*)&C.CnvScheme, v00, &rei, v0, vf, (int*)bcv, vt, &flop);
      }
      else {
        pvec_muscl_(d_wv, size, &guide, &dh, &cnv_scheme, v00, &rei, d_v0, d_bcv, d_bcp, &v_mode, d_ws, &wall_prof, d_bcd, d_cvf, &flop); 
      }
      TIMING_stop(tm_pseudo_vec, flop);
      
      TIMING_start(tm_pvec_flux);
      flop = 0.0;
      BC.mod_Pvec_Flux(d_wv, d_v0, d_bcv, CurrentTime, &C, v_mode, v00, flop);
      TIMING_stop(tm_pvec_flux, flop);
      break;
      
    default:
      Exit(0);
  }

  // 時間積分
  switch (C.AlgorithmF) 
  {
    case Control::Flow_FS_EE_EE:
      TIMING_start(tm_pvec_ee);
      flop = 0.0;
      cbc_ee_ (d_vc, size, &guide, &dt, d_v0, d_bcd, &flop);
      TIMING_stop(tm_pvec_ee, flop);
      break;
      
    case Control::Flow_FS_AB2:
      TIMING_start(tm_pvec_ab);
      flop = 0.0;
      if ( Session_CurrentStep == 1 ) // 初期とリスタート後，1ステップめ
      {
        euler_explicit_ (d_vc, size, &guide, &dt, d_v0, d_bcd, &flop);
      }
      else 
      {
        ab2_(d_vc, size, &guide, &dt, d_v0, d_abf, d_bcd, v00, &flop);
      }
      TIMING_stop(tm_pvec_ab, flop);
      break;
      
    case Control::Flow_FS_AB_CN:
      TIMING_start(tm_pvec_abcn);
      flop = 0.0;
      if ( Session_CurrentStep == 1 ) 
      {
        euler_explicit_ (d_wv, size, &guide, &dt, d_v0, d_bcd, &flop);
      }
      else 
      {
        ab2_(d_wv, size, &guide, &dt, d_v0, d_abf, d_bcd, v00, &flop);
      }
      TIMING_stop(tm_pvec_abcn, flop);
      
      TIMING_start(tm_pvec_abcn_df_ee);
      flop = 0.0;
      vis_ee_(d_vc, size, &guide, &dh, &dt, v00, &rei, d_wv, d_v0, d_bcv, &half, &flop);
      TIMING_stop(tm_pvec_abcn_df_ee, flop);
      
      TIMING_start(tm_pvec_abcn_df_ee_BC);
      flop = 0.0;
      BC.mod_Vis_EE(d_vc, d_v0, half, d_bcv, CurrentTime, dt, v00, flop);
      TIMING_stop(tm_pvec_abcn_df_ee_BC, flop);
      break;
      
    default:
      Exit(0);
  }

  TIMING_stop(tm_frctnl_stp_sct_2, 0.0);
  // <<< Fractional step subsection 2
  

  
  // >>> Fractional step sub-section 3
  TIMING_start(tm_frctnl_stp_sct_3);
  
  // FORCINGコンポーネントの疑似速度ベクトルの方向修正と力の加算
  if ( C.isForcing() == ON ) 
  {
    TIMING_start(tm_forcing);
    flop = 0.0;
    BC.mod_Pvec_Forcing(d_vc, d_v, d_bcd, d_cvf, v00, dt, flop);
    TIMING_stop(tm_forcing, flop);
  }

  // 浮力項
  if ( C.isHeatProblem() && (C.Mode.Buoyancy == BOUSSINESQ) ) 
  {
    TIMING_start(tm_buoyancy);
    REAL_TYPE dgr = dt*C.Grashof*rei*rei;
    flop = 3.0;
    Buoyancy(d_vc, dgr, d_t0, d_bcd, flop);
    TIMING_stop(tm_buoyancy, flop);
  }

  // 疑似ベクトルの境界条件
  TIMING_start(tm_pvec_BC);
  flop = 0.0;
  BC.OuterVBC_Pseudo(d_vc, d_v0, d_bcv, CurrentTime, dt, &C, v00, flop);
  BC.OuterVBC_Periodic(d_vc);
  BC.InnerVBC_Periodic(d_vc, d_bcd);
  TIMING_stop(tm_pvec_BC, flop);

  // 疑似ベクトルの同期
  if ( numProc > 1 ) 
  {
    TIMING_start(tm_pvec_comm);
    if ( paraMngr->BndCommV3DEx(d_vc, size[0], size[1], size[2], guide, 1) != CPM_SUCCESS ) Exit(0);
    TIMING_stop(tm_pvec_comm, comm_size*1.0*3.0); // ガイドセル数 x ベクトル
  }

  TIMING_stop(tm_frctnl_stp_sct_3, 0.0);
  // <<< Fractional step subsection 3
  

  
  // >>> Fractional step sub-section 4
  TIMING_start(tm_frctnl_stp_sct_4);
  
  // Crank-Nicolson Iteration
  if ( C.AlgorithmF == Control::Flow_FS_AB_CN ) 
  {
    TIMING_start(tm_copy_array);
    fb_copy_real_(d_wv, d_vc, &v_length);
    TIMING_stop(tm_copy_array, 0.0);
    
    for (ICv->LoopCount=0; ICv->LoopCount< ICv->get_ItrMax(); ICv->LoopCount++) {
      //CN_Itr(ICv);
      if (  ICv->get_normValue() < ICv->get_eps() ) break;
    }
  }
  
  TIMING_stop(tm_frctnl_stp_sct_4, 0.0);
  // <<< Fractional step subsection 4
  
  TIMING_stop(tm_frctnl_stp_sct, 0.0);
  // <<< Fractional step section
  

  
  // Poissonのソース部分
  // >>> Poisson Source section
  TIMING_start(tm_poi_src_sct);
  
  // vの初期値をvcにしておく
  TIMING_start(tm_copy_array);
  fb_copy_real_(d_v, d_vc, &v_length);
  TIMING_stop(tm_copy_array, 0.0);
  
  // 非反復ソース項のゼロクリア src0
  TIMING_start(tm_assign_const);
  fb_set_real_(d_ws, &s_length, &clear_value);
  TIMING_stop(tm_assign_const, 0.0);
  
  // 非VBC面に対してのみ，セルセンターの値から発散量を計算
  TIMING_start(tm_div_pvec);
  flop = 0.0;
  divergence_(d_ws, size, &guide, &coef, d_vc, d_bcv, v00, &flop);
  TIMING_stop(tm_div_pvec, flop);
  
  // Poissonソース項の速度境界条件（VBC）面による修正
  TIMING_start(tm_poi_src_vbc);
  flop = 0.0;
  BC.mod_Psrc_VBC(d_ws, d_vc, d_v0, coef, d_bcv, CurrentTime, dt, &C, v00, flop);
  TIMING_stop(tm_poi_src_vbc, flop);
  
  // (Neumann_BCType_of_Pressure_on_solid_wall == grad_NS)　のとき，\gamma^{N2}の処理
  //hogehoge
  
  // 連立一次方程式の定数項の計算は圧力相対残差の場合のみ >> @todo 発散項以外の外力の影響なども含める
  if ( ICp->get_normType() == ItrCtl::p_res_l2_r) 
  {
    TIMING_start(tm_poi_src_nrm);
    b2 = 0.0;
    div_cnst_(d_ws, size, &guide, &b2, d_bcp, &flop);
    b2 = sqrt(b2);
    TIMING_stop(tm_poi_src_nrm, flop);
    
    if ( numProc > 1 ) 
    {
      TIMING_start(tm_poi_src_comm);
      REAL_TYPE m_tmp = b2;
      if ( paraMngr->Allreduce(&m_tmp, &b2, 1, MPI_SUM) != CPM_SUCCESS ) Exit(0);
      TIMING_stop(tm_poi_src_comm, 2.0*numProc*(REAL_TYPE)sizeof(REAL_TYPE) ); // 双方向 x ノード数
    }
  }

  TIMING_stop(tm_poi_src_sct, 0.0);
  // <<< Poisson Source section
  

  
  // VP-Iteration
  // >>> Poisson Iteration section
  TIMING_start(tm_poi_itr_sct);
  
  if ( C.Mode.Log_Itr == ON ) 
  {
    TIMING_start(tm_hstry_itr);
    Hostonly_ H->printHistoryItrTitle(fp_i);
    TIMING_stop(tm_hstry_itr, 0.0);
  }
  
  for (ICp->LoopCount=0; ICp->LoopCount< ICp->get_ItrMax(); ICp->LoopCount++) 
  {
    
    // >>> Poisson Iteration subsection 1
    TIMING_start(tm_poi_itr_sct_1);
    
    // 反復ソース項のゼロクリア => src1
    TIMING_start(tm_assign_const);
    fb_set_real_(d_sq, &s_length, &clear_value);
    TIMING_stop(tm_assign_const, 0.0);

    // Forcingコンポーネントによるソース項の寄与分
    if ( C.isForcing() == ON ) 
    {
      TIMING_start(tm_force_src);
      flop=0.0;
      BC.mod_Psrc_Forcing(d_sq, d_v, d_bcd, d_cvf, &dh, v00, component_array, flop);
      TIMING_stop(tm_force_src, flop);
    }

    // 内部周期境界部分のディリクレソース項
    //TIMING_start(tm_prdc_src);
    //BC.InnerPrdc_Src(d_sq, d_p, d_bcd);
    //TIMING_stop(tm_prdc_src, flop);

    TIMING_stop(tm_poi_itr_sct_1, 0.0);
    // <<< Poisson Iteration subsection 1

    // 線形ソルバー
    LS_Binary(ICp, b2);
    

    // >>> Poisson Iteration subsection 4
    TIMING_start(tm_poi_itr_sct_4);

    // 速度のスカラポテンシャルによる射影と速度の発散の計算 src1はdiv(u)のテンポラリ保持に利用
    TIMING_start(tm_prj_vec);
    flop = 0.0;
    update_vec_(d_v, d_sq, size, &guide, &dt, &dh, d_vc, d_p, d_bcp, d_bcv, v00, &flop);
    TIMING_stop(tm_prj_vec, flop);

    // セルフェイス速度の境界条件による修正
    TIMING_start(tm_prj_vec_bc);
    flop=0.0;
    BC.mod_div(d_sq, d_bcv, coef, CurrentTime, v00, m_buf, flop);
    TIMING_stop(tm_prj_vec_bc, flop);
    
    // セルフェイス速度の境界条件の通信部分
    
    if ( C.isOutflow() == ON ) 
    {
      if ( !C.isCDS() ) // Binary
      {
        if ( pn.numProc > 1 ) 
        {
          TIMING_start(tm_prj_vec_bc_comm);
          for (int n=0; n<=2*C.NoBC; n++) 
          {
            m_tmp[n] = m_buf[n];
          }
          if ( paraMngr->Allreduce(m_tmp, m_buf, 2*C.NoBC, MPI_SUM) != CPM_SUCCESS ) Exit(0);
          TIMING_stop(tm_prj_vec_bc_comm, 2.0*(REAL_TYPE)C.NoBC*numProc*(REAL_TYPE)sizeof(REAL_TYPE)*2.0 ); // 双方向 x ノード数 x 変数
        }
        
        for (int n=1; n<=C.NoBC; n++) 
        {
          if ( cmp[n].getType() == OUTFLOW ) 
          {
            cmp[n].val[var_Velocity] = m_buf[2*n]/m_buf[2*n+1]; // 無次元平均流速
          }
        }
      }
      else // Cut-Distance
      {
        ;
      }
    }
    
    // Forcingコンポーネントによる速度と発散値の修正
    if ( C.isForcing() == ON ) 
    {
      TIMING_start(tm_prj_frc_mod);
      flop=0.0;
      BC.mod_Vdiv_Forcing(d_v, d_bcd, d_cvf, d_sq, dt, &dh, v00, m_buf, component_array, flop);
      TIMING_stop(tm_prj_frc_mod, flop);

      // 通信部分
      if ( numProc > 1 ) 
      {
        TIMING_start(tm_prj_frc_mod_comm);
        for (int n=0; n<=2*C.NoBC; n++) {
          m_tmp[n] = m_buf[n];
        }
        if ( paraMngr->Allreduce(m_tmp, m_buf, 2*C.NoBC, MPI_SUM) != CPM_SUCCESS ) Exit(0);
        TIMING_stop(tm_prj_frc_mod_comm, 2.0*(REAL_TYPE)C.NoBC*numProc*(REAL_TYPE)sizeof(REAL_TYPE)*2.0);
      }
      for (int n=1; n<=C.NoBC; n++) 
      {
        if ( cmp[n].isFORCING() ) 
        {
          m_buf[2*n]   /= (REAL_TYPE)cmp[n].getElement();
          m_buf[2*n+1] /= (REAL_TYPE)cmp[n].getElement();
          cmp[n].val[var_Velocity] = m_buf[2*n];   // 平均速度
          cmp[n].val[var_Pressure] = m_buf[2*n+1]; // 平均圧力損失量
        }
      }
    }

    // 周期型の速度境界条件
    TIMING_start(tm_vec_BC);
    flop=0.0;
    BC.OuterVBC_Periodic(d_v);
    BC.InnerVBC_Periodic(d_v, d_bcd);
    TIMING_stop(tm_vec_BC, flop);
    
    TIMING_stop(tm_poi_itr_sct_4, 0.0);
    // <<< Poisson Iteration subsection 4
    
    
    // ノルムの計算
    convergence = Norm_Poisson(ICp);
    
    /* Forcingコンポーネントによる速度の方向修正(収束判定から除外)  >> TEST
    TIMING_start(tm_prj_frc_dir);
    flop=0.0;
    BC.mod_Dir_Forcing(d_v, d_bcd, d_cvf, v00, flop);
    TIMING_stop(tm_prj_frc_dir, flop);
    */
    
    // 収束判定　性能測定モードのときは収束判定を行わない
    if ( (C.Hide.PM_Test == OFF) && (convergence < ICp->get_eps()) ) break;
  } // end of iteration

  TIMING_stop(tm_poi_itr_sct, 0.0);
  // <<< Poisson Iteration section
  

  
  /// >>> NS Loop post section
  TIMING_start(tm_NS_loop_post_sct);
  
  // 同期
  if ( numProc > 1 ) {
    TIMING_start(tm_vectors_comm);
    if ( paraMngr->BndCommV3DEx(d_v, size[0], size[1], size[2], guide, guide) != CPM_SUCCESS ) Exit(0);
    TIMING_stop(tm_vectors_comm, 2*comm_size*(REAL_TYPE)guide*3.0);
  }
  
  // 外部領域境界面での速度や流量を計算 > 外部流出境界条件の移流速度に利用
  TIMING_start(tm_domain_monitor);
  flop=0.0;
  DomainMonitor(BC.export_OBC(), &C, flop );
  TIMING_stop(tm_domain_monitor, flop);
  
  // 流出境界のガイドセル値の更新と速度境界条件
  TIMING_start(tm_VBC_update);
  flop=0.0;
  BC.InnerVBC(d_v, d_bcv, CurrentTime, v00, flop);
  BC.OuterVBC(d_v, d_vc, d_bcv, CurrentTime, dt, &C, v00, flop);
  TIMING_stop(tm_VBC_update, flop);
  
  // 非同期にして隠す
  if (C.LES.Calc==ON) 
  {
    TIMING_start(tm_LES_eddy);
    flop = 0.0;
    eddy_viscosity_(d_vt, size, &guide, &dh, &C.Reynolds, &C.LES.Cs, d_v, d_bcv, range_Ut, range_Yp, v00);
    TIMING_stop(tm_LES_eddy, flop);
    
    if ( numProc > 1 ) 
    {
      TIMING_start(tm_LES_eddy_comm);
      if ( paraMngr->BndCommS3D(d_vt, size[0], size[1], size[2], guide, guide) != CPM_SUCCESS ) Exit(0);
      TIMING_stop(tm_LES_eddy_comm, comm_size*(REAL_TYPE)guide);
    }
  }
  
  // ノルムの増加率が規定値をこえたら，終了
  if (convergence_prev != 0.0 ) 
  {
    convergence_rate = convergence / convergence_prev;
  }
  else 
  {
    convergence_rate = 1.0;
  }
  convergence_prev = convergence;
  
  
  // 圧力値の引き戻しオプション
  if ( C.Mode.Pshift != -1 ) 
  {
    Pressure_Shift(d_p);
  }

  TIMING_stop(tm_NS_loop_post_sct, 0.0);
  // >>> NS loop post section
  
  // 後始末
  if ( m_buf ) delete [] m_buf;
  if ( m_tmp ) delete [] m_tmp;

}