
/*
 * Copyright (C) xunen <leixunen@gmail.com> and others.
 * Copyright (C) Leevid Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    size_t                start;
    size_t                datasize;
} ngx_flv_h264_tag_t;


typedef struct {
    u_char                signature[3];
    u_char                version;
    u_char                flags;
    u_char                headersize[4];
} ngx_flv_header_t;


typedef struct {
    ngx_int_t             haskeyframes;
    ngx_int_t             hasvideo;
    ngx_int_t             hasaudio;
    ngx_int_t             hasmetadata;
    ngx_int_t             hascuePoints;
    ngx_int_t             canseekToEnd;

    double                audiocodecid;
    double                audiosamplerate;
    double                audiodatarate;
    double                audiosamplesize;
    double                audiodelay;

    ngx_int_t             stereo;

    double                videocodecid;
    double                framerate;
    double                videodatarate;
    double                height;
    double                width;

    double                datasize;
    double                audiosize;
    double                videosize;
    double                filesize;

    double                lasttimestamp;
    double                lastvideoframetimestamp;
    double                lastkeyframetimestamp;
    double                lastkeyframelocation;

    ngx_int_t             keyframes;

    double               *filepositions;
    double               *times;
    double                duration;

    char                  metadatacreator[256];
    char                  creator[256];

    ngx_int_t             onmetadatalength;
    ngx_int_t             metadatasize;
    size_t                onlastsecondlength;
    size_t                lastsecondsize;
    ngx_int_t             hasLastsecond;
    ngx_int_t             lastsecondtagCount;
    size_t                onlastkeyframelength;
    size_t                lastkeyframesize;
    ngx_int_t             hasLastKeyframe;
} ngx_flv_meta_data_t;


typedef struct {
    u_char                type;
    u_char                datasize[3];
    u_char                timestamp[3];
    u_char                timestamp_ex;
    u_char                streamid[3];
} ngx_flv_tag_t;


#if 0
typedef struct {
    u_char                flags
} ngx_flv_audio_data_t;
#endif


typedef struct {
    u_char                flags;
} ngx_flv_video_data_t;


static char *ngx_http_tflv(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_sflv(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


static ngx_command_t  ngx_http_eflv_commands[] = {

    { ngx_string("tflv"),
        NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
        ngx_http_tflv,
        0,
        0,
        NULL },

    { ngx_string("sflv"),
      NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
      ngx_http_sflv,
      0,
      0,
      NULL },

    ngx_null_command
};


#define NGX_FLV_METADATALEN 327680


#define ngx_flv_get_32value(p)                                                \
    ( ((uint32_t) ((u_char *) (p))[0] << 24)                                  \
    + (           ((u_char *) (p))[1] << 16)                                  \
    + (           ((u_char *) (p))[2] << 8)                                   \
    + (           ((u_char *) (p))[3]) )

#define ngx_flv_get_24value(p)                                                \
    ( ((uint32_t) ((u_char *) (p))[0] << 16)                                  \
    + (           ((u_char *) (p))[1] << 8)                                   \
    + (           ((u_char *) (p))[2]) )


#define NGX_FLV_AUDIODATA           8
#define NGX_FLV_VIDEODATA           9
#define NGX_FLV_SCRIPTDATAOBJECT    18
#define NGX_FLV_H263VIDEOPACKET     2
#define NGX_FLV_SCREENVIDEOPACKET   3
#define NGX_FLV_VP6VIDEOPACKET      4
#define NGX_FLV_VP6ALPHAVIDEOPACKET 5
#define NGX_FLV_SCREENV2VIDEOPACKET 6
#define NGX_FLV_AVCVIDEOPACKET      7


static u_char  ngx_flv_header[] = "FLV\x1\x1\0\0\0\x9\0\0\0\x9";


static off_t
ngx_atoint(u_char *line, size_t n)
{
    off_t  value;

    if (n == 0) {
        return NGX_ERROR;
    }

    for (value = 0; n--; line++) {
        if (*line < '0' || *line > '9') {
            break;
        }

        value = value * 10 + (*line - '0');
    }

    if (value < 0) {
        return NGX_ERROR;

    } else {
        return value;
    }
}


static ngx_http_module_t  ngx_http_eflv_module_ctx = {
    NULL,                          /* preconfiguration */
    NULL,                          /* postconfiguration */

    NULL,                          /* create main configuration */
    NULL,                          /* init main configuration */

    NULL,                          /* create server configuration */
    NULL,                          /* merge server configuration */

    NULL,                          /* create location configuration */
    NULL                           /* merge location configuration */
};


