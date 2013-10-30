/// LiVES - avformat plugin
// (c) G. Finch 2010 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details
//
// some code adapted from vlc (GPL v2 or higher)

#include "decplugin.h"


///////////////////////////////////////////////////////
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

/* ffmpeg header */
//#if defined(HAVE_LIBAVFORMAT_AVFORMAT_H)
//#   include <libavformat/avformat.h>
//#elif defined(HAVE_FFMPEG_AVFORMAT_H)
//#   include <ffmpeg/avformat.h>
//#endif

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/version.h>
#include <libavutil/samplefmt.h>
#include <libavutil/mathematics.h>

#include "avformat_decoder.h"

const char *plugin_version="LiVES avformat decoder version 1.0";

static pthread_mutex_t avcodec_mutex=PTHREAD_MUTEX_INITIALIZER;

#if !HAVE_AVCODEC_OPEN2
#define avcodec_open2(a, b, c) avcodec_open(a, b)
#endif


#define FAST_SEEK_LIMIT 100000 // microseconds

static void lives_avcodec_lock(void) {
  pthread_mutex_lock(&avcodec_mutex);
}


static void lives_avcodec_unlock(void) {
  pthread_mutex_unlock(&avcodec_mutex);
}



static int stream_peek(int fd, unsigned char *str, size_t len) {
  off_t cpos=lseek(fd,0,SEEK_CUR); // get current posn 
  int rv= pread(fd,str,len,cpos); // read len bytes without changing cpos

  if (rv==-1) {
    fprintf(stderr,"err is %d\n",errno);
  }
  return rv;
}



static int pix_fmt_to_palette(enum PixelFormat pix_fmt, int *clamped) {
  if (clamped) *clamped=WEED_YUV_CLAMPING_CLAMPED;

  switch (pix_fmt) {
  case PIX_FMT_RGB24:
    return WEED_PALETTE_RGB24;
  case PIX_FMT_BGR24:
    return WEED_PALETTE_BGR24;
  case PIX_FMT_RGBA:
    return WEED_PALETTE_RGBA32;
  case PIX_FMT_BGRA:
    return WEED_PALETTE_BGRA32;
  case PIX_FMT_ARGB:
    return WEED_PALETTE_ARGB32;
  case PIX_FMT_YUV444P:
    return WEED_PALETTE_YUV444P;
  case PIX_FMT_YUV422P:
    return WEED_PALETTE_YUV422P;
  case PIX_FMT_YUV420P:
    return WEED_PALETTE_YUV420P;
  case PIX_FMT_YUYV422:
    return WEED_PALETTE_YUYV;
  case PIX_FMT_UYVY422:
    return WEED_PALETTE_UYVY;
  case PIX_FMT_UYYVYY411:
    return WEED_PALETTE_YUV411;
  case PIX_FMT_GRAY8:
  case PIX_FMT_Y400A:
    return WEED_PALETTE_A8;
  case PIX_FMT_MONOWHITE:
  case PIX_FMT_MONOBLACK:
    return WEED_PALETTE_A1;
  case PIX_FMT_YUVJ422P:
    if (clamped) *clamped=WEED_YUV_CLAMPING_UNCLAMPED;
    return WEED_PALETTE_YUV422P;
  case PIX_FMT_YUVJ444P:
    if (clamped) *clamped=WEED_YUV_CLAMPING_UNCLAMPED;
    return WEED_PALETTE_YUV444P;
  case PIX_FMT_YUVJ420P:
    if (clamped) *clamped=WEED_YUV_CLAMPING_UNCLAMPED;
    return WEED_PALETTE_YUV420P;

  default:
    fprintf(stderr,"unknown pix_fmt %d\n",pix_fmt);
    return WEED_PALETTE_END;
  }
}



void get_samps_and_signed(enum AVSampleFormat sfmt, int *asamps, boolean *asigned) {
  *asamps=av_get_bytes_per_sample(sfmt)*8;

  switch (sfmt) {
  case AV_SAMPLE_FMT_U8:
    *asigned=FALSE;
    break;
  default:
    *asigned=TRUE;
  }
}

static int64_t get_current_ticks(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec*1000000+tv.tv_usec;
}


