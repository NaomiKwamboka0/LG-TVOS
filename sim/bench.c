/* Render-time benchmark on this computer: milliseconds per frame for each
 * screen, including the live TV overlays drawn over every video frame. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "channels.h"
#include "platform.h"
#include "view.h"
static uint32_t px[VIEW_W * VIEW_H];
static double now(void){return (double)plat_monotonic_ms();}
int main(void){
  ft_canvas c; shell_config cfg; static shell s; racer r; penalty p; static garden g; uint32_t best[3]={0};
  int i, N=60; double t;
  canvas_init(&c, px, VIEW_W, VIEW_H, VIEW_W);
  live l; player_status ps;
  memset(&cfg,0,sizeof cfg); cfg.child_games=7; { static channel_list all; channels_defaults(&all); channels_group(&all,CH_TV,&cfg.channels); channels_group(&all,CH_SPORTS,&cfg.sports); } shell_init(&s,&cfg);
  t=now(); for(i=0;i<N;i++){s.row=1; s.col[1]=i%cfg.channels.n; view_shell(&c,&s,"20:01");} printf("home (focus moving)  %.2f ms/frame\n",(now()-t)/N);
  t=now(); for(i=0;i<N;i++){view_shell(&c,&s,"20:01");} printf("home (focus still)   %.2f ms/frame\n",(now()-t)/N);
  memset(&ps,0,sizeof ps); ps.state=PLAYER_PLAYING; ps.video_h=480; live_init(&l,cfg.channels.n,0,50);
  t=now(); for(i=0;i<N;i++){l.banner_ms=1; view_live(&c,&l,&ps,&s,&s.cfg.channels,"20:01",1);} printf("live + banner        %.2f ms/frame\n",(now()-t)/N);
  l.banner_ms=0; t=now(); for(i=0;i<N;i++){view_live(&c,&l,&ps,&s,&s.cfg.channels,"20:01",1);} printf("live, no overlay     %.2f ms/frame\n",(now()-t)/N);
  l.panel_open=1; t=now(); for(i=0;i<N;i++){l.panel_item=i%cfg.channels.n; view_live(&c,&l,&ps,&s,&s.cfg.channels,"20:01",1);} printf("live + channel list  %.2f ms/frame\n",(now()-t)/N);
  shell_open_settings(&s); t=now(); for(i=0;i<N;i++){s.item=i%9; view_shell(&c,&s,"20:01");} printf("settings  %.2f ms/frame\n",(now()-t)/N);
  racer_init(&r,1,RACER_MEDIUM,0); racer_key(&r,FT_OK); t=now(); for(i=0;i<N;i++){racer_tick(&r); view_racer(&c,&r,best,LANG_EN,"");} printf("racer     %.2f ms/frame\n",(now()-t)/N);
  penalty_init(&p,1,1); t=now(); for(i=0;i<N;i++) view_penalty(&c,&p,LANG_EN,""); printf("penalty   %.2f ms/frame\n",(now()-t)/N);
  garden_init(&g); t=now(); for(i=0;i<N;i++) view_garden(&c,&g,LANG_EN,""); printf("garden    %.2f ms/frame\n",(now()-t)/N);
  return 0;
}
