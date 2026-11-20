#include "../native/jpeg_validation.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void empty(const SgfJpegValidation *r) {
    assert(!r->width&&!r->height&&!r->channels&&!r->progressive&&!r->peak_decoder_bytes);
}
int main(int argc,char **argv) {
    SgfJpegValidation result;memset(&result,0xff,sizeof(result));
    assert(!SgfValidateJpeg(NULL,0,&result));empty(&result);
    assert(!SgfValidateJpeg(NULL,0,NULL));
    if(argc==1){puts("PASS: JPEG null input and output clearing");return 0;}
    if(argc!=2)return 2;
    FILE *f=fopen(argv[1],"rb");if(!f)return 2;
    if(fseek(f,0,SEEK_END)){fclose(f);return 2;}
    const long length=ftell(f);
    if(length<0||length>4*1024*1024){fclose(f);return 1;}
    rewind(f);unsigned char *data=(unsigned char*)malloc(length?(size_t)length:1);
    if(!data){fclose(f);return 2;}
    if(fread(data,1,(size_t)length,f)!=(size_t)length){free(data);fclose(f);return 2;}
    fclose(f);
    const int valid=SgfValidateJpeg(data,(size_t)length,&result);
    SgfJpegValidation second;memset(&second,0xff,sizeof(second));
    assert(SgfValidateJpeg(data,(size_t)length,&second)==valid);
    assert(result.width==second.width&&result.height==second.height&&result.channels==second.channels&&
           result.progressive==second.progressive&&result.peak_decoder_bytes==second.peak_decoder_bytes);
    free(data);
    if(!valid){empty(&result);empty(&second);return 1;}
    assert(result.width&&result.height&&result.width<=8192&&result.height<=8192);
    assert((uint64_t)result.width*result.height<=16*1024*1024);
    assert(result.channels==1||result.channels==3||result.channels==4);
    assert(result.peak_decoder_bytes&&result.peak_decoder_bytes<=32*1024*1024);
    printf("%u %u %u %u\n",result.width,result.height,result.channels,result.progressive);
    return 0;
}