static int64_t get_real_last_frame(lives_clip_data_t *cdata) {
  int64_t diff=0;
  int64_t olframe=(cdata->nframes==0?0:cdata->nframes-1),lframe=olframe;
  int64_t timex,tottime=0;

  int nseeks=0;

  while (1) {
    nseeks++;
#ifdef DEBUG
    fprintf(stderr,"will check frame %ld of %ld...",lframe+1,cdata->nframes);
#endif
    
    timex=get_current_ticks();
    if (!get_frame(cdata,lframe,NULL,0,NULL)) {
      timex=get_current_ticks()-timex;
      tottime+=timex;
#ifdef DEBUG
      fprintf(stderr,"no (%ld)\n",timex);
#endif
      if (diff==1) {
	tottime/=nseeks;
	//#ifdef DEBUG
	fprintf(stderr,"av seek was %ld\n",tottime);
	//#endif
	if (tottime<=FAST_SEEK_LIMIT)
	  cdata->seek_flag=LIVES_SEEK_FAST;
	return lframe-1;
      }
      if (diff<0) diff*=2;
      else if (diff==0) diff=-1;
      else diff/=2;
      lframe=olframe+diff;
    }
    else {
      timex=get_current_ticks()-timex;
      tottime+=timex;
#ifdef DEBUG
      fprintf(stderr,"yes ! (%ld)\n",timex);
#endif
      if (diff<2&&diff>-4) {
	tottime/=nseeks;
	//#ifdef DEBUG
	fprintf(stderr,"av seek was %ld\n",tottime);
	//#endif
	if (tottime<=FAST_SEEK_LIMIT)
	  cdata->seek_flag=LIVES_SEEK_FAST;
	return lframe;
      }
      diff/=2;
      if (diff<0) diff=-diff/2;
      olframe=lframe;
      lframe+=diff;
    }
  }
}


