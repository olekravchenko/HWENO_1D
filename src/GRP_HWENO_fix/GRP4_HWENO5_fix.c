#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>



#include "Riemann_solver.h"

#ifndef L_STR
#include "file_io_local.h"
#endif

#include "reconstruction.h"


int GRP4_HWENO5_fix
(option OPT, int const m, double const h,
 double rho[], double u[], double p[], runHist *runhist,
 char *add_mkdir, char *label)
{
  delete_runHist(runhist);
  double const gamma     = OPT.gamma;  // the constant of the perfect gas
  double const CFL       = OPT.CFL;  // CFL number
  double const eps       = OPT.eps;  // the largest value could be treat as zero
  double const tol       = OPT.tol;

  double const alp1      = OPT.alp1;
  double const alp2      = OPT.alp2;
  double const bet1      = OPT.bet1;
  double const bet2      = OPT.bet2;
  double const modifier  = modifier;
  double const threshold = OPT.threshold;
  double const thickness = OPT.thickness;

  int const    MaxStp    = OPT.MaxStp;  //the number of time steps
  int const    bod       = OPT.bod;
  int const    Primative = OPT.Primative;
  int const    Deri      = OPT.Deri;
  int const    Limiter   = OPT.Limiter;
  int const    Decomp    = OPT.Decomp;
  double const *TIME     = OPT.TIME;
  int const    nTIME     = OPT.nTIME;
  int itTIME = 0, output = 0;


  int i = 0, j = 0, k = 1, it = 0;
  int state, len = 0;
  char scheme[L_STR] = "G4H5\0";
  char version[L_STR], err_msg[L_STR];
  strcpy(version, add_mkdir);
  strcpy(add_mkdir, "../SOLUTION/\0");
  int SWITCHES[3];
  SWITCHES[0] = Deri;
  SWITCHES[1] = Decomp;
  SWITCHES[2] = Limiter;
  state = make_directory(add_mkdir, label, scheme, version, m, 1, SWITCHES, err_msg);
  if(state)
  { printf("%s", err_msg);
    exit(state); }
  FILE * fp_out;
  char add_out[L_STR+L_STR] = "\0", strIDX[L_STR];
  int it_out;
  double * DATA[OPT.nInitValue];
  int output_flag[OPT.nInitValue];
  for(j = 0; j < OPT.nInitValue; ++j)
    output_flag[j] = OPT.output_flag[j];
  output_flag[4] = 0;
  DATA[0] = rho;
  DATA[1] = u;
  DATA[2] = p;

  int trouble0[m], trouble1[m];
  FILE * fp_tr0, * fp_tr1;
  char add_tr0[L_STR+L_STR], add_tr1[L_STR+L_STR];
  strcpy(add_tr0, add_mkdir);
  strcat(add_tr0, "trouble0.txt\0");
  state = open_fruncate(err_msg, add_tr0, &fp_tr0);
  if(state)
  { printf("%s", err_msg);
    exit(state); }
  strcpy(add_tr1, add_mkdir);
  strcat(add_tr1, "trouble1.txt\0");
  state = open_fruncate(err_msg, add_tr1, &fp_tr1);
  if(state)
  { printf("%s", err_msg);
    exit(state); }


  printf("The output time points are:\n  ");
  for(j = 0; j < nTIME; ++j)
    printf("| %g ", TIME[j]);
  printf("|\n\n");
  printf("===========================\n");
  printf("The scheme [%s] started.\n", scheme);


  clock_t tic, toc;
  int num_print_1 = 0, num_print_2 = 0, it_print;
  double current_cpu_time = 0.0, sum_cpu_time = 0.0, T = 0.0;
  
  

  double running_info[N_RUNNING];
  running_info[3] = bod;        //the boundary condition
  running_info[4] = Deri;       //the choice of the direvative reconstruction
  running_info[5] = Decomp;     //use the charactoristic decomposition or not
  running_info[6] = Limiter;    //use the limiter or not
  running_info[7] = threshold;  //threshold
  int recon_info[3];
  recon_info[1] = Decomp;  //characteristic decomposition or not



  double c, stmp, D[5], U[5], wave_speed[2];
  RSparameters RSpara;
  RSpara.eps = eps;
  RSpara.tol = tol;
  RSpara.N = 500;
  RSpara.radius = 1.0;
  RSpara.nDim = 1;
  RSpara.geo_factor = 0.0;
  int n_trans = 0;
  EulerPack wL, wR, res;
  wL.VAR.trans = NULL;
  wL.DER.trans = NULL;
  wL.TGT.trans = NULL;
  wR.VAR.trans = NULL;
  wR.DER.trans = NULL;
  wR.TGT.trans = NULL;
  res.VAR.trans = NULL;
  res.DER.trans = NULL;
  res.TGT.trans = NULL;


  double mom[m], ene[m];
  double rho_star, u_star, p_star, E_star;
  double rhoI[m+1], momI[m+1], eneI[m+1], uI[m+1], pI[m+1], half_rhoI[m+1], half_momI[m+1], half_eneI[m+1], half_uI[m+1], half_pI[m+1];
  double rho_L[m+1], rho_R[m+1], u_L[m+1], u_R[m+1], p_L[m+1], p_R[m+1];
  double D_rho_L[m+1], D_rho_R[m+1], D_u_L[m+1], D_u_R[m+1], D_p_L[m+1], D_p_R[m+1];

  double half_rho[m], half_u[m], half_p[m], half_mom[m], half_ene[m];

  double f01[m+1], f02[m+1], f03[m+1];
  double f11[m+1], f12[m+1], f13[m+1];
  double g01[m+1], g02[m+1], g03[m+1];
  double g11[m+1], g12[m+1], g13[m+1];
  double F1[m+1], F2[m+1], F3[m+1];

  double sigma, speed_max;  /* speed_max denote the largest character
			     * speed at each time step
			     */
  double tau, half_tau, nu, half_nu, alp, bet;


  double D0 = 1.0/6.0, D1 = 0.5-D0;


  if(Primative)
    for(j = 0; j < m; ++j)
    {
      mom[j] = rho[j]*u[j];
      ene[j] = p[j]/(gamma-1.0)+0.5*mom[j]*u[j];
    }
  else
    for(j = 0; j < m; ++j)
    {
      mom[j] = u[j];
      ene[j] = p[j];
      u[j] = mom[j]/rho[j];
      p[j] = (ene[j]-0.5*mom[j]*u[j])*(gamma-1.0);
    }

  running_info[0] = 0.0;  // k
  running_info[1] = 0.0;  // time
  running_info[2] = 0.0;  // not half
  WENO_50(running_info, m, h, eps, alp2, gamma, rho, mom, ene, rho_L, rho_R, u_L, u_R, p_L, p_R, D_rho_L, D_rho_R, D_u_L, D_u_R, D_p_L, D_p_R);
  /*
  write_column(m, rho[0], "rho", "running");
  write_column(m, ene, "ene", "running");
  write_column(m, p[0], "p", "running");

  write_column(m+1, rho_L, "rhoL", "running");
  write_column(m+1, u_L, "uL", "running");
  write_column(m+1, p_L, "pL", "running");
  write_column(m+1, rho_R, "rhoR", "running");
  write_column(m+1, u_R, "uR", "running");
  write_column(m+1, p_R, "pR", "running");
  write_column(m+1, D_rho_L, "DrhoL", "running");
  write_column(m+1, D_u_L, "DuL", "running");
  write_column(m+1, D_p_L, "DpL", "running");
  write_column(m+1, D_rho_R, "DrhoR", "running");
  write_column(m+1, D_u_R, "DuR", "running");
  write_column(m+1, D_p_R, "DpR", "running");//*/
//------------THE MAIN LOOP-------------
  for(k = 1; k <= MaxStp; ++k)
  {
    if(T+eps > TIME[itTIME])
      if(itTIME == nTIME-1)
	break;
      else
	output = 1;


    state = insert_runHist(runhist);
    if(state)
    {
      printf("Not enough memory for the runhist node!\n\n");
      exit(100);//remains to modify the error code.
    }
    state = locate_runHist(k-1, runhist);
    if(state)
    {
      printf("The record has only %d compunonts while trying to reach runhist[%d].\n\n", state-1, k);
      exit(100);//remains to modify the error code.
    }

    
    running_info[0] = (double)k;
    
    speed_max = 0.0;
    for(j = 0; j < m; ++j)
    {
      c = sqrt(gamma * p[j] / rho[j]);
      sigma = fabs(c) + fabs(u[j]);
      if(speed_max < sigma)
	speed_max = sigma;
    }
    tau = (CFL * h) / speed_max;
    if(T+tau > TIME[itTIME]){tau = TIME[itTIME]-T; T = TIME[itTIME];} else{T += tau;}
    half_tau = 0.5*tau;
    nu = tau/h;
    half_nu = 0.5*nu;
    runhist->current->time[0] = tau;


    for(it_print = 0; it_print < num_print_1; ++it_print)
      printf(" ");
    printf("\r");
    fflush(stdout);
    num_print_1 = printf("%d | %g : %g | %g : %g", k, tau, T, current_cpu_time, sum_cpu_time);
    fflush(stdout);
    printf("\r");
    tic = clock();


    for(j = 0; j < m+1; ++j)
    {
      wL.gamma = gamma;
      wL.VAR.rho = rho_L[j];
      wL.VAR.u   = u_L[j];
      wL.VAR.v   = 0.0;
      wL.VAR.p   = p_L[j];
      wL.DER.rho = D_rho_L[j];
      wL.DER.u   = D_u_L[j];
      wL.DER.v   = 0.0;
      wL.DER.p   = D_p_L[j];
      wL.TGT.rho = 0.0;
      wL.TGT.u   = 0.0;
      wL.TGT.v   = 0.0;
      wL.TGT.p   = 0.0;
      wR.gamma = gamma;
      wR.VAR.rho = rho_R[j];
      wR.VAR.u   = u_R[j];
      wR.VAR.v   = 0.0;
      wR.VAR.p   = p_R[j];
      wR.DER.rho = D_rho_R[j];
      wR.DER.u   = D_u_R[j];
      wR.DER.v   = 0.0;
      wR.DER.p   = D_p_R[j];
      wR.TGT.rho = 0.0;
      wR.TGT.u   = 0.0;
      wR.TGT.v   = 0.0;
      wR.TGT.p   = 0.0;
      Euler_GRP_solver(wave_speed, &res, 0.0, 0.0, n_trans, &wL, &wR, &RSpara);
      U[0] = res.VAR.rho;
      U[1] = res.VAR.u;
      U[2] = res.VAR.v;
      U[3] = res.VAR.p;
      D[0] = res.DER.rho;
      D[1] = res.DER.u;
      D[2] = res.DER.v;
      D[3] = res.DER.p;

      
      f01[j] = U[0]*U[1];
      f02[j] = f01[j]*U[1] + U[3];
      f03[j] = (gamma*U[3]/(gamma-1.0) + 0.5*f01[j]*U[1]) * U[1];

      g01[j] = U[0]*D[1] + U[1]*D[0];
      g02[j] = D[0]*U[1]*U[1] + 2.0*U[0]*U[1]*D[1] + D[3];
      g03[j] = (D[3]*U[1] + U[3]*D[1])*gamma/(gamma-1.0);
      g03[j] = g03[j] + 0.5*D[0]*U[1]*U[1]*U[1] + 1.5*U[0]*U[1]*U[1]*D[1];

      rhoI[j] = U[0];
      momI[j] = U[0]*U[1];
      eneI[j] = 0.5*U[0]*U[1]*U[1] + U[3]/(gamma-1.0);
      half_rhoI[j] = rhoI[j] + half_tau*D[0];
      half_momI[j] = momI[j] + half_tau*(D[1]*U[0] + D[0]*U[1]);
      half_eneI[j] = eneI[j] + half_tau*(0.5*D[0]*U[1]*U[1] + U[0]*U[1]*D[1] + D[3]/(gamma-1.0));

      uI[j] = U[1];
      pI[j] = U[3];
      half_uI[j] = U[1] + half_tau*D[1];
      half_pI[j] = U[3] + half_tau*D[3];
    }
    for(j = 0; j < m; ++j)
    {
      half_rho[j] = rho[j] - half_nu*((f01[j+1]-f01[j]) + 0.5*half_tau*(g01[j+1]-g01[j]));
      half_mom[j] = mom[j] - half_nu*((f02[j+1]-f02[j]) + 0.5*half_tau*(g02[j+1]-g02[j]));
      half_ene[j] = ene[j] - half_nu*((f03[j+1]-f03[j]) + 0.5*half_tau*(g03[j+1]-g03[j]));

      half_u[j] = half_mom[j] / half_rho[j];
      half_p[j] = (half_ene[j] - 0.5*half_mom[j]*half_u[j]) * (gamma-1.0);
      if(half_p[j] < 0.0)
        printf("half(%d,%d), %g\n", k, j, half_p[j]);
    }

    running_info[1] = T - half_tau;  // time
    running_info[2] = 1.0;           // half
    /*
    for(i = 0; i < m+1; ++i)
      printf("%.10lf  ", rho_L[i]);
    printf("\n\n");
    for(i = 0; i < m+1; ++i)
      printf("%.10lf  ", rho_R[i]);
    printf("\n\n");
    for(i = 0; i < m+1; ++i)
      printf("%.10lf  ", u_L[i]);
    printf("\n\n");
    for(i = 0; i < m+1; ++i)
      printf("%.10lf  ", u_R[i]);
    printf("\n\n");
    for(i = 0; i < m+1; ++i)
      printf("%.10lf  ", p_L[i]);
    printf("\n\n");
    for(i = 0; i < m+1; ++i)
      printf("%.10lf  ", p_R[i]);
    printf("\n\n");
    for(i = 0; i < m+1; ++i)
      printf("%.10lf  ", rhoI[i]);
    printf("\n\n");
    for(i = 0; i < m+1; ++i)
      printf("%.10lf  ", half_rhoI[i]);
    printf("\n\n");
    for(i = 0; i < m+1; ++i)
      printf("%.10lf  ", uI[i]);
    printf("\n\n");
    for(i = 0; i < m+1; ++i)
      printf("%.10lf  ", pI[i]);
      printf("\n\n");//*/
    HWENO_5(running_info, m, h, eps, alp2, gamma, half_rho, half_mom, half_ene, half_rhoI, half_momI, half_eneI, half_uI, half_pI, rho_L, rho_R, u_L, u_R, p_L, p_R, D_rho_L, D_rho_R, D_u_L, D_u_R, D_p_L, D_p_R, trouble1);
    /*
  write_column(m, half_rho, "rho", "running");
  write_column(m, half_u, "u", "running");
  write_column(m, half_p, "p", "running");
  write_column(m+1, half_rhoI, "rhoI", "running");
  write_column(m+1, half_uI, "uI", "running");
  write_column(m+1, half_pI, "pI", "running");

  write_column(m+1, rho_L, "rhoL", "running");
  write_column(m+1, u_L, "uL", "running");
  write_column(m+1, p_L, "pL", "running");
  write_column(m+1, rho_R, "rhoR", "running");
  write_column(m+1, u_R, "uR", "running");
  write_column(m+1, p_R, "pR", "running");
  write_column(m+1, D_rho_L, "DrhoL", "running");
  write_column(m+1, D_u_L, "DuL", "running");
  write_column(m+1, D_p_L, "DpL", "running");
  write_column(m+1, D_rho_R, "DrhoR", "running");
  write_column(m+1, D_u_R, "DuR", "running");
  write_column(m+1, D_p_R, "DpR", "running");//*/


    for(j = 0; j < m+1; ++j)
    {
      wL.gamma = gamma;
      wL.VAR.rho = rho_L[j];
      wL.VAR.u   = u_L[j];
      wL.VAR.v   = 0.0;
      wL.VAR.p   = p_L[j];
      wL.DER.rho = D_rho_L[j];
      wL.DER.u   = D_u_L[j];
      wL.DER.v   = 0.0;
      wL.DER.p   = D_p_L[j];
      wL.TGT.rho = 0.0;
      wL.TGT.u   = 0.0;
      wL.TGT.v   = 0.0;
      wL.TGT.p   = 0.0;
      wR.gamma = gamma;
      wR.VAR.rho = rho_R[j];
      wR.VAR.u   = u_R[j];
      wR.VAR.v   = 0.0;
      wR.VAR.p   = p_R[j];
      wR.DER.rho = D_rho_R[j];
      wR.DER.u   = D_u_R[j];
      wR.DER.v   = 0.0;
      wR.DER.p   = D_p_R[j];
      wR.TGT.rho = 0.0;
      wR.TGT.u   = 0.0;
      wR.TGT.v   = 0.0;
      wR.TGT.p   = 0.0;
      Euler_GRP_solver(wave_speed, &res, 0.0, 0.0, n_trans, &wL, &wR, &RSpara);
      U[0] = res.VAR.rho;
      U[1] = res.VAR.u;
      U[2] = res.VAR.v;
      U[3] = res.VAR.p;
      D[0] = res.DER.rho;
      D[1] = res.DER.u;
      D[2] = res.DER.v;
      D[3] = res.DER.p;

      g11[j] = U[0]*D[1] + U[1]*D[0];
      g12[j] = D[0]*U[1]*U[1] + 2.0*U[0]*U[1]*D[1] + D[3];
      g13[j] = (D[3]*U[1] + U[3]*D[1])*gamma/(gamma-1.0);
      g13[j] = g13[j] + 0.5*D[0]*U[1]*U[1]*U[1] + 1.5*U[0]*U[1]*U[1]*D[1];

      F1[j] = f01[j] + tau*(D0*g01[j] + D1*g11[j]);
      F2[j] = f02[j] + tau*(D0*g02[j] + D1*g12[j]);
      F3[j] = f03[j] + tau*(D0*g03[j] + D1*g13[j]);

      rhoI[j] = rhoI[j] + tau*D[0];
      momI[j] = momI[j] + tau*(D[1]*U[0] + D[0]*U[1]);
      eneI[j] = eneI[j] + tau*(0.5*D[0]*U[1]*U[1] + U[0]*U[1]*D[1] + D[3]/(gamma-1.0));

      uI[j] = uI[j] + tau*D[1];
      pI[j] = pI[j] + tau*D[3];
    }

//===============THE CORE ITERATION=================
    for(j = 0; j < m; ++j)
    {
      rho[j] = rho[j] - nu*(F1[j+1]-F1[j]);
      mom[j] = mom[j] - nu*(F2[j+1]-F2[j]);
      ene[j] = ene[j] - nu*(F3[j+1]-F3[j]);
	u[j] = mom[j] / rho[j];
	p[j] = (ene[j] - 0.5*mom[j]*u[j])*(gamma-1.0);

      if(p[j] < 0.0)
        printf("    (%d,%d), %g\n", k, j, p[j]);
    }

    running_info[1] = T;
    running_info[2] = 0.0;  // not half
    /*
    for(i = 0; i < m+1; ++i)
      printf("%.10lf  ", rho_L[i]);
    printf("\n\n");
    for(i = 0; i < m+1; ++i)
      printf("%.10lf  ", rho_R[i]);
    printf("\n\n");
    for(i = 0; i < m+1; ++i)
      printf("%.10lf  ", u_L[i]);
    printf("\n\n");
    for(i = 0; i < m+1; ++i)
      printf("%.10lf  ", u_R[i]);
    printf("\n\n");
    for(i = 0; i < m+1; ++i)
      printf("%.10lf  ", p_L[i]);
    printf("\n\n");
    for(i = 0; i < m+1; ++i)
      printf("%.10lf  ", p_R[i]);
    printf("\n\n");
    for(i = 0; i < m+1; ++i)
      printf("%.10lf  ", rhoI[i]);
    printf("\n\n");
    for(i = 0; i < m+1; ++i)
      printf("%.10lf  ", uI[i]);
    printf("\n\n");
    for(i = 0; i < m+1; ++i)
      printf("%.10lf  ", pI[i]);
      printf("\n\n");//*/
    HWENO_5(running_info, m, h, eps, alp2, gamma, rho, mom, ene, rhoI, momI, eneI, uI, pI, rho_L, rho_R, u_L, u_R, p_L, p_R, D_rho_L, D_rho_R, D_u_L, D_u_R, D_p_L, D_p_R, trouble0);
    /*
  write_column(m, rho[vk1], "rho", "running");
  write_column(m, u[vk1], "u", "running");
  write_column(m, p[vk1], "p", "running");
  write_column(m+1, rhoI, "rhoI", "running");
  write_column(m+1, uI, "uI", "running");
  write_column(m+1, pI, "pI", "running");

  write_column(m+1, rho_L, "rhoL", "running");
  write_column(m+1, u_L, "uL", "running");
  write_column(m+1, p_L, "pL", "running");
  write_column(m+1, rho_R, "rhoR", "running");
  write_column(m+1, u_R, "uR", "running");
  write_column(m+1, p_R, "pR", "running");
  write_column(m+1, D_rho_L, "DrhoL", "running");
  write_column(m+1, D_u_L, "DuL", "running");
  write_column(m+1, D_p_L, "DpL", "running");
  write_column(m+1, D_rho_R, "DrhoR", "running");
  write_column(m+1, D_u_R, "DuR", "running");
  write_column(m+1, D_p_R, "DpR", "running");//*/

    toc = clock();
    if(output)
    {
      printf("\nOutput at time (%d,%g).\n", itTIME, TIME[itTIME]);
      for(it_out = 0; it_out < OPT.nInitValue; ++it_out)
      {
	if(!(output_flag[it_out]))
	  continue;

	strcpy(add_out, add_mkdir);
	strcat(add_out, OPT.addInitValue[it_out]);
	sprintf(strIDX, "_%04d", itTIME);
	strcat(add_out, strIDX);
	strcat(add_out, ".txt\0");

	if((fp_out = fopen(add_out, "w")) == 0)
	{
	  sprintf(err_msg, "Cannot open solution output file: %s!\n", add_out);
	  exit(999);
	}
	for(j = 0; j < OPT.sizeInitValue[it_out]; ++j)
	  fprintf(fp_out, "%.18lf\t", DATA[it_out][j]);

	fclose(fp_out);
      }

      output = 0;
      ++itTIME;
    }
    runhist->current->time[1] = ((double)toc - (double)tic) / (double)CLOCKS_PER_SEC;
    current_cpu_time = runhist->current->time[1];
    sum_cpu_time += runhist->current->time[1];
  }
  k = k-1;

  if(check_runHist(runhist))
  {
    printf("The runhist->length is %d.\nBut the number of the runNodes are %d.\n\n", runhist->length, runhist->length - check_runHist(runhist));
    exit(100);
  }
  if(k - runhist->length)
  {
    printf("After %d steps of computation, the number of the runNodes are %d.\n\n", k, runhist->length);
    exit(100);
  }

  
  /* double S[m+1], rhoS, momS, eneS; */
  /* for(j=2;j<m-1;++j) */
  /* { */
  /*   rhoS = (3.5*(rho[j-1]+rho[j]) - 0.5*(rho[j-2]+rho[j+1]))/6.0; */
  /*   momS = (3.5*(mom[j-1]+mom[j]) - 0.5*(mom[j-2]+mom[j+1]))/6.0; */
  /*   eneS = (3.5*(ene[j-1]+ene[j]) - 0.5*(ene[j-2]+ene[j+1]))/6.0; */
  /*   S[j] = ((eneS - 0.5*momS*momS/rhoS)*(gamma-1.0)) / pow(rhoS,gamma); */
  /* } */
  /* strcpy(add_out, add_mkdir); */
  /* strcat(add_out, "s_0000.txt"); */
  /* if((fp_out = fopen(add_out, "w")) == 0) */
  /* { */
  /*   sprintf(err_msg, "Cannot open solution output file: %s!\n", add_out); */
  /*   exit(999); */
  /* } */
  /* for(j = 0; j < m+1; ++j) */
  /*   fprintf(fp_out, "%.18lf\t", S[j]); */
  /* fclose(fp_out); */


  printf("The cost of CPU time for [%s] computing this\n", scheme);
  printf("problem to time %g with %d steps is %g seconds.\n", T, k, sum_cpu_time);
  printf("===========================\n");
//------------END OFQ THE MAIN LOOP-------------
  if(fp_tr0)
    fclose(fp_tr0);
  if(fp_tr1)
    fclose(fp_tr1);


  return k;
}




