// fg_bg_removal.c
// Weed plugin
// (c) G. Finch (salsaman) 2005
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#ifdef HAVE_SYSTEM_WEED
#include "weed/weed.h"
#include "weed/weed-palettes.h"
#include "weed/weed-effects.h"
#include "weed/weed-plugin.h"
#else
#include "../../libweed/weed.h"
#include "../../libweed/weed-palettes.h"
#include "../../libweed/weed-effects.h"
#include "../../libweed/weed-plugin.h"
#endif

///////////////////////////////////////////////////////////////////

static int num_versions=2; // number of different weed api versions supported
static int api_versions[]={131,100}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED
#include "weed/weed-utils.h" // optional
#include "weed/weed-plugin-utils.h" // optional
#else
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

/////////////////////////////////////////////////////////////

typedef struct {
  uint8_t *av_luma_data;
  unsigned int av_count;
  uint32_t fastrand_val;
} static_data;

#define ABS(a)           (((a) < 0) ? -(a) : (a))

/* precomputed tables */
#define FP_BITS 16

static int Y_R[256];
static int Y_G[256];
static int Y_B[256];

static int Cb_R[256];
static int Cb_G[256];
static int Cb_B[256];
static int Cr_R[256];
static int Cr_G[256];
static int Cr_B[256];
static int conv_RY_inited = 0;


static int myround(double n)
{
  if (n >= 0) 
    return (int)(n + 0.5);
  else
    return (int)(n - 0.5);
}


static void init_RGB_to_YCbCr_tables(void)
{
  int i;

  /*
   * Q_Z[i] =   (coefficient * i
   *             * (Q-excursion) / (Z-excursion) * fixed-pogint-factor)
   *
   * to one of each, add the following:
   *             + (fixed-pogint-factor / 2)         --- for rounding later
   *             + (Q-offset * fixed-pogint-factor)  --- to add the offset
   *             
   */
  for (i = 0; i < 256; i++) {
    Y_R[i] = myround(0.2100 * (double)i 
		     * 219.0 / 255.0 * (double)(1<<FP_BITS));
    Y_G[i] = myround(0.587 * (double)i 
		     * 219.0 / 255.0 * (double)(1<<FP_BITS));
    Y_B[i] = myround((0.114 * (double)i 
		      * 219.0 / 255.0 * (double)(1<<FP_BITS))
		     + (double)(1<<(FP_BITS-1))
		     + (16.0 * (double)(1<<FP_BITS)));

    Cb_R[i] = myround(-0.168736 * (double)i 
		      * 224.0 / 255.0 * (double)(1<<FP_BITS));
    Cb_G[i] = myround(-0.331264 * (double)i 
		      * 224.0 / 255.0 * (double)(1<<FP_BITS));
    Cb_B[i] = myround((0.500 * (double)i 
		       * 224.0 / 255.0 * (double)(1<<FP_BITS))
		      + (double)(1<<(FP_BITS-1))
		      + (128.0 * (double)(1<<FP_BITS)));

    Cr_R[i] = myround(0.500 * (double)i 
		      * 224.0 / 255.0 * (double)(1<<FP_BITS));
    Cr_G[i] = myround(-0.418688 * (double)i 
		      * 224.0 / 255.0 * (double)(1<<FP_BITS));
    Cr_B[i] = myround((-0.081312 * (double)i 
		       * 224.0 / 255.0 * (double)(1<<FP_BITS))
		      + (double)(1<<(FP_BITS-1))
		      + (128.0 * (double)(1<<FP_BITS)));
  }
  conv_RY_inited = 1;
}


static inline uint8_t 
calc_luma (uint8_t *pixel) {
  return (Y_R[pixel[2]] + Y_G[pixel[1]]+ Y_B[pixel[0]]) >> FP_BITS;
}


static inline uint32_t fastrand(static_data *sdata)
{
#define rand_a 1073741789L
#define rand_c 32749L

  return (sdata->fastrand_val= (rand_a*sdata->fastrand_val + rand_c));
}

static inline void 
make_black (uint8_t *pixel) {
  pixel[0]=pixel[1]=pixel[2]=(uint8_t)0;
}

int common_init(weed_plant_t *inst) {
  weed_plant_t *in_channel;
  int error,height,width;

  static_data *sdata=(static_data *)weed_malloc (sizeof(static_data));

  if(sdata == NULL ) return WEED_ERROR_MEMORY_ALLOCATION;

  in_channel=weed_get_plantptr_value(inst,"in_channels",&error);
  height=weed_get_int_value(in_channel,"height",&error);
  width=weed_get_int_value(in_channel,"width",&error);

  sdata->av_luma_data=(uint8_t *)weed_malloc(width*height*3);
  if(sdata->av_luma_data == NULL ) {
    weed_free(sdata);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  sdata->av_count=0;
  sdata->fastrand_val=0;

  weed_memset(sdata->av_luma_data,0,width*height*3);

  //
  //inst->num_in_parameters=1;
  //inst->in_parameters=malloc(sizeof(weed_parameter_t));
  //inst->in_parameters[0]=weed_plugin_info_integer_init("Threshold",64,0,255);
  //
  weed_set_voidptr_value(inst,"plugin_internal",sdata);

  return WEED_NO_ERROR;
}


int common_deinit (weed_plant_t *inst) {
  static_data *sdata;
  int error;

  sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);
  if (sdata != NULL) {
    weed_free (sdata->av_luma_data);
    weed_free (sdata);
  }
  return WEED_NO_ERROR;
}