static boolean attach_stream(lives_clip_data_t *cdata) {
  // open the file and get a handle
  lives_av_priv_t *priv=cdata->priv;
  AVProbeData   pd;
  AVInputFormat *fmt;

  AVCodec *vdecoder;

  int i;

  int64_t i_start_time=0;

  if ((priv->fd=open(cdata->URI,O_RDONLY))==-1) {
    fprintf(stderr, "avformat_decoder: unable to open %s\n",cdata->URI);
    return FALSE;
  }
  
  pd.filename=cdata->URI;

  pd.buf=calloc(128,32);

  if ((pd.buf_size=stream_peek(priv->fd,pd.buf,2261))<2261) {
    fprintf(stderr, "couldn't peek stream %d\n",pd.buf_size );
    return FALSE;
  }

  if( !( fmt = av_probe_input_format( &pd, 1 ) ) ) {
    fprintf(stderr, "couldn't guess format\n" );
    return FALSE;
  }

  free(pd.buf);

  if (!strcmp( fmt->name, "redir" ) ||
      !strcmp( fmt->name, "sdp" ) ) {
    return FALSE;
  }

  /* Don't trigger false alarms on bin files */
  if( ! strcmp( fmt->name, "psxstr" ) ) {
    int i_len;
    i_len = strlen( cdata->URI );
    if( i_len < 4 ) return FALSE;
      
    if( strcasecmp( &cdata->URI[i_len - 4], ".str" ) &&
	strcasecmp( &cdata->URI[i_len - 4], ".xai" ) &&
	strcasecmp( &cdata->URI[i_len - 3], ".xa" ) ) {
      return FALSE;
    }
  }
  
  fprintf( stderr, "avformat detected format: %s\n", fmt->name );

  priv->fmt=fmt;
  priv->ic=NULL;
  /*  priv->io_buffer_size = 32768;
      priv->io_buffer = malloc( priv->io_buffer_size );

      init_put_byte( &priv->io, priv->io_buffer, priv->io_buffer_size,
      0, priv, IORead, NULL, IOSeek );*/
  
  /* Open it */
  if( avformat_open_input( &priv->ic, cdata->URI, priv->fmt, NULL ) ) {
    fprintf( stderr, "avformat_open_input failed\n" );
    return FALSE;
  }
  
  if (!priv->ic->pb->seekable) {
    fprintf( stderr, "avformat stream non-seekable\n" );
    return FALSE;
  }
  
  lives_avcodec_lock(); /* avformat calls avcodec behind our back!!! */
  if( av_find_stream_info( priv->ic ) < 0 ) {
    fprintf( stderr, "av_find_stream_info failed\n" );
  }
  lives_avcodec_unlock();


  
  // fill cdata

  cdata->nclips=1;


  cdata->interlace=LIVES_INTERLACE_NONE;  // TODO - this is set per frame

  cdata->par=1.;
  cdata->offs_x=0;
  cdata->offs_y=0;
  cdata->frame_width=cdata->width=0;
  cdata->frame_height=cdata->height=0;
  cdata->nframes=0;
  cdata->fps=0.;

  sprintf(cdata->container_name,"%s",priv->ic->iformat->name);

  memset(cdata->video_name,0,1);
  memset(cdata->audio_name,0,1);

  cdata->achans=cdata->asamps=cdata->arate=0;

  cdata->asigned=FALSE;
  cdata->ainterleaf=TRUE;

  for (i = 0; i < priv->ic->nb_streams; i++ ) {
    AVStream *s = priv->ic->streams[i];
    AVCodecContext *cc = s->codec;
 
    // vlc_fourcc_t fcc;
    //const char *psz_type = "unknown";

    /*      if( !GetVlcFourcc( cc->codec_id, NULL, &fcc, NULL ) )
	    fcc = VLC_FOURCC( 'u', 'n', 'd', 'f' );*/

    switch( cc->codec_type ) {
    case AVMEDIA_TYPE_AUDIO:
      if (priv->astream!=-1) {
	fprintf(stderr, "Warning - got multiple audio streams\n");
	break;
      }

      cdata->achans = cc->channels;
      cdata->arate = cc->sample_rate;

#if LIBAVCODEC_VERSION_INT < ((52<<16)+(0<<8)+0)
      cdata->asamps = cc->bits_per_sample;
#else
      cdata->asamps = cc->bits_per_coded_sample;
#endif

      get_samps_and_signed(cc->sample_fmt,&cdata->asamps,&cdata->asigned);

      sprintf(cdata->audio_name,"%s",cc->codec_name);

      priv->astream=i;
      break;


    case AVMEDIA_TYPE_VIDEO:
      if (priv->vstream!=-1) {
	fprintf(stderr, "Warning - got multiple video streams\n");
	break;
      }

      vdecoder=avcodec_find_decoder(cc->codec_id);
      avcodec_open2(cc, vdecoder, NULL);

      cdata->frame_width=cdata->width = cc->width;
      cdata->frame_height=cdata->height = cc->height;


      /*if( cc->palctrl )
	{
	fmt.video.p_palette = malloc( sizeof(video_palette_t) );
	*fmt.video.p_palette = *(video_palette_t *)cc->palctrl;
	}*/
	    
	    
      cdata->YUV_subspace=WEED_YUV_SUBSPACE_YCBCR;
      cdata->YUV_sampling=WEED_YUV_SAMPLING_DEFAULT;

      cdata->palettes[0]=pix_fmt_to_palette(cc->pix_fmt,&cdata->YUV_clamping);
      cdata->palettes[1]=WEED_PALETTE_END;

      sprintf(cdata->video_name,"%s",cc->codec_name);

      cdata->par=cc->sample_aspect_ratio.num/cc->sample_aspect_ratio.den;
      if (cdata->par==0) cdata->par=1;

      priv->fps_avg=FALSE;
      cdata->fps=s->time_base.den/s->time_base.num;
      if (cdata->fps>=1000.||cdata->fps==0.) {
	cdata->fps=(float)s->avg_frame_rate.num/(float)s->avg_frame_rate.den;
	priv->fps_avg=TRUE;
	if (isnan(cdata->fps)) {
	  cdata->fps=s->time_base.den/s->time_base.num;
	  priv->fps_avg=FALSE;
	}
      }

      priv->ctx=cc;
	    
      if (priv->ctx->ticks_per_frame==2) {
	// needs checking
	cdata->interlace=LIVES_INTERLACE_BOTTOM_FIRST;
      }

#ifdef DEBUG
      fprintf(stderr,"fps is %.4f %d %d %d %d %d\n",cdata->fps,s->time_base.den,s->time_base.num,cc->time_base.den,cc->time_base.num,priv->ctx->ticks_per_frame);
#endif

      if (priv->ic->duration!=(int64_t)AV_NOPTS_VALUE)
	cdata->nframes=((double)priv->ic->duration/(double)AV_TIME_BASE * cdata->fps -  .5);
      if ((cdata->nframes==0||cdata->fps==1000.)&&s->nb_frames>1) cdata->nframes=s->nb_frames;

      priv->vstream=i;

      break;

      /*        case CODEC_TYPE_SUBTITLE:



		if( strncmp( p_sys->ic->iformat->name, "matroska", 8 ) == 0 &&
                cc->codec_id == CODEC_ID_DVD_SUBTITLE &&
                cc->extradata != NULL &&
                cc->extradata_size > 0 )
		{
                char *psz_start;
                char *psz_buf = malloc( cc->extradata_size + 1);
                if( psz_buf != NULL )
                {
		memcpy( psz_buf, cc->extradata , cc->extradata_size );
		psz_buf[cc->extradata_size] = '\0';

		psz_start = strstr( psz_buf, "size:" );
		if( psz_start &&
		vobsub_size_parse( psz_start,
		&fmt.subs.spu.i_original_frame_width,
		&fmt.subs.spu.i_original_frame_height ) == VLC_SUCCESS )
		{
		msg_Dbg( p_demux, "original frame size: %dx%d",
		fmt.subs.spu.i_original_frame_width,
		fmt.subs.spu.i_original_frame_height );
		}
		else
		{
		msg_Warn( p_demux, "reading original frame size failed" );
		}

		psz_start = strstr( psz_buf, "palette:" );
		if( psz_start &&
		vobsub_palette_parse( psz_start, &fmt.subs.spu.palette[1] ) == VLC_SUCCESS )
		{
		fmt.subs.spu.palette[0] =  0xBeef;
		msg_Dbg( p_demux, "vobsub palette read" );
		}
		else
		{
		msg_Warn( p_demux, "reading original palette failed" );
		}
		free( psz_buf );
                }
		}

		psz_type = "subtitle";
		break;
      */

      //        default:

      /*
	#ifdef HAVE_FFMPEG_CODEC_ATTACHMENT
	if( cc->codec_type == CODEC_TYPE_ATTACHMENT )
	{
	input_attachment_t *p_attachment;
	psz_type = "attachment";
	if( cc->codec_id == CODEC_ID_TTF )
	{
	p_attachment = vlc_input_attachment_New( s->filename, "application/x-truetype-font", NULL,
	cc->extradata, (int)cc->extradata_size );
	TAB_APPEND( p_sys->i_attachments, p_sys->attachments, p_attachment );
	}
	else msg_Warn( p_demux, "unsupported attachment type in ffmpeg demux" );
	}
	break;
	#endif

	if( cc->codec_type == CODEC_TYPE_DATA )
	psz_type = "data";

	msg_Warn( p_demux, "unsupported track type in ffmpeg demux" );
	break;
        }
        fmt.psz_language = strdup( s->language );
        if( s->disposition & AV_DISPOSITION_DEFAULT )
	fmt.i_priority = 1000;

	#ifdef HAVE_FFMPEG_CODEC_ATTACHMENT
        if( cc->codec_type != CODEC_TYPE_ATTACHMENT )
	#endif
        {
	const bool    b_ogg = !strcmp( p_sys->fmt->name, "ogg" );
	const uint8_t *p_extra = cc->extradata;
	unsigned      i_extra  = cc->extradata_size;

	if( cc->codec_id == CODEC_ID_THEORA && b_ogg )
	{
	unsigned pi_size[3];
	void     *pp_data[3];
	unsigned i_count;
	for( i_count = 0; i_count < 3; i_count++ )
	{
	if( i_extra < 2 )
	break;
	pi_size[i_count] = GetWBE( p_extra );
	pp_data[i_count] = (uint8_t*)&p_extra[2];
	if( i_extra < pi_size[i_count] + 2 )
	break;

	p_extra += 2 + pi_size[i_count];
	i_extra -= 2 + pi_size[i_count];
	}
	if( i_count > 0 && xiph_PackHeaders( &fmt.i_extra, &fmt.p_extra,
	pi_size, pp_data, i_count ) )
	{
	fmt.i_extra = 0;
	fmt.p_extra = NULL;
	}
	}
	else if( cc->codec_id == CODEC_ID_SPEEX && b_ogg )
	{
	uint8_t p_dummy_comment[] = {
	0, 0, 0, 0,
	0, 0, 0, 0,
	};
	unsigned pi_size[2];
	void     *pp_data[2];

	pi_size[0] = i_extra;
	pp_data[0] = (uint8_t*)p_extra;

	pi_size[1] = sizeof(p_dummy_comment);
	pp_data[1] = p_dummy_comment;

	if( pi_size[0] > 0 && xiph_PackHeaders( &fmt.i_extra, &fmt.p_extra,
	pi_size, pp_data, 2 ) )
	{
	fmt.i_extra = 0;
	fmt.p_extra = NULL;
	}
	}
	else if( cc->extradata_size > 0 )
	{
	fmt.p_extra = malloc( i_extra );
	if( fmt.p_extra )
	{
	fmt.i_extra = i_extra;
	memcpy( fmt.p_extra, p_extra, i_extra );
	}
	}
        }
        es = es_out_Add( p_demux->out, &fmt );
        if( s->disposition & AV_DISPOSITION_DEFAULT )
	es_out_Control( p_demux->out, ES_OUT_SET_ES_DEFAULT, es );
        es_format_Clean( &fmt );

        msg_Dbg( p_demux, "adding es: %s codec = %4.4s",
	psz_type, (char*)&fcc );
        TAB_APPEND( p_sys->i_tk, p_sys->tk, es );
      */
    default:
      break;

    }
  }


  if( priv->ic->start_time != (int64_t)AV_NOPTS_VALUE )
    i_start_time = priv->ic->start_time * 1000000 / AV_TIME_BASE;




  fprintf( stderr, "AVFormat supported stream\n" );
  fprintf( stderr, "    - format = %s (%s)\n",
	   priv->fmt->name, priv->fmt->long_name );
  fprintf( stderr, "    - start time = %"PRId64"\n", i_start_time );
  fprintf( stderr, "    - duration = %"PRId64"\n",
	   ( priv->ic->duration != (int64_t)AV_NOPTS_VALUE ) ?
	   priv->ic->duration * 1000000 / AV_TIME_BASE : -1 );



#ifdef HAVE_FFMPEG_CHAPTERS
  /*    if( p_sys->ic->nb_chapters > 0 )
        p_sys->p_title = vlc_input_title_New();
	for( i = 0; i < p_sys->ic->nb_chapters; i++ )
	{
        seekpoint_t *s = vlc_seekpoint_New();

        if( p_sys->ic->chapters[i]->title )
        {
	s->psz_name = strdup( p_sys->ic->chapters[i]->title );
	EnsureUTF8( s->psz_name );
	msg_Dbg( p_demux, "    - chapter %d: %s", i, s->psz_name );
        }
        s->i_time_offset = p_sys->ic->chapters[i]->start * 1000000 *
	p_sys->ic->chapters[i]->time_base.num /
	p_sys->ic->chapters[i]->time_base.den -
	(i_start_time != -1 ? i_start_time : 0 );
        TAB_APPEND( p_sys->p_title->i_seekpoint, p_sys->p_title->seekpoint, s );
	}*/
#endif

  priv->last_frame=-1;
  priv->black_fill=FALSE;

  return TRUE;
}