ngx_module_t  ngx_http_eflv_module = {
    NGX_MODULE_V1,
    &ngx_http_eflv_module_ctx,     /* module context */
    ngx_http_eflv_commands,        /* module directives */
    NGX_HTTP_MODULE,               /* module type */
    NULL,                          /* init master */
    NULL,                          /* init module */
    NULL,                          /* init process */
    NULL,                          /* init thread */
    NULL,                          /* exit thread */
    NULL,                          /* exit process */
    NULL,                          /* exit master */
    NGX_MODULE_V1_PADDING
};


static void
ngx_flv_revert_int(char *s, const char *d, ngx_int_t len)
{
    ngx_int_t     i;
    for (i = len -1; i >= 0; i--) {
        *(s+i) = *d;
        d++;
    }
}


static void
ngx_flv_swap_duration(char *p, double value)
{
      union {
            u_char dc[8];
            double dd;
      } d;
      u_char b[8];

      d.dd = value;

      b[0] = d.dc[7];
      b[1] = d.dc[6];
      b[2] = d.dc[5];
      b[3] = d.dc[4];
      b[4] = d.dc[3];
      b[5] = d.dc[2];
      b[6] = d.dc[1];
      b[7] = d.dc[0];

      ngx_memcpy(p, b, 8);
}


static ngx_int_t
ngx_http_eflv_read_secondpass(char *flv, size_t streampos, double filesize, ngx_flv_h264_tag_t *tag)
{
    size_t datasize,  audiosize = 0;
    size_t  audiotags = 0;
    ngx_flv_tag_t *flvtag;

    tag->start = 0;

    for(;;) {

        if(streampos + sizeof(ngx_flv_tag_t) > filesize)
            break;

        flvtag = (ngx_flv_tag_t *)&flv[streampos];

        datasize = sizeof(ngx_flv_tag_t) + ngx_flv_get_24value(flvtag->datasize) + 4;

        if(streampos + datasize > filesize)
            break;

        if(flvtag->type == NGX_FLV_SCRIPTDATAOBJECT) {

            audiosize += ngx_flv_get_24value(flvtag->datasize);
            audiotags++;
            if (tag->start == 0) {
                tag->start = streampos;
                tag->datasize=datasize;
            }

            if (tag->start > 0) {
                return 1;
            }
        }
        streampos += datasize;

    }

    return 0;
}


static int
ngx_http_eflv_read_firstpass(char *flv,size_t streampos,size_t filesize, ngx_flv_h264_tag_t *tag, ngx_flv_h264_tag_t *audio)
{
    size_t datasize, videosize = 0, audiosize = 0;
    size_t videotags = 0, audiotags = 0;
    ngx_flv_tag_t *flvtag;
    //ngx_flv_audio_data_t    *flvaudio;
    ngx_flv_video_data_t    *flvvideo;

    tag->start = 0;
    audio->start = 0;
    for(;;) {
        if (streampos + sizeof(ngx_flv_tag_t) > filesize)
            break;

        flvtag = (ngx_flv_tag_t *)&flv[streampos];

        // TagHeader + TagData + PreviousTagSize
        datasize = sizeof(ngx_flv_tag_t) + ngx_flv_get_24value(flvtag->datasize) + 4;

        if(streampos + datasize > filesize)
            break;

        if(flvtag->type == NGX_FLV_AUDIODATA) {

            audiosize += ngx_flv_get_24value(flvtag->datasize);
            audiotags++;
            if (audio->start == 0) {
                audio->start = streampos;
                audio->datasize=datasize;
            }

            if (tag->start > 0) {
                return 1;
            }
        } else if (flvtag->type == NGX_FLV_VIDEODATA) {
            //flvmetadata.datasize += (double)datasize;
            //// datasize - PreviousTagSize
            //flvmetadata.videosize += (double)(datasize - 4);

            videosize += ngx_flv_get_24value(flvtag->datasize);
            videotags++;

            flvvideo = (ngx_flv_video_data_t *)&flv[streampos + sizeof(ngx_flv_tag_t)];


            switch(flvvideo->flags & 0xf) {
                case NGX_FLV_AVCVIDEOPACKET:
                    if (tag->start == 0) {
                        tag->start = streampos;
                        tag->datasize = datasize;
                    }

                    if (audio->start > 0) {
                        return 1;
                    }

                    break;
                default:
                    return 0;
            }

        }
        streampos += datasize;
    }

    return 0;
}