int common_process (int type, weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  static_data *sdata;

  uint8_t luma;
  uint8_t av_luma;
  int bf;
  uint8_t luma_threshold=128;

  uint8_t *av_luma_data;

  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",&error),*in_param;

  unsigned char *src=weed_get_voidptr_value(in_channel,"pixel_data",&error);
  unsigned char *dest=weed_get_voidptr_value(out_channel,"pixel_data",&error);
  int width=weed_get_int_value(in_channel,"width",&error)*3;
  int height=weed_get_int_value(in_channel,"height",&error);
  int irowstride=weed_get_int_value(in_channel,"rowstrides",&error);
  int orowstride=weed_get_int_value(out_channel,"rowstrides",&error);
  unsigned char *end=src+height*irowstride;
  int inplace=(src==dest);
  register int j;

  in_param=weed_get_plantptr_value(inst,"in_parameters",&error);
  bf=weed_get_int_value(in_param,"value",&error);
  luma_threshold=(uint8_t)bf;

  sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);

  av_luma_data=sdata->av_luma_data;
  sdata->fastrand_val=timestamp&0x0000FFFF;

  for (;src<end;src+=irowstride) {
    for (j=0;j<width-2;j+=3) {

      luma=calc_luma (&src[j]);
      av_luma=(uint8_t)((double)luma/(double)sdata->av_count+(double)(av_luma_data[j/3]*sdata->av_count)/(double)(++sdata->av_count));
      av_luma_data[j/3]=av_luma;
      if (ABS(luma-av_luma)<(luma_threshold)) {
	switch (type) {
	case 1:
	  // fire-ish effect
	  dest[j]=(uint8_t)((uint8_t)((fastrand(sdata)&0x7f00)>>8)+(dest[j+1]=(uint8_t) ((fastrand(sdata)&0x7f00)>>8)));     //R & G
	  dest[j+2]=(uint8_t)0;                       //B
	  break;
	  //
	case 2:
	  // blue glow
	  dest[j]=dest[j+1]=(uint8_t)((fastrand(sdata)&0xff00)>>8);                                                 //R&G
	  dest[j+2]=(uint8_t)255;  //B
	  break;
	case 0:
	  // make moving things black
	  make_black (&dest[j]);
	  break;
	}
      }
      else {
	if (!inplace) weed_memcpy(&dest[j],&src[j],3);
      }}
    dest+=orowstride;
    av_luma_data+=width;
  }
  return WEED_NO_ERROR;
}


int t1_process (weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(0,inst,timestamp);
}

int t2_process (weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(1,inst,timestamp);
}

int t3_process (weed_plant_t *inst, weed_timecode_t timestamp) {
  return common_process(2,inst,timestamp);
}




weed_plant_t *weed_setup (weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);

  if (plugin_info!=NULL) {
    int palette_list[]={WEED_PALETTE_BGR24,WEED_PALETTE_RGB24,WEED_PALETTE_END};
    weed_plant_t *in_chantmpls[]={weed_channel_template_init("in channel 0",WEED_CHANNEL_REINIT_ON_SIZE_CHANGE,palette_list),NULL};
    weed_plant_t *out_chantmpls[]={weed_channel_template_init("out channel 0",WEED_CHANNEL_CAN_DO_INPLACE,palette_list),NULL};
    
    weed_plant_t *in_params[]={weed_integer_init("threshold","_Threshold",64,0,255),NULL};
    weed_plant_t *filter_class=weed_filter_class_init("fg_bg_removal type 1","salsaman",1,WEED_FILTER_HINT_IS_POINT_EFFECT,&common_init,&t1_process,&common_deinit,in_chantmpls,out_chantmpls,in_params,NULL);
    
    weed_plugin_info_add_filter_class (plugin_info,filter_class);

    // we must clone the arrays for the next filter
    filter_class=weed_filter_class_init("fg_bg_removal type 2","salsaman",1,WEED_FILTER_HINT_IS_POINT_EFFECT,&common_init,&t2_process,&common_deinit,weed_clone_plants(in_chantmpls),weed_clone_plants(out_chantmpls),weed_clone_plants(in_params),NULL);
    weed_plugin_info_add_filter_class (plugin_info,filter_class);

    // we must clone the arrays for the next filter
    filter_class=weed_filter_class_init("fg_bg_removal type 3","salsaman",1,WEED_FILTER_HINT_IS_POINT_EFFECT,&common_init,&t3_process,&common_deinit,weed_clone_plants(in_chantmpls),weed_clone_plants(out_chantmpls),weed_clone_plants(in_params),NULL);
    weed_plugin_info_add_filter_class (plugin_info,filter_class);
    
    weed_set_int_value(plugin_info,"version",package_version);
    init_RGB_to_YCbCr_tables();
  }

  return plugin_info;
}
