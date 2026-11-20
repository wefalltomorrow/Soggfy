#define JPEG_INTERNALS
#include "jpeg_validation.h"
#include "jinclude.h"
#include "jpeglib.h"
#include "jmemsys.h"
#include <setjmp.h>

enum { InputLimit=4*1024*1024,DimensionLimit=8192,PixelLimit=16*1024*1024,
       MemoryLimit=32*1024*1024,MarkerLimit=4096,ScanLimit=128 };
typedef struct {
    struct jpeg_decompress_struct decoder;
    struct jpeg_error_mgr error;
    jmp_buf jump;
    size_t allocated,peak;
} Context;
typedef union { max_align_t alignment; size_t bytes; } Allocation;

/* Replace the library's system allocator, including its initial control block.
 * max_memory_to_use alone only advises virtual-array allocation. No globals,
 * temporary backing files, environment-selected limits or untracked pools. */
static void *allocate(j_common_ptr decoder,size_t bytes) {
    Context *c=(Context*)decoder->client_data;
    if(bytes>MemoryLimit-sizeof(Allocation)||c->allocated>MemoryLimit-sizeof(Allocation)-bytes)return NULL;
    Allocation *a=(Allocation*)malloc(sizeof(Allocation)+bytes);if(!a)return NULL;
    a->bytes=sizeof(Allocation)+bytes;c->allocated+=a->bytes;
    if(c->allocated>c->peak)c->peak=c->allocated;
    return a+1;
}
static void release(j_common_ptr decoder,void *memory) {
    if(!memory)return;
    Context *c=(Context*)decoder->client_data;Allocation *a=(Allocation*)memory-1;
    c->allocated-=a->bytes;free(a);
}
void *jpeg_get_small(j_common_ptr d,size_t n){return allocate(d,n);}
void *jpeg_get_large(j_common_ptr d,size_t n){return allocate(d,n);}
void jpeg_free_small(j_common_ptr d,void *p,size_t n){(void)n;release(d,p);}
void jpeg_free_large(j_common_ptr d,void *p,size_t n){(void)n;release(d,p);}
size_t jpeg_mem_available(j_common_ptr d,size_t minimum,size_t maximum,size_t used) {
    Context *c=(Context*)d->client_data;(void)minimum;(void)used;
    const size_t available=MemoryLimit-c->allocated;
    return maximum<available?maximum:available;
}
void jpeg_open_backing_store(j_common_ptr d,backing_store_ptr info,long bytes) {
    (void)info;(void)bytes;ERREXIT(d,JERR_NO_BACKING_STORE);
}
long jpeg_mem_init(j_common_ptr d){(void)d;return 0;}
void jpeg_mem_term(j_common_ptr d){(void)d;}
static void reject(j_common_ptr d){Context *c=(Context*)d->client_data;longjmp(c->jump,1);}
static void message(j_common_ptr d,int level){if(level<0)reject(d);}
static unsigned number(const uint8_t *p){return ((unsigned)p[0]<<8)|p[1];}

/* Bound marker and scan work before decoding. Only single-frame 8-bit Huffman
 * sequential/progressive images are supported. Unknown APP/COM data is opaque;
 * this does not validate EXIF/ICC metadata or multi-picture semantics. */
