/* Render-time benchmark on the development computer (groundwork for P01/P03).
 * Not TV evidence: the TV is far slower and must be measured on the device. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "view.h"
static uint32_t px[VIEW_W * VIEW_H];
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1e3+t.tv_nsec/1e6;}
int main(void){
  ft_canvas c; shell_config cfg; static shell s; racer r; penalty p; static garden g; uint32_t best[3]={0};
  int i, N=60; double t;
  canvas_init(&c, px, VIEW_W, VIEW_H, VIEW_W);
  memset(&cfg,0,sizeof cfg); cfg.child_games=7; shell_init(&s,&cfg);
  t=now(); for(i=0;i<N;i++){s.focus=i%s.ntiles; view_shell(&c,&s,"20:01");} printf("home (focus moving)  %.2f ms/frame\n",(now()-t)/N);
  t=now(); for(i=0;i<N;i++){s.focus=0; view_shell(&c,&s,"20:01");} printf("home (focus still)   %.2f ms/frame\n",(now()-t)/N);
  shell_open_settings(&s); t=now(); for(i=0;i<N;i++){s.item=i%8; view_shell(&c,&s,"20:01");} printf("settings  %.2f ms/frame\n",(now()-t)/N);
  racer_init(&r,1,RACER_MEDIUM,0); racer_key(&r,FT_OK); t=now(); for(i=0;i<N;i++){racer_tick(&r); view_racer(&c,&r,best,LANG_EN,"");} printf("racer     %.2f ms/frame\n",(now()-t)/N);
  penalty_init(&p,1,1); t=now(); for(i=0;i<N;i++) view_penalty(&c,&p,LANG_EN,""); printf("penalty   %.2f ms/frame\n",(now()-t)/N);
  garden_init(&g); t=now(); for(i=0;i<N;i++) view_garden(&c,&g,LANG_EN,""); printf("garden    %.2f ms/frame\n",(now()-t)/N);
  return 0;
}