/*
mrl=0.0, mul=0.0, mpl=0.0, mrr=0.0, mur=0.0, mpr=0.0, mdrl=0.0, mdul=0.0, mdpl=0.0, mdrr=0.0, mdur=0.0, mdpr=0.0, mQ[6];
for(j=0; j < m+1; ++j)
{
if(fabs(D_rho_L2[j]-D_rho_L[j]) > fabs(mdrl))
{
  mdrl = D_rho_L2[j]-D_rho_L[j];
  JJ[0] = j;
}
if(fabs(D_u_L2[j]-D_u_L[j]) > fabs(mdul))
{
  mdul = D_u_L2[j]-D_u_L[j];
  JJ[1] = j;
}
if(fabs(D_p_L2[j]-D_p_L[j]) > fabs(mdpl))
{
  mdpl = D_p_L2[j]-D_p_L[j];
  JJ[2] = j;
}
if(fabs(rho_L2[j]-rho_L[j]) > fabs(mrl))
{
  mrl = rho_L2[j]-rho_L[j];
  J[0] = j;
}
if(fabs(u_L2[j]-u_L[j]) > fabs(mul))
{
  mul = u_L2[j]-u_L[j];
  J[1] = j;
}
if(fabs(p_L2[j]-p_L[j]) > fabs(mpl))
{
  mpl = p_L2[j]-p_L[j];
  J[2] = j;
}
if(fabs(D_rho_R2[j]-D_rho_R[j]) > fabs(mdrr))
{
  mdrr = D_rho_R2[j]-D_rho_R[j];
  JJ[3] = j;
}
if(fabs(D_u_R2[j]-  D_u_R[j]) > fabs(mdur))
{
  mdur = D_u_R2[j]-  D_u_R[j];
  JJ[4] = j;
}
if(fabs(D_p_R2[j]-  D_p_R[j]) > fabs(mdpr))
{
  mdpr = D_p_R2[j]-  D_p_R[j];
  JJ[5] = j;
}
if(fabs(rho_R2[j]-rho_R[j]) > fabs(mrr))
{
  mrr = rho_R2[j]-rho_R[j];
  J[3] = j;
}
if(fabs(u_R2[j]-  u_R[j]) > fabs(mur))
{
  mur = u_R2[j]-  u_R[j];
  J[4] = j;
}
if(fabs(p_R2[j]-  p_R[j]) > fabs(mpr))
{
  mpr = p_R2[j]-  p_R[j];
  J[5] = j;
}
}
mQ[0]=mrl;
mQ[1]=mul;
mQ[2]=mpl;
mQ[3]=mrr;
mQ[4]=mur;
mQ[5]=mpr;
mDQ[0]=mdrl;
mDQ[1]=mdul;
mDQ[2]=mdpl;
mDQ[3]=mdrr;
mDQ[4]=mdur;
mDQ[5]=mdpr;
*/