static int envelope(const uint8_t *p,size_t size) {
    if(!p||size<4||size>InputLimit||p[0]!=255||p[1]!=0xd8)return 0;
    size_t at=2;unsigned markers=0,scans=0,dc_tables=0,ac_tables=0;
    int frame=0,entropy=0,progressive=0;
    while(at<size) {
        if(entropy) {
            while(at<size) {
                if(p[at]!=255){++at;continue;}
                const size_t start=at++;
                while(at<size&&p[at]==255)++at;
                if(at==size)return 0;
                const unsigned code=p[at];
                if(code==0){if(at!=start+1)return 0;++at;continue;}
                if(code>=0xd0&&code<=0xd7){if(++markers>MarkerLimit)return 0;++at;continue;}
                at=start;entropy=0;break;
            }
            if(at==size)return 0;
        }
        if(++markers>MarkerLimit||p[at++]!=255)return 0;
        while(at<size&&p[at]==255)++at;
        if(at==size)return 0;
        const unsigned code=p[at++];
        if(code==0xd9)return frame&&scans&&at==size;
        if(!(code==0xc0||code==0xc1||code==0xc2||code==0xc4||code==0xdb||
             code==0xdd||code==0xda||code==0xfe||(code>=0xe0&&code<=0xef)))return 0;
        if(size-at<2)return 0;
        const unsigned length=number(p+at);
        if(length<2||length>size-at)return 0;
        if(code==0xc0||code==0xc1||code==0xc2) {
            if(frame||length<8||p[at+2]!=8)return 0;
            const unsigned h=number(p+at+3),w=number(p+at+5),components=p[at+7];
            if(!w||!h||w>DimensionLimit||h>DimensionLimit||(uint64_t)w*h>PixelLimit||
               (components!=1&&components!=3&&components!=4)||length!=8+3*components)return 0;
            frame=1;progressive=code==0xc2;
        } else if(code==0xc4) {
            size_t table=at+2;
            while(table<at+length) {
                if(at+length-table<17)return 0;
                const unsigned type=p[table++],index=type&15;
                if(index>3||(type>>4)>1)return 0;
                unsigned symbols=0;
                for(unsigned i=0;i<16;++i)symbols+=p[table++];
                if(!symbols||symbols>256||symbols>at+length-table)return 0;
                table+=symbols;
                if(type>>4)ac_tables|=1u<<index;else dc_tables|=1u<<index;
            }
        } else if(code==0xda) {
            if(!frame||++scans>ScanLimit||length<6||length!=6u+2u*p[at+2])return 0;
            const unsigned components=p[at+2];
            if(!components||components>4)return 0;
            const unsigned spectral=p[at+3+2*components],high=p[at+5+2*components]>>4;
            for(unsigned i=0;i<components;++i) {
                const unsigned selector=p[at+4+2*i],dc=selector>>4,ac=selector&15;
                if(dc>3||ac>3)return 0;
                /* libjpeg can synthesize default MJPEG Huffman tables. Standalone
                 * artwork must carry every table used at this scan boundary. */
                if((!progressive||(spectral==0&&high==0))&&!(dc_tables&(1u<<dc)))return 0;
                if((!progressive||spectral!=0)&&!(ac_tables&(1u<<ac)))return 0;
            }
            entropy=1;
        } else if(code==0xdd&&length!=4)return 0;
        at+=length;
    }
    return 0;
}
static int decode(Context *c,const uint8_t *data,size_t size,SgfJpegValidation *output) {
    c->decoder.err=jpeg_std_error(&c->error);
    c->error.error_exit=reject;c->error.emit_message=message;c->decoder.client_data=c;
    if(setjmp(c->jump)){jpeg_destroy_decompress(&c->decoder);return 0;}
    jpeg_create_decompress(&c->decoder);
    jpeg_mem_src(&c->decoder,data,(unsigned long)size);
    if(jpeg_read_header(&c->decoder,TRUE)!=JPEG_HEADER_OK||c->decoder.data_precision!=8||
       c->decoder.arith_code||!jpeg_start_decompress(&c->decoder))reject((j_common_ptr)&c->decoder);
    JSAMPARRAY row=(*c->decoder.mem->alloc_sarray)((j_common_ptr)&c->decoder,JPOOL_IMAGE,
        c->decoder.output_width*c->decoder.output_components,1);
    while(c->decoder.output_scanline<c->decoder.output_height)
        if(jpeg_read_scanlines(&c->decoder,row,1)!=1)reject((j_common_ptr)&c->decoder);
    if(!jpeg_finish_decompress(&c->decoder)||c->decoder.src->bytes_in_buffer)reject((j_common_ptr)&c->decoder);
    SgfJpegValidation found={c->decoder.output_width,c->decoder.output_height,
        (uint32_t)c->decoder.output_components,c->decoder.progressive_mode?1u:0u,c->peak};
    jpeg_destroy_decompress(&c->decoder);
    if(c->allocated)return 0;
    *output=found;return 1;
}
int SgfValidateJpeg(const uint8_t *data,size_t size,SgfJpegValidation *output) {
    if(!output)return 0;
    memset(output,0,sizeof(*output));
    if(!envelope(data,size))return 0;
    Context *c=(Context*)calloc(1,sizeof(*c));if(!c)return 0;
    const int result=decode(c,data,size,output);free(c);return result;
}