static void detach_stream (lives_clip_data_t *cdata) {
  // close the file, free the decoder
  lives_av_priv_t *priv=cdata->priv;
  close(priv->fd);

  // will close and free the context
  if (priv->ic !=NULL) av_close_input_file(priv->ic);

  priv->ctx=NULL;

  if (priv->pFrame!=NULL) {
    av_free(priv->pFrame);
    priv->pFrame=NULL;
  }

  priv->ic=NULL;

  priv->astream=-1;
  priv->vstream=-1;

}


//////////////////////////////////////////
// std functions



const char *module_check_init(void) {
  lives_avcodec_lock();
  av_register_all(); /* Can be called several times */
  lives_avcodec_unlock();
  return NULL;
}


const char *version(void) {
  return plugin_version;
}



static lives_clip_data_t *init_cdata (void) {
  lives_av_priv_t *priv;
  lives_clip_data_t *cdata=(lives_clip_data_t *)malloc(sizeof(lives_clip_data_t));
  
  cdata->palettes=malloc(2*sizeof(int));
  
  cdata->palettes[1]=WEED_PALETTE_END;
  
  cdata->URI=NULL;
  
  cdata->priv=priv=malloc(sizeof(lives_av_priv_t));

  priv->fd=-1;

  priv->ic=NULL;

  priv->astream=-1;
  priv->vstream=-1;

  cdata->seek_flag=0;

  priv->ctx=NULL;
  priv->pFrame=NULL;

  cdata->sync_hint=SYNC_HINT_AUDIO_PAD_START;

  return cdata;
}






