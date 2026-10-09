/* File version: 1.0.0 | Last updated: 2026-10-09 | Purpose: Shared RGB nearest-palette mapping. */
/* agent: gpt-6 | 2026-10-09 | Share nearest palette mapping across previews | v 1.0.0 | 7378f7 */
#ifndef PIXELPROCESS_PALETTE_MAPPING_H
#define PIXELPROCESS_PALETTE_MAPPING_H
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

typedef struct { uint16_t left[4096],right[4096];uint8_t axis[4096];uint16_t root,count;uint32_t colors[4096]; } PpPaletteTree;
/* Compare palette colors on the selected RGB axis; return signed ordering. */
static inline int pp_palette_compare(const PpPaletteTree*t,int a,int b,int axis){int shift=axis==0?16:axis==1?8:0;unsigned x=(t->colors[a]>>shift)&255u,y=(t->colors[b]>>shift)&255u;return x<y?-1:x>y?1:a-b;}
/* Select median in a palette-index range; return median index. */
static inline int pp_palette_select(PpPaletteTree*t,int*ids,int lo,int hi,int mid,int axis){for(;;){int pivot=ids[lo+(hi-lo)/2],i=lo,j=hi;while(i<=j){while(pp_palette_compare(t,ids[i],pivot,axis)<0)i++;while(pp_palette_compare(t,ids[j],pivot,axis)>0)j--;if(i<=j){int tmp=ids[i];ids[i++]=ids[j];ids[j--]=tmp;}}if(mid<=j)hi=j;else if(mid>=i)lo=i;else return ids[mid];}}
/* Build a balanced nearest-color index for opaque RGB palette entries; return void. */
static inline int pp_palette_build_range(PpPaletteTree*t,int*ids,int lo,int hi,int depth){if(lo>=hi)return -1;int axis=depth%3,mid=(lo+hi)/2,index=pp_palette_select(t,ids,lo,hi-1,mid,axis);t->axis[index]=(uint8_t)axis;t->left[index]=(uint16_t)(pp_palette_build_range(t,ids,lo,mid,depth+1)+1);t->right[index]=(uint16_t)(pp_palette_build_range(t,ids,mid+1,hi,depth+1)+1);return index;}
/* Build tree for packed 0xRRGGBB colors and count 1-4096; return bool. */
static inline int pp_palette_build(PpPaletteTree*t,const uint32_t*colors,size_t count){if(!t||!colors||count<1||count>4096)return 0;t->count=(uint16_t)count;int ids[4096];for(size_t i=0;i<count;i++){t->colors[i]=colors[i]&0xffffffu;ids[i]=(int)i;}t->root=(uint16_t)(pp_palette_build_range(t,ids,0,(int)count,0)+1);return 1;}
/* Search tree for nearest packed RGB color; return palette index. */
static inline void pp_palette_search(const PpPaletteTree*t,int encoded,uint32_t q,int*best,uint32_t*distance){if(!encoded)return;int node=encoded-1,axis=t->axis[node];uint32_t p=t->colors[node];int dr=(int)((q>>16)&255u)-(int)((p>>16)&255u),dg=(int)((q>>8)&255u)-(int)((p>>8)&255u),db=(int)(q&255u)-(int)(p&255u);uint32_t d=(uint32_t)(dr*dr+dg*dg+db*db);if(d<*distance||(d==*distance&&node<*best)){*distance=d;*best=node;}int qv=axis==0?(int)((q>>16)&255u):axis==1?(int)((q>>8)&255u):(int)(q&255u),pv=axis==0?(int)((p>>16)&255u):axis==1?(int)((p>>8)&255u):(int)(p&255u),first=qv<pv?t->left[node]:t->right[node],second=qv<pv?t->right[node]:t->left[node];pp_palette_search(t,first,q,best,distance);if((uint32_t)((qv-pv)*(qv-pv))<=*distance)pp_palette_search(t,second,q,best,distance);}
/* Find nearest palette index with palette-order tie breaking; return index. */
static inline int pp_palette_nearest(const PpPaletteTree*t,uint32_t rgb){int best=0;uint32_t distance=UINT32_MAX;pp_palette_search(t,t->root,rgb&0xffffffu,&best,&distance);return best;}
#endif
/* agent: gpt-6 | 2026-10-09 | Share nearest palette mapping across previews | v 1.0.0 | 7378f7 */
