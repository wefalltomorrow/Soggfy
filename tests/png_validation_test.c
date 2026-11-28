#include "../native/png_validation.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc,char **argv) {
    SgfPngValidation result;
    memset(&result,0xff,sizeof(result));
    assert(!SgfValidatePng(NULL,0,&result));
    assert(!result.width&&!result.height&&!result.peak_decoder_bytes);
    assert(!SgfValidatePng(NULL,0,NULL));
    if(argc==1){puts("PASS: PNG failure clears output and handles null input");return 0;}
    if(argc!=2)return 2;
    FILE *f=fopen(argv[1],"rb");if(!f)return 2;
    if(fseek(f,0,SEEK_END)){fclose(f);return 2;}
    const long length=ftell(f);
    if(length<0||length>4*1024*1024){fclose(f);return 1;}
    rewind(f);
    unsigned char *data=(unsigned char*)malloc(length?(size_t)length:1);
    if(!data){fclose(f);return 2;}
    if(fread(data,1,(size_t)length,f)!=(size_t)length){free(data);fclose(f);return 2;}
    fclose(f);
    const int valid=SgfValidatePng(data,(size_t)length,&result);
    /* A second call must not inherit successful dimensions or failure state. */
    SgfPngValidation second;memset(&second,0xff,sizeof(second));
    assert(SgfValidatePng(data,(size_t)length,&second)==valid);
    assert(result.width==second.width&&result.height==second.height&&
           result.bit_depth==second.bit_depth&&result.channels==second.channels&&
           result.peak_decoder_bytes==second.peak_decoder_bytes);
    free(data);
    if(!valid) {
        assert(!result.width&&!result.height&&!result.bit_depth&&!result.channels&&!result.peak_decoder_bytes);
        return 1;
    }
    assert(result.peak_decoder_bytes>0&&result.peak_decoder_bytes<=32*1024*1024);
    printf("%u %u %u %u\n",result.width,result.height,result.bit_depth,result.channels);
    return 0;
}