lives_clip_data_t *get_clip_data(const char *URI, lives_clip_data_t *cdata) {
  // the first time this is called, caller should pass NULL as the cdata
  // subsequent calls to this should re-use the same cdata

  // if the host wants a different URI or a different current_clip, this must be called again with the same
  // cdata as the second parameter
  int64_t real_frames;

  lives_av_priv_t *priv;

  if (cdata!=NULL&&cdata->current_clip>0) {
    // currently we only support one clip per container

    clip_data_free(cdata);
    return NULL;
  }

  if (cdata==NULL) {
    cdata=init_cdata();
  }

  if (cdata->URI==NULL||strcmp(URI,cdata->URI)) {
    if (cdata->URI!=NULL) {
      detach_stream(cdata);
      free(cdata->URI);
    }
    cdata->URI=strdup(URI);
    if (!attach_stream(cdata)) {
      free(cdata->URI);
      cdata->URI=NULL;
      clip_data_free(cdata);
      return NULL;
    }
    cdata->current_palette=cdata->palettes[0];
    cdata->current_clip=0;
    priv=cdata->priv;
    priv->last_frame=1000000000;
    priv->pFrame=NULL;
  }

  real_frames=get_real_last_frame(cdata)+1;

  if (cdata->nframes>100&&real_frames<cdata->nframes*.75) {
    fprintf(stderr,"avformat_decoder: ERROR - could only seek to %ld frames out of %ld\navformat_decoder: I will pass on this file as it may be broken.\n",
	    real_frames,cdata->nframes);
    detach_stream(cdata);
    free(cdata->URI);
    cdata->URI=NULL;
    clip_data_free(cdata);
    return NULL;
  }

  priv=cdata->priv;

  if (priv->fps_avg&&cdata->nframes>1) {
    //cdata->fps=cdata->fps*(cdata->nframes-1.)/(float)real_frames;
  }

  cdata->nframes=real_frames;

  if (cdata->seek_flag==0)
    cdata->seek_flag=LIVES_SEEK_NEEDS_CALCULATION;

  av_free(priv->pFrame);
  priv->pFrame=NULL;

  return cdata;
}