static void 
ntx_htt_sflv_metadata(ngx_int_t fd, double len, char *send_header_buf, char *send_metadata_buf, char *send_tH264VideoTag_buf, char *send_tH264AudioTag_buf, ngx_int_t *metadata_size, ngx_int_t *video_size, ngx_int_t *audio_size, size_t *metaend_pos, ngx_http_request_t *r)
{
    size_t  streampos;
    ngx_flv_h264_tag_t  tMetaDataTag, tH264VideoTag, tH264AudioTag; 
    bzero(&tMetaDataTag,sizeof(tMetaDataTag));
    bzero(&tH264VideoTag,sizeof(tH264VideoTag));
    bzero(&tH264AudioTag,sizeof(tH264AudioTag)); 
    ngx_flv_header_t *flvfileheader;
    char flv[NGX_FLV_METADATALEN] ={0}; 
    ngx_int_t n; 
    ngx_int_t i_read; 
    ngx_log_t    *log; 
    log = r->connection->log; 

    i_read = NGX_FLV_METADATALEN; 
    if (len< NGX_FLV_METADATALEN){
        i_read = len;	
    }
    lseek(fd, 0, SEEK_SET); 
    n = read((int)fd,flv,i_read); 

    if ( -1 == n){
        ngx_log_error(NGX_LOG_ALERT, log, ngx_errno,
                "ngx_sflv_read"  " \"%d\" failed", (int)fd);
    }

    flvfileheader = (ngx_flv_header_t *)flv;
    streampos = ngx_flv_get_32value(flvfileheader->headersize) + 4;
    ngx_memcpy(send_header_buf,flv,13);

    ngx_http_eflv_read_secondpass(flv, streampos, NGX_FLV_METADATALEN,&tMetaDataTag);
    *metadata_size = (ngx_int_t) tMetaDataTag.datasize;
    *metaend_pos = tMetaDataTag.start + tMetaDataTag.datasize;
    if (tMetaDataTag.start > 0) {
        if (tMetaDataTag.datasize <= NGX_FLV_METADATALEN){
            ngx_memcpy(send_metadata_buf,flv+tMetaDataTag.start,tMetaDataTag.datasize);
        }
    }

    ngx_http_eflv_read_firstpass(flv, streampos, NGX_FLV_METADATALEN,&tH264VideoTag,&tH264AudioTag);
    *video_size = (ngx_int_t)tH264VideoTag.datasize;
    *audio_size = (ngx_int_t)tH264AudioTag.datasize; 
    if (tH264VideoTag.start > 0) {
        ngx_memcpy(send_tH264VideoTag_buf, flv+tH264VideoTag.start,tH264VideoTag.datasize);
    }
    if (tH264AudioTag.start > 0) {
        ngx_memcpy(send_tH264AudioTag_buf, flv+tH264AudioTag.start,tH264AudioTag.datasize);
    } 
}


static double
ngx_http_flv_get_real_value(const char *times, const char *filepos, int num, const double value, int start_index, int *ret_index, double *ret_time)
{

        if ((times ==NULL) || (filepos == NULL) ||
        (ret_index == NULL) || (ret_time == NULL))
    {
                return -1;
        }

        int i  = 0,min_index=0, max_index=0, j=0;
        double temp = 0,min_value = 0;
        char rbuf[32] = {0};
        double file_pos = 0;

        for(i=0; ;i++)  {
                if(times[10+i*9] == 0)
                        break;
                if(i > num){
                        return -1;
                }
        }

        min_index = 0;
        max_index = num-1;
        j = 0;
        double timepos = 0;
	for(i=1;;i++) {

                if (((max_index - min_index) < 2) || (timepos == value) )
                        break;

                if(times[10+((min_index+max_index)/2+j)*9] != 0){

                        if(((min_index+max_index)/2+j) >= max_index){
                                break;
                        } else {
                                j += 1;
                                continue;
                        }
                }

                timepos = 0;
                ngx_flv_revert_int(rbuf, &times[10+((min_index+max_index)/2+j)*9+1], 8);
                ngx_memcpy(&timepos, &rbuf, 8);

                if(timepos < value){
                        min_index = (min_index+max_index)/2+j;

                } else if (timepos >=value){
                        max_index = (min_index+max_index)/2+j;
                }

                j = 0;
    }

        ngx_flv_revert_int(rbuf, &times[10+min_index*9+1],8);
        ngx_memcpy(&min_value, &rbuf, 8);
        ngx_flv_revert_int(rbuf, &times[10+max_index*9+1],8);
        ngx_memcpy(&temp, &rbuf, 8);
	
        if (value > temp){
                min_index = num - 1;
        }

        if ((start_index != -1) && start_index == min_index){
                min_index = min_index + 1;
                if (min_index>=num){
                        return -2;
                }
        }

        ngx_flv_revert_int(rbuf, &times[10+min_index*9+1],8);
        ngx_memcpy(&min_value, &rbuf, 8);

        *ret_time = min_value;

        double timepos2 = 0;
        ngx_flv_revert_int(rbuf, &filepos[18+min_index*9+1],8);
        ngx_memcpy(&timepos2, &rbuf, 8);
        file_pos = timepos2;

        if (start_index != -1 ){
                file_pos = file_pos - 1;
        }

        (*ret_index) = min_index ;
        return file_pos;
}


