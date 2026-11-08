#include "png_validation.h"
#include <png.h>
#include <zlib.h>
#include <stdlib.h>
#include <string.h>

enum { InputLimit=4*1024*1024, DimensionLimit=8192, PixelLimit=16*1024*1024,
       MemoryLimit=32*1024*1024, ChunkLimit=4096 };
typedef struct {
    const uint8_t *data;
    size_t size, at, allocated, peak;
} Input;
/* Preserve malloc alignment for both decoder libraries. */
typedef union { max_align_t alignment; size_t bytes; } Allocation;

static uint32_t number(const uint8_t *p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
static int kind(const uint8_t *p,const char *s) { return memcmp(p,s,4)==0; }
/* libpng is a decoder, not an envelope-policy oracle. Bound the complete byte
 * extent and chunk count first, including unknown ancillary chunks. APNG is
 * deliberately unsupported: decoding only its default image is insufficient. */
static int envelope(const uint8_t *p,size_t size) {
    static const uint8_t signature[]={137,80,78,71,13,10,26,10};
    if(!p||size<8||size>InputLimit||memcmp(p,signature,8))return 0;
    size_t at=8,compressed=0;unsigned chunks=0;
    int header=0,idat=0,ended=0;
    while(at<size) {
        if(++chunks>ChunkLimit||size-at<12)return 0;
        const uint32_t n=number(p+at);const uint8_t *type=p+at+4;
        if(n>size-at-12)return 0;
        for(unsigned i=0;i<4;++i)
            if(!((type[i]>='A'&&type[i]<='Z')||(type[i]>='a'&&type[i]<='z')))return 0;
        if(type[2]&32)return 0;
        if((uint32_t)crc32(0,type,n+4)!=number(type+4+n))return 0;
        if(!header&&!kind(type,"IHDR"))return 0;
        if(kind(type,"IHDR")) {
            if(header||n!=13)return 0;
            const uint32_t w=number(type+4),h=number(type+8);
            if(!w||!h||w>DimensionLimit||h>DimensionLimit||(uint64_t)w*h>PixelLimit)return 0;
            header=1;
        } else if(kind(type,"IDAT")) {
            if(ended)return 0;
            idat=1;compressed+=n;
        } else {
            if(idat)ended=1;
            if(kind(type,"IEND"))return n==0&&idat&&compressed!=0&&at+12==size;
            if(kind(type,"acTL")||kind(type,"fcTL")||kind(type,"fdAT"))return 0;
            if(!(type[0]&32)&&!kind(type,"PLTE"))return 0;
        }
        at+=12+n;
    }
    return 0;
}
static png_voidp allocate(png_structp p,png_alloc_size_t size) {
    Input *in=(Input*)png_get_mem_ptr(p);
    if(size>MemoryLimit-sizeof(Allocation)||in->allocated>MemoryLimit-sizeof(Allocation)-size)return NULL;
    const size_t total=(size_t)size+sizeof(Allocation);
    Allocation *a=(Allocation*)malloc(total);if(!a)return NULL;
    a->bytes=total;in->allocated+=total;
    if(in->allocated>in->peak)in->peak=in->allocated;
    return a+1;
}
static void release(png_structp p,png_voidp memory) {
    if(!memory)return;
    Input *in=(Input*)png_get_mem_ptr(p);Allocation *a=((Allocation*)memory)-1;
    in->allocated-=a->bytes;free(a);
}
static void reject(png_structp p,png_const_charp message) {
    (void)message;png_longjmp(p,1);
}
static void read_bytes(png_structp p,png_bytep out,png_size_t n) {
    Input *in=(Input*)png_get_io_ptr(p);
    if(n>in->size-in->at)png_error(p,"truncated input");
    memcpy(out,in->data+in->at,n);in->at+=n;
}
/* Keep setjmp in C. No C++ destructors or modified automatic cleanup pointers
 * cross the longjmp. Mutable allocation/input state belongs to the caller. */
static int decode(Input *in,SgfPngValidation *output) {
    png_structp p=png_create_read_struct_2(PNG_LIBPNG_VER_STRING,NULL,reject,reject,in,allocate,release);
    if(!p)return 0;
    png_infop info=png_create_info_struct(p);
    if(!info){png_destroy_read_struct(&p,NULL,NULL);return 0;}
    if(setjmp(png_jmpbuf(p))) {png_destroy_read_struct(&p,&info,NULL);return 0;}
    png_set_read_fn(p,in,read_bytes);
    png_set_user_limits(p,DimensionLimit,DimensionLimit);
    png_set_chunk_malloc_max(p,InputLimit);
    png_set_chunk_cache_max(p,ChunkLimit);
    png_set_crc_action(p,PNG_CRC_ERROR_QUIT,PNG_CRC_ERROR_QUIT);
    png_set_benign_errors(p,0);
    png_set_check_for_invalid_index(p,1);
    png_read_info(p,info);
    const int passes=png_set_interlace_handling(p);
    png_read_update_info(p,info);
    const png_uint_32 width=png_get_image_width(p,info),height=png_get_image_height(p,info);
    for(int pass=0;pass<passes;++pass)
        for(png_uint_32 y=0;y<height;++y)png_read_row(p,NULL,NULL);
    png_read_end(p,info);
    if(in->at!=in->size)png_error(p,"trailing bytes");
    SgfPngValidation found={width,height,png_get_bit_depth(p,info),png_get_channels(p,info),in->peak};
    png_destroy_read_struct(&p,&info,NULL);
    if(in->allocated)return 0;
    *output=found;return 1;
}
int SgfValidatePng(const uint8_t *bytes,size_t size,SgfPngValidation *output) {
    if(!output)return 0;
    memset(output,0,sizeof(*output));
    if(!envelope(bytes,size))return 0;
    Input in={bytes,size,0,0,0};
    return decode(&in,output);
}