static size_t write_black_pixel(unsigned char *idst, int pal, int npixels, int y_black) {
  unsigned char *dst=idst;
  register int i;

  for (i=0;i<npixels;i++) {
    switch (pal) {
    case WEED_PALETTE_RGBA32:
    case WEED_PALETTE_BGRA32:
      dst[0]=dst[1]=dst[2]=0;
      dst[3]=255;
      dst+=4;
      break;
    case WEED_PALETTE_ARGB32:
      dst[1]=dst[2]=dst[3]=0;
      dst[0]=255;
      dst+=4;
      break;
    case WEED_PALETTE_UYVY8888:
      dst[1]=dst[3]=y_black;
      dst[0]=dst[2]=128;
      dst+=4;
      break;
    case WEED_PALETTE_YUYV8888:
      dst[0]=dst[2]=y_black;
      dst[1]=dst[3]=128;
      dst+=4;
      break;
    case WEED_PALETTE_YUV888:
      dst[0]=y_black;
      dst[1]=dst[2]=128;
      dst+=3;
      break;
    case WEED_PALETTE_YUVA8888:
      dst[0]=y_black;
      dst[1]=dst[2]=128;
      dst[3]=255;
      dst+=4;
      break;
    case WEED_PALETTE_YUV411:
      dst[0]=dst[3]=128;
      dst[1]=dst[2]=dst[4]=dst[5]=y_black;
      dst+=6;
    default: break;
    }
  }
  return idst-dst;
}