static char *
ngx_http_eflv_get_position(char *str_src, size_t str_len, size_t filesize, const char *str_dest)
{
    if (str_src == NULL || str_dest == NULL){
        return NULL;
    }

    char buf[NGX_FLV_METADATALEN] = {0};
    size_t  i = 0, size = 0;
    int iLen = 0;
    char *p = NULL;
    //int i_copy_len = 0;
    char *buf_tmp = buf;

    i = 0;
    size = 0;
    bzero(buf,sizeof(buf));
    if (filesize <=str_len){
        ngx_memcpy(buf,str_src,filesize);
    }
    else{
        ngx_memcpy(buf,str_src,str_len);
    }
    buf_tmp = buf;

    //澶..4K?..?..瀛..缁..绗?
      while(1){

        p =strstr(buf_tmp,str_dest);

        if (p == NULL){

            i = strlen(buf_tmp);
            buf_tmp = buf_tmp + i;
            size+=(i+1);
            iLen+=(i+1);

            //澶.. buf =  瀛..涓? + 0x0 + 瀛..涓?
            if(size >= str_len){
                return NULL;
            }

            //?..??0x0 
            buf_tmp = buf_tmp+1;
            //size++;
            //iLen ++;
        } else {
            //?惧.?.Щ?
            iLen = iLen + (p -buf_tmp)*sizeof(char);

            p = str_src+iLen;
            return p;
        }
    }
    return NULL;
}    


static int
ngx_http_eflv_time_drag_position(char *flv ,double *start,double *end,double filesize,ngx_int_t have_end, ngx_flv_meta_data_t *drag_FLVMetaData,double **key_times, double **key_filepos,ngx_http_request_t *r)
{
    double temp =0;
    double file_pos = 0;

    if ((flv == NULL) || (start == NULL) || (end == NULL) || (drag_FLVMetaData == NULL) ){
        return -1;
    }

    char *keyframes = ngx_http_eflv_get_position(flv, NGX_FLV_METADATALEN, filesize, "keyframes");
    if (keyframes == NULL){
        return -1;
    }

    char *times = ngx_http_eflv_get_position(keyframes, NGX_FLV_METADATALEN, filesize, "times");
    if ( times == NULL){
        return -1;
    }

    if (times[5] != 10){
        return -1;
    }

    char rbuf[32] = {0};
    unsigned int keyframes_num = 0;
    ngx_flv_revert_int(rbuf, &times[6],4);
    ngx_memcpy(&keyframes_num, &rbuf,4);

    char *filepositions = ngx_http_eflv_get_position(keyframes, NGX_FLV_METADATALEN,filesize,"filepositions");
    if(filepositions == NULL){
        return -1;
    }
    if (filepositions[13] != 10){
        return -1;
    }

    char *p_duration = ngx_http_eflv_get_position(flv, NGX_FLV_METADATALEN,filesize,"duration");
    if (p_duration == NULL){
        ngx_flv_revert_int(rbuf, &times[10+(keyframes_num-1)*9+1],8);
        ngx_memcpy(&temp, &rbuf, 8);
    } else {
        ngx_flv_revert_int(rbuf, &p_duration[9],8);
        ngx_memcpy(&temp, &rbuf, 8);
    }

    int index = 0;
    double start_time = 0, end_time = 0;
    int start_key_index = 0;
    double start_tmp = 0;

    if (*start > temp){
        *start = 0;
    }
    start_tmp = *start;

    file_pos = ngx_http_flv_get_real_value(times, filepositions,keyframes_num,*start,-1,&index,&start_time);
    if (file_pos == -1){
        return -1;
    }
    if (file_pos >0 && file_pos <=filesize){
        *start = file_pos;
    }
    if (index >=0){
        start_key_index = index;
    } else {
        start_key_index = 0;
    }

    if (have_end == 1){

        if (((start_tmp) > (*end)) || ((*end) > temp)){
            *end = filesize;
            drag_FLVMetaData->duration = temp - start_time ;

        } else {
            file_pos = ngx_http_flv_get_real_value(times, filepositions,keyframes_num,*end,start_key_index,&index,&end_time);
            if (file_pos == -1){
                return -1;
            } else if (file_pos == -2) {
                *end = filesize;
                drag_FLVMetaData->duration = temp - start_time ;
            } else if (file_pos >0 && file_pos <=filesize) {
                *end = file_pos;
                drag_FLVMetaData->duration = end_time - start_time ;
            } else {
                ;
            }
        }

    } else {
        drag_FLVMetaData->duration = temp - start_time ;
    }

    return 0;
}


static ngx_int_t
ntx_http_eflv_metadata(ngx_int_t fd,double *start , double *end,ngx_int_t have_end,double len,char *send_metadata_buf, char * send_tH264VideoTag_buf, char * send_tH264AudioTag_buf,ngx_int_t *video_size, ngx_int_t *audio_size,ngx_http_request_t *r)
{
    size_t  streampos;
    ngx_flv_h264_tag_t tH264VideoTag, tH264AudioTag;
    bzero(&tH264VideoTag,sizeof(tH264VideoTag));
    bzero(&tH264AudioTag,sizeof(tH264AudioTag));

    ngx_flv_meta_data_t drag_FLVMetaData;
    bzero(&drag_FLVMetaData, sizeof(ngx_flv_meta_data_t));
    ngx_flv_header_t *flvfileheader;
    ngx_flv_h264_tag_t tMetaDataTag;
    double *key_times = NULL;
    double *key_filepos =NULL;
    char flv[NGX_FLV_METADATALEN] = { 0 };
    ngx_int_t n;
    ngx_int_t i_read;
    ngx_log_t    *log;
    log = r->connection->log;
    //flv = mmap(NULL, len, PROT_READ, MAP_PRIVATE, (int)fd, 0);
    i_read = NGX_FLV_METADATALEN;
    if (len< NGX_FLV_METADATALEN){
        i_read = len;
    }
    n = read((int)fd,flv,i_read);
    if ( -1 == n){
        ngx_log_error(NGX_LOG_ALERT, log, ngx_errno,
                           "ngx_flv_read"  " \"%d\" failed", (int)fd);
    }

    flvfileheader = (ngx_flv_header_t *)flv;
    streampos = ngx_flv_get_32value(flvfileheader->headersize) + 4;

    ngx_http_eflv_time_drag_position(flv,start,end,len,have_end,&drag_FLVMetaData,&key_times,&key_filepos,r);
    ngx_http_eflv_read_secondpass(flv, streampos, len,&tMetaDataTag);

    if (tMetaDataTag.start > 0) {
            char *p_duration = NULL;
            if (tMetaDataTag.datasize <= NGX_FLV_METADATALEN){
                    ngx_memcpy(send_metadata_buf,flv+tMetaDataTag.start,tMetaDataTag.datasize);
                    p_duration= ngx_http_eflv_get_position(send_metadata_buf, NGX_FLV_METADATALEN, len, "duration");
                    if (p_duration != NULL){
                            p_duration = p_duration+9;
                            ngx_flv_swap_duration(p_duration,drag_FLVMetaData.duration);
                    }
            }
    }

    ngx_http_eflv_read_firstpass(flv, streampos, NGX_FLV_METADATALEN, &tH264VideoTag,&tH264AudioTag);
    *video_size = (ngx_int_t)tH264VideoTag.datasize;
    *audio_size = (ngx_int_t)tH264AudioTag.datasize;

    if (tH264VideoTag.start > 0) {
        ngx_memcpy(send_tH264VideoTag_buf, flv+tH264VideoTag.start,tH264VideoTag.datasize);
    }
    if (tH264AudioTag.start > 0) {
        ngx_memcpy(send_tH264AudioTag_buf, flv+tH264AudioTag.start,tH264AudioTag.datasize);
    }

    return (ngx_int_t)tMetaDataTag.datasize;
}