// tune this so small jumps forward are efficient
#define JUMP_FRAMES 64

boolean get_frame(const lives_clip_data_t *cdata, int64_t tframe, int *rowstrides, int height, void **pixel_data) {
  // seek to frame, and return pixel_data

  // tframe starts at 0

  lives_av_priv_t *priv=cdata->priv;
  double time;

  AVStream *s = priv->ic->streams[priv->vstream];
  AVCodecContext *cc = s->codec;

  int64_t target_pts, MyPts, seek_target=10000000000;

  unsigned char *dst, *src;
  unsigned char black[4]={0,0,0,255};

  boolean hit_target=FALSE;

  int gotFrame;
  int p,i;
  int xheight=cdata->frame_height,pal=cdata->current_palette,nplanes=1,dstwidth=cdata->width,psize=1;
  int btop=cdata->offs_y,bbot=xheight-1-btop;
  int bleft=cdata->offs_x,bright=cdata->frame_width-cdata->width-bleft;
  int y_black=(cdata->YUV_clamping==WEED_YUV_CLAMPING_CLAMPED)?16:0;

  if (tframe<0||tframe>cdata->nframes||cdata->fps==0.) return FALSE;

  //cc->get_buffer = our_get_buffer;
  //cc->release_buffer = our_release_buffer;

  if (pixel_data!=NULL) {
    
    // calc frame width and height, including any border
    
    if (pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P||pal==WEED_PALETTE_YUV444P) {
      nplanes=3;
      black[0]=y_black;
      black[1]=black[2]=128;
    }
    else if (pal==WEED_PALETTE_YUVA4444P) {
      nplanes=4;
      black[0]=y_black;
      black[1]=black[2]=128;
      black[3]=255;
    }
    
    if (pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24) psize=3;
    
    if (pal==WEED_PALETTE_RGBA32||pal==WEED_PALETTE_BGRA32||pal==WEED_PALETTE_ARGB32||pal==WEED_PALETTE_UYVY8888||
	pal==WEED_PALETTE_YUYV8888||pal==WEED_PALETTE_YUV888||pal==WEED_PALETTE_YUVA8888) psize=4;
    
    if (pal==WEED_PALETTE_YUV411) psize=6;
    
    if (pal==WEED_PALETTE_A1) dstwidth>>=3;
    
    dstwidth*=psize;
    
    if (cdata->frame_height > cdata->height && height == cdata->height) {
      // host ignores vertical border
      btop=0;
      xheight=cdata->height;
      bbot=xheight-1;
    }
    
    if (cdata->frame_width > cdata->width && rowstrides[0] < cdata->frame_width*psize) {
      // host ignores horizontal border
      bleft=bright=0;
    }
  }

  if (priv->pFrame==NULL||tframe!=priv->last_frame) {
    // same frame -> we reuse priv-pFrame;

#ifdef DEBUG
    fprintf(stderr,"pt a1 %d %ld\n",priv->last_frame,tframe);
#endif

    if (priv->pFrame!=NULL) av_free(priv->pFrame);
    priv->pFrame=NULL;

    time=(double)tframe/cdata->fps;
  
    target_pts = time * (double)AV_TIME_BASE;
  
    if (tframe < priv->last_frame || tframe-priv->last_frame > JUMP_FRAMES ) {
      // seek to new frame
      seek_target=av_rescale_q(target_pts, AV_TIME_BASE_Q, s->time_base);
      av_seek_frame( priv->ic, priv->vstream, seek_target, AVSEEK_FLAG_BACKWARD);
      avcodec_flush_buffers(cc);
      priv->black_fill=FALSE;
      MyPts=-1;
    }
    else {
      MyPts=(priv->last_frame+1.)/cdata->fps*(double)AV_TIME_BASE;
    }

    //cc->skip_frame=AVDISCARD_ALL;

    while (1) {
      boolean got_packet=FALSE;
      do {
	int ret;

	if (got_packet) av_free_packet(&priv->packet);
 
	ret=av_read_frame( priv->ic, &priv->packet );
	got_packet=TRUE;

#ifdef DEBUG
	fprintf(stderr,"ret was %d for tframe %ld\n",ret,tframe);
#endif
	if (ret<0) {
	  av_free_packet(&priv->packet);
	  priv->last_frame=tframe;
	  if (pixel_data==NULL) return FALSE;
	  priv->black_fill=TRUE;
	  goto framedone;
	}

      } while (priv->packet.stream_index!=priv->vstream);

      if (MyPts==-1) {
	MyPts = priv->packet.pts;
	MyPts = av_rescale_q( MyPts, s->time_base, AV_TIME_BASE_Q)-priv->ic->start_time;
      }

#ifdef DEBUG
      fprintf(stderr,"pt b1 %ld %ld %ld\n",MyPts,target_pts,seek_target);
#endif
      // decode any frames from this packet
      if (priv->pFrame==NULL) priv->pFrame=avcodec_alloc_frame();

#if LIBAVCODEC_VERSION_MAJOR >= 53
      avcodec_decode_video2( cc, priv->pFrame, &gotFrame, &priv->packet );
#else 
      avcodec_decode_video( cc, priv->pFrame, &gotFrame, priv->packet.data, priv->packet.size );
#endif
#ifdef DEBUG
      fprintf(stderr,"pt 1 %ld %d %ld\n",tframe,gotFrame,MyPts);
#endif

      av_free_packet(&priv->packet);

      if( MyPts >= target_pts - 100 ) hit_target=TRUE;

      if (hit_target&&gotFrame) break;

      // otherwise discard this frame
      if (gotFrame) {
	MyPts+=(double)AV_TIME_BASE/cdata->fps;
	av_free(priv->pFrame);
	priv->pFrame=NULL;
      }


    } while (!(hit_target&&gotFrame));
  }

 framedone:
    
  priv->last_frame=tframe;

  if (priv->pFrame==NULL||pixel_data==NULL) return TRUE;

  //height=cdata->height;

  if (priv->black_fill) btop=cdata->frame_height;

  for (p=0;p<nplanes;p++) {
    dst=pixel_data[p];
    src=priv->pFrame->data[p];

    for (i=0;i<xheight;i++) {
      if (i<btop||i>bbot) {
	// top or bottom border, copy black row
	if (pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P||pal==WEED_PALETTE_YUV444P||
	    pal==WEED_PALETTE_YUVA4444P||pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24) {
	  memset(dst,black[p],dstwidth+(bleft+bright)*psize);
	  dst+=dstwidth+(bleft+bright)*psize;
	}
	else dst+=write_black_pixel(dst,pal,dstwidth/psize+bleft+bright,y_black);
	continue;
      }

      if (bleft>0) {
	if (pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P||pal==WEED_PALETTE_YUV444P||
	    pal==WEED_PALETTE_YUVA4444P||pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24) {
	  memset(dst,black[p],bleft*psize);
	  dst+=bleft*psize;
	}
	else dst+=write_black_pixel(dst,pal,bleft,y_black);
      }

      memcpy(dst,src,dstwidth);
      dst+=dstwidth;

      if (bright>0) {
	if (pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P||pal==WEED_PALETTE_YUV444P||
	    pal==WEED_PALETTE_YUVA4444P||pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24) {
	  memset(dst,black[p],bright*psize);
	  dst+=bright*psize;
	}
	else dst+=write_black_pixel(dst,pal,bright,y_black);
      }

      src+=priv->pFrame->linesize[p];
    }
    if (p==0&&(pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P)) {
      dstwidth>>=1;
      bleft>>=1;
      bright>>=1;
    }
    if (p==0&&(pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P)) {
      xheight>>=1;
      btop>>=1;
      bbot>>=1;
    }
  }


  return TRUE;
}





void clip_data_free(lives_clip_data_t *cdata) {

  if (cdata->URI!=NULL) {
    detach_stream(cdata);
    free(cdata->URI);
  }

  free(cdata->priv);

  free(cdata->palettes);
  free(cdata);
}


void module_unload(void) {
  //fclose(nulfile);
}