static ngx_int_t
ngx_http_sflv_handler(ngx_http_request_t *r)
{
    u_char                    *last;
    double                     start = 0, end = 0, len;
    size_t                     root, metaend_pos;
    ngx_int_t                  rc;
    ngx_uint_t                 level, i,j;
    ngx_str_t                  path, value;
    ngx_log_t                 *log;
    ngx_buf_t                 *b;
    ngx_chain_t                out[4];
    ngx_open_file_info_t       of;
    ngx_http_core_loc_conf_t  *clcf;
    char send_header_buf[16] = {0};
    char send_metadata_buf[NGX_FLV_METADATALEN] = {0};
    char send_tH264VideoTag_buf[NGX_FLV_METADATALEN] = {0};
    char send_tH264AudioTag_buf[NGX_FLV_METADATALEN] = {0};
    ngx_int_t  metadata_size =0;
    ngx_int_t  video_size =0;
    ngx_int_t  audio_size =0; 

    i= 0; 
    j= 0; 
    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    if (r->uri.data[r->uri.len - 1] == '/') {
        return NGX_DECLINED;
    }

    rc = ngx_http_discard_request_body(r);

    if (rc != NGX_OK) {
        return rc;
    }

    last = ngx_http_map_uri_to_path(r, &path, &root, 0);
    if (last == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    log = r->connection->log;

    path.len = last - path.data;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
            "http flv filename: \"%V\"", &path);

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    ngx_memzero(&of, sizeof(ngx_open_file_info_t));

    of.read_ahead = clcf->read_ahead;
    of.directio = clcf->directio;
    of.valid = clcf->open_file_cache_valid;
    of.min_uses = clcf->open_file_cache_min_uses;
    of.errors = clcf->open_file_cache_errors;
    of.events = clcf->open_file_cache_events;

    if (ngx_open_cached_file(clcf->open_file_cache, &path, &of, r->pool)
            != NGX_OK)
    {
        switch (of.err) {

            case 0:
                return NGX_HTTP_INTERNAL_SERVER_ERROR;

            case NGX_ENOENT:
            case NGX_ENOTDIR:
            case NGX_ENAMETOOLONG:

                level = NGX_LOG_ERR;
                rc = NGX_HTTP_NOT_FOUND;
                break;

            case NGX_EACCES:

                level = NGX_LOG_ERR;
                rc = NGX_HTTP_FORBIDDEN;
                break;

            default:

                level = NGX_LOG_CRIT;
                rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
                break;
        }

        if (rc != NGX_HTTP_NOT_FOUND || clcf->log_not_found) {
            ngx_log_error(level, log, of.err,
                    "%s \"%s\" failed", of.failed, path.data);
        }

        return rc;
    }

    if (!of.is_file) {

        if (ngx_close_file(of.fd) == NGX_FILE_ERROR) {
            ngx_log_error(NGX_LOG_ALERT, log, ngx_errno,
                    ngx_close_file_n " \"%s\" failed", path.data);
        }

        return NGX_DECLINED;
    }

    r->root_tested = !r->error_page;

    start = 0;
    len = of.size;


    if (ngx_http_arg(r, (u_char *) "start", 5, &value) == NGX_OK) {

        start = ngx_atoof(value.data, value.len);

        if (start > len){
            return NGX_DECLINED; 
        }

        if (start == NGX_ERROR) {
            start = 0;
        }

    }

    end = len;
    if (ngx_http_arg(r, (u_char *) "end", 3, &value) == NGX_OK) {

        end = ngx_atoof(value.data, value.len) + 1;
        if (end == NGX_ERROR || (end > len)) {
            end = len;
        }
    }


    log->action = "sending sflv to client";
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.last_modified_time = of.mtime;

    if (ngx_http_set_content_type(r) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (start != 0) { 
        b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
        if (b == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        b->pos = ngx_flv_header;
        b->last = ngx_flv_header + sizeof(ngx_flv_header) - 1;
        b->memory = 1;

        out[j].buf = b;
        out[j].next = &out[j+1];
        j++;

        ntx_htt_sflv_metadata(of.fd,len,send_header_buf,send_metadata_buf,send_tH264VideoTag_buf,send_tH264AudioTag_buf,&metadata_size,&video_size,&audio_size,&metaend_pos, r); 

        if (video_size != 0) { 
            b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));  
            b->pos = (u_char*)send_tH264VideoTag_buf;  
            b->last = (u_char*)(send_tH264VideoTag_buf + video_size);  
            b->memory = 1;  
            out[j].buf = b;
            out[j].next = &out[j+1];
            j++;
        }	

        if (audio_size != 0) {
            b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));  
            b->pos = (u_char*)send_tH264AudioTag_buf;  
            b->last = (u_char*)(send_tH264AudioTag_buf + audio_size);  
            b->memory = 1;  
            out[j].buf = b;
            out[j].next = &out[j+1];
            j++;
        }

        if (start > end) {
            end = len;	
        }

        r->headers_out.content_length_n =sizeof(ngx_flv_header) - 1 + end -start + video_size + audio_size;

        r->allow_ranges = 1;
        rc = ngx_http_send_header(r);
        if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
            return rc;
        }

        if ((start != len) && (start != end)) {	
            b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
            if (b == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

            b->file = ngx_pcalloc(r->pool, sizeof(ngx_file_t));
            if (b->file == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

            b->file_pos = (off_t)start;
            b->file_last = (off_t)end;

            b->in_file = b->file_last ? 1: 0;
            b->last_buf = 1;
            b->last_in_chain = 1;

            b->file->fd = of.fd;
            b->file->name = path;
            b->file->log = log;
            b->file->directio = of.is_directio;

            out[j].buf = b;
            out[j].next = NULL;
        } else {
            b->last_buf = 1;
            b->last_in_chain = 1;	
            out[--j].next = NULL;
        }	
    } else {
        r->headers_out.content_length_n = end -start;

        b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
        if (b == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        b->file = ngx_pcalloc(r->pool, sizeof(ngx_file_t));
        if (b->file == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        r->allow_ranges = 1;
        rc = ngx_http_send_header(r);
        if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
            return rc;
        }

        b->file_pos = (off_t)start;
        b->file_last = (off_t)end;

        b->in_file = b->file_last ? 1: 0;
        b->last_buf = 1;
        b->last_in_chain = 1;

        b->file->fd = of.fd;
        b->file->name = path;
        b->file->log = log;
        b->file->directio = of.is_directio;

        out[0].buf = b;
        out[0].next = NULL;
    } 

    return ngx_http_output_filter(r, &out[i]);
}


static ngx_int_t
ngx_http_tflv_handler(ngx_http_request_t *r)
{
    u_char                    *last;
    double                     start =0,end =0, len;
    size_t                     root;
    ngx_int_t                  rc;
    ngx_uint_t                 level,  i,j;
    ngx_str_t                  path, value;
    ngx_log_t                 *log;
    ngx_buf_t                 *b;
    ngx_chain_t                out[5];
    ngx_open_file_info_t       of;
    ngx_http_core_loc_conf_t  *clcf;
    char send_metadata_buf[NGX_FLV_METADATALEN] = {0};
    char send_tH264VideoTag_buf[NGX_FLV_METADATALEN] = {0};
    char send_tH264AudioTag_buf[NGX_FLV_METADATALEN] = {0};
    ngx_int_t  video_size =0;
    ngx_int_t  audio_size =0;

    ngx_int_t i_have_start = 0;
    ngx_int_t i_have_end = 0;

    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    if (r->uri.data[r->uri.len - 1] == '/') {
        return NGX_DECLINED;
    }
    rc = ngx_http_discard_request_body(r);

    if (rc != NGX_OK) {
        return rc;
    }

    last = ngx_http_map_uri_to_path(r, &path, &root, 0);
    if (last == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    log = r->connection->log;

    path.len = last - path.data;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                   "http flv filename: \"%V\"", &path);

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    ngx_memzero(&of, sizeof(ngx_open_file_info_t));

    of.read_ahead = clcf->read_ahead;
    of.directio = clcf->directio;
    of.valid = clcf->open_file_cache_valid;
    of.min_uses = clcf->open_file_cache_min_uses;
    of.errors = clcf->open_file_cache_errors;
    of.events = clcf->open_file_cache_events;

    if (ngx_open_cached_file(clcf->open_file_cache, &path, &of, r->pool)
        != NGX_OK)
    {
        switch (of.err) {

        case 0:
            return NGX_HTTP_INTERNAL_SERVER_ERROR;

        case NGX_ENOENT:
        case NGX_ENOTDIR:
        case NGX_ENAMETOOLONG:

            level = NGX_LOG_ERR;
            rc = NGX_HTTP_NOT_FOUND;
            break;

        case NGX_EACCES:

            level = NGX_LOG_ERR;
            rc = NGX_HTTP_FORBIDDEN;
            break;

        default:

            level = NGX_LOG_CRIT;
            rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
            break;
        }

        if (rc != NGX_HTTP_NOT_FOUND || clcf->log_not_found) {
            ngx_log_error(level, log, of.err,
                          "%s \"%s\" failed", of.failed, path.data);
        }


        return rc;
    }

    if (!of.is_file) {

        if (ngx_close_file(of.fd) == NGX_FILE_ERROR) {
            ngx_log_error(NGX_LOG_ALERT, log, ngx_errno,
                          ngx_close_file_n " \"%s\" failed", path.data);
        }

        return NGX_DECLINED;
    }

    r->root_tested = !r->error_page;

    start = 0;
    len = of.size;
    i = 0;
    j = 0;

        if (ngx_http_arg(r, (u_char *) "start", 5, &value) == NGX_OK) {

            i_have_start = 1;

            start = ngx_atoint(value.data, value.len);

            if (start > len){
                return NGX_DECLINED;
            }

            if (start == NGX_ERROR) {
                start = 0;
            }

        }


        end = len;
        if (ngx_http_arg(r, (u_char *) "end", 3, &value) == NGX_OK) {
            i_have_end = 1;

            end = ngx_atoint(value.data, value.len);
            if (end == NGX_ERROR || (end > len)) {
                i_have_end = 0;
                end = len;
            }
        }

        if ((0 == i_have_start) && (0 == i_have_end)){
        }

        ngx_int_t meta_size = ntx_http_eflv_metadata(of.fd,&start,&end,i_have_end,len,send_metadata_buf,send_tH264VideoTag_buf,send_tH264AudioTag_buf,&video_size,&audio_size, r);

        log->action = "sending tflv to client";
        r->headers_out.status = NGX_HTTP_OK;
        r->headers_out.last_modified_time = of.mtime;
        if (ngx_http_set_content_type(r) != NGX_OK) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
        if (b == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        b->pos = ngx_flv_header;
        b->last = ngx_flv_header + sizeof(ngx_flv_header) - 1;
        b->memory = 1;

        out[j].buf = b;
        out[j].next = &out[j+1];
        j++;

        b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
        b->pos = (u_char*)send_metadata_buf;
        b->last = (u_char*)(send_metadata_buf + meta_size);
        b->memory = 1;
        out[j].buf = b;
        out[j].next = &out[j+1];
        j++;

        if (video_size !=0){
                b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
                b->pos = (u_char*)send_tH264VideoTag_buf;
                b->last = (u_char*)(send_tH264VideoTag_buf + video_size);
                b->memory = 1;
                out[j].buf = b;
                out[j].next = &out[j+1];
                j++;
        }

        if (audio_size !=0){
                b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
                b->pos = (u_char*)send_tH264AudioTag_buf;
                b->last = (u_char*)(send_tH264AudioTag_buf + audio_size);
                b->memory = 1;
                out[j].buf = b;
                out[j].next = &out[j+1];
                j++;
        }

        r->headers_out.content_length_n =sizeof(ngx_flv_header) - 1 + meta_size + end -start + video_size + audio_size;

        b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
        if (b == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        b->file = ngx_pcalloc(r->pool, sizeof(ngx_file_t));
        if (b->file == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        r->allow_ranges = 1;
        rc = ngx_http_send_header(r);
        if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
                return rc;
        }

        b->file_pos = (off_t)start;
        b->file_last = (off_t)end;

        b->in_file = b->file_last ? 1: 0;
        b->last_buf = 1;
        b->last_in_chain = 1;

        b->file->fd = of.fd;
        b->file->name = path;
        b->file->log = log;
        b->file->directio = of.is_directio;

        out[j].buf = b;
        out[j].next = NULL;

        return ngx_http_output_filter(r, &out[i]);
}


static char *
ngx_http_tflv(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_tflv_handler;

    return NGX_CONF_OK;
}


static char *
ngx_http_sflv(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_sflv_handler;

    return NGX_CONF_OK;
}
